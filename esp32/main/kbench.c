/* kbench.c - Experiment 2: isolated CQ2 pair-table GEMV microbenchmark.
 *
 * Built into the app only with -DNEEDLE_KBENCH=ON, where it replaces the
 * request loop in app_main(). It never touches the model caches, the KV state
 * or the tier: it maps the model partition, copies the real index blob of the
 * real 2-bit projections into PSRAM the way the tier does, builds the real pair
 * table from a prepared activation, and then times nd_lut2_rows_c against the
 * handwritten kernels in engine/src/lut2_tie728.S on the device.
 *
 * Every number is device-side: the CPU cycle counter brackets one row range, so
 * nothing on the host can inflate it. Output is line-oriented for the batch
 * runner:
 *
 *   KB CFG   build, blob placement, geometry, asm preconditions
 *   KB R     one timed round per kernel (raw evidence)
 *   KB NUM   exact-row match and max/mean numeric error of a kernel vs the C C
 *   KSUM     min/median/mean cycles per kernel per mode
 *   KB DELTA candidate vs control, and cycles per inner-loop word
 *   KB PROBE measured cost of add.s / dependent add.s / load-then-dependent-add
 *   KB GDMA  Experiment 15: PSRAM->internal GDMA copy, overlapped pipeline,
 *            cache/DMA coherency, and the heap the double buffer costs
 *   EVT KBENCH_DONE
 */
/* Experiment 15's GDMA double-buffer bench needs esp_cache.h and
 * esp_async_memcpy.h, which the main component does not (and must not) require:
 * an IDF component dependency belongs in idf_component_register's REQUIRES, and
 * adding one for a closed diagnostic would move the shipping image. So the GDMA
 * bench is compiled out by default; .auto/exp15/kbench.c.with_gdma_bench is the
 * copy that ran it, and reproducing those numbers needs esp_mm esp_hw_support in
 * PRIV_REQUIRES of a throwaway checkout. */
#ifndef ND_KBENCH_GDMA
#define ND_KBENCH_GDMA 0
#endif

#include <math.h>
#include <stdio.h>
#include <esp_attr.h>              /* IRAM_ATTR, for the callee-placement bench */
#include <string.h>

#if ND_KBENCH_GDMA
#include "esp_async_memcpy.h"
#include "esp_cache.h"
#endif
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "nd_cact.h"
#include "nd_model.h"
#include "nd_quant.h"

#ifndef ND_KBENCH_ASM
#define ND_KBENCH_ASM 0
#endif


/* The assembly ABI: engine/src/lut2_tie728.S hard-codes these offsets. */
_Static_assert(offsetof(nd_lut2_ctx, packed)   == 0,  "tie728 asm ABI: packed");
_Static_assert(offsetof(nd_lut2_ctx, norms)    == 4,  "tie728 asm ABI: norms");
_Static_assert(offsetof(nd_lut2_ctx, lut)      == 8,  "tie728 asm ABI: lut");
_Static_assert(offsetof(nd_lut2_ctx, y)        == 12, "tie728 asm ABI: y");
_Static_assert(offsetof(nd_lut2_ctx, ngroup)   == 16, "tie728 asm ABI: ngroup");
_Static_assert(offsetof(nd_lut2_ctx, gbytes)   == 20, "tie728 asm ABI: gbytes");
_Static_assert(offsetof(nd_lut2_ctx, gpairs)   == 24, "tie728 asm ABI: gpairs");
_Static_assert(offsetof(nd_lut2_ctx, g)        == 28, "tie728 asm ABI: g");
_Static_assert(offsetof(nd_lut2_ctx, rowbytes) == 32, "tie728 asm ABI: rowbytes");

void nd_lut2_probe_add_thru(const float *src, uint32_t n);
void nd_lut2_stage(void *ctx, uint32_t mode);

void nd_lut2_probe_add_lat(const float *src, uint32_t n);
void nd_lut2_probe_load_lat(const float *src, uint32_t n);

#define KB_ROUNDS 25      /* timed repetitions per kernel per mode     */
#define KB_WARMUP 3       /* untimed repetitions before each mode      */

/* The 2-bit CQ2 shapes decode actually runs, read out of the blob directory:
 * 768x768 x20 (out_proj, gate_proj, ...), 576x768 x8 (q_proj, 12 heads x 48),
 * 128x768 x8 (v_proj, 2 kv heads x 64), 96x768 x8 (k_proj, 2 kv heads x 48).
 * All of them have in_pad 768 and group 128: one 24 KiB pair table per token,
 * 48 packed index words per row. */
typedef struct { const char *tag; uint32_t rows; } kb_shape;
static const kb_shape SHAPES[] = {
    { "768x768", 768 }, { "576x768", 576 }, { "128x768", 128 }, { "96x768", 96 },
};
#define KB_NSHAPES (sizeof(SHAPES) / sizeof(SHAPES[0]))

/* Instructions per inner-loop word (one 32-bit packed index word = 8 nibble
 * lookups) counted from the disassembly, used to report cycles-per-instruction
 * instead of just cycles. C is GCC's body as linked in the accepted image. */
#define IPC_C     39.9f
#define IPC_TIE1  36.4f
#define IPC_TIE2  36.6f

/* Experiment 20 adds the cold modes. The warm modes answer "can the loop go
 * faster if its operands are already in the cache"; the cold modes answer the
 * question decode actually asks, because a decode token sweeps 3.59 MB of 2-bit
 * weights through a 32 KiB cache and nothing is warm when it is read. Each cold
 * round evicts the data cache with a 96 KiB PSRAM sweep before the timed pass,
 * outside the cycle bracket.
 *   cold_psram     blob in PSRAM, pair table in internal RAM  = shipping
 *   cold_blob_int  the same blob copied to internal RAM       = weight residency
 *   cold_tab_ps    the shipping blob, but the pair table in PSRAM = the ablation
 *                  that prices the table residency the shipping build already
 *                  gets for free (m->lut is ND_ALLOC_FAST = internal). */
typedef enum { KB_ISO = 0, KB_SPLIT, KB_FASTBLOB, KB_COLD_PSRAM,
               KB_COLD_INT, KB_COLD_TAB, KB_NMODES } kb_mode;
static const char MODE_NAME[KB_NMODES][14] = {
    "isolated", "split", "blob_int", "cold_psram", "cold_blob_int", "cold_tab_ps" };

/* 96 KiB > 3x the 32 KiB data cache, so one sweep cannot leave anything useful
 * behind. Summed into a volatile sink so the loads are real. */
#define KB_THRASH_FLOATS 24576u
static float *s_thrash;

static void kb_thrash(void)
{
    volatile float sink = 0.0f;
    uint32_t       i;

    for (i = 0; i < KB_THRASH_FLOATS; i++)
        sink += s_thrash[i];
}

typedef struct {
    const char *tag;
    nd_row_fn   fn;
    float       ipc;
} kb_kernel;

static nd_cact        s_c;
static const uint8_t *s_base;
static uint32_t       s_mhz_x100;
static int            s_nkern;
static kb_kernel      s_kern[6];

/* ---- second core, so a measurement can look like production ---------------
 * The firmware splits every row range in half and runs the upper half on core 1
 * inside the same call (main.c's rows_dual_core). A single-core timing does not
 * see that memory traffic, so the split mode repeats every measurement through
 * the same shape of handshake. */
static SemaphoreHandle_t s_go, s_done;
static nd_row_fn         s_fn;
static void             *s_ctx;
static uint32_t          s_r0, s_r1;

static void worker_task(void *arg)
{
    (void)arg;
    for (;;) {
        xSemaphoreTake(s_go, portMAX_DELAY);
        s_fn(s_ctx, s_r0, s_r1);
        xSemaphoreGive(s_done);
    }
}

static void split_rows(nd_row_fn fn, void *ctx, uint32_t nrows)
{
    uint32_t half = nrows / 2;

    if (half < 2) {
        fn(ctx, 0, nrows);
        return;
    }
    s_fn  = fn;
    s_ctx = ctx;
    s_r0  = half;
    s_r1  = nrows;
    xSemaphoreGive(s_go);
    fn(ctx, 0, half);
    xSemaphoreTake(s_done, portMAX_DELAY);
}

static uint32_t time_one(nd_row_fn fn, void *ctx, uint32_t n, kb_mode mode)
{
    uint32_t c0 = esp_cpu_get_cycle_count();

    if (mode == KB_SPLIT)
        split_rows(fn, ctx, n);
    else
        fn(ctx, 0, n);
    return esp_cpu_get_cycle_count() - c0;
}

static int cmp_u32(const void *a, const void *b)
{
    const uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;

    return x < y ? -1 : (x > y ? 1 : 0);
}

/* Find the first CQ2 tensor with this many output rows and d_model inputs. */
static int find_tensor(uint32_t rows, nd_tensor *t)
{
    uint32_t i;

    for (i = 0; i < s_c.n; i++) {
        if (nd_cact_tensor(&s_c, i, t) != 0)
            continue;
        if (t->dtype == ND_DT_CQ && t->bits == 2u &&
            t->shape[0] == rows && t->shape[1] == s_c.h.d_model)
            return (int)i;
    }
    return -1;
}

/* Byte-exact comparison of a candidate kernel against the C result in yref. */
/* Synthetic geometry: with a hand-built table the expected output is arithmetic
 * anyone can do by hand, so a disagreement says which *positions* of the pair
 * table a kernel reads, not just that the values differ.
 *
 *   fill=1.0        -> y = groups x 64 terms (term count)
 *   fill=pos        -> y = groups x sum of the 64 positions read (position map)
 *   fill=slot       -> y = groups x sum of the 4-bit slot read (nibble map)
 *   int_norms       -> table 1.0, norms 1..24: exposes the norm stride and order
 */
