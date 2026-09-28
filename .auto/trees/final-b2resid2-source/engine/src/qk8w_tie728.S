/* ESP32-S3 (TIE728): wide-load paired-head QK dot, ESP-IDF GNU as.
 *
 * void nd_qk8w4(const float *qhA, const float *qhB,
 *               const float *kf0,  const float *kf1,
 *               float *out4, uint32_t ndim)          (ndim % 4 == 0)
 *   out4 = { qhA.kf0, qhA.kf1, qhB.kf0, qhB.kf1 } over the first ndim floats.
 *
 * WHY: run #E51 measured this loop shape (operand rows wide-loaded with
 * ee.ldf.128.ip, four independent madd chains) bit-exact and 38 % faster than
 * the lsi form the shipped DOT8W body compiles to. Products and per-accumulator
 * order match DOT8W term for term - ascending 4-float chunks, one product per
 * chain per chunk - so results are bit-identical BY CONSTRUCTION: loads cannot
 * reassociate. All four rows must be 16-byte aligned (caller-gated).
 *
 * Register plan (16 FPU regs, zero loop-body stack traffic; qB reuses the qA
 * registers after the s0/s1 products, exactly as DOT8W's CSE shares k loads):
 *   f0..f3  = s0, s1, s0b, s1b ; f4..f7 = kf0 chunk ; f8..f11 = kf1 chunk
 *   f12..f15 = row chunk (qhA then qhB). a2..a5 cursors, a6 out, a7 counter.
 * Register-list order is the E50/E51-proven one: written high-to-low, the
 * LAST-named register receives the LOWEST address (f7 = chunk element 0).
 */
	.text
	.align	4
	.global	nd_qk8w4
	.type	nd_qk8w4,@function
nd_qk8w4:
	entry	a1, 32
	movi	a15, 0
	wfr	f0, a15
	wfr	f1, a15
	wfr	f2, a15
	wfr	f3, a15
	srli	a7, a11, 2
.Lq4:
	ee.ldf.128.ip	f7,  f6,  f5,  f4,  a4, 16
	ee.ldf.128.ip	f11, f10, f9,  f8,  a5, 16
	lsi	f12, a2, 0
	lsi	f13, a2, 4
	lsi	f14, a2, 8
	lsi	f15, a2, 12
	addi	a2, a2, 16
	madd.s	f0, f12, f4
	madd.s	f1, f12, f8
	madd.s	f0, f13, f5
	madd.s	f1, f13, f9
	madd.s	f0, f14, f6
	madd.s	f1, f14, f10
	madd.s	f0, f15, f7
	madd.s	f1, f15, f11
	lsi	f12, a3, 0
	lsi	f13, a3, 4
	lsi	f14, a3, 8
	lsi	f15, a3, 12
	addi	a3, a3, 16
	madd.s	f2, f12, f4
	madd.s	f3, f12, f8
	madd.s	f2, f13, f5
	madd.s	f3, f13, f9
	madd.s	f2, f14, f6
	madd.s	f3, f14, f10
	madd.s	f2, f15, f7
	madd.s	f3, f15, f11
	addi	a7, a7, -1
	bnez	a7, .Lq4
	ssi	f0, a6, 0
	ssi	f1, a6, 4
	ssi	f2, a6, 8
	ssi	f3, a6, 12
	retw
	.size	nd_qk8w4, .-nd_qk8w4
