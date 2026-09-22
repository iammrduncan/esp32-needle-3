/* Experiment 23 candidate screen: is a reciprocal-refinement division
 * (initial bit-hack guess + three Newton steps + one Markstein correction, all
 * hardware mul/fma) BIT-IDENTICAL to the correctly-rounded quotient?
 *
 * This matters because the ESP32-S3 has no hardware `div.s` - the accepted image
 * contains zero `div.s` instructions and ~8-10k calls per decode token to the
 * ROM software divider __divsf3 - so replacing that call with an FMA chain is
 * worth several percent, but only if the result is bit-identical. Correct
 * rounding makes that possible in principle; this test decides whether THIS
 * implementation achieves it, on the host, where the hardware divider is
 * correctly rounded by construction.
 *
 * Build: cc -O2 -ffp-contract=off -o /tmp/test_div .auto/divf3/test_div.c -lm
 * Exit 0 only if every class is exact.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nd_div.h"

/* The candidate is the SHIPPING header, not a copy of it: a test of a copy
 * proves nothing about what ships (this campaign has been bitten by that). */
#include "nd_div.h"
#define nd_divf nd_divf

static uint64_t s_state = 0x243F6A8885A308D3ull;
static double urnd(void)
{
    s_state ^= s_state >> 12; s_state ^= s_state << 25; s_state ^= s_state >> 27;
    return (double)((s_state * 0x2545F4914F6CDD1Dull) >> 11) / 9007199254740992.0;
}
static float rnd(void) { return (float)urnd(); }

static uint64_t g_bad;
static uint64_t g_n;
static float    g_maxrel;
static uint32_t g_bad_a, g_bad_b, g_bad_got, g_bad_ref;

static void chk(float a, float b)
{
    float ref = a / b;              /* host: correctly rounded */
    float got = nd_divf(a, b);
    g_n++;
    if (memcmp(&ref, &got, 4) != 0) {
        if (!g_bad) {
            g_bad_a = nd_f2u(a); g_bad_b = nd_f2u(b);
            g_bad_got = nd_f2u(got); g_bad_ref = nd_f2u(ref);
        }
        g_bad++;
        double d = fabs((double)got - (double)ref);
        double rel = ref != 0.0f ? d / fabs((double)ref) : d;
        if (rel > g_maxrel) g_maxrel = (float)rel;
    }
}

static void cls(const char *name, void (*gen)(unsigned, float *, float *))
{
    float a[4096], b[4096];
    uint64_t before = g_bad;
    unsigned i, rep;

    for (rep = 0; rep < 64; rep++) {
        gen(rep, a, b);
        for (i = 0; i < 4096; i++) chk(a[i], b[i]);
    }
    printf("CLASS %-16s n=%llu bad=%llu max_rel=%g\n", name,
           (unsigned long long)(g_n), (unsigned long long)(g_bad - before),
           g_bad == before ? 0.0 : (double)g_maxrel);
}

/* Model-shaped classes: the hot divisions are 1/(1+e) and e/(1+e) in the
 * sigmoid (e = exp(x), x <= 0), 1/sqrt(ss/n+eps) in RMSNorm, and generic
 * projection ratios. */
static void g_sigmoid(unsigned rep, float *a, float *b)
{
    unsigned i;
    for (i = 0; i < 4096; i++) {
        float u = (float)(urnd() * 20.0);          /* x in [-20, 0] */
        float e = expf(-u);
        b[i] = 1.0f + e;
        a[i] = (i & 1) ? e : 1.0f;
    }
    (void)rep;
}
static void g_rms(unsigned rep, float *a, float *b)
{
    unsigned i;
    (void)rep;
    for (i = 0; i < 4096; i++) {
        float ss = (float)(urnd() * 4096.0) * (float)exp2(urnd() * 20.0 - 10.0);
        b[i] = ss / 768.0f + 1e-5f;
        a[i] = 1.0f;
    }
}
static void g_uniform(unsigned rep, float *a, float *b)
{
    unsigned i;
    (void)rep;
    for (i = 0; i < 4096; i++) {
        a[i] = (float)((urnd() * 2.0 - 1.0) * exp2(urnd() * 80.0 - 40.0));
        b[i] = (float)((urnd() * 2.0 - 1.0) * exp2(urnd() * 80.0 - 40.0));
    }
}
static void g_smallint(unsigned rep, float *a, float *b)
{
    unsigned i;
    for (i = 0; i < 4096; i++) {
        a[i] = (float)((i + rep * 4096u) % 1000u + 1u);
        b[i] = (float)((3 * i + rep * 7u) % 997u + 1u);
    }
}
static void g_powers(unsigned rep, float *a, float *b)
{
    unsigned i;
    (void)rep;
    for (i = 0; i < 4096; i++) {
        int ea = (int)(urnd() * 200) - 100, eb = (int)(urnd() * 200) - 100;
        a[i] = (float)((urnd() < 0.5 ? -1.0 : 1.0) * exp2(ea) * (1.0 + urnd()));
        b[i] = (float)((urnd() < 0.5 ? -1.0 : 1.0) * exp2(eb) * (1.0 + urnd()));
    }
}
static void g_extremes(unsigned rep, float *a, float *b)
{
    static const float V[] = { 0.0f, -0.0f, 1.0f, -1.0f, 3.0f, 7.0f, 1e-38f, 1e38f,
                               1.17549435e-38f, 1.17549421e-38f, 8.507059e-45f,
                               1.4e-45f, 3.4028235e38f, 1.1920929e-7f, 0.5f, 2.0f };
    unsigned i, n = sizeof(V) / sizeof(V[0]);
    (void)rep;
    for (i = 0; i < 4096; i++) {
        a[i] = V[(unsigned)(urnd() * (n - 1))];
        b[i] = V[(unsigned)(urnd() * (n - 1))];
    }
}

int main(void)
{
    cls("sigmoid", g_sigmoid);
    cls("rms", g_rms);
    cls("uniform", g_uniform);
    cls("smallint", g_smallint);
    cls("powers", g_powers);
    cls("extremes", g_extremes);

    printf("SUM n=%llu bad=%llu max_rel=%.3e", (unsigned long long)g_n,
           (unsigned long long)g_bad, (double)g_maxrel);
    if (g_bad)
        printf(" first a=%08x b=%08x got=%08x ref=%08x", g_bad_a, g_bad_b,
               g_bad_got, g_bad_ref);
    printf("\n");
    if (g_bad == 0) { printf("RESULT bitexact_vs_host_div=1\n"); return 0; }
    printf("RESULT bitexact_vs_host_div=0\n");
    return 1;
}