static void synth_check2(const char *mode, float (*fill)(uint32_t pos, uint32_t slot),
                         int int_norms)
{
    enum { OUT = 4, NG = 6 };
    nd_lut2_ctx c;
    float      *tab, *yc, *ya;
    uint8_t    *blob;
    uint16_t   *nrm;
    uint32_t    r, k;

    tab  = heap_caps_malloc(NG * 64 * 16 * sizeof(float), MALLOC_CAP_SPIRAM);
    blob = heap_caps_malloc(OUT * NG * 32, MALLOC_CAP_SPIRAM);
    nrm  = heap_caps_malloc(OUT * NG * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    yc   = heap_caps_malloc(OUT * sizeof(float), MALLOC_CAP_SPIRAM);
    ya   = heap_caps_malloc(OUT * sizeof(float), MALLOC_CAP_SPIRAM);
    if (tab == NULL || blob == NULL || nrm == NULL || yc == NULL || ya == NULL) {
        printf("KB SYNTH mode=%s alloc_failed\n", mode);
        fflush(stdout);
        return;
    }
    for (uint32_t pos = 0; pos < NG * 64; pos++)
        for (uint32_t slot = 0; slot < 16; slot++)
            tab[pos * 16 + slot] = fill(pos, slot);
    for (r = 0; r < OUT * NG * 32; r++)
        blob[r] = (uint8_t)(0x27u * (r + 1u));      /* mixed nibbles, no aliasing */
    if (int_norms != 0) {
        /* FP16 of 1,2,3,... so a wrong norm stride or a wrong norm order moves
         * the result by a lot, not by a rounding step. */
        static const uint16_t H[24] = {
            0x3C00u, 0x4000u, 0x4200u, 0x4400u, 0x4500u, 0x4600u, 0x4700u, 0x4800u,
            0x4880u, 0x4900u, 0x4980u, 0x4A00u, 0x4A80u, 0x4B00u, 0x4B80u, 0x4C00u,
            0x4C40u, 0x4C80u, 0x4CC0u, 0x4D00u, 0x4D40u, 0x4D80u, 0x4DC0u, 0x4E00u,
        };

        for (r = 0; r < (uint32_t)OUT * NG; r++)
            nrm[r] = H[r];
    } else {
        for (r = 0; r < OUT * NG; r++)
            nrm[r] = 0x3C00u;                        /* FP16 1.0 */
    }

    c.packed   = blob;
    c.norms    = nrm;
    c.lut      = tab;
    c.y        = yc;
    c.ngroup   = NG;
    c.g        = 128;
    c.gbytes   = 32;
    c.gpairs   = 64;
    c.rowbytes = NG * 32;

    nd_lut2_rows_c(&c, 0, OUT);
    for (k = 1; k < (uint32_t)s_nkern; k++) {
        c.y = ya;
        s_kern[k].fn(&c, 0, OUT);
        c.y = yc;
        printf("KB SYNTH mode=%s kernel=%s c=%.6g/%.6g/%.6g/%.6g got=%.6g/%.6g/%.6g/%.6g\n",
               mode, s_kern[k].tag, (double)yc[0], (double)yc[1], (double)yc[2],
               (double)yc[3], (double)ya[0], (double)ya[1], (double)ya[2],
               (double)ya[3]);
    }
    free(tab); free(blob); free(nrm); free(yc); free(ya);
    fflush(stdout);
}

static float f_units(uint32_t pos, uint32_t slot) { (void)pos; (void)slot; return 1.0f; }
static float f_pos(uint32_t pos, uint32_t slot)   { (void)slot; return (float)pos; }
static float f_slot(uint32_t pos, uint32_t slot)  { (void)pos; return (float)slot; }

static void num_check(const char *tag, const kb_kernel *kk, nd_lut2_ctx *cx,
                      uint32_t out, const float *yref, float *ytmp)
{
    uint32_t r, exact = 0, bad = 0;
    float    maxabs = 0.0f, sum = 0.0f;

    memset(ytmp, 0xA5, sizeof(float) * out);  /* an unwritten row cannot pass */
    cx->y = ytmp;
    kk->fn(cx, 0, out);
    cx->y = (float *)yref;

    for (r = 0; r < out; r++) {
        float d = fabsf(ytmp[r] - yref[r]);

        if (memcmp(&ytmp[r], &yref[r], sizeof(float)) == 0) {
            exact++;
        } else if (exact < 8 && bad < 4) {
            printf("KB NUMBAD shape=%s kernel=%s r=%u ref=%.9g got=%.9g\n",
                   tag, kk->tag, (unsigned)r, (double)yref[r], (double)ytmp[r]);
            bad++;
        }
        if (d > maxabs)
            maxabs = d;
        sum += d;
    }
    printf("KB NUM shape=%s kernel=%s exact=%u/%u bitexact=%d maxabs=%.3e "
           "meanabs=%.3e\n", tag, kk->tag, (unsigned)exact, (unsigned)out,
           exact == out ? 1 : 0, maxabs, out ? sum / (float)out : 0.0f);
    fflush(stdout);
}

/* Length of the archive in the partition, the way main.c computes it: header,
 * codebook and directory, then the last record's end. */
static uint32_t le32b(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

static size_t blob_length(const esp_partition_t *part)
{
    uint8_t  hdr[12], last[44];
    uint32_t count, codebooks, record_at;

    if (esp_partition_read(part, 0, hdr, sizeof(hdr)) != ESP_OK ||
        le32b(hdr) != ND_CACT_TAG)
        return 0;
    count     = le32b(hdr + 4);
    codebooks = le32b(hdr + 8);
    if (count == 0 || count > 4096 || codebooks > 256)
        return 0;
    record_at = 196 + codebooks * 4 + (count - 1) * 44;
    if (record_at + sizeof(last) > part->size ||
        esp_partition_read(part, record_at, last, sizeof(last)) != ESP_OK)
        return 0;
    if (le32b(last + 24) || le32b(last + 32))
        return 0;                         /* 64-bit offsets: cannot map here */
    {
        size_t end = (size_t)le32b(last + 20) + le32b(last + 28);

        return end <= part->size ? end : 0;
    }
}

static void bench_shape(const kb_shape *sh)
{
    nd_tensor   t;
    const void *blob;
    uint8_t    *blob_p = NULL, *blob_i = NULL;
    float      *xh, *lut, *yref, *ytmp, *lut_p = NULL;
    nd_lut2_ctx cx, cxi, cxt;
    uint32_t    in_pad, lutn, words, i, k, m;
    static uint32_t res[KB_NMODES][6][KB_ROUNDS];
    int         idx, fast_ok;

    idx = find_tensor(sh->rows, &t);
    if (idx < 0) {
        printf("KB SKIP shape=%s reason=no_tensor\n", sh->tag);
        return;
    }
    blob   = nd_cact_data(&s_c, &t);
    in_pad = nd_cq_in_pad(&t);
    lutn   = nd_cq_lut_floats(in_pad);
    words  = in_pad * t.bits / 8u / 4u;     /* packed index words per row    */

    blob_p = heap_caps_malloc((size_t)t.nbytes, MALLOC_CAP_SPIRAM);
    xh     = (float *)ND_ALLOC_FAST(sizeof(float) * in_pad);
    lut    = (float *)ND_ALLOC_FAST(sizeof(float) * lutn);
    yref   = (float *)ND_ALLOC_FAST(sizeof(float) * t.shape[0]);
    ytmp   = (float *)ND_ALLOC_FAST(sizeof(float) * t.shape[0]);
    if (!blob_p || !xh || !lut || !yref || !ytmp) {
        printf("KB SKIP shape=%s reason=alloc nbytes=%u lut=%u\n",
               sh->tag, (unsigned)t.nbytes, (unsigned)lutn);
        heap_caps_free(blob_p); heap_caps_free(xh); heap_caps_free(lut);
        heap_caps_free(yref); heap_caps_free(ytmp);
        return;
    }
    memcpy(blob_p, blob, t.nbytes);         /* same staging the tier does    */

    /* A second copy in internal SRAM, where it fits only for the small
     * projections. It separates "the loop is compute bound" from "the loop is
     * waiting on PSRAM", which decides whether any instruction scheduling can
     * help at all. */
    fast_ok = 0;
    if (t.nbytes <= 96u * 1024u) {
        blob_i = (uint8_t *)ND_ALLOC_FAST((size_t)t.nbytes);
        if (blob_i) {
            memcpy(blob_i, blob, t.nbytes);
            fast_ok = 1;
        }
    }

    /* The ablation side: the same table bytes in PSRAM, so the delta is the
     * table's placement and nothing else. Skipped if PSRAM cannot spare it. */
    if (s_thrash == NULL)
        s_thrash = (float *)ND_ALLOC(sizeof(float) * KB_THRASH_FLOATS);
    lut_p = (float *)ND_ALLOC(sizeof(float) * lutn);

    {                                   /* a prepared activation, no denormals */
        uint32_t s = 0x1234abcdu;
        uint32_t j;

        for (j = 0; j < in_pad; j++) {
            s = s * 1664525u + 1013904223u;
            xh[j] = ((float)((s >> 8) & 0xFFFFu) / 32768.0f - 1.0f) * 0.25f;
        }
    }
    nd_cq_lut_build(&s_c, xh, in_pad, lut);
    if (lut_p)
        memcpy(lut_p, lut, sizeof(float) * lutn);   /* after the build, or the
                                                     * copy is uninitialised */

    nd_lut2_fill(&cx, &t, blob_p, lut, yref);
    nd_lut2_fill(&cxi, &t, fast_ok ? blob_i : blob_p, lut, yref);
    if (lut_p)
        nd_lut2_fill(&cxt, &t, blob_p, lut_p, yref);

    printf("KB CFG shape=%s idx=%d rows=%u in_pad=%u g=%u ngroup=%u "
           "rowbytes=%u blob=%u table=%u words=%u blob_int=%d asm_ok=%d "
           "asm_ok_int=%d\n",
           sh->tag, idx, (unsigned)t.shape[0], (unsigned)in_pad, (unsigned)t.group,
           (unsigned)cx.ngroup, (unsigned)cx.rowbytes, (unsigned)t.nbytes,
           (unsigned)(lutn * 4u), (unsigned)words, fast_ok,
           nd_lut2_asm_ok(&cx, 0, t.shape[0]), fast_ok ? nd_lut2_asm_ok(&cxi, 0, t.shape[0]) : -1);
    fflush(stdout);

    /* Numerics: the control result first, then each candidate, over all rows. */
    nd_lut2_rows_c(&cx, 0, t.shape[0]);        /* cx.y == yref */
    for (k = 1; k < (uint32_t)s_nkern; k++)
        num_check(sh->tag, &s_kern[k], &cx, t.shape[0], yref, ytmp);

    /* Experiment 20 step 1: residency must be transparent. yref is now the C
     * walker over the PSRAM blob and the internal pair table; every registered
     * kernel (the shipping nd_lut2_rows_tie1n among them) has to reproduce it
     * bit-exactly from the *internal* blob copy, and the C walker has to
     * reproduce it from a *PSRAM* copy of the table. A mismatch means the copy
     * is not the same operand, and the residency number below would be void. */
    if (fast_ok) {
        char itag[32];

        snprintf(itag, sizeof(itag), "%s/blob_int", sh->tag);
        for (k = 1; k < (uint32_t)s_nkern; k++)
            num_check(itag, &s_kern[k], &cxi, t.shape[0], yref, ytmp);
    }
    if (lut_p)
        num_check("pair_table_in_psram", &s_kern[0], &cxt, t.shape[0], yref, ytmp);

#if ND_KBENCH_ASM
    if (sh == &SHAPES[0]) {
        synth_check2("units", f_units, 0);
        synth_check2("pos", f_pos, 0);
        synth_check2("slot", f_slot, 0);
        synth_check2("norms", f_units, 1);
    }
#endif
#if ND_KBENCH_ASM
    /* Which group first disagrees. ngroup is honoured by the C walker and by the
     * assembly alike, so truncating it compares exactly the same partial sum and
     * the first differing ng names the group at fault.
     */
    {
        nd_lut2_ctx  sv = cx;
        uint32_t     ng, kr, rr;

        for (ng = 1; ng <= cx.ngroup; ng++) {
            cx.ngroup = ng;
            nd_lut2_rows_c(&cx, 0, 8);              /* yref = C, ng groups */
            for (kr = 1; kr < (uint32_t)s_nkern; kr++) {
                float    mx = 0.0f;

                cx.y = ytmp;
                s_kern[kr].fn(&cx, 0, 8);
                cx.y = yref;
                for (rr = 0; rr < 8; rr++) {
                    float d = fabsf(ytmp[rr] - yref[rr]);
                    if (d > mx)
                        mx = d;
                }
                printf("KB GRP shape=%s kernel=%s ng=%u maxabs=%.6e ref0=%.7g got0=%.7g\n",
                       sh->tag, s_kern[kr].tag, (unsigned)ng, mx,
                       (double)yref[0], (double)ytmp[0]);
            }
        }
        cx = sv;
        nd_lut2_rows_c(&cx, 0, t.shape[0]);
        fflush(stdout);
    }
#endif

    for (m = 0; m < KB_NMODES; m++) {
        nd_lut2_ctx *use;
        float        refc = 0.0f;

        if ((m == KB_FASTBLOB || m == KB_COLD_INT) && !fast_ok)
            continue;
        if (m == KB_COLD_TAB && (!lut_p || s_thrash == NULL))
            continue;
        if (m >= KB_COLD_PSRAM && s_thrash == NULL)
            continue;
        use = (m == KB_FASTBLOB || m == KB_COLD_INT) ? &cxi
              : (m == KB_COLD_TAB) ? &cxt : &cx;
        for (k = 0; k < (uint32_t)s_nkern; k++)
            for (i = 0; i < KB_WARMUP; i++) {
                if (m >= KB_COLD_PSRAM)
                    kb_thrash();
                time_one(s_kern[k].fn, use, t.shape[0], (kb_mode)m);
            }
        for (i = 0; i < KB_ROUNDS; i++) {
            for (k = 0; k < (uint32_t)s_nkern; k++) {
                if (m >= KB_COLD_PSRAM)
                    kb_thrash();
                res[m][k][i] = time_one(s_kern[k].fn, use, t.shape[0], (kb_mode)m);
            }
            printf("KB R shape=%s mode=%s round=%u", sh->tag, MODE_NAME[m], (unsigned)i);
            for (k = 0; k < (uint32_t)s_nkern; k++)
                printf(" %s=%u", s_kern[k].tag, (unsigned)res[m][k][i]);
            printf("\n");
        }
        fflush(stdout);

        for (k = 0; k < (uint32_t)s_nkern; k++) {
            uint32_t *v = res[m][k];
            uint32_t  mn, md;
            double    mean = 0.0;

            qsort(v, KB_ROUNDS, sizeof(v[0]), cmp_u32);
            mn = v[0];
            md = v[KB_ROUNDS / 2];
            for (i = 0; i < KB_ROUNDS; i++)
                mean += (double)v[i];
            mean /= (double)KB_ROUNDS;
            if (k == 0)
                refc = (float)md;
            printf("KSUM shape=%s mode=%s kernel=%s n=%u min=%u med=%u "
                   "mean=%.0f us=%.0f cyc_per_word=%.2f ipc=%.1f mhz_x100=%u\n",
                   sh->tag, MODE_NAME[m], s_kern[k].tag, (unsigned)KB_ROUNDS,
                   (unsigned)mn, (unsigned)md, mean,
                   (double)md / (double)(s_mhz_x100 ? s_mhz_x100 / 100u : 240u),
                   (double)md / (double)(words * t.shape[0]),
                   k == 0 ? IPC_C : (k == 1 ? IPC_TIE1 : IPC_TIE2),
                   (unsigned)s_mhz_x100);
        }
        for (k = 1; k < (uint32_t)s_nkern; k++)
            printf("KB DELTA shape=%s mode=%s kernel=%s med_pct=%+.2f min_pct=%+.2f\n",
                   sh->tag, MODE_NAME[m], s_kern[k].tag,
                   100.0 * ((double)refc / (double)res[m][k][KB_ROUNDS / 2] - 1.0),
                   100.0 * ((double)res[m][0][0] / (double)res[m][k][0] - 1.0));
        fflush(stdout);
    }

    heap_caps_free(blob_p);
    heap_caps_free(blob_i);
    heap_caps_free(lut_p);
    heap_caps_free(xh);
    heap_caps_free(lut);
    heap_caps_free(yref);
    heap_caps_free(ytmp);
}

static void probes(void)
{
    static const char *NAMES[3] = { "add_thru", "add_lat", "load_lat" };
    float              src[8] = { 1.0f, 0.5f, 0.25f, 0.125f, 1.0f, 0.5f, 0.25f, 0.125f };
    const uint32_t     n = 20000;
    int                p;

    /* Each probe runs n iterations of a fixed instruction mix; the divisor is
     * the iteration count, so the units are cycles per iteration. add_thru has
     * 8 add.s + 2 bookkeeping instructions, add_lat 8 dependent add.s + 2,
     * load_lat 2 independent loads each feeding one dependent add + 2. */
    for (p = 0; p < 3; p++) {
        uint32_t c0, cy;
        uint32_t i;

        for (i = 0; i < 3; i++) {
            if (p == 0)      nd_lut2_probe_add_thru(src, n / 10);
            else if (p == 1) nd_lut2_probe_add_lat(src, n / 10);
            else             nd_lut2_probe_load_lat(src, n / 10);
        }
        c0 = esp_cpu_get_cycle_count();
        if (p == 0)      nd_lut2_probe_add_thru(src, n);
        else if (p == 1) nd_lut2_probe_add_lat(src, n);
        else             nd_lut2_probe_load_lat(src, n);
        cy = esp_cpu_get_cycle_count() - c0;
        printf("KB PROBE name=%s iters=%u cyc_per_iter=%.3f\n",
               NAMES[p], (unsigned)n, (double)cy / (double)n);
    }
    fflush(stdout);
}

/* ------------------------------------------------------------------ *
 * Experiment 12: paired attention exponential.
 *
 * The attention online softmax calls exp() twice back to back per KV position
 * pair per query head, and each expansion is a serial degree-5 Horner chain.
 * exp_pairs.h holds real captured arguments, so the fixture is the shipping
 * distribution rather than a synthetic range. Both variants accumulate
 * identically (acc += e0 + e1), so the timed difference is the kernel and not
 * the harness. Per-element bit equality against two scalar calls is required.
 * ------------------------------------------------------------------ */
#include "exp_pairs.h"

__attribute__((noinline)) static float kb_exp_scalar(const float *p, unsigned n)
{
    float    acc = 0.0f;
    unsigned i;
    for (i = 0; i < n; i++)
        acc += nd_expf(p[2 * i]) + nd_expf(p[2 * i + 1]);
    return acc;
}

__attribute__((noinline)) static float kb_exp_pair_c(const float *p, unsigned n)
{
    float    acc = 0.0f, a, b;
    unsigned i;
    for (i = 0; i < n; i++) {
        nd_expf_pair(p[2 * i], p[2 * i + 1], &a, &b);
        acc += a + b;
    }
    return acc;
}

static void bench_exp_pair(void)
{
    const unsigned n = (unsigned)KB_EXP_NPAIRS;
    uint32_t best_s = 0xFFFFFFFFu, best_p = 0xFFFFFFFFu, sink = 0;
    unsigned i, r, bad = 0;
    float    s, t, a, b;

    for (i = 0; i < n; i++) {
        float x = nd_expf(kb_exp_pairs[2 * i]);
        float y = nd_expf(kb_exp_pairs[2 * i + 1]);
        nd_expf_pair(kb_exp_pairs[2 * i], kb_exp_pairs[2 * i + 1], &a, &b);
        if (memcmp(&x, &a, 4) || memcmp(&y, &b, 4)) bad++;
    }
    s = kb_exp_scalar(kb_exp_pairs, n);
    t = kb_exp_pair_c(kb_exp_pairs, n);

    for (r = 0; r < KB_ROUNDS; r++) {
        uint32_t c0 = esp_cpu_get_cycle_count(), d;
        sink += (uint32_t)kb_exp_scalar(kb_exp_pairs, n);
        d = esp_cpu_get_cycle_count() - c0;
        if (d < best_s) best_s = d;
        c0 = esp_cpu_get_cycle_count();
        sink += (uint32_t)kb_exp_pair_c(kb_exp_pairs, n);
        d = esp_cpu_get_cycle_count() - c0;
        if (d < best_p) best_p = d;
    }
    printf("KB EXP npairs=%u rounds=%u scalar_cyc_pair=%u.%02u pair_cyc_pair=%u.%02u "
           "saving_pct=%+.2f mismatch=%u sumexact=%d sink=%u\n",
           n, (unsigned)KB_ROUNDS,
           (unsigned)(best_s / n), (unsigned)((best_s % n) * 100 / n),
           (unsigned)(best_p / n), (unsigned)((best_p % n) * 100 / n),
           100.0 * (double)((int)best_s - (int)best_p) / (double)best_s,
           bad, memcmp(&s, &t, 4) == 0, (unsigned)sink);
}

/* ------------------------------------------------------------------ *
 * Experiment 13: dot-product schedule audit.
 *
 * Two shapes matter. The attention Q.K dot is qk_hd=48 floats, evaluated once
 * per query head per KV position (two dots per head per position pair, 12
 * heads, 8 layers), and it is written as one accumulator with a two-term
 * group, i.e. 24 serially dependent adds for 48 products. The Kronecker dot is
 * 32 floats inside kron_apply. Both accumulation orders are frozen by the
 * byte-exact golden gate, so a candidate has to keep them exactly: what is
 * still available is (a) interleaving the two independent dots of a position
 * pair, the same lever that paid off for exp(), and (b) getting the loads out
 * of the way with 128-bit float loads on 16-byte-aligned operands.
 *
 * Espressif's esp-dsp component (dsps_dotprod_f32_aes3) is not present in this
 * tree or in ESP-IDF 5.5.2, so its schedule is reproduced locally rather than
 * called; a reordered multi-accumulator variant is measured here only to size
 * the prize that the byte-exact gate refuses.
 *
 * Dots have no data-dependent timing on the TIE FPU, so the fixture is real
 * int8-derived K values and a plausible activation; alignments are the ones
 * the shipping code actually has (stack arrays for the staged K rows, head rows
 * 192 bytes apart for q).
 * ------------------------------------------------------------------ */

#define ND_DOT_N   48          /* qk_head_dim, floats            */
#define ND_KRON_N  32          /* kron factor side, floats        */

__attribute__((noinline)) static float dot_c4(const float *q, const float *k, int n)
{
    float s = 0.0f;
    int   i;
    for (i = 0; i < n; i += 4) {
        s += q[i + 0] * k[i + 0] + q[i + 1] * k[i + 1];
        s += q[i + 2] * k[i + 2] + q[i + 3] * k[i + 3];
    }
    return s;
}

/* Two dots, chains interleaved, each chain's order untouched: bit-identical to
 * calling dot_c4 twice. */
__attribute__((noinline)) static void dot_pair_c(const float *q, const float *k0,
                                                 const float *k1, int n,
                                                 float *r0, float *r1)
{
    float s0 = 0.0f, s1 = 0.0f;
    int   i;
    for (i = 0; i < n; i += 4) {
        s0 += q[i + 0] * k0[i + 0] + q[i + 1] * k0[i + 1];
        s1 += q[i + 0] * k1[i + 0] + q[i + 1] * k1[i + 1];
        s0 += q[i + 2] * k0[i + 2] + q[i + 3] * k0[i + 3];
        s1 += q[i + 2] * k1[i + 2] + q[i + 3] * k1[i + 3];
    }
    *r0 = s0; *r1 = s1;
}

/* Reordered 4-accumulator dot: measures the prize the byte-exact gate refuses. */
__attribute__((noinline)) static float dot_reord(const float *q, const float *k, int n)
{
    float a = 0.0f, b = 0.0f, c = 0.0f, d = 0.0f;
    int   i;
    for (i = 0; i < n; i += 4) {
        a += q[i + 0] * k[i + 0];
        b += q[i + 1] * k[i + 1];
        c += q[i + 2] * k[i + 2];
        d += q[i + 3] * k[i + 3];
    }
    return (a + b) + (c + d);
}

/* Eight distinct rows per role, so no call is loop-invariant: with one fixed
 * operand pair the compiler hoists a pure noinline call out of the timing loop
 * (measured: 8 cycles for a 48-element dot, and a pair variant that wrote only
 * locals measured 0). Every result is consumed through an FP register barrier,
 * which keeps the value live without adding memory traffic. */
static float s_kf[8][ND_DOT_N], s_qr[ND_DOT_N], s_kr[8][ND_KRON_N], s_xa[8][ND_KRON_N];
static float s_q16[ND_DOT_N] __attribute__((aligned(16)));

__attribute__((noinline)) static float fp_keep(float v)
{
    asm volatile("" : "+f"(v));
    return v;
}

static void bench_dot(void)
{
    float kf0[ND_DOT_N], kf1[ND_DOT_N];                 /* stack-aligned, as in attn_heads */
    float a, b, worst = 0.0f, sink = 0.0f;
    uint32_t best_c4 = 0xFFFFFFFFu, best_pr = 0xFFFFFFFFu, best_ro = 0xFFFFFFFFu;
    uint32_t best_k4 = 0xFFFFFFFFu, best_kp = 0xFFFFFFFFu, best_kr = 0xFFFFFFFFu;
    unsigned r, i, j, bad = 0;
    const float *q = s_q16;                              /* 16-byte aligned, like qh rows */

    for (i = 0; i < 8; i++) {
        unsigned t;
        for (t = 0; t < ND_DOT_N; t++)
            s_kf[i][t] = (float)(((int)(t * 37 + i * 11)) % 255) - 127.0f;  /* int8 range */
        for (t = 0; t < ND_KRON_N; t++) {
            s_kr[i][t] = 0.01f * (float)((t * 7 + i * 5) % 61) - 0.3f;
            s_xa[i][t] = 0.5f * (float)((t * 11 + i * 3) % 37) - 9.0f;
        }
    }
    for (i = 0; i < ND_DOT_N; i++)
        s_q16[i] = 0.25f * (float)((int)(i * 13) % 41) - 5.0f;
    for (i = 0; i < ND_DOT_N; i++) {                     /* the real staged rows */
        kf0[i] = s_kf[0][i];
        kf1[i] = s_kf[1][i];
    }

    for (i = 0; i < 8; i++) {                            /* correctness, not timing */
        float x = dot_c4(s_kf[i], s_kf[(i + 1) & 7], ND_DOT_N);
        float y = dot_reord(s_kf[i], s_kf[(i + 1) & 7], ND_DOT_N);
        float d = fabsf(y - x);
        const float *kk0 = s_kf[(i + 2) & 7], *kk1 = s_kf[(i + 3) & 7];
        float e0 = dot_c4(s_kf[i], kk0, ND_DOT_N), e1 = dot_c4(s_kf[i], kk1, ND_DOT_N);
        dot_pair_c(s_kf[i], kk0, kk1, ND_DOT_N, &a, &b);
        if (memcmp(&a, &e0, 4) || memcmp(&b, &e1, 4)) bad++;
        if (d > worst) worst = d;
    }

    for (r = 0; r < KB_ROUNDS; r++) {
        uint32_t c0, d;
        c0 = esp_cpu_get_cycle_count();
        for (j = 0; j < 64; j++) {
            sink += dot_c4(q, s_kf[j & 7], ND_DOT_N);
            sink += dot_c4(q, s_kf[(j + 1) & 7], ND_DOT_N);
        }
        d = (esp_cpu_get_cycle_count() - c0) / 128;
        if (d < best_c4) best_c4 = d;

        c0 = esp_cpu_get_cycle_count();
        for (j = 0; j < 64; j++) {
            dot_pair_c(q, s_kf[j & 7], s_kf[(j + 1) & 7], ND_DOT_N, &a, &b);
            sink += fp_keep(a) + fp_keep(b);
        }
        d = (esp_cpu_get_cycle_count() - c0) / 64;
        if (d < best_pr) best_pr = d;

        c0 = esp_cpu_get_cycle_count();
        for (j = 0; j < 64; j++)
            sink += dot_reord(q, s_kf[j & 7], ND_DOT_N);
        d = (esp_cpu_get_cycle_count() - c0) / 64;
        if (d < best_ro) best_ro = d;

        c0 = esp_cpu_get_cycle_count();
        for (j = 0; j < 64; j++) {
            sink += dot_c4(s_kr[j & 7], s_xa[j & 7], ND_KRON_N);
            sink += dot_c4(s_kr[(j + 1) & 7], s_xa[(j + 1) & 7], ND_KRON_N);
        }
        d = (esp_cpu_get_cycle_count() - c0) / 128;
        if (d < best_k4) best_k4 = d;

        c0 = esp_cpu_get_cycle_count();
        for (j = 0; j < 64; j++) {
            dot_pair_c(s_kr[j & 7], s_xa[j & 7], s_xa[(j + 1) & 7], ND_KRON_N, &a, &b);
            sink += fp_keep(a) + fp_keep(b);
        }
        d = (esp_cpu_get_cycle_count() - c0) / 64;
        if (d < best_kp) best_kp = d;

        c0 = esp_cpu_get_cycle_count();
        for (j = 0; j < 64; j++)
            sink += dot_reord(s_kr[j & 7], s_xa[j & 7], ND_KRON_N);
        d = (esp_cpu_get_cycle_count() - c0) / 64;
        if (d < best_kr) best_kr = d;
    }
    printf("KB DOT n=%d c4_pair_cyc=%u pair_cyc=%u pair_pct=%+.2f reord_cyc=%u "
           "reord_vs_c4_pct=%+.2f reord_maxabs=%.3e kron_c4_pair_cyc=%u kron_pair_cyc=%u "
           "kron_pair_pct=%+.2f kron_reord_cyc=%u kron_reord_pct=%+.2f bad=%u sink=%.1f\n",
           ND_DOT_N, (unsigned)best_c4, (unsigned)best_pr,
           100.0 * (double)(2 * (int)best_c4 - (int)best_pr) / (double)(2 * (int)best_c4),
           (unsigned)best_ro,
           100.0 * (double)((int)best_c4 - (int)best_ro) / (double)best_c4, worst,
           (unsigned)best_k4, (unsigned)best_kp,
           100.0 * (double)(2 * (int)best_k4 - (int)best_kp) / (double)(2 * (int)best_k4),
           (unsigned)best_kr,
           100.0 * (double)((int)best_k4 - (int)best_kr) / (double)best_k4, bad, sink);
}

/* ------------------------------------------------------------------ *
 * Experiment 23: single-precision division.
 *
 * The accepted image contains ZERO `div.s` instructions - this LX7 FPU
 * configuration has no hardware fp divide - so every '/' in the model is a call
 * to the ROM software divider __divsf3 (0x40002274), roughly 8-10k times per
 * decode token (two per sigmoid pair alone). Candidate: a reciprocal-refinement
 * divide - bit-hack initial guess, three Newton steps, one Markstein correction,
 * all hardware mul.s/fma.s. Correct rounding makes it bit-identical to the
 * software divider, which the host already confirms over 1.3M model-shaped pairs
 * (.auto/divf3/test_div.c); this bench decides whether it is also CHEAPER on the
 * target.
 *
 * Microbench discipline (#130/#230): rotate over distinct operand pairs so GCC
 * cannot hoist a pure noinline call out of the timed loop, and consume results
 * through a float register barrier instead of a memory sink. Both variants reach
 * their divide through a noinline function, so call overhead is common and
 * cancels in the comparison.
 * ------------------------------------------------------------------ */
#define KB_DIV_N 256u
#define KB_DIV_ROUNDS 25u

static inline uint32_t kb_fu(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline float    kb_uf(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

__attribute__((noinline)) static float kb_divf_newton(float a, float b)
{
    uint32_t bu = kb_fu(b) & 0x7FFFFFFFu;
    /* Classic magic-subtract inverse: about 3 % relative error. */
    float    r = kb_uf(0x7EF311C3u - bu);
    float    q, e;

    r = kb_uf(kb_fu(r) ^ (kb_fu(b) & 0x80000000u));    /* sign of the divisor */
    r = __builtin_fmaf(r, __builtin_fmaf(-b, r, 1.0f), r);
    r = __builtin_fmaf(r, __builtin_fmaf(-b, r, 1.0f), r);
    r = __builtin_fmaf(r, __builtin_fmaf(-b, r, 1.0f), r);
    q = a * r;
    e = __builtin_fmaf(-b, q, a);          /* exact residual */
    return __builtin_fmaf(e, r, q);        /* Markstein correction */
}

__attribute__((noinline)) static float kb_divf_rom(float a, float b) { return a / b; }
__attribute__((noinline)) static float kb_divf_mul(float a, float b) { return a * b; }

__attribute__((noinline)) static float kb_div_sweep(const float *a, const float *b,
                                                   unsigned n,
                                                   float (*f)(float, float))
{
    float    acc = 0.0f;
    unsigned i;

    for (i = 0; i < n; i++) acc += f(a[i], b[i]);
    asm volatile("" : "+f"(acc));
    return acc;
}

/* Experiment 48: price `logf`, the last libm-in-a-loop mass in the token.
 *
 * The measured call count is 1,280 per decode token (run #410: 160 per Sinkhorn call x 8 calls,
 * counted on the host over a real frozen prompt), and 13.58 % of the arguments are exactly 1.0f,
 * where logf is exactly 0.0f - so a `sum == 1.0f ? 0.0f : logf(sum)` skip is bit-exact by
 * construction. Run #294 refused it on a break-even computed from an ASSUMED 150-420 cycle cost.
 * This measures it. Three modes, min over rounds, operands rotated per the #230 rule: the mixed
 * set is what the field actually feeds (13.58 % exact ones), the second excludes them, and the
 * third is a bare multiply as the loop floor. Operands are built the way the kernel builds them:
 * one exp(0) term plus three exponentials of arguments in [-10, 0].
 */
#define KB_LOG_N       128u
#define KB_LOG_ROUNDS  12u

static float __attribute__((noinline)) kb_log_sweep(const float *v, unsigned n, int mode)
{
    float acc = 0.0f;
    unsigned i;

    for (i = 0; i < n; i++) {
        if (mode == 2)
            acc += v[i] * 1.0009765625f;          /* floor: one independent multiply */
        else if (mode == 1)
            acc += (v[i] == 1.0f) ? 0.0f : logf(v[i]);   /* what the skip would leave */
        else
            acc += logf(v[i]);                          /* the shipped call, mixed set */
    }
    return acc;
}

static void bench_log(void)
{
    static float lg[KB_LOG_N];
    static const char *NM[3] = { "field_mixed", "non_one_only", "mul_floor" };
    uint32_t st = 0x1234567u;
    uint32_t best[3] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
    float sink = 0.0f;
    unsigned i, r, k, ones = 0;

    printf("KB LOG ENTER\n");
    for (i = 0; i < KB_LOG_N; i++) {
        float sum = 1.0f;                            /* the row maximum contributes exp(0) */
        unsigned j;

        /* 13.58 % exactly 1.0f, the measured hit rate of the proposed skip. */
        if ((i * 100u) % 736u < 100u) { lg[i] = 1.0f; continue; }
        for (j = 0; j < 3; j++) {
            float u;
            st = st * 1664525u + 1013904223u;
            u  = (float)(st >> 8) * 1.1641532e-10f;  /* [0,1) */
            sum += expf(-10.0f * u);
        }
        lg[i] = sum;
    }
    for (i = 0; i < KB_LOG_N; i++) if (lg[i] == 1.0f) ones++;
    for (r = 0; r < KB_LOG_ROUNDS; r++) {
        for (k = 0; k < 3; k++) {
            uint32_t off = (r * 37u) % KB_LOG_N, c0, cy;

            c0 = esp_cpu_get_cycle_count();
            sink += kb_log_sweep(lg + off, KB_LOG_N - off, (int)k);
            cy = esp_cpu_get_cycle_count() - c0;
            if (cy < best[k]) best[k] = cy;
        }
    }
    asm volatile("" : "+f"(sink));
    for (k = 0; k < 3; k++)
        printf("KB LOG mode=%-13s n=%u min_cyc=%u cyc_per_logf=%u.%02u\n",
               NM[k], (unsigned)KB_LOG_N, (unsigned)best[k],
               (unsigned)(best[k] / KB_LOG_N),
               (unsigned)((best[k] * 100u / KB_LOG_N) % 100u));
    /* Break-even for the skip: 0.1358 x 1,280 calls x (cost - 4) must beat 0.2 % of the token,
     * i.e. 386,000 cycles; solve for the printed cost. */
    printf("KB LOG OPERANDS n=%u exact_ones=%u share=%.4f skip_gain_cyc=%u (need >386000 to ship)\n",
           (unsigned)KB_LOG_N, ones, (double)ones / KB_LOG_N,
           (unsigned)(0.1358 * 1280.0 * (double)(best[0] / KB_LOG_N)));
    fflush(stdout);
}

static void bench_div(void)
{
    static float               aa[KB_DIV_N], bb[KB_DIV_N];
    uint32_t                   st = 0x9E3779B9u;
    uint32_t                   best[3] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
    uint32_t                   bad = 0, fa = 0, fb = 0, fg = 0, fr = 0;
    static const char         *NM[3] = { "rom_divsf3", "newton_markstein", "mul_floor" };
    float                      sink = 0.0f;
    unsigned                   i, r, k;

    /* Deterministic, model-shaped, deliberately all fast-path-safe (normal
     * operands, normal results): the two hot classes are 1/(1+e) and e/(1+e)
     * from the sigmoid pair, and 1/(ss/n+eps) from RMSNorm. */
    for (i = 0; i < KB_DIV_N; i++) {
        float u, v, e;

        st = st * 1664525u + 1013904223u; u = (float)(st >> 8) * 1.1641532e-10f;
        st = st * 1664525u + 1013904223u; v = (float)(st >> 8) * 1.1641532e-10f;
        if (i & 1u) {
            e = nd_expf(-20.0f * u);
            bb[i] = 1.0f + e;
            aa[i] = (i & 2u) ? (e == 0.0f ? 1e-30f : e) : 1.0f;
        } else {
            bb[i] = (0.001f + v * 4096.0f) * exp2f(u * 20.0f - 10.0f) + 1e-5f;
            aa[i] = 1.0f;
        }
    }
    for (i = 0; i < KB_DIV_N; i++) {
        float ref = aa[i] / bb[i], got = kb_divf_newton(aa[i], bb[i]);

        if (memcmp(&ref, &got, 4) != 0) {
            if (!bad) {
                fa = kb_fu(aa[i]); fb = kb_fu(bb[i]);
                fg = kb_fu(got);   fr = kb_fu(ref);
            }
            bad++;
        }
    }
    for (r = 0; r < KB_DIV_ROUNDS; r++) {
        for (k = 0; k < 3; k++) {
            float (*f)(float, float) = k == 0 ? kb_divf_rom
                                     : (k == 1 ? kb_divf_newton : kb_divf_mul);
            uint32_t off = (r * 37u) % KB_DIV_N, c0, cy;

            c0 = esp_cpu_get_cycle_count();
            sink += kb_div_sweep(aa + off, bb + off, KB_DIV_N - off, f);
            cy = esp_cpu_get_cycle_count() - c0;
            if (cy < best[k]) best[k] = cy;
        }
    }
    asm volatile("" : "+f"(sink));
    for (k = 0; k < 3; k++)
        printf("KB DIV mode=%-17s n=%u min_cyc=%u cyc_per_div=%u.%02u\n",
               NM[k], (unsigned)KB_DIV_N, (unsigned)best[k],
               (unsigned)(best[k] / KB_DIV_N),
               (unsigned)((best[k] * 100u / KB_DIV_N) % 100u));
    printf("KB DIV NUM pairs=%u bit_mismatch=%u", (unsigned)KB_DIV_N, (unsigned)bad);
    if (bad)
        printf(" first a=%08x b=%08x got=%08x ref=%08x", (unsigned)fa, (unsigned)fb,
               (unsigned)fg, (unsigned)fr);
    printf("\n");
    fflush(stdout);
}

/* ------------------------------------------------------------------ *
 * Experiment 24 screen: the subset-logits gather on the 2-bit assembly kernel.
 *
 * The constrained sampler's dominant cost is `nd_model_logits_subset` (5.7 ms of
 * a real request token, measured in run #166): it prepares the activation once
 * and then projects ~1.3k grammar-legal embedding rows. That projection goes
 * through `nd_cq_gemv_gather` -> `gather_rows`, which walks each row in C with a
 * `dot_group` CALL PER GROUP - the same structural cost that run #333 just
 * proved is worth +0.365 % on the 4-bit phi path. The dense 2-bit projections do
 * not pay it: they use `nd_lut2_rows_tie1n`, worth +13.45 % end to end over the
 * C walker (run #204). The gather path never got the kernel because the kernel
 * walks a consecutive row range and the gather needs an id indirection.
 *
 * The lazy integration is no assembly at all: call the existing kernel once per
 * gathered row with r0 = ids[i], r1 = ids[i]+1, write into a full-size scratch
 * indexed by row id, then compact. This screen prices that per-row call against
 * the shipped C walk, on real archive bytes and a real prepared activation, and
 * requires the outputs to match row for row.
 *
 * The ids list reaches the row function through a file-scope pointer. That is
 * safe HERE because both halves only read it; the campaign's rule against
 * mutable static scratch between nd_parallel_rows workers is about WRITES.
 * ------------------------------------------------------------------ */
#define KB_GATHER_N   512u
#define KB_GATHER_RDS 5u

static const uint32_t *s_kb_ids;
static uint32_t        s_kb_idbuf[KB_GATHER_N];

__attribute__((noinline)) static void kb_gather_asm_rows(void *vc, uint32_t i0, uint32_t i1)
{
    uint32_t i;

    for (i = i0; i < i1; i++)
        nd_lut2_rows_tie1n(vc, s_kb_ids[i], s_kb_ids[i] + 1u);
}

static void bench_gather(void)
{
    nd_tensor             t;
    const void           *blob;
    uint8_t              *blob_p;
    float                *xh, *lut, *yfull, *ya, *yb;
    nd_lut2_ctx           cx;
    uint32_t              in_pad, lutn, rows, i, r;
    uint32_t              best[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu };
    uint32_t              bad = 0, first = 0xFFFFFFFFu;
    uint32_t              s = 0xB16B00F5u;
    int                   idx;

    idx = find_tensor(768, &t);
    if (idx < 0) { printf("KB GATHER SKIP reason=no_tensor\n"); return; }
    blob   = nd_cact_data(&s_c, &t);
    rows   = t.shape[0];
    in_pad = nd_cq_in_pad(&t);
    lutn   = nd_cq_lut_floats(in_pad);

    blob_p = (uint8_t *)heap_caps_malloc((size_t)t.nbytes, MALLOC_CAP_SPIRAM);
    xh     = (float *)ND_ALLOC(sizeof(float) * in_pad);
    lut    = (float *)ND_ALLOC(sizeof(float) * lutn);
    yfull  = (float *)ND_ALLOC(sizeof(float) * rows);
    ya     = (float *)ND_ALLOC(sizeof(float) * KB_GATHER_N);
    yb     = (float *)ND_ALLOC(sizeof(float) * KB_GATHER_N);
    if (!blob_p || !xh || !lut || !yfull || !ya || !yb) {
        printf("KB GATHER SKIP reason=alloc rows=%u nbytes=%u\n",
               (unsigned)rows, (unsigned)t.nbytes);
        heap_caps_free(blob_p); heap_caps_free(xh); heap_caps_free(lut);
        heap_caps_free(yfull); heap_caps_free(ya); heap_caps_free(yb);
        return;
    }
    memcpy(blob_p, blob, t.nbytes);
    for (i = 0; i < in_pad; i++) {
        s = s * 1664525u + 1013904223u;
        xh[i] = 0.02f * (float)((int)(s >> 20) % 511) - 5.1f;
    }
    /* A spread candidate list of the size the real grammar admits. */
    for (i = 0; i < KB_GATHER_N; i++)
        s_kb_idbuf[i] = (i * 768u * 2u) / (KB_GATHER_N * 2u + 1u) + (i & 1u);
    for (i = 1; i < KB_GATHER_N; i++)
        if (s_kb_idbuf[i] <= s_kb_idbuf[i - 1u]) s_kb_idbuf[i] = s_kb_idbuf[i - 1u] + 1u;
    for (i = 0; i < KB_GATHER_N; i++)
        if (s_kb_idbuf[i] >= rows) s_kb_idbuf[i] = rows - 1u - (KB_GATHER_N - 1u - i);
    s_kb_ids = s_kb_idbuf;

    /* Path A: the shipping gather (C walk, dot_group per group). */
    memset(ya, 0x5a, sizeof(float) * KB_GATHER_N);
    nd_cq_gemv_gather(&s_c, &t, blob_p, xh, s_kb_ids, KB_GATHER_N, ya);
    /* Path B: the shipped assembly kernel, one call per gathered row, into a
     * full-size scratch, then compacted back into id order. */
    nd_cq_lut_build(&s_c, xh, in_pad, lut);
    nd_lut2_fill(&cx, &t, blob_p, lut, yfull);
    memset(yfull, 0x7a, sizeof(float) * rows);
    nd_parallel_rows(kb_gather_asm_rows, &cx, KB_GATHER_N);
    for (i = 0; i < KB_GATHER_N; i++) yb[i] = yfull[s_kb_ids[i]];
    for (i = 0; i < KB_GATHER_N; i++)
        if (memcmp(&ya[i], &yb[i], 4) != 0) { if (bad == 0) first = i; bad++; }
    printf("KB Gather NUM rows=%u mismatch=%u", (unsigned)KB_GATHER_N, (unsigned)bad);
    if (bad) printf(" first_i=%u ref=%.9g got=%.9g", (unsigned)first, ya[first], yb[first]);
    printf("\n");

    for (r = 0; r < KB_GATHER_RDS; r++) {
        uint32_t c0, cy;

        c0 = esp_cpu_get_cycle_count();
        nd_cq_gemv_gather(&s_c, &t, blob_p, xh, s_kb_ids, KB_GATHER_N, ya);
        cy = esp_cpu_get_cycle_count() - c0;
        if (cy < best[0]) best[0] = cy;

        c0 = esp_cpu_get_cycle_count();
        nd_parallel_rows(kb_gather_asm_rows, &cx, KB_GATHER_N);
        for (i = 0; i < KB_GATHER_N; i++) yb[i] = yfull[s_kb_ids[i]];
        asm volatile("" : "+f"(yb[0]));
        cy = esp_cpu_get_cycle_count() - c0;
        if (cy < best[1]) best[1] = cy;
    }
    printf("KB GATHER mode=c_gather_dot_group rows=%u in=%u min=%u cyc_per_row=%u.%02u\n",
           (unsigned)KB_GATHER_N, (unsigned)in_pad, (unsigned)best[0],
           (unsigned)(best[0] / KB_GATHER_N), (unsigned)((best[0] * 100u / KB_GATHER_N) % 100u));
    printf("KB GATHER mode=lut2_asm_per_row  rows=%u in=%u min=%u cyc_per_row=%u.%02u pct=%+.2f\n",
           (unsigned)KB_GATHER_N, (unsigned)in_pad, (unsigned)best[1],
           (unsigned)(best[1] / KB_GATHER_N), (unsigned)((best[1] * 100u / KB_GATHER_N) % 100u),
           best[0] ? ((double)best[0] * 100.0 / (double)best[1]) - 100.0 : 0.0);
    /* One build of the pair table is a cost the gather path does not pay today. */
    {
        uint32_t c0 = esp_cpu_get_cycle_count();
        nd_cq_lut_build(&s_c, xh, in_pad, lut);
        printf("KB GATHER note lut_build_cyc=%u (per token, new cost)\n",
               (unsigned)(esp_cpu_get_cycle_count() - c0));
    }
    fflush(stdout);
    heap_caps_free(blob_p); heap_caps_free(xh); heap_caps_free(lut);
    heap_caps_free(yfull); heap_caps_free(ya); heap_caps_free(yb);
}

/* ---- Experiment 15: operator-internal GDMA double buffering --------------
 *
 * A memory-system question, not a schedule question: can the AHB GDMA pull the
 * CQ2 index stream PSRAM -> two small internal DMA buffers faster than the row
 * walker pulls the same bytes through the data cache, and can a 2-deep prefetch
 * hide the copy behind the compute? Measured, not argued. Six numbers per buffer
 * size, all from the same image and the same real blob bytes:
 *
 *   copy_raw   one block, submit then wait                      (copy cost)
 *   copy_pipe  two blocks always in flight, no compute          (copy ceiling)
 *   comp_psram the row walker over the same rows, cached PSRAM   (the shipping path)
 *   comp_int   the row walker over internal-RAM buffers          (the ceiling)
 *   overlap    prefetch block b+1 while consuming block b        (the proposal)
 *   wait       cycles actually blocked on the done semaphore
 *
 * Coherency is checked explicitly, because GDMA writes to internal RAM bypass
 * the data cache: the destination is first dirtied and read by the CPU, then
 * overwritten by DMA, then read again - with and without an explicit
 * invalidate - and the byte sum is compared against the source.
 */
#if ND_KBENCH_GDMA
#define KB_GDMA_BLK   20      /* blocks swept per round, rotated through rows */
#define KB_GDMA_ROUND  5
#define KB_GDMA_ROWS   768u   /* the dominant CQ2 shape: 20 x 768x768         */

static SemaphoreHandle_t s_mcp_sem;

static bool mcp_done_cb(async_memcpy_handle_t h, async_memcpy_event_t *e, void *arg)
{
    BaseType_t woken = pdFALSE;

    (void)h; (void)e; (void)arg;
    xSemaphoreGiveFromISR(s_mcp_sem, &woken);
    return woken == pdTRUE;
}

static uint32_t byte_sum(const uint8_t *p, size_t n)
{
    uint32_t s = 0;
    size_t   i;

    for (i = 0; i < n; i++)
        s += p[i];
    return s;
}

static void gdma_sum(const char *mode, uint32_t bs, uint32_t *v, uint32_t per_round)
{
    uint32_t t[KB_GDMA_ROUND], mn, md, i;
    double   mean = 0.0;

    for (i = 0; i < KB_GDMA_ROUND; i++)
        t[i] = v[i] / per_round;
    qsort(t, KB_GDMA_ROUND, sizeof(t[0]), cmp_u32);
    mn = t[0];
    md = t[KB_GDMA_ROUND / 2];
    for (i = 0; i < KB_GDMA_ROUND; i++)
        mean += (double)t[i];
    mean /= (double)KB_GDMA_ROUND;
    printf("KB GDMA SUM buffer=%u mode=%-11s n=%u min=%u med=%u mean=%.0f "
           "cyc_per_blk=%.0f MBps=%.2f\n",
           (unsigned)bs, mode, (unsigned)KB_GDMA_ROUND, (unsigned)mn, (unsigned)md,
           mean, (double)md / (double)KB_GDMA_BLK,
           (double)bs * (double)KB_GDMA_BLK * 240.0e6 / (double)md / 1.0e6);
}

static void bench_gdma(uint32_t bufsize)
{
    nd_tensor             t;
    uint8_t              *srcp = NULL, *win[2];
    float                *xh = NULL, *lut = NULL, *y = NULL, *y2 = NULL;
    nd_lut2_ctx           cps, cwin, cdma;
    async_memcpy_handle   mcp = NULL;
    async_memcpy_config_t cfg = ASYNC_MEMCPY_DEFAULT_CONFIG();
    uint32_t              rowbytes, rows_blk, bs, nblk, r, b, i, idx;
    uint32_t              c_raw[KB_GDMA_ROUND],    c_pipe[KB_GDMA_ROUND];
    uint32_t              c_psram[KB_GDMA_ROUND],  c_int[KB_GDMA_ROUND];
    uint32_t              c_over[KB_GDMA_ROUND],   c_ovms[KB_GDMA_ROUND];
    uint32_t              w_over[KB_GDMA_ROUND],   w_ovms[KB_GDMA_ROUND];
    uint32_t              coh_dirty = 0, coh_msync = 0, coh_pipe = 0, num_bad = 0;
    uint32_t              free0, free1, free2;
    nd_row_fn             fn = nd_lut2_rows_c;
    const char           *fntag = "c";
    const uint8_t        *norm_src;
    int                   k;

    if (find_tensor(KB_GDMA_ROWS, &t) < 0) {
        printf("KB GDMA SKIP buffer=%u reason=no_tensor\n", (unsigned)bufsize);
        return;
    }
    rowbytes = nd_cq_row_bytes(&t);
    rows_blk = bufsize / rowbytes;
    if (rows_blk < 2) {
        printf("KB GDMA SKIP buffer=%u reason=row_too_big rowbytes=%u\n",
               (unsigned)bufsize, (unsigned)rowbytes);
        return;
    }
    bs   = rows_blk * rowbytes;              /* a whole number of rows        */
    nblk = t.shape[0] / rows_blk;
    for (k = 0; k < s_nkern; k++)            /* the shipping kernel is tie1n   */
        if (!strcmp(s_kern[k].tag, "tie1n")) {
            fn = s_kern[k].fn;
            fntag = s_kern[k].tag;
        }

    free0 = (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    srcp  = (uint8_t *)heap_caps_malloc((size_t)t.nbytes, MALLOC_CAP_SPIRAM);
    xh    = (float *)ND_ALLOC_FAST(sizeof(float) * nd_cq_in_pad(&t));
    lut   = (float *)ND_ALLOC_FAST(sizeof(float) * nd_cq_lut_floats(nd_cq_in_pad(&t)));
    y     = (float *)ND_ALLOC_FAST(sizeof(float) * rows_blk);
    y2    = (float *)ND_ALLOC_FAST(sizeof(float) * rows_blk);
    win[0] = (uint8_t *)ND_ALLOC_FAST((size_t)bs * KB_GDMA_BLK);
    win[1] = NULL;
    free1 = (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (!srcp || !xh || !lut || !y || !y2 || !win[0]) {
        printf("KB GDMA SKIP buffer=%u reason=alloc internal_free=%u\n",
               (unsigned)bufsize, free1);
        heap_caps_free(srcp); heap_caps_free(xh); heap_caps_free(lut);
        heap_caps_free(y); heap_caps_free(y2); heap_caps_free(win[0]);
        return;
    }
    memcpy(srcp, nd_cact_data(&s_c, &t), t.nbytes);   /* real bytes, in PSRAM */
    /* One internal window holding the whole rotation, so the compute-from-
     * internal control reads exactly the bytes the DMA variant will read. */
    memcpy(win[0], srcp, (size_t)bs * KB_GDMA_BLK);

    {
        uint32_t s = 0x5bd1e995u;

        for (i = 0; i < nd_cq_in_pad(&t); i++) {
            s = s * 1664525u + 1013904223u;
            xh[i] = ((float)((s >> 8) & 0xFFFFu) / 32768.0f - 1.0f) * 0.25f;
        }
    }
    nd_cq_lut_build(&s_c, xh, nd_cq_in_pad(&t), lut);
    nd_lut2_fill(&cps, &t, srcp, lut, y);
    cwin  = cps; cwin.packed = win[0]; cwin.y = y2;
    cdma  = cps;

    if (!nd_lut2_asm_ok(&cdma, 0, rows_blk) && fntag[0] == 't') {
        printf("KB GDMA note buffer=%u asm_ok=0 kernel_falls_back_to_c\n", (unsigned)bufsize);
        fn = nd_lut2_rows_c;
        fntag = "c";
    }

    free2 = (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    printf("KB GDMA CFG buffer=%u payload=%u rows_blk=%u nblk=%u rowbytes=%u "
           "ngroup=%u tensor=%u kernel=%s int_free_pre=%u int_free_window=%u "
           "psram_free=%u\n",
           (unsigned)bufsize, (unsigned)bs, (unsigned)rows_blk, (unsigned)nblk,
           (unsigned)rowbytes, (unsigned)cps.ngroup, (unsigned)t.nbytes, fntag,
           free0, free2, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    fflush(stdout);

    cfg.backlog = 4;
    cfg.dma_burst_size = 64;
    if (!s_mcp_sem)
        s_mcp_sem = xSemaphoreCreateCounting(8, 0);
    if (esp_async_memcpy_install_gdma_ahb(&cfg, &mcp) != ESP_OK || !mcp) {
        printf("KB GDMA SKIP buffer=%u reason=async_memcpy_install\n", (unsigned)bufsize);
        heap_caps_free(srcp); heap_caps_free(xh); heap_caps_free(lut);
        heap_caps_free(y); heap_caps_free(y2); heap_caps_free(win[0]);
        return;
    }
    /* A transaction buffer of its own, DMA-capable, same alignment contract. */
    {
        uint8_t *d0 = (uint8_t *)heap_caps_aligned_alloc(64, bs, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        uint8_t *d1 = (uint8_t *)heap_caps_aligned_alloc(64, bs, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        if (!d0 || !d1) {
            printf("KB GDMA SKIP buffer=%u reason=dma_alloc internal_free=%u\n",
                   (unsigned)bufsize,
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            esp_async_memcpy_uninstall(mcp);
            heap_caps_free(d0); heap_caps_free(d1);
            heap_caps_free(srcp); heap_caps_free(xh); heap_caps_free(lut);
            heap_caps_free(y); heap_caps_free(y2); heap_caps_free(win[0]);
            return;
        }
        printf("KB GDMA MEM buffer=%u dma_pair=%u internal_free=%u delta=%d\n",
               (unsigned)bufsize, (unsigned)(2 * bs),
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
               (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) - (int)free2);

        /* --- cache/DMA coherency, checked before any timing -------------------- */
        memset(d0, 0xA5, bs);                 /* dirty the destination in the cache */
        (void)byte_sum(d0, bs);               /* then read it back: lines now cached */
        if (esp_async_memcpy(mcp, d0, srcp, bs, mcp_done_cb, NULL) == ESP_OK) {
            xSemaphoreTake(s_mcp_sem, portMAX_DELAY);
            if (byte_sum(d0, bs) != byte_sum(srcp, bs))
                coh_dirty = 1;                /* the CPU saw stale lines */
        }
        memset(d0, 0xA5, bs);
        (void)byte_sum(d0, bs);
        if (esp_async_memcpy(mcp, d0, srcp, bs, mcp_done_cb, NULL) == ESP_OK) {
            xSemaphoreTake(s_mcp_sem, portMAX_DELAY);
            esp_cache_msync(d0, bs, ESP_CACHE_MSYNC_FLAG_DIR_M2C |
                              ESP_CACHE_MSYNC_FLAG_UNALIGNED);
            if (byte_sum(d0, bs) != byte_sum(srcp, bs))
                coh_msync = 1;
        }
        printf("KB GDMA COHERENCY buffer=%u stale_without_invalidate=%u "
               "stale_with_invalidate=%u\n", (unsigned)bufsize, coh_dirty, coh_msync);

        for (r = 0; r < KB_GDMA_ROUND; r++) {
            uint32_t base = (uint32_t)((r * 7 + 1) % nblk);
            uint32_t c0;

            /* copy_raw: one block at a time, submit then wait */
            c0 = esp_cpu_get_cycle_count();
            for (b = 0; b < KB_GDMA_BLK; b++) {
                uint32_t blk = (base + b) % nblk;

                if (esp_async_memcpy(mcp, d0, srcp + (size_t)blk * bs, bs,
                                     mcp_done_cb, NULL) != ESP_OK)
                    continue;
                xSemaphoreTake(s_mcp_sem, portMAX_DELAY);
            }
            c_raw[r] = esp_cpu_get_cycle_count() - c0;

            /* copy_pipe: keep two copies in flight, no compute at all */
            c0 = esp_cpu_get_cycle_count();
            for (b = 0; b < KB_GDMA_BLK; b++) {
                uint32_t blk = (base + b) % nblk;

                if (b >= 2)
                    xSemaphoreTake(s_mcp_sem, portMAX_DELAY);
                esp_async_memcpy(mcp, d0, srcp + (size_t)blk * bs, bs, mcp_done_cb, NULL);
            }
            xSemaphoreTake(s_mcp_sem, portMAX_DELAY);
            xSemaphoreTake(s_mcp_sem, portMAX_DELAY);
            c_pipe[r] = esp_cpu_get_cycle_count() - c0;

            /* comp_psram: the shipping path, the row walker over cached PSRAM */
            cps.y = y;
            c0 = esp_cpu_get_cycle_count();
            for (b = 0; b < KB_GDMA_BLK; b++) {
                uint32_t blk = (base + b) % nblk;

                cps.packed = srcp + (size_t)blk * bs;
                fn(&cps, blk * rows_blk, blk * rows_blk + rows_blk);
            }
            c_psram[r] = esp_cpu_get_cycle_count() - c0;

            /* comp_int: the same rows from internal RAM, no DMA in flight */
            cwin.y = y2;
            c0 = esp_cpu_get_cycle_count();
            for (b = 0; b < KB_GDMA_BLK; b++) {
                uint32_t blk = (base + b) % nblk;

                cwin.packed = win[0] + (size_t)blk * bs;
                fn(&cwin, blk * rows_blk, blk * rows_blk + rows_blk);
            }
            c_int[r] = esp_cpu_get_cycle_count() - c0;

            /* overlap and wait: prefetch the next block while consuming this one.
             * Norms keep streaming from PSRAM, as they do in the tier. */
            {
                uint32_t w = 0;

                esp_async_memcpy(mcp, d0, srcp + (size_t)base * bs, bs, mcp_done_cb, NULL);
                for (b = 0; b < KB_GDMA_BLK; b++) {
                    uint32_t blk  = (base + b) % nblk;
                    uint8_t *cur  = (b & 1) ? d1 : d0;
                    uint8_t *nxt  = (b & 1) ? d0 : d1;
                    uint32_t nblk_ = (base + b + 1) % nblk;
                    uint32_t tw;

                    if (b + 1 < KB_GDMA_BLK)
                        esp_async_memcpy(mcp, nxt, srcp + (size_t)nblk_ * bs, bs,
                                         mcp_done_cb, NULL);
                    tw = esp_cpu_get_cycle_count();
                    xSemaphoreTake(s_mcp_sem, portMAX_DELAY);
                    w += esp_cpu_get_cycle_count() - tw;
                    cdma.y      = y;
                    cdma.packed = cur;
                    fn(&cdma, blk * rows_blk, blk * rows_blk + rows_blk);
                }
                w_over[r] = w;
                c_over[r] = 0;   /* total measured below in the msync variant */
            }

            /* overlap (with an explicit invalidate) and its total + wait */
            {
                uint32_t tw_all = 0, w = 0, t0 = esp_cpu_get_cycle_count();

                esp_async_memcpy(mcp, d0, srcp + (size_t)base * bs, bs, mcp_done_cb, NULL);
                for (b = 0; b < KB_GDMA_BLK; b++) {
                    uint32_t blk  = (base + b) % nblk;
                    uint8_t *cur  = (b & 1) ? d1 : d0;
                    uint8_t *nxt  = (b & 1) ? d0 : d1;
                    uint32_t nblk_ = (base + b + 1) % nblk;
                    uint32_t tw;

                    if (b + 1 < KB_GDMA_BLK)
                        esp_async_memcpy(mcp, nxt, srcp + (size_t)nblk_ * bs, bs,
                                         mcp_done_cb, NULL);
                    tw = esp_cpu_get_cycle_count();
                    xSemaphoreTake(s_mcp_sem, portMAX_DELAY);
                    w += esp_cpu_get_cycle_count() - tw;
                    esp_cache_msync(cur, bs, ESP_CACHE_MSYNC_FLAG_DIR_M2C |
                                      ESP_CACHE_MSYNC_FLAG_UNALIGNED);
                    cdma.y      = y;
                    cdma.packed = cur;
                    fn(&cdma, blk * rows_blk, blk * rows_blk + rows_blk);
                }
                c_ovms[r]  = esp_cpu_get_cycle_count() - t0;
                w_ovms[r]  = w;
                (void)tw_all;
            }

            /* the overlapped path must produce the same y as the PSRAM path */
            {
                uint32_t blk = base;

                cps.y = y2; cps.packed = srcp + (size_t)blk * bs;
                fn(&cps, blk * rows_blk, blk * rows_blk + rows_blk);
                esp_async_memcpy(mcp, d0, srcp + (size_t)blk * bs, bs, mcp_done_cb, NULL);
                xSemaphoreTake(s_mcp_sem, portMAX_DELAY);
                esp_cache_msync(d0, bs, ESP_CACHE_MSYNC_FLAG_DIR_M2C |
                          ESP_CACHE_MSYNC_FLAG_UNALIGNED);
                cdma.y = y; cdma.packed = d0;
                fn(&cdma, blk * rows_blk, blk * rows_blk + rows_blk);
                for (i = 0; i < rows_blk; i++)
                    num_bad += (y[i] != y2[i]);
                if (byte_sum(d0, bs) != byte_sum(srcp + (size_t)blk * bs, bs))
                    coh_pipe++;
            }
            printf("KB GDMA R buffer=%u round=%u copy_raw=%u copy_pipe=%u "
                   "comp_psram=%u comp_int=%u overlap_msync=%u wait_msync=%u\n",
                   (unsigned)bufsize, (unsigned)r, (unsigned)c_raw[r],
                   (unsigned)c_pipe[r], (unsigned)c_psram[r], (unsigned)c_int[r],
                   (unsigned)c_ovms[r], (unsigned)w_ovms[r]);
            fflush(stdout);
        }
        gdma_sum("copy_raw",  bs, c_raw,  1);
        gdma_sum("copy_pipe", bs, c_pipe, 1);
        gdma_sum("comp_psram",bs, c_psram,1);
        gdma_sum("comp_int",  bs, c_int,  1);
        gdma_sum("overlap_msync", bs, c_ovms, 1);
        printf("KB GDMA NUM buffer=%u y_rows_mismatch=%u dma_bytes_mismatch=%u "
               "coherency_stale_no_invalidate=%u coherency_stale_with_invalidate=%u\n",
               (unsigned)bufsize, (unsigned)num_bad, (unsigned)coh_pipe,
               coh_dirty, coh_msync);
        printf("KB GDMA MEM buffer=%u final_internal_free=%u final_psram_free=%u\n",
               (unsigned)bufsize,
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        fflush(stdout);
        esp_async_memcpy_uninstall(mcp);
        heap_caps_free(d0); heap_caps_free(d1);
    }
    heap_caps_free(srcp); heap_caps_free(xh); heap_caps_free(lut);
    heap_caps_free(y); heap_caps_free(y2); heap_caps_free(win[0]);
}
#endif /* ND_KBENCH_GDMA */

/* ---------------------------------------------------------------- exp27 ----- */
/* Two screens that were missing at the moment they mattered.
 *
 * (1) WAKE LATENCY. rows_dual_core()'s comment in main.c prices the cross-core
 *     handshake at ~15 us, but that was measured back-to-back with the worker hot.
 *     nd_cq_prepare splits only ngroup = 8 FWHT groups, and run #343's per-call
 *     timing put one prepare at 0.156 ms with ~12 prepares per decode token, so the
 *     question "is a small job cheaper run serially on the calling core" is decided
 *     entirely by what a *parked* worker costs to wake - a scheduler round trip
 *     (~3.6k cycles at 15 us, so serial wins for an 8-unit job) or a tick (this is a
 *     100 Hz build, ~2.4M cycles, so nothing small should ever be split at all).
 *     Measure hot and parked, because the ledger's 15 us is only the hot case.
 *
 * (2) MULTI-ROW CURSOR. Run #333's first integrated build of the 4-bit row walker
 *     diverged (5/17 byte-exact, token_delta 175) while measuring FASTER, because
 *     nd_gemv4_rows_tie1()'s row epilogue re-stepped packed and norms that its word
 *     loop had already advanced a whole row - and the isolation screen called the
 *     kernel once per row, where the CALLER does that stepping, so no single-row
 *     test could see it. This is the test whose absence cost that build: the range
 *     kernel over R rows against R single-row calls, row for row, on real archive
 *     bytes. Both forms build their nd_gemv4_ctx exactly as
 *     gemv_rows_offset_asm() does, so a mismatch here is a kernel defect and not a
 *     harness skew.
 */
#define KB_ROW_RDS  15u

static volatile uint32_t s_kb_wake_sink;

static void kb_wake_fn(void *vc, uint32_t a, uint32_t b)
{
    (void)vc;
    s_kb_wake_sink += b - a;
}

static void bench_wake(void)
{
    static const uint32_t units[5] = { 2u, 4u, 8u, 16u, 64u };
    uint32_t ser[5], hot[5], park[5];
    size_t   u, r;

    for (u = 0; u < 5u; u++) ser[u] = hot[u] = park[u] = 0xFFFFFFFFu;
    for (r = 0; r < KB_ROW_RDS; r++) {
        for (u = 0; u < 5u; u++) {
            uint32_t n = units[u], c0, cy;

            c0 = esp_cpu_get_cycle_count();
            kb_wake_fn(0, 0, n);
            cy = esp_cpu_get_cycle_count() - c0;
            if (cy < ser[u]) ser[u] = cy;

            c0 = esp_cpu_get_cycle_count();
            nd_parallel_rows(kb_wake_fn, 0, n);
            cy = esp_cpu_get_cycle_count() - c0;
            if (cy < hot[u]) hot[u] = cy;

            /* Let the worker block on s_go, then wake it with real work. */
            vTaskDelay(pdMS_TO_TICKS(3));
            c0 = esp_cpu_get_cycle_count();
            nd_parallel_rows(kb_wake_fn, 0, n);
            cy = esp_cpu_get_cycle_count() - c0;
            if (cy < park[u]) park[u] = cy;
        }
    }
    for (u = 0; u < 5u; u++)
        printf("KB WAKE units=%u serial=%u split_hot=%u split_parked=%u "
               "wake_hot=%u wake_parked=%u\n",
               (unsigned)units[u], (unsigned)ser[u], (unsigned)hot[u],
               (unsigned)park[u],
               (unsigned)(hot[u]  > ser[u] ? hot[u]  - ser[u] : 0u),
               (unsigned)(park[u] > ser[u] ? park[u] - ser[u] : 0u));
}

/* The 4-bit phi tensors: CQ, 4 bits, group 128, at least 8 output rows. */
static int find_row4_tensor(nd_tensor *t)
{
    uint32_t i;

    for (i = 0; i < s_c.n; i++) {
        if (nd_cact_tensor(&s_c, i, t) != 0)
            continue;
        if (t->dtype == ND_DT_CQ && t->bits == 4u && t->group == 128u &&
            t->shape[0] >= 8u && nd_cq_groups(t) >= 2u)
            return (int)i;
    }
    return -1;
}

static void kb_row4(nd_gemv4_ctx *a, const uint8_t *packed, const uint16_t *norms,
                    const float *xh, const float *cb, float *y, uint32_t row,
                    uint32_t nrows, uint32_t rowbytes, uint32_t ngroup)
{
    a->packed0  = packed + (size_t)row * rowbytes;
    a->norms0   = norms + (size_t)row * ngroup;
    a->xh       = xh;
    a->cb       = cb;
    a->y0       = y + row;
    a->ngroup   = ngroup;
    a->g        = 128u;
    a->rowbytes = rowbytes;
    a->normstep = ngroup * 2u;
    a->nrows    = nrows;
}

static void bench_rowrange(void)
{
    nd_tensor         t;
    const uint8_t    *packed;
    const uint16_t   *norms;
    const float      *cb;
    uint8_t          *blob_p = 0;
    float            *xh = 0, *yr = 0, *yo = 0;
    uint32_t          in_pad, ngroup, rowbytes, out, R, i, r;
    uint32_t          bad = 0, first = 0xFFFFFFFFu, s = 0x5EED1234u;
    nd_gemv4_ctx      a;

    if (find_row4_tensor(&t) < 0) {
        printf("KB ROWRANGE SKIP reason=no_4bit_group128_tensor\n");
        return;
    }
    in_pad   = nd_cq_in_pad(&t);
    ngroup   = nd_cq_groups(&t);
    rowbytes = nd_cq_row_bytes(&t);
    out      = t.shape[0];
    packed   = (const uint8_t *)nd_cact_data(&s_c, &t);
    norms    = (const uint16_t *)(const void *)(packed + (size_t)out * rowbytes);
    cb       = nd_cact_codebook(&s_c, t.bits);

    R = out < 24u ? out : 24u;
    blob_p = (uint8_t *)heap_caps_malloc((size_t)t.nbytes, MALLOC_CAP_SPIRAM);
    xh     = (float *)ND_ALLOC(sizeof(float) * in_pad);
    yr     = (float *)ND_ALLOC(sizeof(float) * out);
    yo     = (float *)ND_ALLOC(sizeof(float) * out);
    if (!blob_p || !xh || !yr || !yo) {
        printf("KB ROWRANGE SKIP reason=alloc out=%u in_pad=%u nbytes=%u\n",
               (unsigned)out, (unsigned)in_pad, (unsigned)t.nbytes);
        heap_caps_free(blob_p); heap_caps_free(xh);
        heap_caps_free(yr); heap_caps_free(yo);
        return;
    }
    memcpy(blob_p, packed, t.nbytes);
    for (i = 0; i < in_pad; i++) {
        s = s * 1664525u + 1013904223u;
        xh[i] = 0.02f * (float)((int)(s >> 20) % 511) - 5.1f;
    }
    /* The three chunk sizes a caller can legitimately hand the kernel: one row
     * (what the isolation screen did), a half split, and a whole one-core sweep.
     * The comparison is always range-form against R single-row calls. */
    {
        uint32_t chunks[3] = { 1u, (R / 2u) ? (R / 2u) : 1u, R };

        for (i = 0; i < 3u; i++) {
            memset(yr, 0x5a, sizeof(float) * out);
            memset(yo, 0xa5, sizeof(float) * out);
            kb_row4(&a, blob_p, norms, xh, cb, yr, 0u, R, rowbytes, ngroup);
            nd_gemv4_rows_tie1(&a);
            for (r = 0; r < R; r++) {
                kb_row4(&a, blob_p, norms, xh, cb, yo, r, 1u, rowbytes, ngroup);
                nd_gemv4_rows_tie1(&a);
            }
            for (r = 0; r < R; r++)
                if (memcmp(&yr[r], &yo[r], 4) != 0) {
                    if (bad == 0) first = r;
                    bad++;
                }
            printf("KB ROWRANGE out=%u in_pad=%u ngroup=%u rowbytes=%u chunk=%u "
                   "rows=%u mismatch=%u",
                   (unsigned)out, (unsigned)in_pad, (unsigned)ngroup,
                   (unsigned)rowbytes, (unsigned)chunks[i], (unsigned)R,
                   (unsigned)bad);
            if (bad) printf(" first_r=%u ref=%.9g got=%.9g",
                            (unsigned)first, yo[first], yr[first]);
            printf("\n");
            bad = 0;
        }
    }
    heap_caps_free(blob_p); heap_caps_free(xh);
    heap_caps_free(yr); heap_caps_free(yo);
}

/* ---- Experiment 49: the phi field-shape kernel at the post-tie0 stack -------
 *
 * #295/#296 measured the C-era phi sweep (warm 6.41, cold 7.00 cyc/weight) and
 * tied the phase's only addressable share to delivery; run #333 then shipped a
 * DIFFERENT kernel (nd_gemv4_rows_tie1) after that anchor was recorded, and the
 * sweep has never been re-measured under it. The kbench isolation rules were
 * fixed afterwards (#374: price the field's call structure, not a cache sweep),
 * so this is the field-shape rerun: the per-token sequence exactly as
 * nd_model.c issues it (8 layer slices x 3 tensors, range-form row calls), real
 * archive bytes staged to PSRAM like the tier, warm vs a 160 KiB-eviction cold
 * pass, and a per-row differential of the whole sweep against single-row calls
 * (the #333 rule: a row-range kernel is tested over rows). Both numbers decide
 * a real question: warm-vs-5.647-isolated says whether the tie0 win survives
 * field shapes, and cold-minus-warm says whether any delivery headroom is left.
 */
static void bench_e49(void)
{
    nd_tensor    tt[3];
    uint32_t     ti[3], nt = 0, i;
    uint8_t     *blob[3];
    const uint8_t *packed[3];
    const uint16_t *norms[3];
    const float  *cb;
    float        *xh = NULL, *y[3], *yr = NULL, *yo = NULL, *evict = NULL;
    uint32_t      in_pad = 0, ngroup = 0, rowbytes = 0;
    uint32_t      weights = 0, r, li, rd, bad = 0;
    uint32_t      warm = 0xFFFFFFFFu, cold = 0xFFFFFFFFu;
    float         sink = 0.0f;
    nd_gemv4_ctx  a;

    for (i = 0; i < s_c.n && nt < 3u; i++) {
        nd_tensor t;
        if (nd_cact_tensor(&s_c, i, &t) != 0) continue;
        if (t.dtype != ND_DT_CQ || t.bits != 4u || t.group != 128u) continue;
        /* phi tensors are in=3072; the predicate must NOT pick the 4-bit
         * embedding (8192x768) which shares dtype/group - first e49 run did
         * and swept nonsense geometry. */
        if (t.shape[1] != 3072u || t.shape[0] < 8u) continue;
        if (nd_cq_groups(&t) < 2u) continue;
        tt[nt] = t; ti[nt] = i; nt++;
    }
    if (nt != 3u) { printf("KB E49 SKIP reason=tensors=%u\n", (unsigned)nt); return; }
    in_pad   = nd_cq_in_pad(&tt[0]);
    ngroup   = nd_cq_groups(&tt[0]);
    rowbytes = nd_cq_row_bytes(&tt[0]);
    cb       = nd_cact_codebook(&s_c, 4u);
    for (i = 0; i < 3u; i++) {
        packed[i] = (const uint8_t *)nd_cact_data(&s_c, &tt[i]);
        blob[i]   = (uint8_t *)heap_caps_malloc((size_t)tt[i].nbytes,
                                                MALLOC_CAP_SPIRAM);
        y[i]      = (float *)ND_ALLOC(sizeof(float) * tt[i].shape[0]);
        if (!blob[i] || !y[i]) { printf("KB E49 SKIP reason=alloc\n"); return; }
        memcpy(blob[i], packed[i], tt[i].nbytes);
        /* norms live INSIDE the copied blob (the archive copy of nbytes covers
         * packed+norms): pointing them at the mmap'd archive would mix operand
         * classes in the warm/cold attribution. */
        norms[i]  = (const uint16_t *)(const void *)(blob[i] +
                     (size_t)tt[i].shape[0] * rowbytes);
    }
    xh    = (float *)ND_ALLOC(sizeof(float) * in_pad);
    yr    = (float *)ND_ALLOC(sizeof(float) * 192u);
    yo    = (float *)ND_ALLOC(sizeof(float) * 192u);
    evict = (float *)heap_caps_malloc(163840u, MALLOC_CAP_SPIRAM);
    if (!xh || !yr || !yo || !evict) { printf("KB E49 SKIP reason=alloc2\n"); return; }
    { uint32_t s = 12345u;
      for (i = 0; i < in_pad; i++) {
          s = s * 1664525u + 1013904223u;
          xh[i] = ((float)((s >> 8) & 0xFFFFu) / 32768.0f - 1.0f) * 0.25f;
      } }
    {   /* Gate + norm audit: does the SHIPPING route even take tie1 for these
         * tensors, and are all their norms plain fp16 (the gate's condition)? */
        extern int nd_gemv4_asm_ok(uint32_t,uint32_t,uint32_t,uint32_t,const uint16_t *);
        uint32_t odd[3] = {0,0,0};
        for (i = 0; i < 3u; i++) {
            uint32_t nn = tt[i].shape[0] * ngroup, k, e;
            for (k = 0; k < nn; k++) {
                e = (norms[i][k] >> 10) & 0x1Fu;
                if (e == 0u || e == 31u) odd[i]++;
            }
        }
        printf("KB E49 gate ok=%d/%d/%d oddnorms=%u/%u/%u of %u\n",
               nd_gemv4_asm_ok(4u,128u,ngroup,tt[0].shape[0],norms[0]),
               nd_gemv4_asm_ok(4u,128u,ngroup,tt[1].shape[0],norms[1]),
               nd_gemv4_asm_ok(4u,128u,ngroup,tt[2].shape[0],norms[2]),
               (unsigned)odd[0],(unsigned)odd[1],(unsigned)odd[2],
               (unsigned)ngroup);
    }
    printf("KB E49 sel t=%u/%u/%u out=%u/%u/%u in_pad=%u ngroup=%u rowbytes=%u\n",
           (unsigned)ti[0], (unsigned)ti[1], (unsigned)ti[2],
           (unsigned)tt[0].shape[0], (unsigned)tt[1].shape[0], (unsigned)tt[2].shape[0],
           (unsigned)in_pad, (unsigned)ngroup, (unsigned)rowbytes);
    /* per-token weights: the slice actually swept (per-layer rows 4,4,16) */
    weights = 8u * (4u + 4u + 16u) * 3072u;

    /* differential first: range-form whole sweep vs single-row calls */
    memset(yr, 0x5a, sizeof(float) * 192u);
    memset(yo, 0xa5, sizeof(float) * 192u);
    for (li = 0; li < 8u; li++) {
        const uint32_t rc[3] = { 4u, 4u, 16u };
        for (i = 0; i < 3u; i++) {
            uint32_t base = li * rc[i], k;
            kb_row4(&a, blob[i], norms[i], xh, cb, yr, base, rc[i], rowbytes, ngroup);
            a.y0 = &yr[li * 24u + (i == 0 ? 0u : (i == 1 ? 4u : 8u))];
            nd_gemv4_rows_tie1(&a);
            /* kb_row4 takes the ARRAY BASE and adds `row` itself (y0 = y + row,
             * store = y0[i]) - passing a pre-offset pointer AND row double-counts
             * (first two runs: 180/192 mismatch, all in slices with base>0). */
            for (k = 0; k < rc[i]; k++) {
                kb_row4(&a, blob[i], norms[i], xh, cb, yo, base + k, 1u,
                        rowbytes, ngroup);
                a.y0 = &yo[li * 24u + (i == 0 ? 0u : (i == 1 ? 4u : 8u)) + k];
                nd_gemv4_rows_tie1(&a);
            }
        }
    }
    { uint32_t shown = 0;
      for (i = 0; i < 8u * 24u; i++)
          if (memcmp(&yr[i], &yo[i], 4) != 0) {
              if (shown < 12u) {
                  printf("KB E49 bad r=%u (li=%u sl=%u) ref=%.9g got=%.9g\n",
                         (unsigned)i, (unsigned)(i / 24u), (unsigned)(i % 24u),
                         yo[i], yr[i]);
                  shown++;
              }
              bad++;
          } }
    printf("KB E49 diff rows=%u mismatch=%u\n", 8u * 24u, (unsigned)bad);

    /* Localize by nrows: for each tensor, a range call of R rows vs R single-row
     * calls, R = 1,2,4,8. This isolates the row-carry path, not the arithmetic. */
    for (i = 0; i < 3u; i++) {
        const uint32_t Rs[4] = { 1u, 2u, 4u, 8u };
        uint32_t z;
        for (z = 0; z < 4u; z++) {
            uint32_t R = Rs[z], mm = 0;
            memset(yr, 0x5a, sizeof(float) * 64u);
            memset(yo, 0xa5, sizeof(float) * 64u);
            kb_row4(&a, blob[i], norms[i], xh, cb, yr, 0u, R, rowbytes, ngroup);
            nd_gemv4_rows_tie1(&a);
            for (li = 0; li < R; li++) {
                kb_row4(&a, blob[i], norms[i], xh, cb, yo, li, 1u, rowbytes, ngroup);
                nd_gemv4_rows_tie1(&a);
            }
            for (li = 0; li < R; li++)
                if (memcmp(&yr[li], &yo[li], 4) != 0) mm++;
            printf("KB E49 nrows t=%u R=%u mismatch=%u ref0=%.6g got0=%.6g ref1=%.6g got1=%.6g\n",
                   (unsigned)i, (unsigned)R, (unsigned)mm, yo[0], yr[0],
                   R > 1 ? yo[1] : 0.0f, R > 1 ? yr[1] : 0.0f);
        }
    }
    if (bad) { printf("KB E49 DIFF_FAIL skip timing\n"); return; }

    for (rd = 0; rd < KB_ROUNDS; rd++) {
        uint32_t c0, cy;
        const uint32_t rc[3] = { 4u, 4u, 16u };
        c0 = esp_cpu_get_cycle_count();
        for (li = 0; li < 8u; li++)
            for (i = 0; i < 3u; i++) {
                kb_row4(&a, blob[i], norms[i], xh, cb, y[i], li * rc[i], rc[i],
                        rowbytes, ngroup);
                nd_gemv4_rows_tie1(&a);
            }
        cy = esp_cpu_get_cycle_count() - c0;
        if (cy < warm) warm = cy;
        for (r = 0; r < 163840u / 4u; r++) sink += evict[r];
        c0 = esp_cpu_get_cycle_count();
        for (li = 0; li < 8u; li++)
            for (i = 0; i < 3u; i++) {
                kb_row4(&a, blob[i], norms[i], xh, cb, y[i], li * rc[i], rc[i],
                        rowbytes, ngroup);
                nd_gemv4_rows_tie1(&a);
            }
        cy = esp_cpu_get_cycle_count() - c0;
        if (cy < cold) cold = cy;
    }
    printf("KB E49 t=%u/%u/%u out=%u/%u/%u in_pad=%u ngroup=%u weights=%u "
           "warm_cyc=%u cold_cyc=%u warm_cpw=%u cold_cpw=%u x1000 cold_warm_x100=%u sink=%d\n",
           (unsigned)ti[0], (unsigned)ti[1], (unsigned)ti[2],
           (unsigned)tt[0].shape[0], (unsigned)tt[1].shape[0], (unsigned)tt[2].shape[0],
           (unsigned)in_pad, (unsigned)ngroup, (unsigned)weights,
           (unsigned)warm, (unsigned)cold,
           (unsigned)((uint64_t)warm * 1000u / weights),
           (unsigned)((uint64_t)cold * 1000u / weights),
           (unsigned)((uint64_t)cold * 100u / warm),
           (int)sink);
}


/* ================= Experiment 29: fused/shared-activation CQ2 lanes =========
 *
 * Three questions, one per board, all about the mass the phase map puts in
 * `proj2bit` (86.2 ms of a 197 ms token). What this bench deliberately does NOT
 * re-ask, because it is closed on measurement: the cross-core handshake (23
 * cycles, hot AND parked, run #344) and pair-table reuse - nd_model.c:1464-1471
 * already shares one nd_cq_prepare + one nd_cq_lut_build across q/k/v/gate, and
 * :1082-1102 does the same for the engram key/value pair. So "fusing" those
 * projections cannot be about the table or the handshake. What is left, and what
 * is measured here on real archive bytes:
 *
 *   LANE 1  engram-like  - two 768x768 CQ2 tensors that share one activation:
 *            one sweep over both row sets vs the shipping two sweeps.
 *   LANE 2  attention-like - the real 576/96/128/768 q,k,v,gate row counts, one
 *            sweep vs four, plus a row-block-interleaved order. The 96- and
 *            128-row tensors are the suspicion: each half-split gives a core
 *            48/64 rows, so four jobs may stream the tier worse than one.
 *   LANE 3  amortization curve - cycles per row against rows-per-call on the
 *            dominant 768x768 shape. Flat means there is no per-row cost for a
 *            multi-row / two-stream assembly schedule to amortize, and that lane
 *            closes without writing assembly.
 *
 * Weights are real needle3.cact bytes copied into PSRAM exactly as the 12 MB
 * tier stages them; the activation is the same deterministic LCG vector
 * bench_shape() and bench_gather() already use (so the three benches are
 * comparable); the pair table is built once, as shipping does. Every fused
 * output is compared row-for-row against the shipping per-tensor path.
 */
#ifndef ND_KB_LANE
#define ND_KB_LANE 1
#endif
#define KB_MAXSEG 8

typedef struct {
    nd_lut2_ctx ctx[KB_MAXSEG];
    const void *blob[KB_MAXSEG];
    float      *y[KB_MAXSEG];
    uint32_t    nrows[KB_MAXSEG];
    uint32_t    cum[KB_MAXSEG + 1];
    uint32_t    nseg;
    nd_row_fn   fn;
} kb_fuse;

/* A global row range [r0,r1) of the concatenated row space, dispatched to the
 * segment(s) it overlaps - local row indices, because each kernel indexes
 * packed/norms/y by the row number it is handed (the #287 harness fact). */
static void kb_fuse_rows(void *arg, uint32_t r0, uint32_t r1)
{
    kb_fuse *f = (kb_fuse *)arg;
    uint32_t s;

    for (s = 0; s < f->nseg; s++) {
        uint32_t a = f->cum[s], b = f->cum[s + 1u], lo, hi;
        if (r1 <= a || r0 >= b) continue;
        lo = (r0 > a) ? r0 - a : 0u;
        hi = (r1 < b) ? r1 - a : b - a;
        if (hi > lo) f->fn((void *)&f->ctx[s], lo, hi);
    }
}

/* The same total work, but 64-row blocks of every segment before the next, so
 * the two cores walk the tensors together instead of finishing one first. */
static void kb_fuse_rows_il(void *arg, uint32_t r0, uint32_t r1)
{
    kb_fuse *f = (kb_fuse *)arg;
    uint32_t  blk = 64u, lo;

    for (lo = r0; lo < r1; lo += blk) {
        uint32_t hi = (lo + blk < r1) ? lo + blk : r1, s;
        for (s = 0; s < f->nseg; s++) {
            uint32_t a = f->cum[s], b = f->cum[s + 1u], x, y;
            if (hi <= a || lo >= b) continue;
            x = (lo > a) ? lo - a : 0u;
            y = (hi < b) ? hi - a : b - a;
            if (y > x) f->fn((void *)&f->ctx[s], x, y);
        }
    }
}

/* Distinct tensors matching one 2-bit geometry. find_tensor() always returns the
 * FIRST match, which would make a multi-segment lane measure the same bytes N
 * times, so walk the directory and skip indices this lane already took. */
static uint32_t kb_find_shape(uint32_t rows, uint32_t skip, nd_tensor *t, int *idx)
{
    uint32_t i, seen = 0u;

    for (i = 0; nd_cact_tensor(&s_c, i, t) == 0; i++) {
        if (t->bits != 2u || t->group != 128u) continue;
        if (t->shape[0] != rows || t->shape[1] != 768u) continue;
        if (seen++ < skip) continue;
        *idx = (int)i;
        return 1u;
    }
    return 0u;
}

/* The deterministic activation bench_shape()/bench_gather() use, so cycles per
 * weight is comparable with the campaign's existing 2-bit numbers. */
static void kb_lane_xh(float *xh, uint32_t in_pad, uint32_t seed)
{
    uint32_t j, s = 12345u + seed * 7919u;
    for (j = 0; j < in_pad; j++) {
        s = s * 1664525u + 1013904223u;
        xh[j] = ((float)((s >> 8) & 0xFFFFu) / 32768.0f - 1.0f) * 0.25f;
    }
}

static void bench_fused(void)
{
    static const uint32_t LANE1[] = { 768u, 768u };
    static const uint32_t LANE2[] = { 576u, 96u, 128u, 768u };
    const uint32_t *want = (ND_KB_LANE == 2) ? LANE2 : LANE1;
    uint32_t        nwant = (ND_KB_LANE == 2) ? 4u : 2u;
    nd_tensor   t[KB_MAXSEG];
    int         idx[KB_MAXSEG];
    uint8_t    *blobp[KB_MAXSEG];
    float      *yship[KB_MAXSEG], *yfus[KB_MAXSEG];
    kb_fuse     f, fs;
    nd_lut2_ctx cship[KB_MAXSEG];
    float      *xh = NULL, *lut = NULL;
    uint32_t    s, i, in_pad, lutn, total = 0u, weights = 0u;
    uint32_t    rounds = 12u, r, bad = 0u, first = 0u;
    uint32_t    best[3] = { 0xffffffffu, 0xffffffffu, 0xffffffffu };

    (void)LANE1; (void)LANE2;

    /* ---------------- LANE 3: cost per row against rows-per-call ---------- */
    if (ND_KB_LANE == 3) {
        static uint32_t res[5];
        nd_tensor  t3;
        int        i3 = -1;
        uint8_t   *b3 = NULL;
        float     *y3 = NULL, *l3 = NULL;
        uint32_t   rows3, ci = 0u, chunk;

        if (!kb_find_shape(768u, 0u, &t3, &i3)) { printf("KB L3 no_tensor\n"); return; }
        rows3  = t3.shape[0];
        in_pad = nd_cq_in_pad(&t3);
        lutn   = nd_cq_lut_floats(in_pad);
        b3 = heap_caps_malloc((size_t)t3.nbytes, MALLOC_CAP_SPIRAM);
        y3 = heap_caps_malloc(sizeof(float) * rows3, MALLOC_CAP_SPIRAM);
        l3 = (float *)ND_ALLOC_FAST(sizeof(float) * lutn);
        xh = (float *)ND_ALLOC_FAST(sizeof(float) * in_pad);
        if (!b3 || !y3 || !l3 || !xh) { printf("KB L3 alloc_failed\n"); return; }
        memcpy(b3, nd_cact_data(&s_c, &t3), t3.nbytes);
        kb_lane_xh(xh, in_pad, 3u);
        nd_cq_lut_build(&s_c, xh, in_pad, l3);
        nd_lut2_fill(&cship[0], &t3, b3, l3, y3);
        /* The kernel choice follows nd_lut2_asm_ok() alone, the way the shipping
         * dispatcher decides: ND_LUT2_ASM is a PRIVATE define of the needle
         * component and is therefore never visible in this file, so guarding on it
         * silently screens the C walker and reports the wrong cost (run #347). */
        nd_row_fn fn3 = nd_lut2_asm_ok(&cship[0], 0u, rows3) ? nd_lut2_rows_tie1n
                                                            : nd_lut2_rows_c;
        printf("KB L3 CFG tensor=%d rows=%u in_pad=%u rowbytes=%u asm=%d\n",
               i3, (unsigned)rows3, (unsigned)in_pad,
               (unsigned)nd_cq_row_bytes(&t3), (fn3 == nd_lut2_rows_tie1n));
        for (chunk = 1u; chunk <= 16u; chunk *= 2u, ci++) {
            uint32_t bc = 0xffffffffu;
            for (r = 0; r < rounds; r++) {
                uint32_t c0 = esp_cpu_get_cycle_count(), rb;
                for (rb = 0u; rb < rows3; rb += chunk) {
                    uint32_t hi = (rb + chunk < rows3) ? rb + chunk : rows3;
                    fn3((void *)&cship[0], rb, hi);
                }
                { uint32_t cy = esp_cpu_get_cycle_count() - c0; if (cy < bc) bc = cy; }
                asm volatile("" : "+f"(y3[0]));
            }
            res[ci] = bc;
            printf("KB L3 rows_per_call=%u calls=%u cycles=%u cyc_per_row=%.3f\n",
                   (unsigned)chunk, (unsigned)((rows3 + chunk - 1u) / chunk),
                   (unsigned)bc, (double)bc / (double)rows3);
        }
        printf("KB L3 VERDICT flat=%d spread=%.2f%% (1-row %.3f vs 16-row %.3f cyc/row)\n",
               (res[0] == res[4]),
               100.0 * ((double)res[0] / (double)res[4] - 1.0),
               (double)res[0] / rows3, (double)res[4] / rows3);
        return;
    }

    /* ---------------- LANE 1 / LANE 2: N tensors, one activation --------- */
    memset(&f, 0, sizeof(f));
    memset(&fs, 0, sizeof(fs));
    for (s = 0; s < nwant; s++) {
        if (!kb_find_shape(want[s], s, &t[s], &idx[s])) {
            printf("KB FUSE lane=%d missing_shape=%u\n", ND_KB_LANE, (unsigned)want[s]);
            return;
        }
        in_pad   = nd_cq_in_pad(&t[s]);
        blobp[s] = heap_caps_malloc((size_t)t[s].nbytes, MALLOC_CAP_SPIRAM);
        yship[s] = heap_caps_malloc(sizeof(float) * t[s].shape[0], MALLOC_CAP_SPIRAM);
        yfus[s]  = heap_caps_malloc(sizeof(float) * t[s].shape[0], MALLOC_CAP_SPIRAM);
        if (!blobp[s] || !yship[s] || !yfus[s]) {
            printf("KB FUSE lane=%d alloc_failed shape=%u\n", ND_KB_LANE, (unsigned)want[s]);
            return;
        }
        memcpy(blobp[s], nd_cact_data(&s_c, &t[s]), t[s].nbytes);   /* tier staging */
        f.nrows[s] = t[s].shape[0];
        f.blob[s]  = blobp[s];
        total    += t[s].shape[0];
        weights  += t[s].shape[0] * in_pad;
        f.cum[s + 1u] = total;
        f.nseg = s + 1u;
    }
    in_pad = nd_cq_in_pad(&t[0]);
    lutn   = nd_cq_lut_floats(in_pad);
    xh  = (float *)ND_ALLOC_FAST(sizeof(float) * in_pad);
    lut = (float *)ND_ALLOC_FAST(sizeof(float) * lutn);
    if (!xh || !lut) { printf("KB FUSE lane=%d lut_alloc_failed\n", ND_KB_LANE); return; }
    kb_lane_xh(xh, in_pad, 1u);
    nd_cq_lut_build(&s_c, xh, in_pad, lut);          /* ONE table, as shipping */

    /* Two context sets: the shipping path writes yship[], the fused one yfus[]. */
    fs.nseg = f.nseg; fs.fn = f.fn;
    for (s = 0; s < f.nseg; s++) {
        nd_lut2_fill(&cship[s], &t[s], blobp[s], lut, yship[s]);
        nd_lut2_fill(&f.ctx[s], &t[s], blobp[s], lut, yfus[s]);
        /* cum[] is set once while gathering the segments; re-deriving it here is
         * what made segment 0 claim every row (run #347: mismatch = exactly the
         * rows of the segments after the first, plus a read past the blob). */
    }
    memcpy(fs.ctx, f.ctx, sizeof(fs.ctx));   /* fused ctx writes yfus */
    memcpy(fs.y, f.y, sizeof(fs.y));
    fs.fn = nd_lut2_rows_tie1n;
    for (s = 0; s < f.nseg; s++)
        if (!nd_lut2_asm_ok(&f.ctx[s], 0u, f.nrows[s])) fs.fn = nd_lut2_rows_c;
    f.fn = fs.fn;
    for (s = 0; s < f.nseg; s++) f.y[s] = yfus[s];

    printf("KB FUSE lane=%d segs=%u rows=%u weights=%u asm=%d shapes=",
           ND_KB_LANE, (unsigned)f.nseg, (unsigned)total, (unsigned)weights,
           (fs.fn == nd_lut2_rows_tie1n));
    for (s = 0; s < f.nseg; s++) printf("%u%s", (unsigned)f.nrows[s], (s + 1u < f.nseg) ? "," : "\n");

    /* Bit-exactness first: shipping per-tensor (writes yship) vs the fused
     * single sweep over the concatenated row space (writes yfus). */
    for (s = 0; s < f.nseg; s++) {
        memset(yship[s], 0x5a, sizeof(float) * f.nrows[s]);
        memset(yfus[s],  0x7a, sizeof(float) * f.nrows[s]);
        nd_parallel_rows(fs.fn, (void *)&cship[s], f.nrows[s]);
    }
    nd_parallel_rows(kb_fuse_rows, &f, total);
    for (s = 0; s < f.nseg; s++) {
        for (i = 0u; i < f.nrows[s]; i++) {
            if (memcmp(&yship[s][i], &yfus[s][i], 4) != 0) {
                if (bad == 0u) first = i;
                bad++;
            }
        }
    }
    printf("KB FUSE NUM rows_compared=%u mismatch=%u%s\n",
           (unsigned)total, (unsigned)bad,
           bad ? "" : " bitexact=1");
    if (bad) { printf("KB FUSE ABORT reason=not_bitexact first_row=%u\n", (unsigned)first); return; }

    for (r = 0; r < rounds; r++) {
        uint32_t c0, cy;

        c0 = esp_cpu_get_cycle_count();                       /* A: shipping */
        for (s = 0; s < f.nseg; s++)
            nd_parallel_rows(fs.fn, (void *)&cship[s], f.nrows[s]);
        cy = esp_cpu_get_cycle_count() - c0;
        if (cy < best[0]) best[0] = cy;
        asm volatile("" : "+f"(yship[0][0]));

        c0 = esp_cpu_get_cycle_count();                       /* B: one sweep */
        nd_parallel_rows(kb_fuse_rows, &f, total);
        cy = esp_cpu_get_cycle_count() - c0;
        if (cy < best[1]) best[1] = cy;
        asm volatile("" : "+f"(yfus[0][0]));

        c0 = esp_cpu_get_cycle_count();                       /* C: interleaved */
        nd_parallel_rows(kb_fuse_rows_il, &f, total);
        cy = esp_cpu_get_cycle_count() - c0;
        if (cy < best[2]) best[2] = cy;
        asm volatile("" : "+f"(yfus[0][0]));
    }
    printf("KB FUSE RES lane=%d ship=%u fused=%u interleaved=%u cycles | cyc_per_weight %.4f %.4f %.4f | fused=%+.2f%% interleaved=%+.2f%%\n",
           ND_KB_LANE, (unsigned)best[0], (unsigned)best[1], (unsigned)best[2],
           (double)best[0] / weights, (double)best[1] / weights, (double)best[2] / weights,
           100.0 * ((double)best[0] / (double)best[1] - 1.0),
           100.0 * ((double)best[0] / (double)best[2] - 1.0));
}
/* ======================= end Experiment 29 ============================== */

/* Experiment 30 sits behind its own option so an Experiment 29 re-run stays
 * byte-reproducible from the same tree. */
#if ND_KB_EXP30
/* ============ Experiment 30: where the bytes are, not how they are called ==
 *
 * Experiment 29 closed scheduling: fusing the CQ2 projections that share an
 * activation and a pair table bought +0.04 % / +0.02 %, an interleaved order
 * cost -0.03 % / -0.08 %, and the rows-per-call curve converged (2.16 % from 1
 * to 16 rows, 0.14 % over the last doubling). What those nulls do NOT price is
 * the one number this campaign has measured and never collected: a FLAT 13.90 %
 * delivery tax on every cold PSRAM 2-bit pass (run #330). Lanes here attack the
 * delivery side, on real archive bytes, before any shipping change:
 *
 *   LANE 1  TIER COMPACTION. The 12 MB tier copies an archive span, so a
 *           layer's four CQ2 tensors land at their ARCHIVE offsets, spread over
 *           megabytes. A per-token read of q,k,v,gate touches ~0.3 MB of packed
 *           rows across that span. Laying the same bytes out contiguously in
 *           read order is the only untested way to reduce the address space a
 *           token walks. Bit-exact by construction: same bytes, new addresses.
 *   LANE 2  ATTENTION Q.K OPERAND REUSE. ~14,400 dots x 138 cycles = 8.3 ms of
 *           the attention stage. The shipped loop hands the dot a fresh q and a
 *           fresh k every position; q is invariant across positions of one head,
 *           so keeping it resident is operand reuse, which #230 never tested (it
 *           tested interleaving and extra accumulators, both of which lost).
 *   LANE 3  READ-ORDER SENSITIVITY. Cold passes over one tensor with the rows
 *           visited ascending / descending / strided / shuffled. Flat means no
 *           permutation of bytes can pay, which closes lane 1's whole family off
 *           the device; non-flat prices exactly how much layout is worth.
 */
#ifndef ND_KB_LANE
#define ND_KB_LANE 1
#endif

/* ---------------- LANE 1: compacted vs archive-offset tier layout ---------- */
#if ND_KB_LANE == 1
static void bench_e30(void)
{
    static const uint32_t WANT[4] = { 576u, 96u, 128u, 768u };   /* q,k,v,gate */
    nd_tensor    t[4];
    int          idx[4];
    uint8_t     *sep[4], *packed_buf = NULL, *pk[4];
    float       *ya[4], *yb[4], *xh = NULL, *lut = NULL;
    nd_lut2_ctx  ca[4], cb[4];
    kb_fuse      fa, fb;
    uint32_t     s, i, in_pad, lutn, rows = 0u, weights = 0u, span = 0u;
    uint32_t     rounds = 12u, r, bad = 0u, best[2] = { 0xffffffffu, 0xffffffffu };
    nd_row_fn    fn;

    memset(&fa, 0, sizeof(fa));
    memset(&fb, 0, sizeof(fb));
    for (s = 0; s < 4u; s++) {
        if (!kb_find_shape(WANT[s], s, &t[s], &idx[s])) {
            printf("KB E30 missing_shape=%u\n", (unsigned)WANT[s]);
            return;
        }
        in_pad   = nd_cq_in_pad(&t[s]);
        sep[s]   = heap_caps_malloc((size_t)t[s].nbytes, MALLOC_CAP_SPIRAM);
        ya[s]    = heap_caps_malloc(sizeof(float) * t[s].shape[0], MALLOC_CAP_SPIRAM);
        yb[s]    = heap_caps_malloc(sizeof(float) * t[s].shape[0], MALLOC_CAP_SPIRAM);
        if (!sep[s] || !ya[s] || !yb[s]) { printf("KB E30 alloc_failed\n"); return; }
        memcpy(sep[s], nd_cact_data(&s_c, &t[s]), t[s].nbytes);
        pk[s]      = sep[s];                     /* layout A: four allocations */
        rows      += t[s].shape[0];
        weights   += t[s].shape[0] * in_pad;
        fa.nrows[s] = fb.nrows[s] = t[s].shape[0];
        fa.cum[s + 1u] = fb.cum[s + 1u] = rows;
        span += t[s].nbytes;
    }
    fa.nseg = fb.nseg = 4u;

    /* Layout B: the same bytes, one buffer, concatenated in per-token read
     * order - what a compacted tier would look like. */
    packed_buf = heap_caps_malloc((size_t)span, MALLOC_CAP_SPIRAM);
    if (!packed_buf) { printf("KB E30 packed_alloc_failed span=%u\n", (unsigned)span); return; }
    {
        uint32_t off = 0u;
        for (s = 0; s < 4u; s++) {
            memcpy(packed_buf + off, sep[s], (size_t)t[s].nbytes);
            pk[s] = packed_buf + off;
            off  += t[s].nbytes;
        }
    }

    in_pad = nd_cq_in_pad(&t[0]);
    lutn   = nd_cq_lut_floats(in_pad);
    xh  = (float *)ND_ALLOC_FAST(sizeof(float) * in_pad);
    lut = (float *)ND_ALLOC_FAST(sizeof(float) * lutn);
    if (!xh || !lut) { printf("KB E30 lut_alloc_failed\n"); return; }
    kb_lane_xh(xh, in_pad, 1u);
    nd_cq_lut_build(&s_c, xh, in_pad, lut);

    fn = nd_lut2_rows_tie1n;
    for (s = 0; s < 4u; s++) {
        nd_lut2_fill(&ca[s], &t[s], sep[s], lut, ya[s]);
        nd_lut2_fill(&cb[s], &t[s], pk[s],  lut, yb[s]);
        if (!nd_lut2_asm_ok(&ca[s], 0u, fa.nrows[s]) ||
            !nd_lut2_asm_ok(&cb[s], 0u, fb.nrows[s])) fn = nd_lut2_rows_c;
    }
    for (s = 0; s < 4u; s++) { fa.ctx[s] = ca[s]; fb.ctx[s] = cb[s]; }
    fa.fn = fb.fn = fn;

    printf("KB E30 L1 segs=4 rows=%u weights=%u span=%u asm=%d tensor_ids=%u,%u,%u,%u\n",
           (unsigned)rows, (unsigned)weights, (unsigned)span, (fn == nd_lut2_rows_tie1n),
           (unsigned)idx[0], (unsigned)idx[1], (unsigned)idx[2], (unsigned)idx[3]);

    /* Same bytes at different addresses must give the same rows. */
    nd_parallel_rows(kb_fuse_rows, &fa, rows);
    nd_parallel_rows(kb_fuse_rows, &fb, rows);
    for (s = 0; s < 4u; s++)
        for (i = 0u; i < fa.nrows[s]; i++)
            if (memcmp(&ya[s][i], &yb[s][i], 4) != 0) bad++;
    printf("KB E30 NUM rows_compared=%u mismatch=%u%s\n", (unsigned)rows, (unsigned)bad,
           bad ? "" : " bitexact=1");
    if (bad) { printf("KB E30 ABORT reason=not_bitexact\n"); return; }

    for (r = 0; r < rounds; r++) {
        uint32_t c0 = esp_cpu_get_cycle_count(), cy;
        nd_parallel_rows(kb_fuse_rows, &fa, rows);          /* A: archive offsets */
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;
        asm volatile("" : "+f"(ya[0][0]));

        c0 = esp_cpu_get_cycle_count();
        nd_parallel_rows(kb_fuse_rows, &fb, rows);          /* B: compacted tier */
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;
        asm volatile("" : "+f"(yb[0][0]));
    }
    printf("KB E30 RES cycles spread_bytes=%u compacted=%u | cyc_per_weight %.4f %.4f | compacted=%+.2f%%\n",
           (unsigned)best[0], (unsigned)best[1],
           (double)best[0] / (double)weights, (double)best[1] / (double)weights,
           100.0 * ((double)best[0] / (double)best[1] - 1.0));
}
#endif

/* ---------------- LANE 2: does a resident q make the Q.K dot cheaper? ------ */
#if ND_KB_LANE == 2
#define KBQ 48u                       /* qk_head_dim, the shipped dot length */
#define KBNBUF 8u

static float s_qa[KBQ] __attribute__((aligned(16)));   /* the q a head could keep live */
static float s_kbuf[KBNBUF][KBQ] __attribute__((aligned(16)));
static float s_qbuf[KBNBUF][KBQ] __attribute__((aligned(16)));

/* The shipped two-term group into one accumulator (#230's dot_c4 schedule),
 * written here so the schedule under test is the one that ships. */
static float kb_dot(const float *a, const float *b)
{
    float s0 = 0.0f, s1 = 0.0f;
    uint32_t j;
    for (j = 0u; j < KBQ; j += 2u) { s0 += a[j] * b[j]; s1 += a[j + 1u] * b[j + 1u]; }
    return s0 + s1;
}

/* Same dots, q fixed for the whole inner block: the operand the head loop could
 * keep live across positions. */
static float kb_dot_qfixed(const float *q, uint32_t nbuf)
{
    float acc = 0.0f, t;
    uint32_t i;
    for (i = 0u; i < nbuf; i++) {
        t = kb_dot(q, s_kbuf[i % KBNBUF]);
        acc += t;
    }
    return acc;
}

/* Both operands rotate - the shipping loop's shape. */
static float kb_dot_both_rotate(uint32_t nbuf)
{
    float acc = 0.0f, t;
    uint32_t i;
    for (i = 0u; i < nbuf; i++) {
        t = kb_dot(s_qbuf[i % KBNBUF], s_kbuf[i % KBNBUF]);
        acc += t;
    }
    return acc;
}

/* Two positions per q, one load of q's operands per pair of dots, each dot's own
 * accumulation order unchanged (so every result is bit-identical to kb_dot). */
static float kb_dot_q2(const float *q, uint32_t nbuf)
{
    float acc = 0.0f, t0, t1;
    uint32_t i;
    for (i = 0u; i + 1u < nbuf; i += 2u) {
        const float *k0 = s_kbuf[i % KBNBUF], *k1 = s_kbuf[(i + 1u) % KBNBUF];
        float a0 = 0.0f, a1 = 0.0f, b0 = 0.0f, b1 = 0.0f;
        uint32_t j;
        for (j = 0u; j < KBQ; j += 2u) {
            float q0 = q[j], q1 = q[j + 1u];
            a0 += q0 * k0[j];      a1 += q1 * k0[j + 1u];
            b0 += q0 * k1[j];      b1 += q1 * k1[j + 1u];
        }
        t0 = (a0 + a1); t1 = (b0 + b1);
        acc += t0 + t1;
    }
    return acc;
}

static void bench_e30(void)
{
    uint32_t r, i, j, n = 256u, rounds = 25u;
    uint32_t best[3] = { 0xffffffffu, 0xffffffffu, 0xffffffffu };
    uint32_t seed = 20260923u;
    float v = 0.0f, a, b;
    uint32_t bad = 0u;

    for (i = 0u; i < KBNBUF; i++)
        for (j = 0u; j < KBQ; j++) {
            seed = seed * 1664525u + 1013904223u;
            s_kbuf[i][j] = ((float)((seed >> 8) & 0xFFFFu) / 32768.0f - 1.0f);
            seed = seed * 1664525u + 1013904223u;
            s_qbuf[i][j] = ((float)((seed >> 8) & 0xFFFFu) / 32768.0f - 1.0f);
        }
    for (j = 0u; j < KBQ; j++) s_qa[j] = s_qbuf[3u][j];

    /* The two-position form must agree with two independent shipped dots. */
    for (i = 0u; i + 1u < KBNBUF; i += 2u) {
        a = kb_dot(s_qa, s_kbuf[i]); b = kb_dot(s_qa, s_kbuf[i + 1u]);
        v = kb_dot_q2(s_qa, i + 2u);
        if (v != a + b) bad++;
    }
    printf("KB E30 L2 n=%u dots_per_round=%u qk_head_dim=%u pair_vs_scalar_mismatch=%u\n",
           (unsigned)n, (unsigned)n, (unsigned)KBQ, (unsigned)bad);

    for (r = 0; r < rounds; r++) {
        uint32_t c0, cy, k;
        c0 = esp_cpu_get_cycle_count();
        for (k = 0u; k < n / KBNBUF; k++) v = kb_dot_both_rotate(KBNBUF);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;
        asm volatile("" : "+f"(v));

        c0 = esp_cpu_get_cycle_count();
        for (k = 0u; k < n / KBNBUF; k++) v = kb_dot_qfixed(s_qa, KBNBUF);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;
        asm volatile("" : "+f"(v));

        c0 = esp_cpu_get_cycle_count();
        for (k = 0u; k < n / (KBNBUF & ~1u); k++) v = kb_dot_q2(s_qa, KBNBUF & ~1u);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[2]) best[2] = cy;
        asm volatile("" : "+f"(v));
    }
    printf("KB E30 RES both_rotate=%.2f q_fixed=%.2f q_shared_pair=%.2f cycles/dot | qfixed=%+.2f%% pair=%+.2f%% (attention dots/token ~14400 = %.1f ms at %.2f cyc/dot)\n",
           (double)best[0] / n, (double)best[1] / n, (double)best[2] / n,
           100.0 * ((double)best[0] / (double)best[1] - 1.0),
           100.0 * ((double)best[0] / (double)best[2] - 1.0),
           14400.0 * ((double)best[0] - (double)best[2]) / 240000000.0 * 1000.0,
           (double)best[0] / n);
}
#endif

/* ---------------- LANE 3: cold-pass read-order sensitivity ---------------- */
#if ND_KB_LANE == 3
static uint32_t s_ord[1024];

static void bench_e30(void)
{
    nd_tensor  t;
    int        ti = -1;
    uint8_t   *blob = NULL, *evict = NULL;
    float     *y = NULL, *lut = NULL, *xh = NULL;
    nd_lut2_ctx c;
    nd_row_fn  fn;
    uint32_t   rows, in_pad, lutn, r, rounds = 10u, s;
    uint32_t   best[6] = { ~0u, ~0u, ~0u, ~0u, ~0u, ~0u };
    static const char *NAME[6] = { "asc", "desc", "stride4", "stride16", "stride64", "shuffled" };
    uint32_t   seed = 7u, k, wtot;

    if (!kb_find_shape(768u, 0u, &t, &ti)) { printf("KB E30 L3 no_tensor\n"); return; }
    rows   = t.shape[0];
    in_pad = nd_cq_in_pad(&t);
    lutn   = nd_cq_lut_floats(in_pad);
    blob  = heap_caps_malloc((size_t)t.nbytes, MALLOC_CAP_SPIRAM);
    evict = heap_caps_malloc(163840u, MALLOC_CAP_SPIRAM);
    y     = heap_caps_malloc(sizeof(float) * rows, MALLOC_CAP_SPIRAM);
    lut   = (float *)ND_ALLOC_FAST(sizeof(float) * lutn);
    xh    = (float *)ND_ALLOC_FAST(sizeof(float) * in_pad);
    if (!blob || !evict || !y || !lut || !xh) { printf("KB E30 L3 alloc_failed\n"); return; }
    memcpy(blob, nd_cact_data(&s_c, &t), t.nbytes);
    kb_lane_xh(xh, in_pad, 3u);
    nd_cq_lut_build(&s_c, xh, in_pad, lut);
    nd_lut2_fill(&c, &t, blob, lut, y);
    fn = nd_lut2_asm_ok(&c, 0u, rows) ? nd_lut2_rows_tie1n : nd_lut2_rows_c;
    wtot = rows * in_pad;
    printf("KB E30 L3 tensor=%d rows=%u in_pad=%u weights=%u asm=%d cold=1(evict 160KB)\n",
           ti, (unsigned)rows, (unsigned)in_pad, (unsigned)wtot, (fn == nd_lut2_rows_tie1n));

    for (s = 0u; s < 6u; s++) {
        /* Build the visit order for this mode. */
        for (k = 0u; k < rows; k++) s_ord[k] = k;
        if (s == 1u) { for (k = 0u; k < rows; k++) s_ord[k] = rows - 1u - k; }
        if (s >= 2u && s <= 4u) {
            uint32_t st = (s == 2u) ? 4u : (s == 3u) ? 16u : 64u, n = 0u, rr;
            for (rr = 0u; rr < st; rr++)
                for (k = rr; k < rows; k += st) s_ord[n++] = k;
        }
        if (s == 5u) {
            for (k = rows; k > 1u; k--) {
                uint32_t j2; seed = seed * 1664525u + 1013904223u;
                j2 = seed % k;
                { uint32_t tmp = s_ord[k - 1u]; s_ord[k - 1u] = s_ord[j2]; s_ord[j2] = tmp; }
            }
        }
        for (r = 0; r < rounds; r++) {
            uint32_t c0, cy, kk;
            volatile uint32_t sink = 0u;
            for (kk = 0u; kk < 163840u / 4u; kk++) sink += evict[kk];   /* evict */
            c0 = esp_cpu_get_cycle_count();
            for (kk = 0u; kk < rows; kk++) fn((void *)&c, s_ord[kk], s_ord[kk] + 1u);
            cy = esp_cpu_get_cycle_count() - c0;
            if (cy < best[s]) best[s] = cy;
            asm volatile("" : "+f"(y[0]));
        }
        printf("KB E30 L3 order=%-9s cycles=%u cyc_per_weight=%.4f\n",
               NAME[s], (unsigned)best[s], (double)best[s] / wtot);
    }
    printf("KB E30 VERDICT order_spread=%.2f%% worst=%u best=%u => layout headroom is %s\n",
           100.0 * ((double)best[0] / (double)best[5] - 1.0),
           (unsigned)best[0], (unsigned)best[5],
           "at most the spread above");
}
#endif
/* ====================== end Experiment 30 =============================== */

#endif


#if ND_KB_EXP31
/* =============== Experiment 31: is the shipping tier already packed? =======
 *
 * Run #350 measured +0.89 % for packed-vs-four-mallocs, but shipping does not
 * use four mallocs: nd_tier_ptr maps ONE contiguous archive span 1:1 into PSRAM,
 * so a layer's tensors sit at their archive offsets. This lane rebuilds exactly
 * that mapping and asks whether packing buys anything over it - and prints the
 * archive gaps between the tensors, which is the mechanism that would explain
 * either answer. Lanes B and C price the two remaining un-attributed costs.
 *
 *   LANE 1  A = one buffer, each tensor at (archive_off - min_off) = shipping's
 *           1:1 tier mapping.  B = the SAME bytes in the SAME buffer, packed at
 *           64 B-aligned offsets in per-token read order. Bit-exact by
 *           construction, so any delta is placement and nothing else.
 *   LANE 2  attention P.V accumulate: the shipped read-modify-write of the
 *           output rows vs a (position x dim) chunked form. Every oh cell is an
 *           independent accumulator, so chunking cannot move a bit.
 *   LANE 3  nd_cq_prepare composition: copy / +memset / +FWHT / full scale pass,
 *           separately. #344 priced the whole call at 0.156 ms (~2 cycles/op)
 *           but never split it, so a TIE FWHT was a guess. This says whether the
 *           FWHT is even the majority of it.
 */
#ifndef ND_KB_LANE
#define ND_KB_LANE 1
#endif

/* ---------------- LANE 1: shipping's 1:1 tier mapping vs packed ------------ */
#if ND_KB_LANE == 1
static void bench_e31(void)
{
    static const uint32_t WANT[4] = { 576u, 96u, 128u, 768u };
    nd_tensor    t[4];
    int          idx[4];
    uint8_t     *src[4];
    uint8_t     *buf = NULL, *pk[4], *ar[4];
    float       *ya[4], *yb[4], *xh = NULL, *lut = NULL;
    nd_lut2_ctx  ca[4], cb[4];
    kb_fuse      fa, fb;
    uint32_t     s, i, in_pad, lutn, rows = 0u, weights = 0u;
    uint32_t     lo = 0xFFFFFFFFu, hi = 0u, packed_sz = 0u;
    uint32_t     rounds = 14u, r, bad = 0u, best[2] = { ~0u, ~0u };
    nd_row_fn    fn;

    memset(&fa, 0, sizeof(fa));
    memset(&fb, 0, sizeof(fb));
    for (s = 0; s < 4u; s++) {
        if (!kb_find_shape(WANT[s], s, &t[s], &idx[s])) {
            printf("KB E31 missing_shape=%u\n", (unsigned)WANT[s]); return;
        }
        src[s] = (uint8_t *)nd_cact_data(&s_c, &t[s]);
        if (t[s].offset < lo) lo = t[s].offset;
        if (t[s].offset + t[s].nbytes > hi) hi = t[s].offset + t[s].nbytes;
        in_pad = nd_cq_in_pad(&t[s]);
        ya[s] = heap_caps_malloc(sizeof(float) * t[s].shape[0], MALLOC_CAP_SPIRAM);
        yb[s] = heap_caps_malloc(sizeof(float) * t[s].shape[0], MALLOC_CAP_SPIRAM);
        if (!ya[s] || !yb[s]) { printf("KB E31 alloc_failed\n"); return; }
        rows    += t[s].shape[0];
        weights += t[s].shape[0] * in_pad;
        fa.nrows[s] = fb.nrows[s] = t[s].shape[0];
        fa.cum[s + 1u] = fb.cum[s + 1u] = rows;
        packed_sz += (t[s].nbytes + 63u) & ~63u;
    }
    fa.nseg = fb.nseg = 4u;

    buf = heap_caps_malloc((size_t)(hi - lo), MALLOC_CAP_SPIRAM);
    if (!buf) { printf("KB E31 alloc_failed span=%u\n", (unsigned)(hi - lo)); return; }
    /* A: shipping's mapping - archive offsets, gaps included. */
    for (s = 0; s < 4u; s++) {
        ar[s] = buf + (t[s].offset - lo);
        memcpy(ar[s], src[s], t[s].nbytes);
    }
    /* B: packed in read order, 64 B aligned so no tensor shares a cache line. */
    {
        uint32_t off = 0u;
        for (s = 0; s < 4u; s++) {
            pk[s] = buf + off;                       /* separate buffer below */
            off  += (t[s].nbytes + 63u) & ~63u;
        }
    }
    /* Put B in its own buffer so the two layouts cannot share lines and so a
     * comparison is layout-only, not "B happens to be the same pages". */
    {
        uint8_t *bbuf = heap_caps_malloc((size_t)packed_sz, MALLOC_CAP_SPIRAM);
        uint32_t off = 0u;
        if (!bbuf) { printf("KB E31 packed_alloc_failed\n"); return; }
        for (s = 0; s < 4u; s++) {
            pk[s] = bbuf + off;
            memcpy(pk[s], src[s], t[s].nbytes);
            off  += (t[s].nbytes + 63u) & ~63u;
        }
        printf("KB E31 L1 archive_span=%u packed_span=%u ids=%u,%u,%u,%u\n",
               (unsigned)(hi - lo), (unsigned)packed_sz,
               (unsigned)idx[0], (unsigned)idx[1], (unsigned)idx[2], (unsigned)idx[3]);
        /* The gap structure IS the mechanism: zero gaps mean shipping is already
         * packed and no layout lever exists for these tensors. */
        for (s = 0; s < 4u; s++)
            printf("KB E31 GAP id=%u off=%u nbytes=%u gap_before=%d\n",
                   (unsigned)idx[s], (unsigned)t[s].offset, (unsigned)t[s].nbytes,
                   (int)((s == 0u) ? 0 : (long)t[s].offset -
                        (long)(t[s - 1u].offset + t[s - 1u].nbytes)));

        in_pad = nd_cq_in_pad(&t[0]);
        lutn   = nd_cq_lut_floats(in_pad);
        xh  = (float *)ND_ALLOC_FAST(sizeof(float) * in_pad);
        lut = (float *)ND_ALLOC_FAST(sizeof(float) * lutn);
        if (!xh || !lut) { printf("KB E31 lut_alloc_failed\n"); return; }
        kb_lane_xh(xh, in_pad, 1u);
        nd_cq_lut_build(&s_c, xh, in_pad, lut);

        fn = nd_lut2_rows_tie1n;
        for (s = 0; s < 4u; s++) {
            nd_lut2_fill(&ca[s], &t[s], ar[s], lut, ya[s]);
            nd_lut2_fill(&cb[s], &t[s], pk[s], lut, yb[s]);
            if (!nd_lut2_asm_ok(&ca[s], 0u, fa.nrows[s]) ||
                !nd_lut2_asm_ok(&cb[s], 0u, fb.nrows[s])) fn = nd_lut2_rows_c;
        }
        for (s = 0; s < 4u; s++) { fa.ctx[s] = ca[s]; fb.ctx[s] = cb[s]; }
        fa.fn = fb.fn = fn;

        nd_parallel_rows(kb_fuse_rows, &fa, rows);
        nd_parallel_rows(kb_fuse_rows, &fb, rows);
        for (s = 0; s < 4u; s++)
            for (i = 0u; i < fa.nrows[s]; i++)
                if (memcmp(&ya[s][i], &yb[s][i], 4) != 0) bad++;
        printf("KB E31 NUM rows=%u mismatch=%u%s asm=%d\n", (unsigned)rows,
               (unsigned)bad, bad ? "" : " bitexact=1", (fn == nd_lut2_rows_tie1n));
        if (bad) { printf("KB E31 ABORT reason=not_bitexact\n"); return; }

        for (r = 0; r < rounds; r++) {
            uint32_t c0 = esp_cpu_get_cycle_count(), cy;
            nd_parallel_rows(kb_fuse_rows, &fa, rows);      /* A: 1:1 tier map */
            cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;
            asm volatile("" : "+f"(ya[0][0]));
            c0 = esp_cpu_get_cycle_count();
            nd_parallel_rows(kb_fuse_rows, &fb, rows);      /* B: packed order */
            cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;
            asm volatile("" : "+f"(yb[0][0]));
        }
        printf("KB E31 RES cycles tier_1to1=%u packed=%u | cyc_per_weight %.4f %.4f | packed_over_shipping_map=%+.2f%%\n",
               (unsigned)best[0], (unsigned)best[1],
               (double)best[0] / (double)weights, (double)best[1] / (double)weights,
               100.0 * ((double)best[0] / (double)best[1] - 1.0));
    }
}
#endif

/* ---------------- LANE 2: attention P.V accumulate, chunked vs shipped ----- */
#if ND_KB_LANE == 2
#define KBD 24u                       /* half of head_dim, as attn_heads splits */
#define KBP 8u                        /* positions in the timed block */
static float s_oh[KBP][KBD] __attribute__((aligned(16)));
static float s_v[KBP][KBD] __attribute__((aligned(16)));
static float s_v2[KBP][KBD];
static float s_w[KBP];

/* The shipped shape: for each position, walk the whole dim, read-modify-writing
 * oh[pos][d]. */
static void kb_pv_shipped(uint32_t reps)
{
    uint32_t p, d, k;
    for (k = 0u; k < reps; k++)
        for (p = 0u; p < KBP; p++) {
            float w = s_w[p] + (float)k * 1e-6f;
            for (d = 0u; d < KBD; d++) s_oh[p][d] += w * s_v[p][d];
        }
}

/* Same updates, dim-major outside: every oh cell is an independent accumulator,
 * so the value of each cell after the block is identical bit-for-bit. */
static void kb_pv_chunked(uint32_t reps)
{
    uint32_t p, d, k;
    for (d = 0u; d < KBD; d++)
        for (k = 0u; k < reps; k++)
            for (p = 0u; p < KBP; p++) {
                float w = s_w[p] + (float)k * 1e-6f;
                s_oh[p][d] += w * s_v[p][d];
            }
}

static void bench_e31(void)
{
    uint32_t r, p, d, rounds = 25u, reps = 32u;
    uint32_t best[2] = { ~0u, ~0u };
    uint32_t seed = 991u, bad = 0u;
    double updates = (double)KBP * KBD * reps;

    for (p = 0u; p < KBP; p++)
        for (d = 0u; d < KBD; d++) {
            seed = seed * 1664525u + 1013904223u;
            s_v[p][d] = ((float)((seed >> 8) & 0xFFFFu) / 32768.0f - 1.0f);
            seed = seed * 1664525u + 1013904223u;
            s_v2[p][d] = ((float)((seed >> 8) & 0xFFFFu) / 32768.0f - 1.0f);
        }
    for (p = 0u; p < KBP; p++) s_w[p] = 0.5f + 0.01f * (float)p;

    /* Exactness of the reordering, cell by cell. */
    for (p = 0u; p < KBP; p++) for (d = 0u; d < KBD; d++) s_oh[p][d] = s_v2[p][d];
    kb_pv_shipped(reps);
    for (p = 0u; p < KBP; p++) for (d = 0u; d < KBD; d++) s_v2[p][d] = s_oh[p][d];
    for (p = 0u; p < KBP; p++) for (d = 0u; d < KBD; d++) s_oh[p][d] = s_v[p][d] * 0.0f + s_v2[p][d] * 0.0f + s_v[p][d];
    /* reset both to the same start and run the two orders separately */
    for (p = 0u; p < KBP; p++) for (d = 0u; d < KBD; d++) { s_oh[p][d] = s_v[p][d]; }
    kb_pv_shipped(reps);
    for (p = 0u; p < KBP; p++) for (d = 0u; d < KBD; d++) s_v2[p][d] = s_oh[p][d];
    for (p = 0u; p < KBP; p++) for (d = 0u; d < KBD; d++) s_oh[p][d] = s_v[p][d];
    kb_pv_chunked(reps);
    for (p = 0u; p < KBP; p++) for (d = 0u; d < KBD; d++)
        if (s_oh[p][d] != s_v2[p][d]) bad++;

    printf("KB E31 L2 cells=%u updates=%u order_mismatch=%u%s\n",
           (unsigned)(KBP * KBD), (unsigned)(KBP * KBD * reps), (unsigned)bad,
           bad ? "" : " bitexact=1");

    for (r = 0; r < rounds; r++) {
        uint32_t c0, cy;
        c0 = esp_cpu_get_cycle_count(); kb_pv_shipped(reps);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;
        asm volatile("" : "+f"(s_oh[0][0]));
        c0 = esp_cpu_get_cycle_count(); kb_pv_chunked(reps);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;
        asm volatile("" : "+f"(s_oh[0][0]));
    }
    printf("KB E31 RES cycles shipped=%u chunked=%u | cyc_per_update %.4f %.4f | chunked=%+.2f%% (P.V phase ~15 ms/token)\n",
           (unsigned)best[0], (unsigned)best[1], (double)best[0] / updates,
           (double)best[1] / updates, 100.0 * ((double)best[0] / (double)best[1] - 1.0));
}
#endif

/* ---------------- LANE 3: what is actually inside nd_cq_prepare ------------ */
#if ND_KB_LANE == 3
static float s_pin[3072] __attribute__((aligned(16)));
static float s_pout[3072] __attribute__((aligned(16)));

static void bench_e31(void)
{
    nd_tensor  tq;
    int        ti = -1;
    uint32_t   r, rounds = 25u, n, ngroup, g = 128u;
    uint32_t   best[4] = { ~0u, ~0u, ~0u, ~0u }, seed = 4242u, j;

    /* The engine's own geometry, not an invented one: nd_cq_prepare takes the
     * tensor and derives in_pad and the group count from it. */
    if (!kb_find_shape(768u, 0u, &tq, &ti)) { printf("KB E31 L3 no_tensor\n"); return; }
    n = nd_cq_in_pad(&tq);
    ngroup = n / g;
    if (n > 3072u) { printf("KB E31 L3 in_pad_too_big=%u\n", (unsigned)n); return; }

    for (j = 0u; j < n; j++) {
        seed = seed * 1664525u + 1013904223u;
        s_pin[j] = ((float)((seed >> 8) & 0xFFFFu) / 32768.0f - 1.0f);
    }

    for (r = 0; r < rounds; r++) {
        uint32_t c0, cy, gi, st, half, k, base;
        float scale;

        /* 1: the activation copy alone (xh <- xin), as prepare does it. */
        c0 = esp_cpu_get_cycle_count();
        memcpy(s_pout, s_pin, sizeof(float) * n);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;

        /* 2: + the FWHT. Each group's 128-point transform is what prepare runs
         * per group; here it is the same butterfly schedule the engine uses. */
        c0 = esp_cpu_get_cycle_count();
        for (gi = 0; gi < ngroup; gi++) {
            float *x = s_pout + (size_t)gi * g;
            for (half = 1u; half < g; half <<= 1) {
                for (base = 0; base < g; base += (half << 1)) {
                    for (k = 0; k < half; k++) {
                        float a = x[base + k], b = x[base + k + half];
                        x[base + k] = a + b;
                        x[base + k + half] = a - b;
                    }
                }
            }
            (void)st;
        }
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;

        /* 3: + the scale pass (prepare divides by in_pad and writes xh). */
        c0 = esp_cpu_get_cycle_count();
        scale = 1.0f / (float)n;
        for (j = 0u; j < n; j++) s_pout[j] *= scale;
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[2]) best[2] = cy;

        /* 4: the whole call, the engine's own function. */
        c0 = esp_cpu_get_cycle_count();
        nd_cq_prepare(&tq, s_pin, s_pout);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[3]) best[3] = cy;
        asm volatile("" : "+f"(s_pout[0]));
    }
    printf("KB E31 L3 in_pad=%u ngroup=%u group=%u\n", (unsigned)n, (unsigned)ngroup, (unsigned)g);
    printf("KB E31 RES cycles copy=%u fwht=%u scale=%u full_nd_cq_prepare=%u | share of full: copy %.1f%% fwht %.1f%% scale %.1f%%\n",
           (unsigned)best[0], (unsigned)best[1], (unsigned)best[2], (unsigned)best[3],
           100.0 * (double)best[0] / (double)best[3],
           100.0 * (double)best[1] / (double)best[3],
           100.0 * (double)best[2] / (double)best[3]);
    printf("KB E31 VERDICT fwht_is_majority=%d -> a TIE FWHT is worth at most %.1f%% of a %.3f ms call, x ~16 calls/token = %.2f ms\n",
           (best[1] * 2u > best[3]),
           100.0 * (double)best[1] / (double)best[3],
           (double)best[3] / 240000000.0,
           16.0 * (double)best[1] / 240000000.0);
}
#endif
/* ====================== end Experiment 31 =============================== */

#endif

#if ND_KB_EXP32
/* ============ Experiment 32: the FWHT is 64 % of prepare - is it SIMD-able? =
 *
 * Run #351 lane 3 split nd_cq_prepare (0.245 ms/call, ~16 calls/token = 3.9 ms,
 * which reproduces the phase map's prep+lut 3.6 ms) and found the transform owns
 * 64.1 % of it: 37,763 cycles for 6 groups x 128-point FWHT = 2,688 butterflies =
 * 14 cycles each, against a ~6-instruction scalar butterfly. If a vector form is
 * bit-exact and 2-3x faster the prize is ~1.2 ms/token = +0.6 % decode, which is
 * three keep bars.
 *
 * Bit-exactness is structural, not hoped for: a butterfly is x[k]=a+b,
 * x[k+h]=a-b over the SAME two operands in the SAME order, so a vector version
 * computes identical IEEE results, just several cells per instruction.
 *
 * Wide loads are exactly the thing run #335 caught returning wrong values on
 * this core, so every variant here carries a KNOWN-ANSWER probe (delta input ->
 * all ones; constant input -> exact powers of two) alongside the differential
 * against the scalar reference. A fast variant that fails the probe is discarded
 * on the spot, as #335's ee.ldf forms were.
 *
 *   LANE 1  scalar reference vs unrolled scalar vs ee.ldf.64/ee.add.64 vector
 *           butterflies vs the 128-bit form, on the real 128-point group.
 *   LANE 2  price it on the REAL function: is the model's transform bit-identical
 *           to nd_cq_prepare's, and what does the engine's call cost at each real
 *           in_pad (768 and 3072), so the prize has a real denominator.
 *   LANE 3  the remaining 17.8 % of the call (copy 3.3 %, scale 14.5 %): the
 *           scale pass costs 11 cycles per element, which is far off a multiply's
 *           floor, so test restrict and vector forms for it.
 */
#ifndef ND_KB_LANE
#define ND_KB_LANE 1
#endif
#ifndef KB_TIE_ASM
#define KB_TIE_ASM 1
#endif

#define KBG 128u
static float s_fw[KBG] __attribute__((aligned(16)));
static float s_fr[KBG] __attribute__((aligned(16)));

/* The reference: the scalar butterfly order the engine uses. */
static void kb_fwht_scalar(float *x, uint32_t g)
{
    uint32_t half, base, k;
    for (half = 1u; half < g; half <<= 1)
        for (base = 0; base < g; base += (half << 1))
            for (k = 0; k < half; k++) {
                float a = x[base + k], b = x[base + k + half];
                x[base + k] = a + b;
                x[base + k + half] = a - b;
            }
}

/* Unrolled in k. Cannot move a bit: each cell reads its own two operands. */
static void kb_fwht_unroll(float *x, uint32_t g)
{
    uint32_t half, base, k;
    for (half = 1u; half < g; half <<= 1)
        for (base = 0; base < g; base += (half << 1)) {
            for (k = 0u; k + 1u < half; k += 2u) {
                float a0 = x[base + k],      b0 = x[base + k + half];
                float a1 = x[base + k + 1u], b1 = x[base + k + 1u + half];
                x[base + k]            = a0 + b0;
                x[base + k + half]     = a0 - b0;
                x[base + k + 1u]       = a1 + b1;
                x[base + k + 1u + half]= a1 - b1;
            }
            for (; k < half; k++) {
                float a = x[base + k], b = x[base + k + half];
                x[base + k] = a + b;
                x[base + k + half] = a - b;
            }
        }
}

#if KB_TIE_ASM
/* FLOAT SIMD IS NOT AVAILABLE ON THIS PART, measured, not assumed: ee.add.64,
 * ee.adds.64, ee.adds.sp, ee.addsp.64, ee.vadd.64, ee.sub*.64, ee.mul*.64 and
 * ee.ldf.32 are ALL rejected by the assembler this toolchain ships ("unknown
 * opcode or format name"), which is also what engine/src/lut2_tie728.S:24 records
 * from the TIE728 work. Only the wide FLOAT LOADS assemble (ee.ldf.64.ip /
 * ee.ldf.128.ip, the forms run #332/#335 used). So a vectorised butterfly is
 * impossible; what IS still possible is fewer load/store instructions around the
 * same scalar adds and subs - which is bit-exact by construction because the
 * arithmetic is the scalar arithmetic, only the operand movement changes.
 * Run #335 proved ee.ldf can return values that are not the ones asked for when
 * the access pattern goes through the 2-bit decoder, so this variant earns its
 * keep only if it passes the same known-answer probe and the bit-differential. */
static void kb_fwht_ld64(float *x, uint32_t g)
{
    uint32_t half, base, k;
    for (half = 1u; half < g; half <<= 1) {
        for (base = 0; base < g; base += (half << 1)) {
            if (half == 1u) {
                float a = x[base], b = x[base + 1u];
                x[base] = a + b;
                x[base + 1u] = a - b;
                continue;
            }
            for (k = 0u; k + 1u < half; k += 2u) {
                float a0, a1, b0, b1;
                float *pa = &x[base + k];
                float *pb = &x[base + k + half];
                __asm__ __volatile__(
                    "ee.ldf.64.ip f4, f5, %[_a], 8\n\t"
                    "ee.ldf.64.ip f6, f7, %[_b], 8\n\t"
                    "add.s  f8, f4, f6\n\t"
                    "add.s  f10, f5, f7\n\t"
                    "sub.s  f12, f4, f6\n\t"
                    "sub.s  f14, f5, f7\n\t"
                    "ee.stf.64.ip f8, f10, %[_c], 8\n\t"
                    "ee.stf.64.ip f12, f14, %[_d], 8\n\t"
                    : [_a] "+a"(pa), [_b] "+a"(pb), [_c] "+a"(pa), [_d] "+a"(pb)
                    :
                    : "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11",
                      "f12", "f13", "f14", "f15", "memory");
            }
        }
    }
}
#endif

/* Four butterflies per inner step, all eight operands held in locals: fewer
 * dependent reloads per cell, and every cell still sees its own two operands. */
static void kb_fwht_block4(float *x, uint32_t g)
{
    uint32_t half, base, k;
    for (half = 1u; half < g; half <<= 1)
        for (base = 0; base < g; base += (half << 1)) {
            for (k = 0u; k + 3u < half; k += 4u) {
                float a0 = x[base+k],      b0 = x[base+k+half];
                float a1 = x[base+k+1u],   b1 = x[base+k+1u+half];
                float a2 = x[base+k+2u],   b2 = x[base+k+2u+half];
                float a3 = x[base+k+3u],   b3 = x[base+k+3u+half];
                x[base+k]          = a0 + b0;  x[base+k+half]     = a0 - b0;
                x[base+k+1u]       = a1 + b1;  x[base+k+1u+half]  = a1 - b1;
                x[base+k+2u]       = a2 + b2;  x[base+k+2u+half]  = a2 - b2;
                x[base+k+3u]       = a3 + b3;  x[base+k+3u+half]  = a3 - b3;
            }
            for (; k < half; k++) {
                float a = x[base + k], b = x[base + k + half];
                x[base + k] = a + b;
                x[base + k + half] = a - b;
            }
        }
}

static void kb_fill_delta(float *x, uint32_t g)   /* FWHT = all ones */
{
    uint32_t j;
    for (j = 0; j < g; j++) x[j] = (j == 0u) ? 1.0f : 0.0f;
}
static void kb_fill_const(float *x, uint32_t g)    /* FWHT = [g,0,0,...] */
{
    uint32_t j;
    for (j = 0; j < g; j++) x[j] = 0.25f;
}
/* Returns 0 when every cell equals the hand-computed answer. */
static uint32_t kb_probe(float *x, uint32_t g, uint32_t which)
{
    uint32_t j, bad = 0u;
    for (j = 0; j < g; j++) {
        float want = (which == 0u) ? 1.0f : (j == 0u ? (float)g * 0.25f : 0.0f);
        if (x[j] != want) bad++;
    }
    return bad;
}

static uint32_t kb_diff(float *a, float *b, uint32_t g)
{
    uint32_t j, bad = 0u;
    for (j = 0; j < g; j++) if (memcmp(&a[j], &b[j], 4) != 0) bad++;
    return bad;
}

/* ---------------- LANE 1: the kernel screen -------------------------------- */
#if ND_KB_LANE == 1
#define KN 6u            /* groups per timed round, as one 768-wide prepare has */
static void kb_apply(void (*fn)(float *, uint32_t), uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) fn(s_fw, KBG);
    asm volatile("" : "+f"(s_fw[0]));
}

static void bench_e32(void)
{
    uint32_t r, rounds = 25u, n = 24u, seed = 31337u, j;
    uint32_t best[4] = { ~0u, ~0u, ~0u, ~0u }, bad[4] = {0,0,0,0}, pb[4] = {0,0,0,0};
    double butterflies = (double)(KBG / 2u) * 7u;   /* 448 per group */

    for (j = 0; j < KBG; j++) {
        seed = seed * 1664525u + 1013904223u;
        s_fr[j] = ((float)((seed >> 8) & 0xFFFFu) / 32768.0f - 1.0f);
    }

    /* Correctness first: each variant must reproduce the scalar reference on
     * real-shaped data AND answer the two hand-computable probes exactly. */
    {
        float ref[KBG];
        memcpy(ref, s_fr, sizeof(ref));
        kb_fwht_scalar(ref, KBG);
        memcpy(s_fw, s_fr, sizeof(ref)); kb_fwht_unroll(s_fw, KBG);
        bad[1] = kb_diff(ref, s_fw, KBG);
        memcpy(s_fw, s_fr, sizeof(ref)); kb_fwht_scalar(s_fw, KBG);
        bad[0] = kb_diff(ref, s_fw, KBG);
#if KB_TIE_ASM
        memcpy(s_fw, s_fr, sizeof(ref)); kb_fwht_block4(s_fw, KBG);
        bad[2] = kb_diff(ref, s_fw, KBG);
#if KB_TIE_ASM
        memcpy(s_fw, s_fr, sizeof(ref)); kb_fwht_ld64(s_fw, KBG);
        bad[3] = kb_diff(ref, s_fw, KBG);
#endif
#endif
        kb_fill_delta(s_fw, KBG); kb_fwht_scalar(s_fw, KBG); pb[0] = kb_probe(s_fw, KBG, 0u);
        kb_fill_const(s_fw, KBG);  kb_fwht_scalar(s_fw, KBG);
        pb[0] += kb_probe(s_fw, KBG, 1u);
        kb_fill_delta(s_fw, KBG); kb_fwht_block4(s_fw, KBG); pb[2] = kb_probe(s_fw, KBG, 0u);
        kb_fill_const(s_fw, KBG);  kb_fwht_block4(s_fw, KBG); pb[2] += kb_probe(s_fw, KBG, 1u);
#if KB_TIE_ASM
        kb_fill_delta(s_fw, KBG); kb_fwht_ld64(s_fw, KBG);   pb[3] = kb_probe(s_fw, KBG, 0u);
        kb_fill_const(s_fw, KBG);  kb_fwht_ld64(s_fw, KBG);   pb[3] += kb_probe(s_fw, KBG, 1u);
#endif
        printf("KB E32 L1 group=%u cells=%u diff_vs_scalar=%u,%u,%u,%u probe_bad=%u,%u,%u,%u (scalar,unroll,block4,wide_load64)\n",
               (unsigned)KBG, (unsigned)KBG, (unsigned)bad[0], (unsigned)bad[1],
               (unsigned)bad[2], (unsigned)bad[3], (unsigned)pb[0], (unsigned)pb[1],
               (unsigned)pb[2], (unsigned)pb[3]);
    }

    for (r = 0; r < rounds; r++) {
        uint32_t c0, cy;
        c0 = esp_cpu_get_cycle_count(); kb_apply(kb_fwht_scalar, n);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;
        c0 = esp_cpu_get_cycle_count(); kb_apply(kb_fwht_unroll, n);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;
#if KB_TIE_ASM
        c0 = esp_cpu_get_cycle_count(); kb_apply(kb_fwht_block4, n);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[2]) best[2] = cy;
        c0 = esp_cpu_get_cycle_count(); kb_apply(kb_fwht_ld64, n);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[3]) best[3] = cy;
#endif
    }
    printf("KB E32 L1 RES cycles=%u,%u,%u,%u | cyc_per_butterfly %.2f %.2f %.2f %.2f | unroll=%+.2f%% block4=%+.2f%% wide_load64=%+.2f%%\n",
           (unsigned)best[0], (unsigned)best[1], (unsigned)best[2], (unsigned)best[3],
           (double)best[0] / (butterflies * n), (double)best[1] / (butterflies * n),
           (double)best[2] / (butterflies * n), (double)best[3] / (butterflies * n),
           100.0 * ((double)best[0] / (double)best[1] - 1.0),
           100.0 * ((double)best[0] / (double)best[2] - 1.0),
           100.0 * ((double)best[0] / (double)(best[3] ? best[3] : 1u) - 1.0));
    printf("KB E32 PRICE fwht_share=64.1%% of a 3.9 ms/token phase; a %.2fx transform saves %.2f ms = %+.2f%% decode\n",
           (double)best[0] / (double)best[2],
           3.9 * 0.641 * (1.0 - (double)best[2] / (double)best[0]),
           100.0 * (3.9 * 0.641 * (1.0 - (double)best[2] / (double)best[0])) / 197.0);
}
#endif

/* ---------------- LANE 2: is the model the engine, and what does the real
 * call cost at each real geometry ------------------------------------------ */
#if ND_KB_LANE == 2
static float s_pin[3072] __attribute__((aligned(16)));
static float s_eng[3072] __attribute__((aligned(16)));
static float s_mod[3072] __attribute__((aligned(16)));

static void bench_e32(void)
{
    static const uint32_t ROWS[4] = { 768u, 768u, 768u, 576u };
    nd_tensor  t[4];
    int        idx[4];
    uint32_t   s, j, q, rounds = 20u, seed = 5150u;

    printf("KB E32 L2 engine_vs_model\n");
    for (s = 0u; s < 4u; s++) {
        if (!kb_find_shape(ROWS[s], s, &t[s], &idx[s])) {
            printf("KB E32 L2 missing=%u\n", (unsigned)ROWS[s]); continue;
        }
        {
            uint32_t n = nd_cq_in_pad(&t[s]), ngroup, gi, half, base, k, bad;
            uint32_t best = ~0u, c0, cy;
            if (n > 3072u) { printf("KB E32 L2 in_pad_too_big=%u\n", (unsigned)n); continue; }
            for (j = 0; j < n; j++) {
                seed = seed * 1664525u + 1013904223u;
                s_pin[j] = ((float)((seed >> 8) & 0xFFFFu) / 32768.0f - 1.0f);
            }
            /* The engine's own call, timed. */
            for (q = 0; q < rounds; q++) {
                c0 = esp_cpu_get_cycle_count();
                nd_cq_prepare(&t[s], s_pin, s_eng);
                cy = esp_cpu_get_cycle_count() - c0; if (cy < best) best = cy;
                asm volatile("" : "+f"(s_eng[0]));
            }
            /* My model, then bit-compare with the engine's output. */
            memcpy(s_mod, s_pin, sizeof(float) * n);
            ngroup = n / KBG;
            for (gi = 0; gi < ngroup; gi++) kb_fwht_scalar(s_mod + (size_t)gi * KBG, KBG);
            {
                float sc = 1.0f / (float)n;
                for (j = 0; j < n; j++) s_mod[j] *= sc;
            }
            bad = 0u;
            for (j = 0; j < n; j++) if (memcmp(&s_eng[j], &s_mod[j], 4) != 0) bad++;
            printf("KB E32 L2 tensor=%d out=%u in_pad=%u ngroup=%u engine_cycles=%u model_ms=%.4f model_vs_engine_bad=%u\n",
                   idx[s], (unsigned)t[s].shape[0], (unsigned)n, (unsigned)ngroup,
                   (unsigned)best, (double)best / 240000.0, (unsigned)bad);
        }
    }
}
#endif

/* ---------------- LANE 3: the scale pass (14.5 % of the call) -------------- */
#if ND_KB_LANE == 3
static float s_v[3072] __attribute__((aligned(16)));
static float s_ref32[3072] __attribute__((aligned(16)));

static void kb_fillv(float *x, uint32_t n, uint32_t sd)
{
    uint32_t jj;
    for (jj = 0u; jj < n; jj++) {
        sd = sd * 1664525u + 1013904223u;
        x[jj] = ((float)((sd >> 8) & 0xFFFFu) / 32768.0f - 1.0f);
    }
}

static void kb_scale_plain(float *x, uint32_t n, float sc)
{
    uint32_t jj;
    for (jj = 0; jj < n; jj++) x[jj] *= sc;
}
static void kb_scale_restrict(float *restrict x, uint32_t n, float sc)
{
    uint32_t jj;
    for (jj = 0; jj < n; jj++) x[jj] *= sc;
}
/* There is no vector float multiply on this part (the same probe table that
 * rejects ee.add.64 rejects ee.muls.64), so the only scalar lever left is giving
 * the loop scheduler independent work. */
static void kb_scale_unroll4(float *restrict x, uint32_t n, float sc)
{
    uint32_t jj;
    for (jj = 0u; jj + 3u < n; jj += 4u) {
        float v0 = x[jj], v1 = x[jj+1u], v2 = x[jj+2u], v3 = x[jj+3u];
        x[jj] = v0 * sc; x[jj+1u] = v1 * sc;
        x[jj+2u] = v2 * sc; x[jj+3u] = v3 * sc;
    }
    for (; jj < n; jj++) x[jj] *= sc;
}

static void bench_e32(void)
{
    uint32_t r, rounds = 25u, n = 768u, jj, bad = 0u;
    uint32_t best[3] = { ~0u, ~0u, ~0u };
    float sc = 1.0f / 768.0f;

    /* One canonical input, one canonical reference, then each variant against it
     * from the SAME start - the #341 lesson, that a comparison must not advance
     * the generator between the two sides it is comparing. */
    kb_fillv(s_ref32, n, 6161u);
    kb_scale_plain(s_ref32, n, sc);

    kb_fillv(s_v, n, 6161u); kb_scale_restrict(s_v, n, sc);
    for (jj = 0u; jj < n; jj++) if (memcmp(&s_v[jj], &s_ref32[jj], 4) != 0) bad++;
    kb_fillv(s_v, n, 6161u); kb_scale_unroll4(s_v, n, sc);
    for (jj = 0u; jj < n; jj++) if (memcmp(&s_v[jj], &s_ref32[jj], 4) != 0) bad++;
    printf("KB E32 L3 n=%u scale_mismatch=%u%s\n", (unsigned)n, (unsigned)bad,
           bad ? "" : " bitexact=1");

    for (r = 0; r < rounds; r++) {
        uint32_t c0, cy;
        c0 = esp_cpu_get_cycle_count(); kb_scale_plain(s_v, n, sc);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;
        asm volatile("" : "+f"(s_v[0]));
        c0 = esp_cpu_get_cycle_count(); kb_scale_restrict(s_v, n, sc);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;
        asm volatile("" : "+f"(s_v[0]));
        c0 = esp_cpu_get_cycle_count(); kb_scale_unroll4(s_v, n, sc);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[2]) best[2] = cy;
        asm volatile("" : "+f"(s_v[0]));
    }
    printf("KB E32 L3 RES cycles=%u,%u,%u | cyc_per_element %.2f %.2f %.2f | restrict=%+.2f%% unroll4=%+.2f%% (scale is 14.5%% of a 3.9 ms/token phase)\n",
           (unsigned)best[0], (unsigned)best[1], (unsigned)best[2],
           (double)best[0] / n, (double)best[1] / n, (double)best[2] / n,
           100.0 * ((double)best[0] / (double)best[1] - 1.0),
           100.0 * ((double)best[0] / (double)(best[2] ? best[2] : 1u) - 1.0));
}
#endif

/* ====================== end Experiment 32 =============================== */

#endif

#if ND_KB_EXP33
/* ============ Experiment 33: prepare's two inner loops, diffed against the
 * ================= shipped functions, before anything is edited ============
 *
 * Run #351 split nd_cq_prepare (64.1 % transform, 14.5 % scale) and run #352
 * screened my own reconstruction: unrolling the k-loop by 2 was +16.2 % and
 * feeding the same scalar adds/subs from ee.ldf.64.ip wide loads +18.78 %, all
 * bit-exact, and the scale loop's unroll-4 was +85.8 %.
 *
 * But #352's reference was MY transform, not the engine's, so nothing here is
 * allowed to rest on it: every variant below is diffed against the SHIPPED
 * nd_fwht symbol (engine/src/nd_quant.c:39) rather than a copy of it - the rule
 * run #338 learned, that a test must include the shipping implementation and not
 * a transcription - and the end-to-end number is taken through
 * nd_parallel_rows, because nd_cq_prepare runs the transform two-core and a
 * one-core comparison would price the wrong thing.
 *
 *   LANE 1  nd_fwht (the real function) vs k-unrolled vs wide-load-fed vs both.
 *   LANE 2  the per-group rescale in fwht_rows: shipped vs unroll-4 vs unroll-8,
 *           and whether the two-core split of 6 groups is even worth it.
 *   LANE 3  the whole fwht_rows body, shipped vs both changes combined, through
 *           nd_parallel_rows on the real geometry. This is the number that decides
 *           whether to edit the engine at all.
 */
#ifndef ND_KB_LANE
#define ND_KB_LANE 1
#endif
#ifndef KB_TIE_ASM
#define KB_TIE_ASM 1
#endif

#define KBG 128u                                  /* every CQ group in this model */
static float s_a[KBG] __attribute__((aligned(16)));
static float s_b[KBG] __attribute__((aligned(16)));

/* Variant A: unroll the inner j-loop by 2. Each cell still computes a+b / a-b
 * over its own two operands, so it cannot move a bit - but that claim is checked
 * against the shipped nd_fwht below, not assumed. */
static void kb_fwht_u2(float *x, uint32_t n)
{
    uint32_t len;
    for (len = 1; len < n; len <<= 1) {
        uint32_t i;
        for (i = 0; i < n; i += len << 1) {
            uint32_t j = i;
            for (; j + 1u < i + len; j += 2u) {
                float a0 = x[j], b0 = x[j + len];
                float a1 = x[j + 1u], b1 = x[j + 1u + len];
                x[j]            = a0 + b0;
                x[j + len]      = a0 - b0;
                x[j + 1u]       = a1 + b1;
                x[j + 1u + len] = a1 - b1;
            }
            for (; j < i + len; j++) {
                float a = x[j], b = x[j + len];
                x[j] = a + b;
                x[j + len] = a - b;
            }
        }
    }
}

#if KB_TIE_ASM
/* Variant B: the SAME scalar adds and subs, but two floats per load and per
 * store. Half the memory instructions per butterfly, no change to arithmetic.
 * Run #335 proved ee.ldf can hand back values that are not the ones asked for
 * when the access pattern goes through the 2-bit decoder, so this is only
 * admissible because the differential below is against the shipped function on
 * real data. Alignment: (j, j+1) and (j+len, j+len+1) are 8-byte aligned
 * whenever len >= 2, so len == 1 stays on the shipped path. */
static void kb_fwht_ld2(float *x, uint32_t n)
{
    uint32_t len;
    for (len = 1u; len < n; len <<= 1) {
        uint32_t i;
        for (i = 0; i < n; i += len << 1) {
            if (len == 1u) {
                float a = x[i], b = x[i + 1u];
                x[i] = a + b;
                x[i + 1u] = a - b;
                continue;
            }
            for (uint32_t j = i; j + 1u < i + len; j += 2u) {
                float *pa = &x[j];
                float *pb = &x[j + len];
                float *qa = &x[j];
                float *qb = &x[j + len];
                __asm__ __volatile__(
                    "ee.ldf.64.ip  f4, f5, %[_a], 8\n\t"
                    "ee.ldf.64.ip  f6, f7, %[_b], 8\n\t"
                    "add.s  f8, f4, f6\n\t"
                    "add.s  f10, f5, f7\n\t"
                    "sub.s  f12, f4, f6\n\t"
                    "sub.s  f14, f5, f7\n\t"
                    "ee.stf.64.ip  f8, f10, %[_c], 8\n\t"
                    "ee.stf.64.ip  f12, f14, %[_d], 8\n\t"
                    : [_a] "+a"(pa), [_b] "+a"(pb), [_c] "+a"(qa), [_d] "+a"(qb)
                    :
                    : "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11",
                      "f12", "f13", "f14", "f15", "memory");
            }
        }
    }
}
#endif

/* Variant C: both. */
static void kb_fwht_u2ld2(float *x, uint32_t n)
{
    uint32_t len;
    for (len = 1u; len < n; len <<= 1) {
        uint32_t i;
        for (i = 0; i < n; i += len << 1) {
            if (len == 1u) {
                float a = x[i], b = x[i + 1u];
                x[i] = a + b;
                x[i + 1u] = a - b;
                continue;
            }
            for (uint32_t j = i; j + 3u < i + len; j += 4u) {
                /* Separate load and store cursors ON PURPOSE: .ip post-updates the
                 * base register, so two +8 loads advance the cursor 16 bytes, and a
                 * store that reuses that same register would write 16 bytes past the
                 * butterflies it just read. The single-pair variant above is only
                 * correct because it happens to use distinct variables. */
                float *pa = &x[j];
                float *pb = &x[j + len];
                float *sa = &x[j];
                float *sb = &x[j + len];
                __asm__ __volatile__(
                    "ee.ldf.64.ip  f4, f5, %[_a], 8\n\t"
                    "ee.ldf.64.ip  f6, f7, %[_b], 8\n\t"
                    "ee.ldf.64.ip  f0, f1, %[_a], 8\n\t"
                    "ee.ldf.64.ip  f2, f3, %[_b], 8\n\t"
                    "add.s  f8,  f4, f6\n\t"
                    "add.s  f10, f5, f7\n\t"
                    "sub.s  f12, f4, f6\n\t"
                    "sub.s  f14, f5, f7\n\t"
                    "ee.stf.64.ip  f8, f10, %[_c], 8\n\t"
                    "ee.stf.64.ip  f12, f14, %[_d], 8\n\t"
                    "add.s  f8,  f0, f2\n\t"
                    "add.s  f10, f1, f3\n\t"
                    "sub.s  f12, f0, f2\n\t"
                    "sub.s  f14, f1, f3\n\t"
                    "ee.stf.64.ip  f8, f10, %[_c], 8\n\t"
                    "ee.stf.64.ip  f12, f14, %[_d], 8\n\t"
                    : [_a] "+a"(pa), [_b] "+a"(pb), [_c] "+a"(sa), [_d] "+a"(sb)
                    :
                    : "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11",
                      "f12", "f13", "f14", "f15", "f0", "f1", "f2", "f3",
                      "memory");
            }
        }
    }
}

static void kb_fill(float *x, uint32_t n, uint32_t sd)
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        sd = sd * 1664525u + 1013904223u;
        x[j] = ((float)((sd >> 8) & 0xFFFFu) / 32768.0f - 1.0f);
    }
}
static uint32_t kb_bits_diff(const float *a, const float *b, uint32_t n)
{
    uint32_t j, bad = 0u;
    for (j = 0; j < n; j++) if (memcmp(&a[j], &b[j], 4) != 0) bad++;
    return bad;
}

/* ---------------- LANE 1: the shipped nd_fwht vs the variants -------------- */
#if ND_KB_LANE == 1
#define KNG 6u
static float s_grp[KNG * KBG] __attribute__((aligned(16)));
static float s_ref[KNG * KBG] __attribute__((aligned(16)));

static void kb_run_all(void (*fn)(float *, uint32_t), float *base, uint32_t n)
{
    uint32_t gi;
    for (gi = 0; gi < n; gi++) fn(base + (size_t)gi * KBG, KBG);
    asm volatile("" : "+f"(base[0]));
}

static void bench_e33(void)
{
    printf("KB E33 ENTER lane=%d\n", (int)ND_KB_LANE);
    uint32_t r, rounds = 25u, gi, seed = 777u;
    uint32_t bad[4] = {0,0,0,0}, best[4] = { ~0u, ~0u, ~0u, ~0u };
    double butterflies = (double)(KBG / 2u) * 7u * KNG;   /* per timed round */

    (void)seed;
    kb_fill(s_grp, KNG * KBG, 777u);
    memcpy(s_ref, s_grp, sizeof(s_grp));
    for (gi = 0; gi < KNG; gi++) nd_fwht(s_ref + (size_t)gi * KBG, KBG);  /* SHIPPED */

    kb_fill(s_grp, KNG * KBG, 777u);
    kb_run_all(kb_fwht_u2,     s_grp, KNG); bad[1] = kb_bits_diff(s_ref, s_grp, KNG * KBG);
    kb_fill(s_grp, KNG * KBG, 777u);
#if KB_TIE_ASM
    kb_run_all(kb_fwht_ld2,    s_grp, KNG); bad[2] = kb_bits_diff(s_ref, s_grp, KNG * KBG);
    kb_fill(s_grp, KNG * KBG, 777u);
    kb_run_all(kb_fwht_u2ld2,  s_grp, KNG); bad[3] = kb_bits_diff(s_ref, s_grp, KNG * KBG);
#endif
    printf("KB E33 L1 reference=shipped_nd_fwht groups=%u g=%u diff=%u,%u,%u,%u%s (shipped,u2,ld64,u2+ld64)\n",
           (unsigned)KNG, (unsigned)KBG, (unsigned)bad[0], (unsigned)bad[1],
           (unsigned)bad[2], (unsigned)bad[3], bad[1] + bad[2] + bad[3] ? " NOT_EXACT" : " bitexact=1");

    for (r = 0; r < rounds; r++) {
        uint32_t c0, cy;
        c0 = esp_cpu_get_cycle_count(); kb_run_all(nd_fwht, s_grp, KNG);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;
        c0 = esp_cpu_get_cycle_count(); kb_run_all(kb_fwht_u2, s_grp, KNG);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;
#if KB_TIE_ASM
        c0 = esp_cpu_get_cycle_count(); kb_run_all(kb_fwht_ld2, s_grp, KNG);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[2]) best[2] = cy;
        c0 = esp_cpu_get_cycle_count(); kb_run_all(kb_fwht_u2ld2, s_grp, KNG);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[3]) best[3] = cy;
#endif
    }
    printf("KB E33 L1 RES cycles=%u,%u,%u,%u | cyc_per_butterfly %.3f %.3f %.3f %.3f | u2=%+.2f%% ld64=%+.2f%% u2ld64=%+.2f%%\n",
           (unsigned)best[0], (unsigned)best[1], (unsigned)best[2], (unsigned)best[3],
           (double)best[0] / butterflies, (double)best[1] / butterflies,
           (double)best[2] / butterflies, (double)best[3] / butterflies,
           100.0 * ((double)best[0] / (double)best[1] - 1.0),
           100.0 * ((double)best[0] / (double)(best[2] ? best[2] : 1u) - 1.0),
           100.0 * ((double)best[0] / (double)(best[3] ? best[3] : 1u) - 1.0));
    printf("KB E33 L1 PRICE transform = 64.1%% of a 0.2454 ms call x ~16 calls/token = 2.52 ms/token; best variant %+.2f%% => %.3f ms saved => %+.3f%% decode\n",
           100.0 * ((double)best[0] / (double)(best[3] ? best[3] : best[0]) - 1.0),
           2.52 * (1.0 - (double)(best[3] ? best[3] : best[0]) / (double)best[0]),
           100.0 * 2.52 * (1.0 - (double)(best[3] ? best[3] : best[0]) / (double)best[0]) / 197.0);
}
#endif

/* The engine's per-group rescale, unrolled. Used by lane 2 (isolated) and
 * lane 3 (in the full body), so both test the same code that would ship. */
static void kb_scale_u4(float *restrict blk, uint32_t g, float scale)
{
    uint32_t j = 0u;
    for (; j + 3u < g; j += 4u) {
        float v0 = blk[j], v1 = blk[j+1u], v2 = blk[j+2u], v3 = blk[j+3u];
        blk[j] = v0 * scale; blk[j+1u] = v1 * scale;
        blk[j+2u] = v2 * scale; blk[j+3u] = v3 * scale;
    }
    for (; j < g; j++) blk[j] *= scale;
}

/* ---------------- LANE 2: the per-group rescale ---------------------------- */
#if ND_KB_LANE == 2
static float s_sc[KBG] __attribute__((aligned(16)));
static float s_sr[KBG] __attribute__((aligned(16)));

/* Exactly what fwht_rows does after the transform. */
static void kb_scale_shipped(float *blk, uint32_t g, float scale)
{
    for (uint32_t j = 0; j < g; j++) blk[j] *= scale;
}
static void kb_scale_u8(float *restrict blk, uint32_t g, float scale)
{
    uint32_t j = 0u;
    for (; j + 7u < g; j += 8u) {
        float v[8];
        for (int k = 0; k < 8; k++) v[k] = blk[j + (uint32_t)k];
        for (int k = 0; k < 8; k++) blk[j + (uint32_t)k] = v[k] * scale;
    }
    for (; j < g; j++) blk[j] *= scale;
}

static void bench_e33(void)
{
    printf("KB E33 ENTER lane=%d\n", (int)ND_KB_LANE);
    uint32_t r, rounds = 25u, gi, bad = 0u, best[3] = { ~0u, ~0u, ~0u };
    float scale = 1.0f / sqrtf((float)KBG);          /* the engine's own scale */
    double elems = (double)KBG * 6u;

    for (r = 0; r < 2u; r++) {
        kb_fill(s_sc, KBG, 313u);
        kb_scale_shipped(s_sc, KBG, scale);
        memcpy(s_sr, s_sc, sizeof(s_sc));
        kb_fill(s_sc, KBG, 313u); kb_scale_u4(s_sc, KBG, scale);
        bad += kb_bits_diff(s_sr, s_sc, KBG);
        kb_fill(s_sc, KBG, 313u); kb_scale_u8(s_sc, KBG, scale);
        bad += kb_bits_diff(s_sr, s_sc, KBG);
    }
    printf("KB E33 L2 scale_mismatch=%u%s scale=1/sqrt(g) g=%u groups=6\n",
           (unsigned)bad, bad ? " NOT_EXACT" : " bitexact=1", (unsigned)KBG);

    for (r = 0; r < rounds; r++) {
        uint32_t c0, cy;
        c0 = esp_cpu_get_cycle_count();
        for (gi = 0; gi < 6u; gi++) kb_scale_shipped(s_sc, KBG, scale);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;
        asm volatile("" : "+f"(s_sc[0]));
        c0 = esp_cpu_get_cycle_count();
        for (gi = 0; gi < 6u; gi++) kb_scale_u4(s_sc, KBG, scale);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;
        asm volatile("" : "+f"(s_sc[0]));
        c0 = esp_cpu_get_cycle_count();
        for (gi = 0; gi < 6u; gi++) kb_scale_u8(s_sc, KBG, scale);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[2]) best[2] = cy;
        asm volatile("" : "+f"(s_sc[0]));
    }
    printf("KB E33 L2 RES cycles=%u,%u,%u | cyc_per_element %.3f %.3f %.3f | u4=%+.2f%% u8=%+.2f%% (scale is 14.5%% of prepare = 0.57 ms/token)\n",
           (unsigned)best[0], (unsigned)best[1], (unsigned)best[2],
           (double)best[0] / elems, (double)best[1] / elems, (double)best[2] / elems,
           100.0 * ((double)best[0] / (double)best[1] - 1.0),
           100.0 * ((double)best[0] / (double)best[2] - 1.0));
}
#endif

