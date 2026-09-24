/* Guard for the first-W8D seed in nd_lut2_rows_tie1n (run #577, +0.60/+0.63 % on two
 * boards). The kernel stops resetting f0..f3 with mov.s from the +0.0 register and
 * instead makes the first four adds of a group read add.s f0, f6, fN with f6 = +0.0f.
 *
 * That is only bit-exact if +0.0 + x == x for every value the accumulator can hold.
 * Running this test FOUND the exception classes rather than assuming them (the first
 * version claimed only -0.0 and failed): IEEE-754 add differs from a move in exactly two
 * cases - x = -0.0 (sign of zero) and any SIGNALING NaN (an addition quiets its operand,
 * so ffbffe12 becomes fffffe12). Both are unreachable for the operand this seed actually
 * sees: the first add of a group consumes a pair-LUT entry, i.e. a product of a finite
 * codebook value and a finite prepared activation value, which is finite, +0.0 or a
 * QUIET NaN - hardware arithmetic never emits a signaling NaN - and a -0.0 partial would
 * need every term of that partial to be -0.0. Device byte-exactness on two boards over
 * all 20 frozen cases is the empirical half of the same claim.
 *
 * The host golden gate cannot be the only evidence here: the host builds the C fallback,
 * so no host run can see an asm defect (measured in #595/#598, where only the device
 * caught a deliberate tie1n mis-seed). Hence a C-level characterisation of the
 * transformation, next to the device result.
 *
 * cc -O2 -ffp-contract=off .auto/exp87/test_seed_equiv.c -lm -o /tmp/seed && /tmp/seed
 */
#include <stdint.h>
#include <stdio.h>

static uint32_t u(float f) { uint32_t r; __builtin_memcpy(&r, &f, 4); return r; }
static float    f(uint32_t x) { float r; __builtin_memcpy(&r, &x, 4); return r; }

static long checked, mismatch, neg_zero, signaling_nan;

static int is_snan(uint32_t w) {          /* exponent all-ones, mantissa != 0, quiet bit clear */
    return (w & 0x7f800000u) == 0x7f800000u && (w & 0x007fffffu) && !(w & 0x00400000u);
}

static void check(float x) {
    const float kZero = 0.0f;          /* the kernel's dedicated +0.0 register (f6) */
    float seeded = kZero + x;          /* what add.s f0, f6, fN computes          */
    float reset  = x;                  /* what mov.s f0, fN after a reset gives   */
    ++checked;
    if (u(seeded) != u(reset)) {
        ++mismatch;
        if (u(x) == 0x80000000u) ++neg_zero;               /* -0.0 */
        else if (is_snan(u(x))) ++signaling_nan;           /* add quiets sNaN */
        else printf("UNEXPECTED seed difference at x=%08x (%g): %08x vs %08x\n",
                    u(x), x, u(seeded), u(reset));
    }
}

int main(void) {
    /* Every special and boundary that a partial accumulator could ever hold. */
    const uint32_t specials[] = {
        0x00000000u, 0x80000000u,                 /* +0.0, -0.0 */
        0x007fffffu, 0x807fffffu,                 /* largest subnormals */
        0x00800000u, 0x80800000u,                 /* smallest normals */
        0x7f7fffffu, 0xff7fffffu,                 /* largest finite */
        0x7f800000u, 0xff800000u, 0x7fc00000u,    /* +-inf, NaN */
        0x3f800000u, 0xbf800000u, 0x4b7fffffu,    /* 1.0, -1.0, ~2^23 */
    };
    for (unsigned i = 0; i < sizeof specials / sizeof *specials; ++i) check(f(specials[i]));

    /* Values with at most 4 significant bits over the whole exponent range: these are
     * what a small number of summed table entries actually look like. */
    for (uint32_t e = 0; e < 255u; ++e)
        for (uint32_t mant = 0; mant < 16u; ++mant)
            for (unsigned s = 0; s < 2u; ++s) {
                uint32_t mb = (e << 23) | (mant << (23 - 4));
                check(f(mb | (s << 31)));
                check(f(mb | (s << 31)) * 1.0f);   /* also as a computed summand */
            }

    /* A dense stride over the whole 32-bit pattern space, skipping nothing but cost. */
    for (uint64_t i = 0; i < 0x100000000ull; i += 1031u) check(f((uint32_t)i));

    printf("checked=%ld seed_vs_reset_differences=%ld negative_zero=%ld signaling_nan=%ld\n",
           checked, mismatch, neg_zero, signaling_nan);
    if (neg_zero + signaling_nan != mismatch) {
        puts("FAIL: a seed difference exists outside {-0.0, signaling NaN}");
        return 1;
    }
    if (neg_zero == 0 || signaling_nan == 0) {
        puts("FAIL: sweep lost its positive controls (-0.0 and an sNaN must both appear)");
        return 1;
    }
    puts("OK: seed-vs-reset differs ONLY for -0.0 and signaling NaNs, neither of which a");
    puts("    pair-LUT entry can be (products of finite operands are finite, +0.0, or a");
    puts("    QUIET NaN; hardware arithmetic never emits an sNaN).");
    return 0;
}
