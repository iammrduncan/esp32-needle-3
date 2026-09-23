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
    printf("KB E30 RES spread_bytes=%u_compact cycles=%u,%u cyc_per_weight %.4f %.4f compacted=%+.2f%%\n",
           (unsigned)best[0], (unsigned)best[1], (double)best[0] / weights,
           (double)best[1] / weights, 100.0 * ((double)best[0] / (double)best[1] - 1.0));
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