/* ---------------- LANE 3: the whole prepare, two-core, as shipping runs it - */
#if ND_KB_LANE == 3
#define KPIN 768u
static float s_xin[KPIN] __attribute__((aligned(16)));
static float s_xa[KPIN] __attribute__((aligned(16)));
static float s_xb[KPIN] __attribute__((aligned(16)));

typedef struct { float *xh; uint32_t g; float scale; } kb_fw;

/* The shipped body, verbatim in structure: transform with the shipped nd_fwht,
 * then the shipped scalar rescale. */
static void kb_rows_shipped(void *vc, uint32_t g0, uint32_t g1)
{
    const kb_fw *c = (const kb_fw *)vc;
    for (uint32_t gi = g0; gi < g1; gi++) {
        float *blk = c->xh + (size_t)gi * c->g;
        nd_fwht(blk, c->g);
        for (uint32_t j = 0; j < c->g; j++) blk[j] *= c->scale;
    }
}
/* The candidate body: wide-load butterflies + unrolled rescale. */
static void kb_rows_fast(void *vc, uint32_t g0, uint32_t g1)
{
    const kb_fw *c = (const kb_fw *)vc;
    for (uint32_t gi = g0; gi < g1; gi++) {
        float *blk = c->xh + (size_t)gi * c->g;
        kb_fwht_u2ld2(blk, c->g);
        kb_scale_u4(blk, c->g, c->scale);
    }
}
static void bench_e33(void)
{
    printf("KB E33 ENTER lane=%d\n", (int)ND_KB_LANE);
    nd_tensor t;
    int       ti = -1;
    kb_fw     f;
    uint32_t  r, rounds = 25u, bad, in_pad, ngroup;
    uint32_t  best[2] = { ~0u, ~0u };
    float     scale;

    if (!kb_find_shape(768u, 0u, &t, &ti)) { printf("KB E33 L3 no_tensor\n"); return; }
    in_pad = nd_cq_in_pad(&t);
    if ((uint32_t)sizeof(s_xin) / 4u < in_pad) { printf("KB E33 L3 in_pad_too_big\n"); return; }
    ngroup = in_pad / KBG;
    scale  = 1.0f / sqrtf((float)KBG);
    kb_fill(s_xin, in_pad, 9091u);

    /* Correctness through the real split, against the real prepare. */
    f.xh = s_xa; f.g = KBG; f.scale = scale;
    memcpy(s_xa, s_xin, sizeof(float) * in_pad);
    memcpy(s_xb, s_xin, sizeof(float) * in_pad);
    nd_cq_prepare(&t, s_xin, s_xa);              /* the engine's own function */
    nd_parallel_rows(kb_rows_fast, &f, ngroup);  /* candidate from the same input */
    memcpy(s_xb, s_xin, sizeof(float) * in_pad);
    nd_parallel_rows(kb_rows_shipped, &f, ngroup);
    bad = kb_bits_diff(s_xa, s_xb, in_pad);
    printf("KB E33 L3 tensor=%d in_pad=%u ngroup=%u prepare_vs_fast_mismatch=%u%s\n",
           ti, (unsigned)in_pad, (unsigned)ngroup, (unsigned)bad,
           bad ? " NOT_EXACT" : " bitexact=1");

    for (r = 0; r < rounds; r++) {
        uint32_t c0, cy;
        c0 = esp_cpu_get_cycle_count();
        nd_cq_prepare(&t, s_xin, s_xa);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[0]) best[0] = cy;
        asm volatile("" : "+f"(s_xa[0]));
        memcpy(s_xb, s_xin, sizeof(float) * in_pad);
        c0 = esp_cpu_get_cycle_count();
        nd_parallel_rows(kb_rows_fast, &f, ngroup);
        cy = esp_cpu_get_cycle_count() - c0; if (cy < best[1]) best[1] = cy;
        asm volatile("" : "+f"(s_xb[0]));
    }
    printf("KB E33 L3 RES cycles full_prepare=%u transform_only_fast=%u (not like-for-like: full includes copy+pad+split) | transform share of prepare is 64.1%%, so the candidate saves about %.3f ms/token = %+.3f%% decode\n",
           (unsigned)best[0], (unsigned)best[1],
           2.52 * (1.0 - (double)best[1] / (double)best[0]),
           100.0 * 2.52 * (1.0 - (double)best[1] / (double)best[0]) / 197.0);
}
#endif
/* ====================== end Experiment 33 =============================== */
#endif

