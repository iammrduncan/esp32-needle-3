/* #B2 guard: nd_expf_pair_attn (nonpositive-domain specialization) must be
 * bit-identical to the SHIPPED nd_expf_pair — include the actual header, no
 * transcription. Sweeps the whole attention-reachable domain plus every
 * boundary the fallback could mishandle. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "nd_quant.h"

static int fails = 0;
static void chk(float x0, float x1)
{
    float a0, a1, b0, b1;
    nd_expf_pair(x0, x1, &a0, &a1);
    nd_expf_pair_attn(x0, x1, &b0, &b1);
    if (memcmp(&a0, &b0, 4) || memcmp(&a1, &b1, 4)) {
        if (fails < 8) printf("MISMATCH x=(%g,%g) ship=(%a,%a) attn=(%a,%a)\n",
                              x0, x1, a0, a1, b0, b1);
        fails++;
    }
}
int main(void)
{
    long n = 0;
    /* dense sweep of the in-domain square */
    for (long i = -200000; i <= 0; i++) {
        float x0 = (float)i * 0.0005f;
        for (int j = 0; j < 64; j++) {
            float x1 = (float)(i + j * 131) * 0.0005f;
            if (x1 > 0.0f) x1 = 0.0f;
            chk(x0, x1); n += 2;
        }
    }
    /* boundaries: signed zero, exact -88 and neighbours, halves, huge, inf, NaN, tiny */
    float pts[] = { 0.0f, -0.0f, -88.0f, -87.99999f, -88.00001f, -88.0078125f,
                    -0.5f, 0.5f, -0.25f, -1.0f, -1e-30f, -1e30f,
                    -INFINITY, INFINITY, NAN, -87.68f, -12.6f, -2.42826e-8f };
    unsigned np = sizeof pts / sizeof *pts;
    for (unsigned a = 0; a < np; a++)
        for (unsigned b = 0; b < np; b++) { chk(pts[a], pts[b]); chk(pts[b], pts[a]); n += 2; }
    /* half-integer k transitions across the whole exponent range */
    for (int k = -126; k <= 0; k++) {
        float z = (float)k, eps = 1.0f;
        float xs = (float)((double)k / 1.44269504);
        for (float d = -0.15f; d <= 0.15f; d += 0.001f) chk(xs + d, xs - d);
        (void)z; (void)eps;
    }
    printf("EXPPAIR_NP cases=%ld fails=%d\n", n, fails);
    return fails != 0;
}
