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
