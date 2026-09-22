/* kbench.c - Experiment 22 SCREEN: does a handwritten 4-bit phi row kernel that
 * uses the ESP32-S3 TIE paired float loads (ee.ldf.64/128.ip) on the contiguous
 * prepared activation beat the shipped C loop?
 *
 * This is a dev-only image (NEEDLE_KBENCH=ON); the shipping component never
 * compiles it. It lives in .auto/exp22/kbench_phi22.c and is copied over
 * esp32/main/kbench.c inside one board's checkout to run.
 *
 * Why the screen exists before the integration: run #141 predicted -12 % of the
 * phi phase from instruction counts (41 -> 36 per index word), run #295 measured
 * the *delivery* ceiling of the same phase at 9.3-10 %, and run #230 measured a
 * hand-written dot that used ee.ldf.128.ip at -107 %. The three cannot all be
 * right, and the end-to-end prize (+0.2-0.45 %) is small enough that it must be
 * decided in an isolated microbenchmark, not with a 13-minute flash.
 *
 * Discipline (runs #144, #230, #287): CCOUNT calibrated against esp_timer; the
 * shipping kernel is the control; every result consumed through an FP register
 * barrier so nothing is hoisted; and the asm output compared BITWISE against the
 * shipping kernel's output before any cycle count is read.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_cpu.h"
#include "esp_partition.h"

#include "nd_cact.h"
#include "nd_quant.h"
#include "nd_model.h"

extern float nd_cq_row4_tie0(const uint8_t *row, const float *cb, const float *xh,
                             const uint16_t *nrm, unsigned ngroup, unsigned g);
extern float nd_cq_row4_tie1(const uint8_t *row, const float *cb, const float *xh,
                             const uint16_t *nrm, unsigned ngroup, unsigned g);
extern float nd_cq_row4_tie2(const uint8_t *row, const float *cb, const float *xh,
                             const uint16_t *nrm, unsigned ngroup, unsigned g);

#define KB_ROUNDS 25
#define KB_NMODE  4

static const char *MODE[KB_NMODE] = { "c_ship", "tie0_lsi", "tie1_ldf128",
                                             "tie2_ldf64" };

/* One row through variant m. tie0 runs FIRST so a fault in a TIE-load variant
 * still leaves the plain-asm-vs-C measurement complete. */
static float run_asm(int m, const uint8_t *row, const float *cb, const float *xh,
                     const uint16_t *nrm, unsigned ngroup, unsigned g)
{
    if (m == 1) return nd_cq_row4_tie0(row, cb, xh, nrm, ngroup, g);
    if (m == 2) return nd_cq_row4_tie1(row, cb, xh, nrm, ngroup, g);
    return nd_cq_row4_tie2(row, cb, xh, nrm, ngroup, g);
}

static uint32_t s_mhz_x100;

/* Copied verbatim from esp32/main/main.c (both static there) so the screen opens
 * exactly the archive the shipping build opens. */
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static size_t model_length(const esp_partition_t *part)
{
    uint8_t  hdr[12], last[44];
    uint32_t count, codebooks, record_at;

    if (esp_partition_read(part, 0, hdr, sizeof(hdr)) != ESP_OK ||
        le32(hdr) != ND_CACT_TAG)
        return 0;
    count     = le32(hdr + 4);
    codebooks = le32(hdr + 8);
    if (count == 0 || count > 4096 || codebooks > 256)
        return 0;
    record_at = 196 + codebooks * 4 + (count - 1) * 44;
    if (record_at + sizeof(last) > part->size ||
        esp_partition_read(part, record_at, last, sizeof(last)) != ESP_OK)
        return 0;
    if (le32(last + 24) || le32(last + 32))
        return 0;
    {
        size_t end = (size_t)le32(last + 20) + le32(last + 28);
        return end <= part->size ? end : 0;
    }
}

static int cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static void fp_sink(float v)
{
    asm volatile("" : "+f"(v));
}

