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
