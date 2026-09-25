/* gemv4_tie728.S - handwritten CQ 4-bit row walker: the mHC phi projections.
 *
 * void nd_gemv4_rows_tie1(void *vc);   (a2 = context, see below)
 *
 * WHY THIS EXISTS, AND WHY IT IS NOT THE IDEA EVERYONE PRICED. Run #141 priced a
 * handwritten 4-bit kernel at -12 % from instruction counts; run #295 measured the
 * operand-DELIVERY ceiling of the same phase at 9.3-10 %. On those two numbers the
 * campaign declined to write it, and for the reason they name it is still right:
 * this kernel does not move a weight and does not remove an instruction per
 * weight. The win is somewhere neither of them measured: in the shipping build
 * `dot_group` is a *call* per group, so gemv_rows_offset() pays a function call
 * plus a per-group FP16 norm conversion across the boundary once per group - 24
 * times per row for phi - while one call per row-range keeps every group's four
 * partials in registers inside a single `loop`. Isolated screen (run #332), real
 * needle3.cact bytes, three phi tensors, min of 25 rounds, CCOUNT 24000: 5.647 vs
 * 6.338 cycles/weight (+11.6 %) and bit-exact row for row (0/128 rows mismatched,
 * known-answer probe exact).
 *
 * NO TIE INSTRUCTIONS, DELIBERATELY. The same screen measured ee.ldf.128.ip
 * (+25 %) and ee.ldf.64.ip (+21 %) on the contiguous xh operand and BOTH FAILED
 * the known-answer probe (63.0 against exactly 64.0): one of the eight floats per
 * index word is not the value asked for, so that speed is a dropped term, not a
 * win. Ordinary lsi loads are also why this kernel needs no 16-byte alignment -
 * the C path's 4-byte guarantee is enough.
 *
 * Bit-exact by construction against gemv_rows_offset() + dot_group() in
 * engine/src/nd_quant.c (bits == 4 branch):
 *   for gi: acc += nd_f16(nrm[gi]) * dot_group(...)
 *   dot_group: four partials s0..s3; per 32-bit index word, nibbles 0..3 feed
 *              s0..s3 against xh[j..j+3] and nibbles 4..7 feed s0..s3 against
 *              xh[j+4..j+7]; every term is a FUSED multiply-add, because under
 *              GCC's default -ffp-contract=fast `s += cb[p]*xh[j]` compiles to
 *              madd.s (confirmed by disassembling this build's dot_group). Fold is
 *              (s0+s1)+(s2+s3). Nothing reassociates, reorders a partial, or swaps
 *              mul for fma.
 *
 * Context (nd_gemv4_ctx in engine/include/nd_quant.h; C resolves every cursor so
 * this body needs no integer multiply, only adds):
 *    0 packed0   rows start here (= packed + r0 * rowbytes)
 *    4 norms0    (= norms + r0 * ngroup)
 *    8 xh        activation, reset per row, +512 B per group
 *   12 cb        codebook, 16 floats
 *   16 y0        (= y + i0 * 4), split-local row index
 *   20 ngroup
 *   24 g         weights per group (128)
 *   28 rowbytes
 *   32 normstep  ngroup * 2 bytes
 *   36 nrows
 *
 * Preconditions checked by nd_gemv4_asm_ok(), not here: bits == 4, g == 128, and
 * every norm in the range an ordinary FP16 (exponent neither 0 nor 31) - the
 * inline conversion below is the normal path only, the same restriction
 * nd_lut2_asm_ok() applies.
 *
 * Register map (16 FP registers, no FP register window on this core):
 *   f0..f3 partials   f4..f11 the eight xh floats   f12 codebook value
 *   f13 acc           f14 converted norm
 *   a2 ctx            a3 cb     a4 xh cursor        a5 norm cursor
 *   a6 group counter  a7 g      a8 row cursor       a9 word-loop count
 *   a10 rowbytes      a11 index word                a12 row counter
 *   a13 y cursor      a14 permanent FP zero         a15 gather address / NF16V scratch
 *
 * addx4 order (paid for twice now): GAS spells `addx4 ar, as, at` as
 * ar = (as << 2) + at, DESTINATION FIRST - as lut2_tie728.S already writes it.
 */