#if ND_KB_EXP36
/* Experiment 36 - what does one IRAM-to-flash callee crossing actually cost?
 *
 * This is the denominator for the last unpriced candidate class. Run #336 moved
 * `sigmoidf_pair` from flash into IRAM and gained +0.18 % for roughly 3,600
 * crossings, which implies ~30 cycles per crossing but was never measured as such.
 * The one remaining mass that pays a flash callee per call is Sinkhorn's `logf`:
 * about 1,280 calls per decode token (8 per iteration x 20 x 8 layers), the last
 * transcendental in the model that libm owns. Whether a locally-resident copy is
 * worth writing depends ONLY on the crossing cost, and that number is measurable
 * without writing the copy: two identical-shape noinline FP chains, one left where
 * the linker puts it (flash) and one forced into IRAM, called back to back.
 *
 * Two honesty guards, both bought by past failures:
 *  * identical function bodies are a garbage-collect/ICF folding risk, so the chains
 *    use different constants and the bench PRINTS BOTH SYMBOL ADDRESSES - if they are
 *    not in different memory regions the whole measurement is fiction (#359's rule:
 *    prove the candidate is in the image before believing a number from it);
 *  * the first call is reported separately from the steady-state calls, because the
 *    field's failure mode is the cold instruction fetch, not the call instruction,
 *    and a warm loop over one tiny function measures the cache rather than the bus
 *    (run #374's correction that a tight bench loop inflates instruction mix).
 */
