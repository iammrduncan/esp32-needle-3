/* kb_wide.S - kbench-only wide-load experiment (mentor-corrected #333 reopen).
 * nd_kb_widetest: stores back the 4 floats loaded by one ee.ldf.128.ip so the
 * register FILL ORDER is measured empirically instead of assumed (the #333
 * closure rested on a broken probe; the dot4 body listed f4,f5,f6,f7 against
 * Espressif's documented f7,f6,f5,f4).
 * nd_kb_gemv4_wide: multirow tie1 body with the 8 xh loads replaced by two
 * 128-bit loads in the order proven by the store-back probe (f7,f6,f5,f4 and
 * f11,f10,f9,f8 - i.e. LOWEST address lands in the LAST named register).
 * Requires 16-byte-aligned xh; the caller checks alignment. Arithmetic and
 * every per-partial chain are unchanged from nd_gemv4_rows_tie1.
 */
	.section .text
	.align	4
	.global	nd_kb_widetest
	.type	nd_kb_widetest,@function
nd_kb_widetest:	/* void nd_kb_widetest(float *p) : in-place ldf+stf */
	entry	a1, 32
	ee.ldf.128.ip	f7, f6, f5, f4, a2, 16
	ee.stf.128.ip	f4, f5, f6, f7, a2, -16
	retw
	.size	nd_kb_widetest, .-nd_kb_widetest

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

	.align	4
	.global	nd_kb_gemv4_wide
	.type	nd_kb_gemv4_wide,@function
nd_kb_gemv4_wide:
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
	.size	nd_kb_gemv4_wide, .-nd_kb_gemv4_wide