/* FP16 -> FP32, inline normal path, copied verbatim from lut2_tie728.S so the
 * norm conversion cannot be the reason the two kernels differ. */
	.macro	NF16V v, fpd, t1, t2
	extui	\t1, \v, 10, 5
	extui	\t2, \v, 15, 1
	extui	\v,  \v, 0, 10
	slli	\v,  \v, 13
	addi	\t1, \t1, 112
	slli	\t1, \t1, 23
	slli	\t2, \t2, 31
	or	\v,  \v, \t1
	or	\v,  \v, \t2
	wfr	\fpd, \v
	.endm

	.set	GC_PACKED,   0
	.set	GC_NORMS,    4
	.set	GC_XH,       8
	.set	GC_CB,      12
	.set	GC_Y,       16
	.set	GC_NGROUP,  20
	.set	GC_G,       24
	.set	GC_ROWBYTES, 28
	.set	GC_NORMSTEP, 32
	.set	GC_NROWS,   36

	.section .iram1.gemv4_tie1, "ax", @progbits
	.align	4
	.global	nd_gemv4_rows_tie1
	.type	nd_gemv4_rows_tie1,@function
nd_gemv4_rows_tie1:
	entry	a1, 32
	l32i	a3,  a2, GC_CB		/* loop-invariant cursors               */
	l32i	a7,  a2, GC_G
	l32i	a10, a2, GC_ROWBYTES
	l32i	a12, a2, GC_NROWS
	l32i	a8,  a2, GC_PACKED
	l32i	a5,  a2, GC_NORMS
	l32i	a13, a2, GC_Y
	movi	a14, 0			/* permanent FP zero: this core has no  */
					/* movi.s, so constants enter via wfr   */
.Lrow:
	wfr	f13, a14		/* acc = 0.0f                             */
	l32i	a6, a2, GC_NGROUP	/* group counter                          */
	l32i	a4, a2, GC_XH		/* xh restarts every row                  */
.Lgroup:
	wfr	f0, a14			/* s0..s3                                 */
	wfr	f1, a14
	wfr	f2, a14
	wfr	f3, a14
	srli	a9, a7, 3		/* index words per group = g / 8          */
	loop	a9, .Lwend
	l32i	a11, a8, 0
	addi	a8, a8, 4
	lsi	f4, a4, 0
	lsi	f5, a4, 4
	lsi	f6, a4, 8
	lsi	f7, a4, 12
	lsi	f8, a4, 16
	lsi	f9, a4, 20
	lsi	f10, a4, 24
	lsi	f11, a4, 28
	addi	a4, a4, 32
	extui	a15, a11, 0, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f0, f12, f4
	extui	a15, a11, 4, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f1, f12, f5
	extui	a15, a11, 8, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f2, f12, f6
	extui	a15, a11, 12, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f3, f12, f7
	extui	a15, a11, 16, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f0, f12, f8
	extui	a15, a11, 20, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f1, f12, f9
	extui	a15, a11, 24, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f2, f12, f10
	extui	a15, a11, 28, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f3, f12, f11
.Lwend:
	add.s	f0, f0, f1		/* (s0+s1)+(s2+s3), C's order             */
	add.s	f2, f2, f3
	add.s	f0, f0, f2
	l16ui	a15, a5, 0
	addi	a5, a5, 2
	NF16V	a15, f14, a9, a11	/* a9 and a11 are dead here; a12 is the  */
	madd.s	f13, f14, f0		/* row counter and must stay live         */
	addi	a6, a6, -1
	bnez	a6, .Lgroup
	ssi	f13, a13, 0		/* y[i] = acc                             */
	addi	a13, a13, 4
	/* No cursor fix-up here, and that is the whole reason the first integrated
	 * build diverged (device_output_exact 5/17, device_token_delta 175): the word
	 * loop already consumes a whole row - a8 advances 4 B per index word (64 B per
	 * group) and a5 2 B per group - so both sit exactly on the NEXT row when the
	 * groups run out. Adding rowbytes / normstep again skipped every row after the
	 * first. The isolation screen called the kernel once per row, where the caller
	 * did that stepping, so no isolated test could see this: only a multi-row
	 * kernel measured against a multi-row reference can. */
	addi	a12, a12, -1
	bnez	a12, .Lrow
	retw
	.size	nd_gemv4_rows_tie1, .-nd_gemv4_rows_tie1