__attribute__((noinline)) static float kb_cross_flash(float x)
{
    float a = x * 1.00097656f;      /* 1 + 2^-10 */
    a = fmaf(a, 0.99951172f, 0.00390625f);
    a = fmaf(a, 1.00048828f, -0.00195312f);
    a = fmaf(a, 0.99975586f, 0.00097656f);
    a = fmaf(a, 1.00024414f, -0.00048828f);
    a = fmaf(a, 0.99987793f, 0.00024414f);
    a = fmaf(a, 1.00012207f, -0.00012207f);
    a = fmaf(a, 0.99993896f, 0.00006103f);
    a = fmaf(a, 1.00006104f, -0.00003052f);
    return a;
}

__attribute__((noinline)) IRAM_ATTR static float kb_cross_iram(float x)
{
    float a = x * 1.00146484f;      /* 1 + 1.5*2^-10, deliberately different */
    a = fmaf(a, 0.99902344f, 0.00195312f);
    a = fmaf(a, 1.00073242f, -0.00097656f);
    a = fmaf(a, 0.99963379f, 0.00048828f);
    a = fmaf(a, 1.00036621f, -0.00024414f);
    a = fmaf(a, 0.99981689f, 0.00012207f);
    a = fmaf(a, 1.00018311f, -0.00006103f);
    a = fmaf(a, 0.99990845f, 0.00003052f);
    a = fmaf(a, 1.00009155f, -0.00001526f);
    return a;
}

