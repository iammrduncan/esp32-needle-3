/* Bit-equivalence guard for the Sinkhorn exponential pairing (#exp70): the shipped one-term-at-a-
 * time log-sum-exp versus computing an adjacent pair with nd_expf_pair and adding the partials in
 * the SAME sequential order. The sum's bits must be identical, including which term takes the exact
 * exp(0) literal, so any mismatch is a transcription bug rather than a rounding question. */
#include <stdio.h>
#include <stdint.h>
#include "nd_quant.h"

static float sum_shipped(const float *v, uint32_t n, float mx, uint32_t stride, uint32_t step)
{
    float sum = 0.0f;
    for (uint32_t j = 0; j < n; j++) {
        float d = v[j * step] - mx;
        sum += (d == 0.0f) ? 1.0f : nd_expf(d);
    }
    (void)stride;
    return sum;
}

static float sum_paired(const float *v, uint32_t n, float mx, uint32_t stride, uint32_t step)
{
    float sum = 0.0f;
    (void)stride;
    /* Exactly the generator's loop shape: the shipped `for (j = 0; j < n; j++)` header, an extra
     * j++ inside the paired branch, and the fallback handling only the first element so the second
     * is reconsidered on the next iteration. */
    for (uint32_t j = 0; j < n; j++) {
        float d0 = v[j * step] - mx, d1 = v[(j + 1u) * step] - mx;
        if (j + 1u < n && d0 != 0.0f && d1 != 0.0f) {
            float e0, e1;
            nd_expf_pair(d0, d1, &e0, &e1);
            sum += e0;
            sum += e1;
            j++;
        } else {
            sum += (d0 == 0.0f) ? 1.0f : nd_expf(d0);
        }
    }
    return sum;
}

int main(void)
{
    uint32_t bad = 0, cmp = 0;
    uint32_t seed = 0x12345u;
    float v[16];
    for (unsigned trial = 0; trial < 60000u; trial++) {
        for (unsigned k = 0; k < 16u; k++) {
            seed = seed * 1103515245u + 12345u;
            /* a log-domain matrix: differences span the clamp, and exact zeros occur
             * because the maximum is one of the elements, as in the shipped kernel */
            v[k] = ((float)(seed >> 8) / 8388608.0f) - 60.0f;
        }
        for (uint32_t n = 2u; n <= 4u; n += 2u)
            for (uint32_t step = 1u; step <= 4u; step++) {
                float mx = v[0];
                for (uint32_t j = 0; j < n; j++)
                    if (v[j * step] > mx) mx = v[j * step];
                if (trial % 7u == 0u) v[(trial % n) * step] = mx;      /* force the exact-zero term */
                float a = sum_shipped(v, n, mx, 1u, step);
                float b = sum_paired(v, n, mx, 1u, step);
                if (!(a == b && (a == 0.0f || (a > 0.0f) == (b > 0.0f)) ||
                      *(uint32_t *)&a == *(uint32_t *)&b)) {
                    if (bad < 3) printf("MISMATCH n=%u step=%u trial=%u %.9g vs %.9g\n", n, step, trial, a, b);
                    bad++;
                }
                cmp++;
            }
    }
    printf("SINKPAIR_EQUIV comparisons=%u mismatches=%u %s\n", cmp, bad, bad ? "FAIL" : "OK");
    return bad != 0;
}