/* nd_pv_pair2w - E53: the wide-load form of the B4W paired-head P.V update.
 *
 * void nd_pv_pair2w(float *ohA, float *ohB, const float *vf0, const float *vf1,
 *                   uint32_t nchunk, float w0, float w1, float w2, float w3);
 *   a2 ohA, a3 ohB, a4 vf0, a5 vf1, a6 nchunk (= v_hd/4, must be >= 1),
 *   a7 = w0, a1+32 = w1, a1+36 = w2, a1+40 = w3   (mixed int/float args: the
 *   windowed ABI puts the first six in a2-a7 whatever their type - measured with
 *   a probe, not assumed - and the rest on the caller's frame)
 *   per 4-float chunk j:
 *     ohA[j] = madd(w0, vf0[j], madd(w1, vf1[j], ohA[j]))
 *     ohB[j] = madd(w2, vf0[j], madd(w3, vf1[j], ohB[j]))
 *
 * Two statements per cell, term0 then term1, is what the shipped body has and
 * what GCC -O2 emits for it: lsi oh; madd.s oh,w0,v0; madd.s oh,w1,u0; ssi oh.
 * The accumulator is updated by TWO separately rounded FMAs, so no pairing of
 * them into one expression is allowed - only the operand delivery changes.
 *
 * Against the shipped body this is 6 memory operations per chunk instead of 32:
 * both staged rows and each output row arrive as one 128-bit load, and the
 * outputs leave as one 128-bit store. The rows stay resident across all eight
 * madds (the compiler already keeps each output row in registers, so the whole
 * saving is the row transport).
 *
 * BOTH 128-bit forms are POST-INCREMENT and for BOTH the LAST-named register
 * receives the LOWEST address (TRM 1.8.25 for ee.ldf.128.ip, 1.8.65 for
 * ee.stf.128.ip). Two consequences the first version of this kernel got wrong:
 *   - a load followed by a store of the same row advances the pointer TWICE, so
 *     the store must be preceded by `addi aS, aS, -16` and the pair then nets
 *     +16 per chunk (a double advance walks 384 bytes past a 192-byte row and
 *     corrupts the caller's frame - the first version crashed the board in its
 *     own self-test, which is what the canaries in nd_model.c now assert);
 *   - the store list must be written in the SAME order as the load list
 *     (`stf f15,f14,f13,f12`), not reversed, or the four cells are permuted.
 * E50's store-back probe cannot settle the store order: it stores to the address
 * it has just read and then rewinds the pointer, so a permutation is invisible.
 *
 * f0-f3 hold the four weight scalars for the whole loop; f4-f7 are the vf0 row,
 * f8-f11 the vf1 row and f12-f15 the output row being updated. Preconditions are
 * the caller's: 16-byte-aligned ohA/ohB/vf0/vf1 (nd_model.c checks once per pair
 * and keeps the C body otherwise), nchunk = v_hd/4 with the v_hd % 4 remainder
 * left to the C tail.
 */
	.align	4
	.global	nd_pv_pair2w
	.type	nd_pv_pair2w,@function
nd_pv_pair2w:
	entry	a1, 32
	wfr	f0, a7
	l32i	a8, a1, 32
	l32i	a9, a1, 36
	l32i	a10, a1, 40
	wfr	f1, a8
	wfr	f2, a9
	wfr	f3, a10
	loop	a6, .Lpvend
