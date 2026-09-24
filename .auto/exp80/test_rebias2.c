/* Oracle for the five-instruction norm rebias: does the ACTUAL eligibility
 * predicate admit exactly the halfwords the fast conversion is exact for?
 *
 * Why the real function and not a copy: the safety argument is not "a wrong
 * verdict only costs speed" - the Xtensa row walker and the C row walker agree
 * bit-for-bit only when nd_lut2_asm_ok() holds, so a halfword the new fast form
 * would misconvert must be REJECTED by the predicate that is actually compiled
 * into the shipping file. A transcription of that predicate would prove the
 * transcription (run #363's class), so this includes the candidate nd_quant.c
 * itself, with ND_LUT2_ASM on, and gc-sections drops the Xtensa walkers the host
 * cannot link. .auto/exp77/test_asmemo.c is the same pattern for the memo.
 *
 * Build:
 *   cc -O2 -ffunction-sections -fdata-sections -Iengine/include -Iengine/src \
 *      .auto/exp80/test_rebias2.c -Wl,--gc-sections -lm -o /tmp/trb2 && /tmp/trb2
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ND_LUT2_ASM 1
#include "nd_quant.c"

/* What the five-instruction form computes, and what the ten-instruction form
 * computed, as C models of the two assembled sequences. */
static uint32_t fast5(uint16_t h) { return ((uint32_t)h << 13) + 0x38000000u; }
static uint32_t old10(uint16_t h)
{
    uint32_t e = (h >> 10) & 0x1Fu, s = (h >> 15) & 1u, m = h & 0x3FFu;
    m <<= 13; e += 112; e <<= 23; s <<= 31;
    return (m | e) | s;
}
/* The value an ordinary positive halfword really has, built from its fields. */
static uint32_t truth(uint16_t h)
{
    uint32_t e = (h >> 10) & 0x1Fu, m = h & 0x3FFu;
    return ((e + 112u) << 23) | (m << 13);
}

static uint8_t  packed[64] __attribute__((aligned(16)));
static uint16_t norm;
static float    lut[512], y[1];

int main(void)
{
    nd_lut2_ctx c;
    long accepted = 0, rejected = 0, bad_domain = 0, bad_bits = 0;

    memset(packed, 0, sizeof packed);
    memset(&c, 0, sizeof c);
    c.packed = packed; c.norms = &norm; c.lut = lut; c.y = y;
    c.ngroup = 1u; c.g = 128u; c.gbytes = 32u; c.gpairs = 64u; c.rowbytes = 32u;

    for (uint32_t h = 0; h < 65536u; h++) {
        uint16_t u = (uint16_t)h;
        uint32_t e = (u >> 10) & 0x1Fu;
        int want  = (e != 0u && e != 0x1Fu && (u & 0x8000u) == 0u);
        int got   = nd_lut2_asm_ok(&c, 0u, 1u) != 0;
        norm = u;
        got  = nd_lut2_asm_ok(&c, 0u, 1u) != 0;
        if (got) accepted++; else rejected++;
        if (got != want) { if (bad_domain < 4) printf("DOMAIN h=%04X want=%d got=%d\n", u, want, got); bad_domain++; continue; }
        if (got && (fast5(u) != old10(u) || fast5(u) != truth(u))) {
            if (bad_bits < 4) printf("BITS h=%04X fast=%08X old=%08X truth=%08X\n", u, fast5(u), old10(u), truth(u));
            bad_bits++;
        }
    }
    printf("REBIAS accepted=%ld rejected=%ld domain_mismatch=%ld bits_mismatch=%ld\n",
           accepted, rejected, bad_domain, bad_bits);
    /* Argument evaluation order is unspecified and GCC evaluates right to left:
     * the first version of these controls set the negative norm inside the
     * argument list, so BOTH calls saw 0xBC00 and the positive one printed 0.
     * Sequence the assignments, then print. */
    norm = 0x3C00u; int ok_pos = nd_lut2_asm_ok(&c, 0u, 1u);
    norm = 0xBC00u; int ok_neg = nd_lut2_asm_ok(&c, 0u, 1u);
    norm = 0x0000u; int ok_zero = nd_lut2_asm_ok(&c, 0u, 1u);
    norm = 0x7C00u; int ok_inf = nd_lut2_asm_ok(&c, 0u, 1u);
    printf("controls: pos_1.0=%d(nant 1) negative=%d zero=%d inf=%d bits(0x3c00)=%08X want 3F800000\n",
           ok_pos, ok_neg, ok_zero, ok_inf, 0x3F800000u);
    if (!(ok_pos == 1 && ok_neg == 0 && ok_zero == 0 && ok_inf == 0 && fast5(0x3C00u) == 0x3F800000u)) {
        printf("CONTROLS_FAILED\n"); return 2;
    }
    return (bad_domain || bad_bits) ? 1 : 0;
}
