/* dot4_tie728.S - Experiment 22 SCREEN kernels: handwritten CQ 4-bit row walkers
 * that differ ONLY in how the eight contiguous xh floats per index word are
 * loaded. Kbench-only (esp32/main/CMakeLists.txt adds it under NEEDLE_KBENCH), so
 * a shipping configure never places any of this in IRAM.
 *
 *   float nd_cq_row4_tieN(const uint8_t *row, const float *cb, const float *xh,
 *                         const uint16_t *nrm, unsigned ngroup, unsigned g)
 *     a2 row, a3 cb, a4 xh, a5 nrm, a6 ngroup, a7 g
 *
 * Bit-exact by construction against engine/src/nd_quant.c's 4-bit row walk:
 *   for gi: acc += nd_f16(nrm[gi]) * dot_group(row, cb, xh + gi*g)
 *   dot_group: four partials s0..s3; per 32-bit index word the nibbles feed
 *              s0,s1,s2,s3 against xh[j..j+3], then s0,s1,s2,s3 against
 *              xh[j+4..j+7]; every term is a FUSED multiply-add, because under
 *              GCC's default -ffp-contract=fast the C form `s += cb[p]*xh[j]`
 *              compiles to madd.s (run #141's disassembly of the shipped loop).
 *   fold is (s0+s1)+(s2+s3). Nothing reassociates or reorders a partial.
 *
 * The three variants, per 32-bit index word (= 8 weights):
 *   tie0  8 x lsi(f)              -> 41 instructions, GCC's own shape, the asm
 *                                    control that separates "hand written" from
 *                                    "wide load" in the measurement.
 *   tie1  2 x ee.ldf.128.ip       -> 35 instructions (-14.6 %): four floats per
 *   tie2  4 x ee.ldf.64.ip        -> 37 instructions (-9.8 %):  instruction.
 * Both TIE forms assemble with xtensa-esp32s3-elf-as (esp-14.2.0_20251107) and
 * land in the *scalar* FP register file (ee.ldf.64.ip fA, fB, aS, m). GCC never
 * emits them: it emits two lsi for an adjacent float pair (verified by compiling
 * the same shape with -O3).
 *
 * addx4 operand order (bought with a LoadProhibited on device): GAS spells this
 * `addx4 ar, as, at` -> ar = (as << 2) + at, i.e. DESTINATION FIRST. Writing it as
 * `addx4 idx, base, idx` silently computes (base << 2) + idx and faults on the
 * next lsi - which is why lut2_tie728.S writes `addx4 a12, a12, a4`.
 *
 * Preconditions the caller (kbench) checks and does not assume: g == 128, xh
 * 16-byte aligned at every group base (128 floats = 512 B stride keeps it), and
 * every norm an ordinary FP16 (exponent neither 0 nor 31 - no nd_f16_slow path
 * here, same restriction as nd_lut2_asm_ok()).
 */

/* FP16 -> FP32, inline normal path, copied verbatim from lut2_tie728.S so the
 * norm conversion cannot be the reason two variants differ. */
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

/* The eight xh floats of one index word, three ways. All leave a4 advanced by
 * 32 bytes; tie0 does it explicitly because lsi has no post-increment. */
	.macro	XH_WIDE _
	ee.ldf.128.ip f4, f5, f6, f7, a4, 16
	ee.ldf.128.ip f8, f9, f10, f11, a4, 16
	.endm

	.macro	XH_PAIR _
	ee.ldf.64.ip f4, f5, a4, 8
	ee.ldf.64.ip f6, f7, a4, 8
	ee.ldf.64.ip f8, f9, a4, 8
	ee.ldf.64.ip f10, f11, a4, 8
	.endm

	.macro	XH_SCALAR _
	lsi	f4, a4, 0
	lsi	f5, a4, 4
	lsi	f6, a4, 8
	lsi	f7, a4, 12
	lsi	f8, a4, 16
	lsi	f9, a4, 20
	lsi	f10, a4, 24
	lsi	f11, a4, 28
	addi	a4, a4, 32
	.endm

/* One row walker. \sec section name, \sym symbol, \l label prefix, \xh xh loads. */
	.macro	ROW4 sec, sym, l, xh
	.section .iram1.\sec, "ax", @progbits
	.align	4
	.global	\sym
	.type	\sym,@function
\sym:
	entry	a1, 32
	movi	a14, 0			/* permanent FP zero: this core has no    */
	wfr	f13, a14		/* movi.s, so constants enter through wfr */
.L\l\()_group:
	wfr	f0, a14			/* s0..s3                               */
	wfr	f1, a14
	wfr	f2, a14
	wfr	f3, a14
	srli	a10, a7, 3		/* index words in this group = g/8        */
	loop	a10, .L\l\()_end
	l32i	a8, a2, 0
	addi	a2, a2, 4
	\xh(_)
	extui	a9, a8, 0, 4
	addx4	a9, a9, a3
	lsi	f12, a9, 0
	madd.s	f0, f12, f4
	extui	a9, a8, 4, 4
	addx4	a9, a9, a3
	lsi	f12, a9, 0
	madd.s	f1, f12, f5
	extui	a9, a8, 8, 4
	addx4	a9, a9, a3
	lsi	f12, a9, 0
	madd.s	f2, f12, f6
	extui	a9, a8, 12, 4
	addx4	a9, a9, a3
	lsi	f12, a9, 0
	madd.s	f3, f12, f7
	extui	a9, a8, 16, 4
	addx4	a9, a9, a3
	lsi	f12, a9, 0
	madd.s	f0, f12, f8
	extui	a9, a8, 20, 4
	addx4	a9, a9, a3
	lsi	f12, a9, 0
	madd.s	f1, f12, f9
	extui	a9, a8, 24, 4
	addx4	a9, a9, a3
	lsi	f12, a9, 0
	madd.s	f2, f12, f10
	extui	a9, a8, 28, 4
	addx4	a9, a9, a3
	lsi	f12, a9, 0
	madd.s	f3, f12, f11
.L\l\()_end:
	add.s	f0, f0, f1
	add.s	f2, f2, f3
	add.s	f0, f0, f2
	l16ui	a9, a5, 0
	addi	a5, a5, 2
	NF16V	a9, f14, a11, a12
	madd.s	f13, f14, f0
	addi	a6, a6, -1
	bnez	a6, .L\l\()_group
	mov.s	f0, f13
	/* THIS CORE'S ABI: a float return value comes back in the INTEGER return
	 * register, not in f0 - GCC's own float function ends with
	 * `ssi f0, a1, 0` / `l32i.n a2, a1, 0` (verified by disassembling a
	 * -O3 float function from this toolchain). Reading the result from f0 in
	 * C produced one constant value from all three variants, which is what
	 * the known-answer unit probe in kbench then localised. */
	ssi	f0, a1, 16
	l32i	a2, a1, 16
	retw
	.size	\sym, .-\sym
	.endm

	ROW4(dot4_tie0, nd_cq_row4_tie0, t0, XH_SCALAR)
	ROW4(dot4_tie1, nd_cq_row4_tie1, t1, XH_WIDE)
	ROW4(dot4_tie2, nd_cq_row4_tie2, t2, XH_PAIR)