.Lpvchunk:
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a4, 16	/* vf0[j..j+3] */
	ee.ldf.128.ip	f11, f10, f9,  f8,  a5, 16	/* vf1[j..j+3] */
	ee.ldf.128.ip	f15, f14, f13, f12, a2, 16	/* ohA[j..j+3] */
	madd.s	f12, f0, f4
	madd.s	f13, f0, f5
	madd.s	f14, f0, f6
	madd.s	f15, f0, f7
	madd.s	f12, f1, f8
	madd.s	f13, f1, f9
	madd.s	f14, f1, f10
	madd.s	f15, f1, f11
	addi	a2, a2, -16			/* stf also post-increments: same address */
	ee.stf.128.ip	f15, f14, f13, f12, a2, 16
	ee.ldf.128.ip	f15, f14, f13, f12, a3, 16	/* ohB[j..j+3] */
	madd.s	f12, f2, f4
	madd.s	f13, f2, f5
	madd.s	f14, f2, f6
	madd.s	f15, f2, f7
	madd.s	f12, f3, f8
	madd.s	f13, f3, f9
	madd.s	f14, f3, f10
	madd.s	f15, f3, f11
	addi	a3, a3, -16
	ee.stf.128.ip	f15, f14, f13, f12, a3, 16
.Lpvend:
	retw
	.size	nd_pv_pair2w, .-nd_pv_pair2w

/* nd_kron1_w - E55: wide-load factor row for the kron1 4x2 pass.
 *
 * void nd_kron1_w(float *out8, const float *src_j, const float *a_k,
 *                 uint32_t na, uint32_t nb_bytes, uint32_t a_skip);
 *   a2 out8, a3 src_j, a4 a_k, a5 na (i count, >= 1), a6 nb*4 (src row stride),
 *   a7 na*4 - 16 (bytes to add after the 128-bit load, so each i advances one
 *   full factor row)
 *   out[0..3] = s[0..3] (column j), out[4..7] = u[0..3] (column j+1)
 *
 * The shipped kron1 body, per i: two scalar src reads (v0 at j, v1 at j+1), four
 * scalar factor reads ar[0..3] through a pointer, and eight madd.s - the four
 * factor values are CONTIGUOUS, which is the only reason a 128-bit load can
 * replace them. The per-output sums stay in i order with the same two terms, so
 * every stored float is identical; only the factor transport changes (4 lsi -> 1
 * ldf.128 per i, ~19 % of the loop's instructions).
 *
 * This half is register-feasible where the kron2 half is not: eight accumulators
 * (f0-f3 for column j, f4-f7 for j+1), the four factor values (f8-f11) and the
 * two src values (f12, f13) are 14 of the 16 fp registers. kron2 needs 8
 * accumulators AND two eight-float factor rows, which cannot fit, and the
 * four-accumulator fallback measured -0.30 % (#674).
 *
 * Last-named register receives the LOWEST address (TRM 1.8.25). Preconditions
 * are the caller's: a_k 16-byte aligned (k0 is a multiple of four and the factor
 * pool is 16-byte aligned), na a multiple of four so every row stays aligned.
 */
	.align	4
	.global	nd_kron1_w
	.type	nd_kron1_w,@function
nd_kron1_w:
	entry	a1, 32
	movi	a8, 0
	wfr	f0, a8
	wfr	f1, a8
	wfr	f2, a8
	wfr	f3, a8
	wfr	f4, a8
	wfr	f5, a8
	wfr	f6, a8
	wfr	f7, a8
	loop	a5, .Lk1end
.Lk1:
	ee.ldf.128.ip	f11, f10, f9,  f8,  a4, 16	/* ar[0..3] */
	lsi	f12, a3, 0				/* v0 = src[i*nb + j]     */
	lsi	f13, a3, 4				/* v1 = src[i*nb + j + 1] */
	add.n	a3, a3, a6
	add.n	a4, a4, a7
	madd.s	f0, f12, f8
	madd.s	f4, f13, f8
	madd.s	f1, f12, f9
	madd.s	f5, f13, f9
	madd.s	f2, f12, f10
	madd.s	f6, f13, f10
	madd.s	f3, f12, f11
	madd.s	f7, f13, f11
.Lk1end:
	ssi	f0, a2, 0
	ssi	f1, a2, 4
	ssi	f2, a2, 8
	ssi	f3, a2, 12
	ssi	f4, a2, 16
	ssi	f5, a2, 20
	ssi	f6, a2, 24
	ssi	f7, a2, 28
	retw
	.size	nd_kron1_w, .-nd_kron1_w