__attribute__((noinline)) IRAM_ATTR static float kb_cross_sink(float x) { return x; }

static void bench_e36(void)
{
    enum { ROUNDS = 21u, CALLS = 9u };
    float  in[CALLS];
    uint32_t i, r, k;
    uint32_t bestf[CALLS], besti[CALLS], bestn[CALLS];
    float sink = 0.0f;

    printf("KB E36 ENTER lane=crossing calls=%u rounds=%u\n", (unsigned)CALLS, (unsigned)ROUNDS);
    printf("KB E36 addr flash=0x%08x iram=0x%08x nop=0x%08x  (flash is 0x42xxxxxx, IRAM 0x403xxxxx)\n",
           (unsigned)(uint32_t)(void *)&kb_cross_flash,
           (unsigned)(uint32_t)(void *)&kb_cross_iram,
           (unsigned)(uint32_t)(void *)&kb_cross_sink);
    fflush(stdout);
    if (((uint32_t)(uint32_t)(void *)&kb_cross_flash & 0xff000000u) ==
        ((uint32_t)(uint32_t)(void *)&kb_cross_iram  & 0xff000000u)) {
        printf("KB E36 FAIL both_bodies_in_same_region (ICF folded them; number would be fiction)\n");
        return;
    }
    for (k = 0; k < CALLS; k++) { bestf[k] = 0xFFFFFFFFu; besti[k] = 0xFFFFFFFFu; bestn[k] = 0xFFFFFFFFu; }

    for (r = 0; r < ROUNDS; r++) {
        for (k = 0; k < CALLS; k++) in[k] = (float)(k + 1) * 0.03125f + (float)r * 1e-5f;
        for (int mode = 0; mode < 3; mode++) {
            uint32_t *best = mode == 0 ? bestf : (mode == 1 ? besti : bestn);
            for (k = 0; k < CALLS; k++) {
                uint64_t t0 = esp_cpu_get_cycle_count();
                float v = (mode == 0) ? kb_cross_flash(in[k])
                        : (mode == 1) ? kb_cross_iram(in[k])
                                      : kb_cross_sink(in[k]);
                uint64_t t1 = esp_cpu_get_cycle_count();
                sink += v;
                if ((uint32_t)(t1 - t0) < best[k]) best[k] = (uint32_t)(t1 - t0);
            }
        }
    }
    {
        uint32_t wf = 0, wi = 0, wn = 0;
        for (k = 1; k < CALLS; k++) { wf += bestf[k]; wi += besti[k]; wn += bestn[k]; }
        wf /= (CALLS - 1u); wi /= (CALLS - 1u); wn /= (CALLS - 1u);
        printf("KB E36 first_call flash=%u iram=%u nop=%u | steady flash=%u iram=%u nop=%u"
               " | crossing_penalty=%u cycles | body=%u cycles"
               " | predicted_pct_for_1280_logf_crossings=.%03u sink=%g\n",
               (unsigned)bestf[0], (unsigned)besti[0], (unsigned)bestn[0],
               (unsigned)wf, (unsigned)wi, (unsigned)wn,
               (unsigned)(wf > wi ? wf - wi : 0u), (unsigned)(wf > wn ? wf - wn : 0u),
               (unsigned)((wf > wi ? (wf - wi) * 1280u * 1000u / 240000u / 197u : 0u)),
               (double)sink);
        fflush(stdout);
    }
}
#endif /* ND_KB_EXP36 */

