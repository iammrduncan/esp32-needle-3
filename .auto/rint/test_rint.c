/* Rule #1 after the nd_expf exponent-field bug: differential a numeric primitive
 * against the SHIPPED expression copied verbatim, over a dense sweep that includes
 * every tie, not against a model of it.
 *
 * The candidate removes a flash-mapped lrintf CALL per element. This part has no
 * rounding instruction (run #368 measured frint.* / itrunc.s / quou.s all rejected)
 * but it DOES have `trunc.s`, the float->int truncate that GCC emits for (int)x --
 * found by compiling `(int)x` and disassembling, not by guessing mnemonics. So the
 * rounding is done in FLOAT, by adding MAGIC = 1.5*2^23, whose ulp at that
 * magnitude is exactly 1.0: the hardware's own round-to-nearest-even then produces
 * MAGIC + rint(q), and the integer is read out with trunc.s and MAGIC subtracted in
 * integer arithmetic. Ties go to even because the ADD rounds, so this is not the
 * forbidden naive +0.5f-and-truncate (that is round-half-away, rejected at run #2e).
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAGIC 12582912.0f          /* 1.5f * 8388608.0f == 0x4B400000 */

/* The shipped line, verbatim from engine/src/nd_model.c kv_store_int8(). */
static int8_t ref_row(float q) { return (int8_t)lrintf(q); }

/* The candidate, in the form it will be written in the engine. The subtract is
 * INTEGER so the compiler cannot reassociate it back into a float add. */
static int8_t cand_row(float q)
{
    if (q >= -127.0f && q <= 127.0f) {          /* finite and in range */
        float t = q + MAGIC;
        return (int8_t)((int)t - 12582912);
    }
    return (int8_t)lrintf(q);                    /* NaN / out of range: shipped path */
}

static uint32_t bad = 0, n = 0;
static float worst_a = 0.0f, worst_b = 0.0f;
static int8_t ref_bad = 0, cand_bad = 0;

static void one(float q)
{
    int8_t a = ref_row(q), b = cand_row(q);
    n++;
    if (memcmp(&a, &b, 1) != 0) {
        if (bad < 5) printf("MISMATCH q=%.9g ref=%d cand=%d\n", (double)q, a, b);
        bad++;
    }
}

int main(void)
{
    long i;  int k;
    /* Every int8 value, dense around every .5 tie, and both signs. */
    for (k = -130; k <= 130; k++) {
        float c = (float)k + 0.5f;
        one((float)k); one(-((float)k));
        one(c); one(-c);
        one(c - 1e-6f); one(c + 1e-6f);
        one((float)k + 0.4999999f); one((float)k - 0.4999999f);
    }
    /* Dense sweep at ~1e5 points across the whole usable range plus sub-far-ties. */
    for (i = -60000; i <= 60000; i++) {
        one((float)i * (1.0f / 500.0f));
        one((float)i * (1.0f / 65536.0f));
    }
    /* Values the Markstein fixup actually produces: exactly representable integers,
     * and integers plus one ulp of 1.0 magnitude. */
    for (i = -127; i <= 127; i++) {
        float v = (float)i;
        one(nextafterf(v, INFINITY)); one(nextafterf(v, -INFINITY));
        one(nextafterf(v, INFINITY) * 0.9999f);
    }
    /* Pathological classes must take the fallback and stay bit-identical. */
    one(0.0f); one(-0.0f); one(INFINITY); one(-INFINITY);
    one(NAN); one(-NAN); one(1.0f / 0.0f);
    one(1e-45f);
    printf("rint_diff n=%u mismatches=%u\n", n, bad);
    return bad != 0;
}
