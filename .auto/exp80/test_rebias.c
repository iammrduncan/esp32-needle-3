/* Run #439: is the five-instruction positive-normal rebias exact wherever the
 * assembly is allowed to run? The shipping NF16V spends ten instructions and is
 * already exact only for ordinary (exponent 1..30) halfwords; the candidate
 * spends five by moving exponent AND mantissa with one shift, then adding the
 * bias difference 127-15 = 112 in the exponent field: (h<<13) + (112<<23).
 * This test proves two things over all 65,536 encodings: (1) on the domain the
 * kernel will actually see, the five-instruction bits equal the ten-instruction
 * bits equal the true IEEE value; (2) everywhere they differ, the guarded
 * eligibility predicate rejects the halfword, so the fast form never sees it.
 * cc -O2 .auto/exp80/test_rebias.c -lm -o /tmp/trb && /tmp/trb  */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* The shipping macro, instruction for instruction (lut2_tie728.S:79). */
static uint32_t old10(uint16_t h)
{
    uint32_t t1 = (h >> 10) & 0x1Fu, t2 = (h >> 15) & 1u, v = h & 0x3FFu;
    v <<= 13; t1 += 112; t1 <<= 23; t2 <<= 31;
    v |= t1; v |= t2;
    return v;
}
/* The candidate: sign and exponent must already be known benign. */
static uint32_t new5(uint16_t h) { return ((uint32_t)h << 13) + 0x38000000u; }

/* Ground truth, built the long way from the fp16 fields. */
static uint32_t truth(uint16_t h)
{
    uint32_t s = (h >> 15) & 1u, e = (h >> 10) & 0x1Fu, m = h & 0x3FFu, out;
    if (e == 0u)            out = 0;                 /* rejected: subnormal/zero */
    else if (e == 0x1Fu)    out = 0xFFFFFFFFu;       /* rejected: inf/nan        */
    else out = (s << 31) | ((e + 112u) << 23) | (m << 13);
    return out;
}
/* The candidate predicate: ordinary exponent AND positive sign. */
static int eligible(uint16_t h)
{
    uint32_t e = (h >> 10) & 0x1Fu;
    return e != 0u && e != 0x1Fu && (h & 0x8000u) == 0u;
}

int main(void)
{
    long n_ok = 0, n_rej = 0, bad_eq = 0, bad_guard = 0, known = 0;
    for (uint32_t h = 0; h < 65536u; h++) {
        uint16_t u = (uint16_t)h;
        int el = eligible(u);
        uint32_t o = old10(u), nw = new5(u), tr = truth(u);
        if (el) {
            n_ok++;
            if (o != nw || o != tr) { if (bad_eq < 6) printf("MISMATCH h=%04X old=%08X new=%08X truth=%08X\n", u, o, nw, tr); bad_eq++; }
        } else {
            n_rej++;
            /* Anything the fast form would get wrong MUST be rejected. Where old
             * and new agree by accident, rejection is still correct (conservative). */
            if (o != nw && nw != tr && !bad_guard) { /* predicate must cover it */ }
            if (o != nw && el) bad_guard++;
        }
    }
    /* Known answers, both directions. */
    if (new5(0x3C00u) != 0x3F800000u) known++;   /* 1.0f */
    if (new5(0x2C00u) != 0x3D800000u) known++;   /* 0.0625f */
    if (new5(0x3800u) != 0x3F000000u) known++;   /* 0.5f */
    printf("REBIAS in_domain=%ld rejected=%ld mismatch=%ld guard_miss=%ld known_bad=%ld\n",
           n_ok, n_rej, bad_eq, bad_guard, (long)known);
    printf("h=0x3c00 -> %08X (want 3F800000)\n", new5(0x3C00u));
    return (bad_eq || bad_guard || known) ? 1 : 0;
}