/* nd_fwht4r - E59: the LIVE FWHT path's first pass as fused radix-4 + wide loads.
 *
 * void nd_fwht4r(float *x0, float *x1, float *x2, uint32_t n);
 *   a2/a3/a4 the three group bases, a5 n (a power of two >= 8)
 *   for each 4-cell block [i..i+3] of each group, with p,q,r,s = x[i..i+3]:
 *     t1 = p+q; t2 = p-q; t3 = r+s; t4 = r-s;      (the len=1 stage)
 *     x[i] = t1+t3; x[i+1] = t2+t4; x[i+2] = t1-t3; x[i+3] = t2-t4;   (len=2)
 *
 * WHY THIS SHAPE. fwht_rows calls nd_fwht3s for the model's per-core group walk
 * (ngroup 6 split 3+3), and its first two stages are the only ones whose four
 * cells are CONTIGUOUS in each group - which is what a 128-bit load needs. Fusing
 * the two stages per 4-cell block computes exactly the values the two-stage walk
 * produces: every output is `t1+t3` etc. from the same adds/subs in the same
 * order, because the len=2 stage reads only the len=1 results. Per block this is
 * 3 loads + 24 fp ops + 3 stores for three groups (12 cells) against 12 loads +
 * 24 fp ops + 12 stores for the same 12 cells in scalar form.
 *
 * Registers: 12 loaded values (f0-f11) + 3 free temps (f12-f14) + the block base
 * pointers; the four temps per group are reused across the three groups. Store
 * lists are written high-to-low so the LAST-named register is the LOWEST address
 * (TRM 1.8.65), and each store is preceded by the rewind its own load needs
 * because both 128-bit forms post-increment.
 *
 * Caller-gated: n >= 8, all three bases 16-byte aligned (m->xh is a FAST16
 * allocation), and the C walk keeps everything else.
 */
	.align	4
	.global	nd_fwht4r
	.type	nd_fwht4r,@function
nd_fwht4r:
	entry	a1, 32
	srli	a5, a5, 2			/* n/4 blocks */
	loop	a5, .Lfr_end
.Lfr:
	ee.ldf.128.ip	f3,  f2,  f1,  f0,  a2, 16	/* g0: p q r s */
	add.s	f12, f0, f1
	sub.s	f13, f0, f1
	add.s	f14, f2, f3
	sub.s	f15, f2, f3
	add.s	f0, f12, f14
	sub.s	f2, f12, f14
	add.s	f1, f13, f15
	sub.s	f3, f13, f15
	addi	a2, a2, -16
	ee.stf.128.ip	f3,  f2,  f1,  f0,  a2, 16
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a3, 16	/* g1 */
	add.s	f12, f4, f5
	sub.s	f13, f4, f5
	add.s	f14, f6, f7
	sub.s	f15, f6, f7
	add.s	f4, f12, f14
	sub.s	f6, f12, f14
	add.s	f5, f13, f15
	sub.s	f7, f13, f15
	addi	a3, a3, -16
	ee.stf.128.ip	f7,  f6,  f5,  f4,  a3, 16
	ee.ldf.128.ip	f11, f10, f9,  f8,  a4, 16	/* g2 */
	add.s	f12, f8, f9
	sub.s	f13, f8, f9
	add.s	f14, f10, f11
	sub.s	f15, f10, f11
	add.s	f8,  f12, f14
	sub.s	f10, f12, f14
	add.s	f9,  f13, f15
	sub.s	f11, f13, f15
	addi	a4, a4, -16
	ee.stf.128.ip	f11, f10, f9,  f8,  a4, 16
.Lfr_end:
	retw
	.size	nd_fwht4r, .-nd_fwht4r