static void *alloc_in(size_t bytes)
{
    return heap_caps_aligned_alloc(16, (bytes + 15u) & ~(size_t)15u,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static void *alloc_ps(size_t bytes)
{
    return heap_caps_malloc((bytes + 31u) & ~(size_t)31u,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

int kbench_run(void)
{
    const esp_partition_t *part;
    const void            *mapped = NULL;
    nd_cact                s_c;
    nd_tensor              t;
    uint32_t               out, in, g, ngroup, rowbytes, nrows_slice, nlayers;
    uint8_t               *blob_ps = NULL;
    float                 *xh = NULL, *xsrc = NULL, *yref = NULL, *yas = NULL;
    const float           *cb;
    const uint16_t        *norms;
    int                    tidx[4], nt = 0, i, m, r, k;
    uint32_t               mism = 0, slow_norm = 0, misalign = 0, rows_checked = 0;
    uint32_t               dbg_mism[KB_NMODE] = { 0 };
    float                  dbg_maxabs[KB_NMODE] = { 0 }, dbg_ref[KB_NMODE] = { 0 },
                           dbg_asm[KB_NMODE] = { 0 };
    uint32_t               cycles[4][KB_NMODE][KB_ROUNDS];
    esp_partition_mmap_handle_t handle;
    float                  s = 0.0f;

    printf("KB CFG kbench=1 exp22_phi_screen rounds=%u\n", (unsigned)KB_ROUNDS);

    /* UNIT TEST FIRST, on inputs whose answer is arithmetic rather than model
     * output, because all three asm variants returned one identical value that
     * did not depend on their inputs: a known-answer probe localises a wrong
     * kernel in one build, where diffing model rows only says "not equal".
     *   cb[i]   = i            -> level k contributes k
     *   row     = 0x10 bytes   -> nibbles alternate 0,1: per index word four 1s
     *   xh[*]   = 1.0f
     *   norm    = 1.0f (fp16 0x3C00), one group of 128 weights = 16 words
     *   expected dot  = 16 words x 4 x (1*1.0f) = 64.0f, acc = 1.0f * 64.0f
     * Half of that also fails if the nibble->partial map is wrong, because the
     * four partials would then hold different values than C's. */
    {
        static float ucb[16], uxh[128], uy[4];
        static uint8_t urow[64];
        static uint16_t unrm[4];
        int q;
        for (q = 0; q < 16; q++) ucb[q] = (float)q;
        for (q = 0; q < 128; q++) uxh[q] = 1.0f;
        for (q = 0; q < 64; q++) urow[q] = 0x10;
        unrm[0] = 0x3C00u;                     /* fp16 1.0f */
        for (m = 1; m < KB_NMODE; m++) {
            float got = run_asm(m, urow, ucb, uxh, unrm, 1u, 128u);
            printf("KB P22 unit mode=%-12s expected=64.000000 got=%.6f\n",
                   MODE[m], (double)got);
        }
        (void)uy;
    }
    {
        int64_t  t0 = esp_timer_get_time();
        uint32_t c0 = esp_cpu_get_cycle_count();
        while (esp_timer_get_time() - t0 < 50000)
            ;
        s_mhz_x100 = (uint32_t)((uint32_t)(esp_cpu_get_cycle_count() - c0) * 100u /
                               50000u);
    }
    printf("KB CFG cycles_per_us_x100=%u (expect ~24000)\n", (unsigned)s_mhz_x100);

    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "model");
    if (!part) { printf("KB FAIL reason=no_model_partition\n"); return -1; }
    {
        size_t bytes = model_length(part);
        if (!bytes) { printf("KB FAIL reason=model_header\n"); return -1; }
        if (esp_partition_mmap(part, 0, bytes, ESP_PARTITION_MMAP_DATA,
                               &mapped, &handle) != ESP_OK || !mapped) {
            printf("KB FAIL reason=mmap bytes=%u\n", (unsigned)bytes);
            return -1;
        }
        if (nd_cact_open(&s_c, (const uint8_t *)mapped, bytes) != 0) {
            printf("KB FAIL reason=cact_open\n");
            return -1;
        }
    }
    nlayers = s_c.h.num_layers;
    printf("KB CFG tensors=%u layers=%u lanes=%u d_model=%u codebook=%u\n",
           (unsigned)s_c.n, (unsigned)nlayers, (unsigned)s_c.h.mhc_lanes,
           (unsigned)s_c.h.d_model, (unsigned)s_c.h.codebook_len);

    /* The three mHC phi tensors: 4-bit, reducing over lanes * d_model, with a
     * row count that splits evenly across layers (the model's own slicing). */
    for (i = 0; i < (int)s_c.n && nt < 3; i++) {
        nd_tensor cand;
        if (nd_cact_tensor(&s_c, i, &cand) != 0) continue;
        if (cand.bits != 4) continue;
        if (cand.shape[1] != s_c.h.mhc_lanes * s_c.h.d_model) continue;
        if (cand.shape[0] % nlayers) continue;
        tidx[nt++] = i;
    }
    if (!nt) { printf("KB FAIL reason=no_phi_tensors\n"); return -1; }
    memset(cycles, 0, sizeof(cycles));

    for (i = 0; i < nt; i++) {
        if (nd_cact_tensor(&s_c, tidx[i], &t) != 0) continue;
        out        = t.shape[0];
        in         = t.shape[1];
        g          = t.group;
        ngroup     = nd_cq_groups(&t);
        rowbytes   = nd_cq_row_bytes(&t);
        nrows_slice = out / nlayers;
        cb         = nd_cact_codebook(&s_c, t.bits);
        blob_ps    = alloc_ps((size_t)out * rowbytes +
                              (size_t)ngroup * out * sizeof(uint16_t));
        xh         = alloc_in(nd_cq_scratch(&t) * sizeof(float));
        xsrc       = alloc_in((size_t)in * sizeof(float));
        yref       = alloc_in((size_t)out * sizeof(float));
        yas        = alloc_in((size_t)out * sizeof(float));
        if (!blob_ps || !xh || !xsrc || !yref || !yas) {
            printf("KB FAIL reason=alloc tensor=%d blob=%u internal_free=%u\n",
                   tidx[i], (unsigned)(out * rowbytes),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            return -1;
        }
        memcpy(blob_ps, nd_cact_data(&s_c, &t),
               (size_t)out * rowbytes + (size_t)ngroup * out * sizeof(uint16_t));
        for (k = 0; k < (int)in; k++)
            xsrc[k] = (float)((k * 37) % 511) * 0.0019f - 0.48f;
        nd_cq_prepare(&t, xsrc, xh);
        norms = (const uint16_t *)(blob_ps + (size_t)out * rowbytes);

        /* Preconditions of the asm, checked not assumed. */
        if ((((uintptr_t)xh) & 15u) || (g != 128))
            misalign |= 1u;
        {
            uint32_t j, total = ngroup * out;
            for (j = 0; j < total; j++) {
                uint32_t e = (norms[j] >> 10) & 0x1fu;
                if (e == 0u || e == 31u) { slow_norm++; }
            }
        }

        /* Reference rows from the SHIPPING kernel, layer-0 slice. */
        nd_cq_gemv_rows(&s_c, &t, blob_ps, xh, 0, nrows_slice, yref);

        /* Bit-exactness first, for all three asm variants. On a mismatch print
         * the FIRST pair of values, not just a count: whether the answer is off
         * by 1 ULP (fma vs mul-then-add) or by 30 % (wrong operand) decides what
         * to fix. */
        for (m = 1; m < KB_NMODE; m++) {
            uint32_t rr;
            float    maxabs = 0.0f, first_ref = 0.0f, first_asm = 0.0f;
            int      printed = 0;
            dbg_maxabs[m] = 0.0f;
            memset(yas, 0xA5, (size_t)out * sizeof(float));
            for (rr = 0; rr < nrows_slice; rr++) {
                float v = run_asm(m, blob_ps + (size_t)rr * rowbytes, cb, xh,
                                  norms + (size_t)rr * ngroup, ngroup, g);
                yas[rr] = v;
                {
                    float d = yas[rr] - yref[rr];
                    if (d < 0.0f) d = -d;
                    if (d > maxabs) maxabs = d;
                    if (memcmp(&yas[rr], &yref[rr], sizeof(float))) {
                        mism++;
                        dbg_mism[m]++;
                        if (!printed) {
                            first_ref = yref[rr];
                            first_asm = yas[rr];
                            dbg_ref[m]  = first_ref;
                            dbg_asm[m]  = first_asm;
                            printed     = 1;
                        }
                    }
                }
                rows_checked++;
                if (maxabs > dbg_maxabs[m]) dbg_maxabs[m] = maxabs;
            }
        }

        /* Timing: the same row slice, three kernels, min over rounds. */
        for (m = 0; m < KB_NMODE; m++) {
            for (r = 0; r < KB_ROUNDS; r++) {
                uint32_t c0, c1, rr;
                c0 = esp_cpu_get_cycle_count();
                if (m == 0) {
                    nd_cq_gemv_rows(&s_c, &t, blob_ps, xh, 0, nrows_slice, yref);
                } else {
                    for (rr = 0; rr < nrows_slice; rr++) {
                        s += run_asm(m, blob_ps + (size_t)rr * rowbytes, cb, xh,
                                     norms + (size_t)rr * ngroup, ngroup, g);
                    }
                    fp_sink(s);
                }
                c1 = esp_cpu_get_cycle_count();
                cycles[i][m][r] = c1 - c0;
            }
        }

        for (m = 0; m < KB_NMODE; m++) {
            uint32_t tmin[KB_ROUNDS], mn, md;
            double   weights = (double)nrows_slice * in;
            for (k = 0; k < KB_ROUNDS; k++)
                tmin[k] = cycles[i][m][k];
            qsort(tmin, KB_ROUNDS, sizeof(tmin[0]), cmp_u32);
            mn = tmin[0];
            md = tmin[KB_ROUNDS / 2];
            printf("KB P22 tensor=%d out=%u rows=%u mode=%-12s weights=%u "
                   "min=%u med=%u cyc_per_weight=%.3f delta_vs_c=%+.2f%%\n",
                   tidx[i], (unsigned)out, (unsigned)nrows_slice, MODE[m],
                   (unsigned)(nrows_slice * in), (unsigned)mn, (unsigned)md,
                   (double)mn / weights,
                   m == 0 ? 0.0 :
                       100.0 * ((double)cycles[i][0][0] / weights -
                                (double)mn / weights) /
                       ((double)cycles[i][0][0] / weights));
        }
        printf("KB P22 tensor=%d exact_rows_checked=%u slow_norm=%u misalign=%u "
               "xh=%p internal_free=%u\n",
               tidx[i], (unsigned)rows_checked, (unsigned)slow_norm,
               (unsigned)misalign, (void *)xh,
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        for (m = 1; m < KB_NMODE; m++)
            printf("KB P22 tensor=%d mode=%-12s mismatch_rows=%u max_abs=%.6g "
                   "first_ref=%.9g first_asm=%.9g ref_y0=%.9g\n",
                   tidx[i], MODE[m], (unsigned)dbg_mism[m],
                   (double)dbg_maxabs[m], (double)dbg_ref[m], (double)dbg_asm[m],
                   (double)yref[0]);
        free(blob_ps); free(xh); free(xsrc); free(yref); free(yas);
        blob_ps = NULL; xh = NULL; xsrc = NULL; yref = NULL; yas = NULL;
    }

    printf("KB P22 SUMMARY exact_mismatch=%u rows_checked=%u slow_norm=%u\n",
           (unsigned)mism, (unsigned)rows_checked, (unsigned)slow_norm);
    printf("KB MEM internal_free=%u psram_free=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("EVT KBENCH_DONE\n");
    fflush(stdout);
    return 0;
}