#if ND_KB_EXP37
/* Experiment 37 - the KV store's flash-mapped rounding call, priced per element.
 *
 * Run #368 closed this loop because the part "implements no rounding instruction
 * at all" - true, and re-confirmed by assembler probe - but it never checked the
 * CONVERSION instructions. Compiling `(int)x` for this target and disassembling
 * shows GCC emits `trunc.s a2, f0, 0`, a hardware float->int truncate, while
 * lrintf and even __builtin_floorf/__builtin_rintf turn into calls. So the
 * rounding can be done in FLOAT by adding MAGIC = 1.5*2^23, whose ulp at that
 * magnitude is exactly 1.0: RN(MAGIC + q) == MAGIC + rint(q), ties resolved to
 * even by the adder, and MAGIC is removed again in INTEGER arithmetic so the
 * compiler cannot fold it back. That is not the forbidden +0.5f-and-truncate
 * (round-half-away, rejected at run #2e): .auto/rint/test_rint.c diffs it against
 * lrintf over 242,863 values including every int8 tie and the non-finite classes,
 * with zero mismatches.
 *
 * What this bench decides is the SIZE, which an end-to-end run cannot separate
 * from the rest of the phase: the shipped loop is ~1.1 ms for ~1,792 elements =
 * ~147 cycles/element for what should be a clamp and a store, and the only
 * plausible reason is a flash-mapped newlib call per element issued from an
 * IRAM-resident function - the callee-crossing class worth +0.18 % at run #336, at
 * four times the call rate and with a much bigger callee.
 *
 * Both variants run with the FIELD's call structure - one call per 48-element row
 * (qk_head_dim), result consumed before the next row - because run #374 measured
 * that a tight re-swept loop inflates instruction mix rather than delivery.
 */
