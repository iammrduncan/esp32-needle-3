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
