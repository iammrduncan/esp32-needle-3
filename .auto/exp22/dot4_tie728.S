/* dot4_tie728.S - Experiment 22 SCREEN: handwritten CQ 4-bit row kernel.
 *
 * float nd_cq_row4_tie1(const uint8_t *row, const float *cb, const float *xh,
 *                       const uint16_t *nrm, unsigned ngroup, unsigned g)
 *   a2 row, a3 cb, a4 xh, a5 nrm, a6 ngroup, a7 g  (g = weights per group)
 *
 * Bit-exact by construction against engine/src/nd_quant.c:
 *   for gi: acc += nd_f16(nrm[gi]) * dot_group(row, cb, xh + gi*g, g)
 *   dot_group: four partials s0..s3, per 32-bit index word the nibbles go
 *              s0,s1,s2,s3 then s0,s1,s2,s3 against xh[j..j+3] then xh[j+4..j+7],
 *              each term a FUSED multiply-add (`s += cb[p]*xh[j]` under GCC's
 *              default -ffp-contract=fast is madd.s, which is what the shipped
 *              loop's disassembly shows), and the fold is (s0+s1)+(s2+s3).
 * Nothing here reassociates, reorders a partial, or swaps mul for fma.
 *
 * THE ONE THING THIS TESTS. The shipped loop is
 *   1 l32i + 1 addi + 8 extui + 8 addx4 + 8 lsi(cb) + 8 lsi(xh) + 8 madd.s = 41
 * instructions per 32-bit word = per 8 weights. `xh` is the only operand in the
 * loop that is contiguous, and the ESP32-S3 TIE offers paired float loads that
 * land in the *scalar* FP register file:
 *   ee.ldf.64.ip  fA, fB, aS, m   -> two floats, one instruction
 *   ee.ldf.128.ip fA, fB, fC, fD, aS, m -> four floats, one instruction
 * (both assemble with xtensa-esp32s3-elf-as from esp-14.2.0_20251107; GCC itself
 * never emits them - it emits two lsi for an adjacent float pair.) This kernel
 * replaces the eight lsi(xh) with two ee.ldf.128.ip: 36 instructions per word,
 * -12.2 %. Run #141 predicted -12 % from exactly this count; run #13 measured a
 * hand-written dot that used ee.ldf.128.ip at -107 %, on the theory that a wide
 * TIE float load is not one LSU operation. That disagreement is the experiment.
 *
 * Preconditions (the caller checks them; the screen checks them in kbench):
 *   g == 128 and the group count divides evenly (rowbytes == ngroup * g/2);
 *   row and xh 4-byte aligned, xh 16-byte aligned at every group base (the
 *   paired loads' alignment), all norms ordinary FP16 (exponent neither 0 nor 31).
 */

/* FP16 -> FP32, inline normal path, same bit arithmetic as nd_f16(). Copied
 * verbatim from engine/src/lut2_tie728.S so the norm conversion cannot be the
 * reason the two kernels differ. */
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

	.section .iram1.dot4_tie1, "ax", @progbits
	.align	4
	.global	nd_cq_row4_tie1
	.type	nd_cq_row4_tie1,@function
nd_cq_row4_tie1:
	entry	a1, 32
	movi	a14, 0			/* permanent FP zero (this core has no  */
	wfr	f13, a14		/* movi.s: constants enter via wfr)     */
.Lgroup:
	wfr	f0, a14			/* s0..s3, the four partials            */
	wfr	f1, a14
	wfr	f2, a14
	wfr	f3, a14
	srli	a10, a7, 3		/* words in this group = g/8 (4 bits)   */
	loop	a10, .Linner_end
	l32i	a8, a2, 0
	addi	a2, a2, 4
	ee.ldf.128.ip f4, f5, f6, f7, a4, 16	/* xh[j..j+3]   */
	ee.ldf.128.ip f8, f9, f10, f11, a4, 16	/* xh[j+4..j+7] */
	extui	a9, a8, 0, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f0, f12, f4
	extui	a9, a8, 4, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f1, f12, f5
	extui	a9, a8, 8, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f2, f12, f6
	extui	a9, a8, 12, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f3, f12, f7
	extui	a9, a8, 16, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f0, f12, f8
	extui	a9, a8, 20, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f1, f12, f9
	extui	a9, a8, 24, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f2, f12, f10
	extui	a9, a8, 28, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f3, f12, f11
.Linner_end:
	add.s	f0, f0, f1		/* fold in C's order                    */
	add.s	f2, f2, f3
	add.s	f0, f0, f2
	l16ui	a9, a5, 0
	addi	a5, a5, 2
	NF16V	a9, f14, a11, a12
	madd.s	f13, f14, f0		/* acc += norm * dot                    */
	addi	a6, a6, -1
	bnez	a6, .Lgroup
	mov.s	f0, f13
	retw
	.size	nd_cq_row4_tie1, .-nd_cq_row4_tie1

	.section .iram1.dot4_tie2, "ax", @progbits
	.align	4
	.global	nd_cq_row4_tie2
	.type	nd_cq_row4_tie2,@function
/* Same kernel with ee.ldf.64.ip instead: 4 loads per word, 38 instructions
 * (-7.3 %). If the 128-bit form is microcoded into several LSU operations while
 * the 64-bit form is not, the two variants separate "wide load" from "fewer
 * issue slots", which is the question run #13 left open. */
nd_cq_row4_tie2:
	entry	a1, 32
	movi	a14, 0
	wfr	f13, a14
.Lgroup2:
	wfr	f0, a14
	wfr	f1, a14
	wfr	f2, a14
	wfr	f3, a14
	srli	a10, a7, 3
	loop	a10, .Linner_end2
	l32i	a8, a2, 0
	addi	a2, a2, 4
	ee.ldf.64.ip f4, f5, a4, 8
	ee.ldf.64.ip f6, f7, a4, 8
	ee.ldf.64.ip f8, f9, a4, 8
	ee.ldf.64.ip f10, f11, a4, 8
	extui	a9, a8, 0, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f0, f12, f4
	extui	a9, a8, 4, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f1, f12, f5
	extui	a9, a8, 8, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f2, f12, f6
	extui	a9, a8, 12, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f3, f12, f7
	extui	a9, a8, 16, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f0, f12, f8
	extui	a9, a8, 20, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f1, f12, f9
	extui	a9, a8, 24, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f2, f12, f10
	extui	a9, a8, 28, 4
	addx4	a9, a3, a9
	lsi	f12, a9, 0
	madd.s	f3, f12, f11
.Linner_end2:
	add.s	f0, f0, f1
	add.s	f2, f2, f3
	add.s	f0, f0, f2
	l16ui	a9, a5, 0
	addi	a5, a5, 2
	NF16V	a9, f14, a11, a12
	madd.s	f13, f14, f0
	addi	a6, a6, -1
	bnez	a6, .Lgroup2
	mov.s	f0, f13
	retw
	.size	nd_cq_row4_tie2, .-nd_cq_row4_tie2