__attribute__((noinline)) static void kb_kv_ref(int8_t *dst, const float *src,
                                                uint32_t n, float scale)
{
    const float r = 1.0f / scale;
    uint32_t i;
    for (i = 0; i < n; i++) {
        float x = src[i], q, e;
        if (x > -1e30f && x < 1e30f) { q = x * r; e = fmaf(-scale, q, x); q = fmaf(e, r, q); }
        else q = x / scale;
        if (q > 127.0f)  q = 127.0f;
        if (q < -127.0f) q = -127.0f;
        dst[i] = (int8_t)lrintf(q);
    }
}

__attribute__((noinline)) static void kb_kv_magic(int8_t *dst, const float *src,
                                                  uint32_t n, float scale)
{
    const float r = 1.0f / scale;
    uint32_t i;
    for (i = 0; i < n; i++) {
        float x = src[i], q, e;
        if (x > -1e30f && x < 1e30f) { q = x * r; e = fmaf(-scale, q, x); q = fmaf(e, r, q); }
        else q = x / scale;
        if (q > 127.0f)  q = 127.0f;
        if (q < -127.0f) q = -127.0f;
        if (q >= -127.0f && q <= 127.0f)
            dst[i] = (int8_t)((int)(q + 12582912.0f) - 12582912);
        else
            dst[i] = (int8_t)lrintf(q);
    }
}

static void bench_e37(void)
{
    enum { ROW = 48u, ROWS = 32u, ROUNDS = 15u };
    float  *src  = heap_caps_malloc(sizeof(float) * ROW * ROWS, MALLOC_CAP_INTERNAL);
    int8_t *a    = heap_caps_malloc(ROW * ROWS, MALLOC_CAP_INTERNAL);
    int8_t *b    = heap_caps_malloc(ROW * ROWS, MALLOC_CAP_INTERNAL);
    uint32_t r, row, i, best[2] = {0xFFFFFFFFu, 0xFFFFFFFFu}, mism = 0;
    uint64_t t0, t1;
    int32_t  sink = 0;

    if (!src || !a || !b) { printf("KB E37 FAIL alloc_failed\n"); free(src); free(a); free(b); return; }
    printf("KB E37 ENTER lane=kvround row=%u rows=%u rounds=%u\n",
           (unsigned)ROW, (unsigned)ROWS, (unsigned)ROUNDS);

    /* Operand classes, in two passes over the same two loops.
     *
     * Class 0 is what I originally wrote: integers and .5 ties. It is wrong as a
     * stand-in for the field, and that turned out to be the whole story of this
     * bench - a software rounding routine can shortcut exactly these inputs, so the
     * reference loop measured 7.04 cycles/element while the same change was worth
     * ~18x more end to end.
     * Class 1 is the field's shape: values in the int8 range with GENERIC mantissas
     * (an LCG fraction), which is what a Markstein-corrected quotient actually looks
     * like, and which crosses the .5 boundaries irregularly instead of landing on
     * them. If the reference loop gets much slower here while the magic loop does
     * not, the isolation/field contradiction is explained by operands and not by
     * anything about the memory system.
     * The magic loop's result must stay byte-identical for BOTH classes, which is
     * also the point .auto/rint/test_rint.c makes on the host. */
    for (int cls = 0; cls < 2; cls++) {
        uint32_t st = 12345u;
        mism = 0;              /* per-class exactness, not a running total */
        for (i = 0; i < ROW * ROWS; i++) {
            if (cls == 0) {
                src[i] = (float)((int)(i % 257u) - 128) + 0.5f * (float)((i % 3u) - 1);
            } else {
                st = st * 1664525u + 1013904223u;
                src[i] = (float)((int)(i % 257u) - 128)
                       + (float)((st >> 8) & 0xFFFFFFu) * (1.0f / 16777216.0f);
            }
        }

    for (int mode = 0; mode < 2; mode++) {
        int8_t *dst = mode ? b : a;
        best[mode] = 0xFFFFFFFFu;
        for (r = 0; r < ROUNDS; r++) {
            t0 = esp_cpu_get_cycle_count();
            for (row = 0; row < ROWS; row++)
                (mode ? kb_kv_magic : kb_kv_ref)(dst + row * ROW, src + (size_t)row * ROW,
                                                 ROW, 1.0732422f);
            t1 = esp_cpu_get_cycle_count();
            if ((uint32_t)(t1 - t0) < best[mode]) best[mode] = (uint32_t)(t1 - t0);
            for (i = 0; i < ROW * ROWS; i++) sink += dst[i];   /* consume everything */
        }
    }
    for (i = 0; i < ROW * ROWS; i++) if (a[i] != b[i]) mism++;
    printf("KB E37 class=%d %s rows=%u cycles_ref=%u cycles_magic=%u gain_pct=%u.%02u"
           " byte_mismatch=%u cyc_per_elem_ref=%u.%02u cyc_per_elem_magic=%u.%02u"
           " saving_ms_per_token_at_1792=%u.%03u sink=%d\n",
           /* uint32_t is `long unsigned int` for this target, so a bare uint32_t
            * against %u is a -Werror=format error rather than a warning. */
           cls, cls == 0 ? "ties" : "generic",
           (unsigned)(ROWS * ROUNDS), (unsigned)best[0], (unsigned)best[1],
           (unsigned)((best[0] * 100u) / best[1] - 100u),
           (unsigned)(((best[0] * 10000u) / best[1]) % 100u), (unsigned)mism,
           (unsigned)(best[0] / (ROWS * ROW * ROUNDS)),
           (unsigned)((best[0] * 100u / (ROWS * ROW * ROUNDS)) % 100u),
           (unsigned)(best[1] / (ROWS * ROW * ROUNDS)),
           (unsigned)((best[1] * 100u / (ROWS * ROW * ROUNDS)) % 100u),
           (unsigned)((best[0] - best[1]) * 1792u / 240000u),
           (unsigned)((((best[0] - best[1]) * 1792u * 1000u) / 240000u) % 1000u),
           (int)sink);
    fflush(stdout);
    }
    free(src); free(a); free(b);
}
#endif /* ND_KB_EXP37 */

#if ND_KB_EXP38
/* Experiment 38 - how much does a warm screen overstate, and was run #364's
 * diagnosis even right?
 *
 * Run #364 is the campaign's starkest method failure: the paired-butterfly
 * transform screened +36.75 % bit-exact against the shipped nd_fwht and delivered
 * exactly +0.000 % on two boards. The recorded diagnosis was cache warmth - the
 * bench re-swept one block 25 times, so the transform ran in the data cache,
 * while the field "calls nd_fwht on a freshly prepared activation". That
 * diagnosis was never numbered, and it is worth checking, because the field
 * operand is not PSRAM: fwht_rows transforms c->xh, which is ND_ALLOC_FAST (i.e.
 * internal SRAM, which this part does not even route through the data cache). A
 * PSRAM eviction sweep therefore cannot make the field's operand cold, which
 * means "data warmth" may not be the mechanism at all.
 *
 * So run the shipped nd_fwht on the field shape - nd_fwht(blk, 128) six times is
 * one nd_cq_prepare - and change only (a) which backing store the block lives in
 * and (b) whether a 160 KiB PSRAM sweep runs between timed calls. Same function,
 * same bytes, same call; refills happen outside the timed window because the
 * transform is in place and applying it twice scales the data by n.
 *
 * Read it as a 2x2. int_cold vs int_warm is the pair the field can actually
 * reach: if a sweep does not slow an internal-SRAM operand down, then #364's null
 * was NOT data warmth and every warm screen must be discounted for a different
 * reason. psram_cold vs psram_warm numbers the delivery tax on a transform over
 * cached PSRAM, which is what a staged-operand design would pay.
 */
static void kb_e38_fill(const float *src, float *dst, uint32_t n)
{
    memcpy(dst, src, sizeof(float) * n);
}

static void bench_e38(void)
{
    enum { G = 128u, GROUPS = 6u, ROUNDS = 25u, EVICT = 163840u };
    const uint32_t n = G * GROUPS;
    float *src   = heap_caps_malloc(sizeof(float) * n, MALLOC_CAP_SPIRAM);
    float *pbs   = heap_caps_malloc(sizeof(float) * n, MALLOC_CAP_SPIRAM);
    float *pint  = heap_caps_malloc(sizeof(float) * n, MALLOC_CAP_INTERNAL);
    uint8_t *evi = heap_caps_malloc(EVICT, MALLOC_CAP_SPIRAM);
    float *ref   = heap_caps_malloc(sizeof(float) * n, MALLOC_CAP_SPIRAM);
    uint32_t r, i, best[4], mism[4] = {0, 0, 0, 0};
    uint64_t t0, t1, acc;
    float sink = 0.0f, evict_acc = 0.0f;

    if (!src || !pbs || !pint || !evi || !ref) {
        printf("KB E38 FAIL alloc_failed\n");
        free(src); free(pbs); free(pint); free(evi); free(ref);
        return;
    }
    printf("KB E38 ENTER lane=coldwarm n=%u groups=%u rounds=%u evict_bytes=%u\n",
           (unsigned)n, (unsigned)GROUPS, (unsigned)ROUNDS, (unsigned)EVICT);

    /* A transform input with real structure: the field's operand is an FWHT of a
     * prepared activation, not zeros, and zeros would let a speculative core
     * short-circuit nothing but is at least the same values everywhere. */
    for (i = 0; i < n; i++) src[i] = (float)((int)(i % 251u) - 125) * 0.03125f;

    for (int mode = 0; mode < 4; mode++) {
        const int psram = (mode < 2);
        const int cold  = (mode & 1);
        float *buf = psram ? pbs : pint;
        best[mode] = 0xFFFFFFFFu;
        for (r = 0; r < ROUNDS; r++) {
            kb_e38_fill(src, buf, n);                    /* untimed */
            if (cold) {                                  /* untimed eviction */
                const volatile uint8_t *e = evi;
                for (i = 0; i < EVICT; i += 64u) evict_acc += (float)e[i];
            }
            t0 = esp_cpu_get_cycle_count();
            for (i = 0; i < GROUPS; i++) nd_fwht(buf + (size_t)i * G, G);
            t1 = esp_cpu_get_cycle_count();
            acc = (uint64_t)(t1 - t0);
            if (acc < best[mode]) best[mode] = (uint32_t)acc;
            /* consume every result so nothing above is dead code */
            for (i = 0; i < n; i++) sink += buf[i];
            if (r == 0u) memcpy(ref, buf, sizeof(float) * n);
        }
        /* exactness across backing stores: the same bytes in must give the same
         * bytes out, or the comparison is between two computations. */
        kb_e38_fill(src, buf, n);
        for (i = 0; i < GROUPS; i++) nd_fwht(buf + (size_t)i * G, G);
        for (i = 0; i < n; i++) if (buf[i] != ref[i]) mism[mode]++;
        printf("KB E38 mode=%s buf=%s cycles_per_prepare=%u cycles_per_call=%u"
               " mismatch_vs_ref=%u\n",
               cold ? "cold" : "warm", psram ? "psram" : "internal",
               (unsigned)best[mode], (unsigned)(best[mode] / GROUPS), (unsigned)mism[mode]);
        fflush(stdout);
    }
    printf("KB E38 evict_penalty_psram=.%2u%% evict_penalty_internal=.%2u%%"
           " sink=%g evict_acc=%g\n",
           (unsigned)((best[1] * 100u) / best[0] - 100u),
           (unsigned)((best[3] * 100u) / best[2] - 100u),
           (double)sink, (double)evict_acc);
    free(src); free(pbs); free(pint); free(evi); free(ref);
}
#endif /* ND_KB_EXP38 */

int kbench_run(void)
{
    const esp_partition_t      *part;
    esp_partition_mmap_handle_t handle;
    const void               *mapped;
    size_t                    bytes;
    size_t                    i;
    int64_t                   t0;
    uint32_t                  c0;

    printf("KB CFG kbench=1 asm=%d rounds=%u\n", ND_KBENCH_ASM, (unsigned)KB_ROUNDS);

    s_nkern = 0;
    s_kern[s_nkern].tag = "c";
    s_kern[s_nkern].fn  = nd_lut2_rows_c;
    s_nkern++;
#if ND_KBENCH_ASM
    s_kern[s_nkern].tag = "tie1";
    s_kern[s_nkern].fn  = nd_lut2_rows_tie1;
    s_nkern++;
    s_kern[s_nkern].tag = "tie2";   /* row blocking: measured worse, kept for the record */
    s_kern[s_nkern].fn  = nd_lut2_rows_tie2;
    s_nkern++;
    s_kern[s_nkern].tag = "tie1p";
    s_kern[s_nkern].fn  = nd_lut2_rows_tie1p;
    s_nkern++;
    s_kern[s_nkern].tag = "tie1n";
    s_kern[s_nkern].fn  = nd_lut2_rows_tie1n;
    s_nkern++;
    s_kern[s_nkern].tag = "tie1m";
    s_kern[s_nkern].fn  = nd_lut2_rows_tie1m;
    s_nkern++;
#endif

    /* CCOUNT has to be real before any of this means anything: bracket 50 ms of
     * busy wait with the timer and the cycle counter and report the ratio. */
    bench_exp_pair();
    bench_dot();

    t0 = esp_timer_get_time();
    c0 = esp_cpu_get_cycle_count();
    while (esp_timer_get_time() - t0 < 50000)
        ;
    s_mhz_x100 = (uint32_t)((uint32_t)(esp_cpu_get_cycle_count() - c0) * 100u / 50000u);
    printf("KB CFG cycles_per_us_x100=%u (expect ~24000)\n", (unsigned)s_mhz_x100);

    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "model");
    if (!part) {
        printf("KB FAIL reason=no_model_partition\n");
        return -1;
    }
    bytes = blob_length(part);
    if (!bytes) {
        printf("KB FAIL reason=model_header\n");
        return -1;
    }
    if (esp_partition_mmap(part, 0, bytes, ESP_PARTITION_MMAP_DATA,
                           &mapped, &handle) != ESP_OK) {
        printf("KB FAIL reason=mmap size=%u\n", (unsigned)bytes);
        return -1;
    }
    s_base = (const uint8_t *)mapped;
    if (nd_cact_open(&s_c, s_base, bytes) != 0) {
        printf("KB FAIL reason=cact_open\n");
        return -1;
    }
    printf("KB CFG blob=%u tensors=%u d_model=%u layers=%u\n",
           (unsigned)bytes, (unsigned)s_c.n, (unsigned)s_c.h.d_model,
           (unsigned)s_c.h.num_layers);
    printf("KB MEM internal_free=%u psram_free=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    fflush(stdout);

    s_go = xSemaphoreCreateBinary();
    s_done = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(worker_task, "kb_worker", 4096, NULL,
                            configMAX_PRIORITIES - 2, NULL, 1);

#if ND_KBENCH_ASM
    probes();          /* the ISA probes live in the assembly translation unit */
#endif
    bench_div();
#if ND_KB_EXP38
    bench_log();
#endif
    bench_gather();
    bench_wake();
    bench_rowrange();
    bench_e49();
#if ND_KB_EXP36
    bench_e36();
#elif ND_KB_EXP37
    bench_e37();
#elif ND_KB_EXP38
    bench_e38();
#elif ND_KB_EXP33
    bench_e33();
#elif ND_KB_EXP32
    bench_e32();
#elif ND_KB_EXP31
    bench_e31();
#elif ND_KB_EXP30
    bench_e30();
#else
    bench_fused();
#endif

    for (i = 0; i < KB_NSHAPES; i++)
        bench_shape(&SHAPES[i]);

    /* Experiment 15: the memory-system test, at the two buffer sizes the
     * experiment names. Runs last: it installs and uninstalls a DMA driver. */
#if ND_KBENCH_GDMA
    bench_gdma(2048);
    bench_gdma(4096);
#endif

    printf("KB MEM internal_free=%u psram_free=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("EVT KBENCH_DONE\n");
    fflush(stdout);
    return 0;
}
