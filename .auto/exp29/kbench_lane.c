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

    for (lo = 0u; lo < f->cum[f->nseg]; lo += blk) {
        uint32_t hi = lo + blk, s;
        for (s = 0; s < f->nseg; s++) {
            uint32_t a = f->cum[s], b = f->cum[s + 1u], x, y;
            if (hi <= a || lo >= b) continue;
            x = (lo > a) ? lo - a : 0u;
            y = (hi < b) ? hi - a : b - a;
            if (y > x && x < r1 && y > 0u) {
                /* only the part of this block inside the caller's range */
                uint32_t p = (lo > r0) ? lo : r0, q = (hi < r1) ? hi : r1;
                if (q > p) f->fn((void *)&f->ctx[s], p - a > x ? p - a : x,
                                 q - a < y ? q - a : y);
            }
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
        nd_row_fn fn3 = nd_lut2_rows_c;
#if ND_LUT2_ASM
        if (nd_lut2_asm_ok(&cship[0], 0u, rows3)) fn3 = nd_lut2_rows_tie1n;
#endif
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
        f.cum[s + 1u] = total;
    }
    memcpy(fs.ctx, f.ctx, sizeof(fs.ctx));   /* fused ctx writes yfus */
    memcpy(fs.y, f.y, sizeof(fs.y));
    fs.fn = nd_lut2_rows_c;
#if ND_LUT2_ASM
    fs.fn = nd_lut2_rows_tie1n;
    for (s = 0; s < f.nseg; s++)
        if (!nd_lut2_asm_ok(&f.ctx[s], 0u, f.nrows[s])) fs.fn = nd_lut2_rows_c;
#endif
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
