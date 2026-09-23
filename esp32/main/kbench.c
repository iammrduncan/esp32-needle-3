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
    bench_gather();
    bench_wake();
    bench_rowrange();

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
