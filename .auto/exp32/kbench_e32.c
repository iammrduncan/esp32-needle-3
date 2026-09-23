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
