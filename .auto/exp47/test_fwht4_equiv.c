/* nd_fwht4s (radix-4 fused stages) must equal the exported nd_fwht + the same
 * scale, for EVERY power-of-two n, not just the model's g == 128.
 *
 * Why this exists: the field only ever calls the triple walk with g == 128, so the
 * fused loop's tail (`if (len < n/2)`) and the g < 8 dispatch are unreachable there.
 * Run #363's defect was exactly a stage that the shipping geometry never exercised
 * being wrong, and the golden suite could not see it. Reference is the exported
 * nd_fwht - the same oracle that proved run #388's fold over 184,800 comparisons.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nd_quant.c"

/* The candidate's walk name changes across the family (nd_fwht4s, nd_fwht4w, ...).
 * Same oracle, any variant: -DND_CAND=nd_fwht4w */
#ifndef ND_CAND
#define ND_CAND nd_fwht4s
#endif

/* How many group blocks the candidate walks at once (3 = the shipped triple walk).
 * Without this the oracle could only ever prove a 3-block kernel, which would have
 * silently blocked the width-curve candidates (4-wide, and radix-8 at 2 and 3
 * blocks). Reference is always the exported nd_fwht applied to each block. */
#ifndef ND_CAND_BLOCKS
#define ND_CAND_BLOCKS 3
#endif

#if ND_CAND_BLOCKS == 2
#define CALL_CAND(p, n, sc) ND_CAND((p), (p) + (n), (n), (sc))
#elif ND_CAND_BLOCKS == 4
#define CALL_CAND(p, n, sc) ND_CAND((p), (p) + (n), (p) + 2 * (n), (p) + 3 * (n), (n), (sc))
#else
#define CALL_CAND(p, n, sc) ND_CAND((p), (p) + (n), (p) + 2 * (n), (n), (sc))
#endif

int main(void)
{
    unsigned ns[] = {2, 4, 8, 16, 32, 64, 128, 256, 1024};
    uint64_t st = 0x9E3779B97F4A7C15ull;
    long cmp = 0, bad = 0;
    float *a = malloc(sizeof(float) * ND_CAND_BLOCKS * 1024u),
         *b = malloc(sizeof(float) * ND_CAND_BLOCKS * 1024u);

    for (unsigned i = 0; i < sizeof(ns) / sizeof(ns[0]); i++) {
        uint32_t n = ns[i];
        float scales[] = {0.0f, 1.0f, 1.0f / sqrtf((float)n), 0.375f, -2.0f};
        for (unsigned si = 0; si < sizeof(scales) / sizeof(scales[0]); si++) {
            float scale = scales[si];
            for (int rep = 0; rep < 64; rep++) {
                for (uint32_t k = 0; k < ND_CAND_BLOCKS * n; k++) {
                    st = st * 6364136223846793005ull + 1442695040888963407ull;
                    /* Values that stress cancellation: sums that land near zero. */
                    a[k] = (float)((int32_t)(st >> 40) % 2001 - 1000) * 0.001f;
                }
                if (n == 2u) a[0] = -a[1];                       /* exact cancellation */
                memcpy(b, a, sizeof(float) * ND_CAND_BLOCKS * n);

                CALL_CAND(b, n, scale);                        /* candidate */
                for (uint32_t gg = 0; gg < ND_CAND_BLOCKS; gg++) {            /* reference */
                    nd_fwht(a + (size_t)gg * n, n);
                    for (uint32_t j = 0; j < n; j++) a[(size_t)gg * n + j] *= scale;
                }
                for (uint32_t k = 0; k < ND_CAND_BLOCKS * n; k++) {
                    float u = b[k], v = a[k];
                    cmp++;
                    if (!(u == v || (isnan(u) && isnan(v)))) bad++;
                }
            }
        }
    }
    printf("FWHT4_EQUIV comparisons=%ld mismatches=%ld n_listed=%zu\n", cmp, bad,
           sizeof(ns) / sizeof(ns[0]));
    printf("VERDICT: %s\n", bad ? "NOT BIT-EXACT - do not flash" : "bit-exact vs exported nd_fwht over every n");
    return bad != 0;
}
