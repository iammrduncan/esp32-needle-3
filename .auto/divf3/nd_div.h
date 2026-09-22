/* nd_div.h - correctly-rounded single-precision division for a core that has no
 * hardware divider.
 *
 * MEASURED FACT about this target, not an assumption: the accepted shipping
 * image contains ZERO `div.s` instructions (xtensa-esp32s3-elf-objdump over
 * every byte of .iram.text and .flash.text), and resolving its indirect calls
 * against IDF's own esp_rom/esp32s3 symbol maps shows ~8-10k calls per decode
 * token to the ROM software divider __divsf3 (0x40002274). Two of them come
 * from every sigmoid pair alone (`1/(1+e)` and `e/(1+e)`, ~3,600 pairs/token),
 * the rest from RMSNorm's `1/sqrtf(ss/n+eps)`, the confidence pool, KV storage
 * and the attention normalisation.
 *
 * A reciprocal-refinement divide is therefore worth real decode time, and it is
 * admissible for a byte-exact campaign because IEEE-754 division is *specified*
 * to be correctly rounded: there is exactly one right answer, so a faster
 * implementation that is correctly rounded returns identical bits. The fast path
 * below is the standard construction - approximate reciprocal, Newton iterations
 * that each double the precision, one quotient multiply, the exact residual
 * computed with fma, and Markstein's correction - which is provably the
 * correctly rounded quotient once the reciprocal is accurate to about an ulp and
 * nothing overflows.
 *
 * The guard is what makes it safe rather than clever: the fast path runs only
 * when both operands are finite, non-zero, normal, and the quotient's exponent
 * lands well inside the normal range. EVERYTHING else returns `a / b`, i.e. the
 * division the shipping build already performs, so subnormals, zeros, infinities,
 * NaNs (with their exact payload), overflow and underflow keep the current
 * implementation bit-for-bit. That is deliberate: reimplementing the exceptional
 * classes would be a second place where rounding could disagree, and the model
 * does not pay for them.
 *
 * Verified bit-for-bit against the host's correctly-rounded hardware divider over
 * the model's real operand shapes plus adversarial and exhaustive classes in
 * .auto/divf3/test_div.c (that test includes THIS header, so what it proves is
 * what ships). */
#ifndef ND_DIV_H
#define ND_DIV_H

#include <stdint.h>

static inline uint32_t nd_f2u(float f) { uint32_t u; __builtin_memcpy(&u, &f, 4); return u; }
static inline float    nd_u2f(uint32_t u) { float f; __builtin_memcpy(&f, &u, 4); return f; }

static inline float nd_divf(float a, float b)
{
    uint32_t ua = nd_f2u(a) & 0x7FFFFFFFu;
    uint32_t ub = nd_f2u(b) & 0x7FFFFFFFu;
    int      ea = (int)(ua >> 23);
    int      eb = (int)(ub >> 23);

    /* Both normal (exponent field 1..253, so no zero/subnormal/inf/NaN) and the
     * quotient normal with margin. Nine integer instructions. */
    if (ea >= 1 && ea <= 253 && eb >= 1 && eb <= 253) {
        int x = ea - eb + 127;
        if (x >= 2 && x <= 253) {
            uint32_t bu = ub;
            /* Magic-subtract inverse of |b|: about 3 % relative error, which the
             * three refinements below turn into far better than a float ulp. */
            float r = nd_u2f(0x7EF311C3u - bu);
            float q, e;

            r = nd_u2f(nd_f2u(r) ^ (nd_f2u(b) & 0x80000000u));   /* sign of b */
            r = __builtin_fmaf(r, __builtin_fmaf(-b, r, 1.0f), r);   /* ~1e-3  */
            r = __builtin_fmaf(r, __builtin_fmaf(-b, r, 1.0f), r);   /* ~1e-6  */
            r = __builtin_fmaf(r, __builtin_fmaf(-b, r, 1.0f), r);   /* ~1e-12 */
            q = a * r;
            e = __builtin_fmaf(-b, q, a);       /* exact residual of a - b*q */
            return __builtin_fmaf(e, r, q);     /* correct to within rounding */
        }
    }
    return a / b;       /* the shipping software divider, unchanged */
}

#endif /* ND_DIV_H */