/* nd_lanemix4w / nd_mul4w - E64: wide delivery for the mHC lane mix.
 *
 * void nd_lanemix4w(float *dst, const float *src, float w, uint32_t n4);
 *   dst[i] += w * src[i] over 4*n4 floats, one madd.s per cell, four cells per
 *   128-bit load/store pair.
 * void nd_mul4w(float *dst, const float *src, float w, uint32_t n4);
 *   dst[i] = w * src[i], the same transport for the mix's initialiser.
 *
 * The shipped body is `dst[i] = hpost[k]*u[i]` followed by four passes of
 * `dst[i] += w*src[i]` over d_model - five read-modify-write passes per output
 * lane with contiguous rows, independent cells and no accumulator chain (one
 * madd per cell per pass). That is the shape the P.V kernel already proved pays:
 * the compiler cannot keep dst in registers across the passes, so every cell is
 * loaded and stored twice per pass; here it is loaded once and stored once.
 *
 * Bit-exactness is structural: one rounding per cell per pass, same order of
 * passes, same madd. Register lists high-to-low (the LAST-named register is the
 * LOWEST address, TRM 1.8.25); both stores are preceded by the rewind their own
 * load needs because 128-bit loads and stores BOTH post-increment (1.8.65).
 *
 * Caller-gated: dst and src 16-byte aligned, n4 = d_model/4.
 */
	.align	4
	.global	nd_lanemix4w
	.type	nd_lanemix4w,@function
nd_lanemix4w:
	entry	a1, 32
	wfr	f0, a4			/* w: a2 dst, a3 src, a4 w, a5 n4 */
	loop	a5, .Llm_end
.Llm:
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a2, 16	/* dst */
	ee.ldf.128.ip	f11, f10, f9,  f8,  a3, 16	/* src */
	madd.s	f4,  f8,  f0
	madd.s	f5,  f9,  f0
	madd.s	f6,  f10, f0
	madd.s	f7,  f11, f0
	addi	a2, a2, -16
	ee.stf.128.ip	f7,  f6,  f5,  f4,  a2, 16
.Llm_end:
	retw
	.size	nd_lanemix4w, .-nd_lanemix4w

	.align	4
	.global	nd_mul4w
	.type	nd_mul4w,@function
nd_mul4w:
	entry	a1, 32
	wfr	f0, a4			/* w: a2 dst, a3 src, a4 w, a5 n4 */
	loop	a5, .Lmu_end
.Lmu:
	ee.ldf.128.ip	f11, f10, f9,  f8,  a3, 16	/* src */
	mul.s	f8,  f8,  f0
	mul.s	f9,  f9,  f0
	mul.s	f10, f10, f0
	mul.s	f11, f11, f0
	ee.stf.128.ip	f11, f10, f9,  f8,  a2, 16
.Lmu_end:
	retw
	.size	nd_mul4w, .-nd_mul4w

/* nd_lanepre4w - E67: wide delivery for the mHC lane pre-mix reduction.
 *
 * void nd_lanepre4w(float *dst, const float *lane, const float *hpre,
 *                   uint32_t dm_bytes, uint32_t n4);
 *   a2 dst, a3 lane (row 0 at the first column), a4 hpre (four weights, possibly
 *   a stack array - loaded SCALARLY for that reason), a5 dm in BYTES (row stride),
 *   a6 n4 (four-column chunks).
 *
 * The shipped loop is one serial chain per column:
 *   acc = 0; acc += hpre[j]*lane[j*dm + i] for j = 0..3; dst[i] = acc
 * so four independent columns can be walked together: one 128-bit load per input
 * lane covers four columns, and each column keeps its own j-ascending chain with a
 * single madd per term (the first adds to +0.0f, exactly as the C statement does).
 *
 * FOUR row pointers (a8-a11) advance by 16 bytes per tile, so every tile starts at
 * row 0 again. The first version used ONE pointer and added a stride between rows,
 * which walked off the end of row 3 after the first tile - the multi-tile case in
 * the selftest is what covers that, not the four-row single-tile case.
 *
 * Register lists high-to-low: the LAST-named register is the LOWEST address (TRM
 * 1.8.25). Caller-gated: 16-byte-aligned dst and lane row bases, dm a multiple of
 * four, n4 = (hi-lo)/4.
 */
	.align	4
	.global	nd_lanepre4w
	.type	nd_lanepre4w,@function
