#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* Experiment 41 guard (keeps the cond generator honest).
 * Run: cc -O2 .auto/exp41/test_cond_equiv.c -lm -o /tmp/ce && /tmp/ce
 *
 * Diagnosis origin: is "two channels interleaved" bit-identical to two separate
 * per-channel reductions, compiled the same way, on the same data? */
static void shipped(const float *restrict x, const float *restrict cv, float *dst, unsigned dm, unsigned c0, unsigned c1) {
    for (unsigned ch = c0; ch < c1; ch++) { float acc = 0.0f;
        for (unsigned i = 0; i < dm; i++) acc += x[i] * cv[(size_t)i*8 + ch];
        dst[ch] = acc; }
}
static void interleaved(const float *restrict x, const float *restrict cv, float *dst, unsigned dm, unsigned c0, unsigned c1) {
    unsigned ch;
    for (ch = c0; ch + 2u <= c1; ch += 2u) { float a0 = 0.0f, a1 = 0.0f;
        for (unsigned i = 0; i < dm; i++) { a0 += x[i] * cv[(size_t)i*8+ch+0u]; a1 += x[i] * cv[(size_t)i*8+ch+1u]; }
        dst[ch+0u] = a0; dst[ch+1u] = a1; }
    for (; ch < c1; ch++) { float acc = 0.0f;
        for (unsigned i = 0; i < dm; i++) acc += x[i] * cv[(size_t)i*8+ch];
        dst[ch] = acc; }
}
int main(void) {
    unsigned dm = 768, i, t;
    float *x = malloc(sizeof(float)*dm), *cv = malloc(sizeof(float)*(size_t)dm*8);
    float a[8], b[8];
    unsigned bad = 0, first_t = -1;
    srand(7);
    for (t = 0; t < 200; t++) {
        for (i = 0; i < dm; i++) x[i] = (float)rand()/RAND_MAX - 0.5f + (t%3)*1e-6f;
        for (i = 0; i < dm*8; i++) cv[i] = (float)rand()/RAND_MAX - 0.5f;
        memset(a,0,sizeof a); memset(b,0,sizeof b);
        shipped(x, cv, a, dm, 0, 8);
        interleaved(x, cv, b, dm, 0, 8);
        if (memcmp(a, b, sizeof a)) { if (first_t == (unsigned)-1) { first_t = t;
            for (unsigned k=0;k<8;k++) if (memcmp(&a[k],&b[k],4)) printf("first case t=%u ch=%u ref=%.9g got=%.9g ulp=%d\n", t,k,a[k],b[k], *(int*)&a[k]-*(int*)&b[k]); } bad++; }
    }
    printf("CREP cases=200 mismatches=%u\n", bad);
    return bad ? 1 : 0;
}
