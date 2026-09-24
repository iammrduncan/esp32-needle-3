/* Bit-equivalence guard for condT+cond2: the shipped strided one-chain loop versus the staged
 * channel-major two-chain loop, over random d_model x 8 cases. Each accumulator must see only its
 * own channel's terms in ascending row order, so any mismatch is a transcription bug (the class
 * that cost run #380 a board: a literal channel offset inside a loop over channels). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static void shipped(float *dst, const float *x, const float *cv, unsigned dm)
{
    for (unsigned ch = 0; ch < 8u; ch++) {
        float acc = 0.0f;
        for (unsigned i = 0; i < dm; i++) acc += x[i] * cv[(size_t)i * 8 + ch];
        dst[ch] = acc;
    }
}
static void staged(float *dst, const float *x, const float *cv, unsigned dm)
{
    float *tp = malloc((size_t)dm * 8u * sizeof(float));
    for (unsigned ch = 0; ch < 8u; ch++)
        for (unsigned i = 0; i < dm; i++) tp[(size_t)ch * dm + i] = cv[(size_t)i * 8u + ch];
    unsigned ch = 0;
    for (; ch + 1u < 8u; ch += 2u) {
        const float *cva = tp + (size_t)ch * dm, *cvb = cva + dm;
        float a = 0.0f, b = 0.0f;
        for (unsigned i = 0; i < dm; i++) { float xi = x[i]; a += xi * cva[i]; b += xi * cvb[i]; }
        dst[ch] = a; dst[ch + 1u] = b;
    }
    if (ch < 8u) { const float *cva = tp + (size_t)ch * dm; float a = 0.0f;
        for (unsigned i = 0; i < dm; i++) a += x[i] * cva[i]; dst[ch] = a; }
    free(tp);
}
int main(void)
{
    unsigned dm = 768, cases = 200, bad = 0;
    float *x = malloc(dm * sizeof(float)), *cv = malloc((size_t)dm * 8 * sizeof(float));
    float a[8], b[8];
    for (unsigned c = 0; c < cases; c++) {
        for (unsigned i = 0; i < dm; i++) x[i] = (float)((i * 37 + c * 11) % 991) * 1.7e-3f - 0.8f;
        for (size_t i = 0; i < (size_t)dm * 8; i++)
            cv[i] = (float)((i * 91 + c * 7) % 1013) * 2.3e-3f - 1.1f;
        memset(a, 0, sizeof a); memset(b, 0, sizeof b);
        shipped(a, x, cv, dm); staged(b, x, cv, dm);
        for (unsigned ch = 0; ch < 8u; ch++)
            if (memcmp(&a[ch], &b[ch], sizeof(float))) { if (bad < 3) printf("case %u ch %u %.9g vs %.9g\n", c, ch, a[ch], b[ch]); bad++; }
    }
    printf("COND2_EQUIV cases=%u dm=%u mismatches=%u %s\n", cases, dm, bad, bad ? "FAIL" : "OK");
    return bad != 0;
}