nd_lanepre4w:
	entry	a1, 32
	lsi	f8,  a4, 0				/* hpre may be a stack array: scalar */
	lsi	f9,  a4, 4
	lsi	f10, a4, 8
	lsi	f11, a4, 12
	mov.n	a8,  a3
	add.n	a9,  a3,  a5
	add.n	a10, a9,  a5
	add.n	a11, a10, a5
	loop	a6, .Llp_end
.Llp:
	movi	a7, 0
	wfr	f0, a7
	wfr	f1, a7
	wfr	f2, a7
	wfr	f3, a7
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a8,  16
	madd.s	f0, f4, f8
	madd.s	f1, f5, f8
	madd.s	f2, f6, f8
	madd.s	f3, f7, f8
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a9,  16
	madd.s	f0, f4, f9
	madd.s	f1, f5, f9
	madd.s	f2, f6, f9
	madd.s	f3, f7, f9
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a10, 16
	madd.s	f0, f4, f10
	madd.s	f1, f5, f10
	madd.s	f2, f6, f10
	madd.s	f3, f7, f10
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a11, 16
	madd.s	f0, f4, f11
	madd.s	f1, f5, f11
	madd.s	f2, f6, f11
	madd.s	f3, f7, f11
	ee.stf.128.ip	f3,  f2,  f1,  f0,  a2,  16
.Llp_end:
	retw
	.size	nd_lanepre4w, .-nd_lanepre4w

/* nd_lanemix_row4 - E65: the mHC lane mix with the output row resident.
 *
 * void nd_lanemix_row4(float *dst, const float *u, float hpost,
 *                      const float *lane, uint32_t stride_bytes,
 *                      const float *w4, uint32_t n4);
 *   a2 dst, a3 u, a4 hpost, a5 lane (row 0), a6 stride in BYTES, a7 w4 (the four
 *   weights for this output lane), n4 on the caller's frame at a1+32.
 *
 * E64 moved four cells per 128-bit pair but still made five passes over the output
 * row (init + four accumulations), so dst was loaded and stored five times. Here
 * the four-cell tile of dst stays in f4-f7 across all four input lanes: one u load,
 * four weight madds, four src loads and ONE store per tile instead of ten memory
 * operations. Per 4 cells: 6 memory ops + 20 fp ops against E64's 15 + 20.
 *
 * Bit-exact by construction: per cell the order is  dst = hpost*u, then += w_j*src_j
 * for j ascending - identical to the two-loop form, and the tile is what changes,
 * not the arithmetic.
 *
 * Register lists high-to-low (LAST-named = LOWEST address, TRM 1.8.25); the store
 * replaces the load's increment, so no rewind is needed here (the loads all advance
 * their own pointers by 16 and the dst pointer is advanced by the store itself).
 * Caller-gated: 16-byte-aligned dst/u/lane row bases, n4 = dm/4.
 */
	.align	4
	.global	nd_lanemix_row4
	.type	nd_lanemix_row4,@function
nd_lanemix_row4:
	entry	a1, 32
	l32i	a14, a1, 32			/* n4: 7th argument, same offset E53 proved on device */
	ee.ldf.128.ip	f3,  f2,  f1,  f0,  a7, 16	/* w0..w3 */
	wfr	f12, a4				/* hpost */
	/* four row pointers, one per input lane */
	mov.n	a8,  a5
	add.n	a9,  a5,  a6
	add.n	a10, a5,  a6
	add.n	a10, a10, a6
	add.n	a11, a5,  a6
	add.n	a11, a11, a6
	add.n	a11, a11, a6
	loop	a14, .Llmr_end
