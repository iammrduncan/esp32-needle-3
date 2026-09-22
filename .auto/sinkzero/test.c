/* Rule #1 test: the new sinkhorn must be BIT-identical to the shipped one, which is
 * copied here verbatim from engine/src/nd_model.c (runs #230-231 established that the
 * byte-exact goldens cannot see a numeric bug in an input range the frozen prompts
 * never reach). The only change is that the term whose argument is exactly zero - the
 * row/column maximum - contributes the literal 1.0f instead of nd_expf(0.0f). */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nd_quant.h"

#define ND_SINKHORN 20

static void sinkhorn_old(float *a, uint32_t n)
{
    uint32_t it, i, j;
    for (it = 0; it < ND_SINKHORN; it++) {
        for (i = 0; i < n; i++) {
            float mx = a[i * n];
            float sum = 0.0f;
            for (j = 1; j < n; j++)
                if (a[i * n + j] > mx) mx = a[i * n + j];
            for (j = 0; j < n; j++)
                sum += nd_expf(a[i * n + j] - mx);
            { float lse = mx + logf(sum);
              for (j = 0; j < n; j++) a[i * n + j] -= lse; }
        }
        for (j = 0; j < n; j++) {
            float mx = a[j];
            float sum = 0.0f;
            for (i = 1; i < n; i++)
                if (a[i * n + j] > mx) mx = a[i * n + j];
            for (i = 0; i < n; i++)
                sum += nd_expf(a[i * n + j] - mx);
            { float lse = mx + logf(sum);
              for (i = 0; i < n; i++) a[i * n + j] -= lse; }
        }
    }
    for (i = 0; i < n * n; i++) a[i] = nd_expf(a[i]);
}

static void sinkhorn_new(float *a, uint32_t n)
{
    uint32_t it, i, j;
    for (it = 0; it < ND_SINKHORN; it++) {
        for (i = 0; i < n; i++) {
            float mx = a[i * n];
            float sum = 0.0f;
            for (j = 1; j < n; j++)
                if (a[i * n + j] > mx) mx = a[i * n + j];
            for (j = 0; j < n; j++) {
                float d = a[i * n + j] - mx;
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);
            }
            { float lse = mx + logf(sum);
              for (j = 0; j < n; j++) a[i * n + j] -= lse; }
        }
        for (j = 0; j < n; j++) {
            float mx = a[j];
            float sum = 0.0f;
            for (i = 1; i < n; i++)
                if (a[i * n + j] > mx) mx = a[i * n + j];
            for (i = 0; i < n; i++) {
                float d = a[i * n + j] - mx;
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);
            }
            { float lse = mx + logf(sum);
              for (i = 0; i < n; i++) a[i * n + j] -= lse; }
        }
    }
    for (i = 0; i < n * n; i++) a[i] = nd_expf(a[i]);
}

static unsigned long st = 0x243F6A8885A308D3ull;
static float frnd(void){ st = st*6364136223846793005ull + 1442695040888963407ull;
  return (float)((double)(int)(uint32_t)(st>>32))/2147483648.0; }

int main(void)
{
    long bad = 0, n_cases = 0, zeros = 0;
    /* Precondition first: nd_expf of both signed zeros must be exactly 1.0f. */
    if (memcmp(&(float){nd_expf(0.0f)},  &(float){1.0f}, 4) ||
        memcmp(&(float){nd_expf(-0.0f)}, &(float){1.0f}, 4)) { printf("nd_expf(+/-0) != 1.0f\n"); return 1; }
    printf("precondition ok: nd_expf(+0.0f) and nd_expf(-0.0f) are exactly 1.0f\n");

    for (long c = 0; c < 200000; c++) {
        float a[16], b[16];
        uint32_t n = 4;
        for (unsigned k = 0; k < n*n; k++) {
            float v;
            switch (c % 5) {
                case 0: v = frnd() * 8.0f - 4.0f; break;             /* typical log-res  */
                case 1: v = frnd() * 200.0f - 100.0f; break;          /* wide, clamped    */
                case 2: v = (k % 4) * 0.0f; break;                    /* all equal        */
                case 3: v = frnd() * 1e-6f; break;                    /* near-tied max    */
                default: v = (k % 4 == 0) ? 3.0f : -1e30f; break;     /* one dominant     */
            }
            a[k] = v;
        }
        memcpy(b, a, sizeof a);
        sinkhorn_old(a, n); sinkhorn_new(b, n);
        n_cases += n*n;
        if (memcmp(a, b, sizeof a)) { bad++; if (bad < 4) printf("MISMATCH case %ld\n", c); }
    }
    /* every exact-zero argument the guard can catch really yields 1.0f */
    for (float x = -30.0f; x <= 30.0f; x += 0.25f)
        for (unsigned k = 0; k < 4; k++) {
            float d = x - x;   /* exactly zero, as the max term is */
            float one = (d == 0.0f) ? 1.0f : nd_expf(d);
            float ship = nd_expf(x - x);
            zeros++; if (memcmp(&one, &ship, 4)) bad++;
        }
    printf("cases=%ld elements=%ld zero-arg probes=%ld bit_mismatches=%ld\n",
           (long)200000, n_cases, zeros, bad);
    return bad != 0;
}
