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
 *   EVT KBENCH_DONE
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

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

typedef enum { KB_ISO = 0, KB_SPLIT, KB_FASTBLOB, KB_NMODES } kb_mode;
static const char MODE_NAME[KB_NMODES][14] = { "isolated", "split", "blob_int" };

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
    float      *xh, *lut, *yref, *ytmp;
    nd_lut2_ctx cx, cxi;
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

    {                                   /* a prepared activation, no denormals */
        uint32_t s = 0x1234abcdu;
        uint32_t j;

        for (j = 0; j < in_pad; j++) {
            s = s * 1664525u + 1013904223u;
            xh[j] = ((float)((s >> 8) & 0xFFFFu) / 32768.0f - 1.0f) * 0.25f;
        }
    }
    nd_cq_lut_build(&s_c, xh, in_pad, lut);

    nd_lut2_fill(&cx, &t, blob_p, lut, yref);
    nd_lut2_fill(&cxi, &t, fast_ok ? blob_i : blob_p, lut, yref);

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
        nd_lut2_ctx *use = (m == KB_FASTBLOB) ? &cxi : &cx;
        float        refc = 0.0f;

        if (m == KB_FASTBLOB && !fast_ok)
            continue;
        for (k = 0; k < (uint32_t)s_nkern; k++)
            for (i = 0; i < KB_WARMUP; i++)
                time_one(s_kern[k].fn, use, t.shape[0], (kb_mode)m);
        for (i = 0; i < KB_ROUNDS; i++) {
            for (k = 0; k < (uint32_t)s_nkern; k++)
                res[m][k][i] = time_one(s_kern[k].fn, use, t.shape[0], (kb_mode)m);
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
    for (i = 0; i < KB_NSHAPES; i++)
        bench_shape(&SHAPES[i]);

    printf("KB MEM internal_free=%u psram_free=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("EVT KBENCH_DONE\n");
    fflush(stdout);
    return 0;
}