.Llmr:
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a3, 16	/* u tile */
	mul.s	f4, f4, f12
	mul.s	f5, f5, f12
	mul.s	f6, f6, f12
	mul.s	f7, f7, f12
	ee.ldf.128.ip	f11, f10, f9,  f8,  a8, 16
	madd.s	f4, f8,  f0
	madd.s	f5, f9,  f0
	madd.s	f6, f10, f0
	madd.s	f7, f11, f0
	ee.ldf.128.ip	f11, f10, f9,  f8,  a9, 16
	madd.s	f4, f8,  f1
	madd.s	f5, f9,  f1
	madd.s	f6, f10, f1
	madd.s	f7, f11, f1
	ee.ldf.128.ip	f11, f10, f9,  f8,  a10, 16
	madd.s	f4, f8,  f2
	madd.s	f5, f9,  f2
	madd.s	f6, f10, f2
	madd.s	f7, f11, f2
	ee.ldf.128.ip	f11, f10, f9,  f8,  a11, 16
	madd.s	f4, f8,  f3
	madd.s	f5, f9,  f3
	madd.s	f6, f10, f3
	madd.s	f7, f11, f3
	ee.stf.128.ip	f7,  f6,  f5,  f4,  a2, 16
.Llmr_end:
	retw
	.size	nd_lanemix_row4, .-nd_lanemix_row4

/* nd_gemv4_rows_tie1W - wide-load twin of nd_gemv4_rows_tie1.
 *
 * WHY THIS IS NOT RUN #333'S WIDE LOAD. That screen's negative rested on a
 * probe that never reset its A fixture and on a register list (f4,f5,f6,f7)
 * OPPOSITE to the hardware fill: the extension's 128-bit forms are all
 * post-increment and the LAST named register receives the LOWEST address (TRM
 * 1.8.25/1.8.65; E50's store-back probe is consistent with it and is not used
 * here as proof - it only ever wrote the address it had just read). The list
 * must therefore be written f7,f6,f5,f4 so f4 receives the LOWEST address. With
 * that order the kernel is
 * bit-exact against tie1 row-for-row (96/96, real phi bytes, internal xh) and
 * warm 5.400 vs 6.281 cycles per weight = +16.3 % on the field-shape sweep.
 * Requires 16-byte-aligned xh: the wrapper checks, and nd_model_open now
 * allocates m->xh with heap_caps_aligned_alloc(16).
 */
	.align	4
	.global	nd_gemv4_rows_tie1W
	.type	nd_gemv4_rows_tie1W,@function
nd_gemv4_rows_tie1W:
	entry	a1, 32
	l32i	a3,  a2, GC_CB
	l32i	a7,  a2, GC_G
	l32i	a12, a2, GC_NROWS
	l32i	a8,  a2, GC_PACKED
	l32i	a5,  a2, GC_NORMS
	l32i	a13, a2, GC_Y
	movi	a14, 0
.Lwrow:
	wfr	f13, a14
	l32i	a6, a2, GC_NGROUP
	l32i	a4, a2, GC_XH
.Lwgroup:
	wfr	f0, a14
	wfr	f1, a14
	wfr	f2, a14
	wfr	f3, a14
	srli	a9, a7, 3
	loop	a9, .Lwwend
	l32i	a11, a8, 0
	addi	a8, a8, 4
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a4, 16
	ee.ldf.128.ip	f11, f10, f9,  f8,  a4, 16
	extui	a15, a11, 0, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f0, f12, f4
	extui	a15, a11, 4, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f1, f12, f5
	extui	a15, a11, 8, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f2, f12, f6
	extui	a15, a11, 12, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f3, f12, f7
	extui	a15, a11, 16, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f0, f12, f8
	extui	a15, a11, 20, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f1, f12, f9
	extui	a15, a11, 24, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f2, f12, f10
	extui	a15, a11, 28, 4
	addx4	a15, a15, a3
	lsi	f12, a15, 0
	madd.s	f3, f12, f11
.Lwwend:
	add.s	f0, f0, f1
	add.s	f2, f2, f3
	add.s	f0, f0, f2
	l16ui	a15, a5, 0
	addi	a5, a5, 2
	NF16V	a15, f14, a9, a11
	madd.s	f13, f14, f0
	addi	a6, a6, -1
	bnez	a6, .Lwgroup
	ssi	f13, a13, 0
	addi	a13, a13, 4
	addi	a12, a12, -1
	bnez	a12, .Lwrow
	retw
	.size	nd_gemv4_rows_tie1W, .-nd_gemv4_rows_tie1W
