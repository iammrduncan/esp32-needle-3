/* Guard for candidate `asmemo`: the memo must agree with nd_lut2_asm_ok() itself.
 *
 * Why this exists and why host goldens are not enough: the host build compiles the engine with
 * ND_LUT2_ASM off, so `lut2_asm_usable` is never instantiated there and 19/19 byte-exact output says
 * nothing about the memo. And the safety argument is NOT "a wrong verdict only costs speed": the
 * assembly row walker and the C row walker are bit-equivalent only WHEN nd_lut2_asm_ok() is true, so
 * a stale FALSE POSITIVE (the memo says the blob is safe, the blob actually carries a norm whose
 * exponent is 0 or 31) changes arithmetic. Only a false NEGATIVE is merely slower. So this test
 * includes the CANDIDATE FILE - not a transcription, which is run #363's failure class - and calls
 * its static predicate directly over synthetic norm arrays.
 *
 * Build: see the cc line in .auto/exp77/run_guard.sh. -ffunction-sections -fdata-sections plus
 * --gc-sections discard the Xtensa row-walker entry points the engine references but this test never
 * reaches, so no Xtensa assembly is needed on the host.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ND_LUT2_ASM 1
#include "nd_quant.c"                  /* the actual candidate, statics and all */

#define CHECK(cond, what)                                                        \
    do {                                                                         \
        if (!(cond)) { printf("GUARD_FAILED %s\n", what); failures++; }          \
        else { printf("ok  %s\n", what); passes++; }                             \
    } while (0)

/* A tensor with clean norms: group 128, 32 packed bytes, 64 pairs, every exponent in
 * [1,30] so nothing is subnormal, zero, infinite or NaN. */
#define FIX_ROWS 16u
#define FIX_NG   8u
static uint16_t norms_clean[FIX_ROWS * FIX_NG];
static uint16_t norms_bad[FIX_ROWS * FIX_NG];        /* exponent 0 at one entry */
static uint16_t norms_hi[FIX_ROWS * FIX_NG];         /* exponent 31 at one entry */
static uint8_t  packed_clean[FIX_ROWS * 32];
/* 96 distinct, stable, 4-byte-aligned blob addresses, so the capacity case can present
 * more genuinely distinct keys than there are memo slots instead of 20. */
static uint8_t  blobs[96][64] __attribute__((aligned(64)));

static nd_lut2_ctx ctx_of(uint16_t *norms, uint32_t rows, uint32_t ngroup)
{
    nd_lut2_ctx c;

    memset(&c, 0, sizeof(c));
    c.g = 128u; c.gbytes = 32u; c.gpairs = 64u; c.ngroup = ngroup;
    c.packed = packed_clean;
    c.norms = norms;
    (void)rows;
    return c;
}

int main(void)
{
    int passes = 0, failures = 0;
    uint32_t i, r;
    const uint32_t rows = 8u, ngroup = 6u;

    for (i = 0; i < FIX_ROWS * FIX_NG; i++) {
        norms_clean[i] = (uint16_t)(0x3C00u + (i % 7u) * 0x800u);   /* exponents in [1,30] */
        norms_bad[i] = norms_clean[i];
        norms_hi[i] = norms_clean[i];
    }
    norms_bad[3u * ngroup + 2u] = 0x0000u;      /* exponent field 0: subnormal/zero */
    norms_hi[5u * ngroup + 1u] = (uint16_t)((norms_hi[5u * ngroup + 1u] & 0x8000u) | 0x7C00u
                                            | (norms_hi[5u * ngroup + 1u] & 0x03FFu));

    nd_cq_asmemo_reset();

    /* 1. accept path, and a repeat call on the same blob agrees (the hit) */
    nd_lut2_ctx ok = ctx_of(norms_clean, rows, ngroup);
    int first  = lut2_asm_usable(&ok, packed_clean, rows);
    int second = lut2_asm_usable(&ok, packed_clean, rows);
    CHECK(first == 1, "clean blob is accepted");
    CHECK(second == first, "repeat call returns the same verdict (memo hit)");
    CHECK(lut2_asm_usable(&ok, packed_clean, rows) == 1, "third call still accepted");

    /* 2. rejection is computed AND remembered: same blob pointer, different rows so the
     *    accepted key would miss, proving the walk itself rejects this blob. */
    nd_lut2_ctx bad = ctx_of(norms_bad, rows, ngroup);
    CHECK(lut2_asm_usable(&bad, norms_bad, rows) == 0, "blob with a zero-exponent norm is rejected");
    CHECK(lut2_asm_usable(&bad, norms_bad, rows) == 0, "cached rejection stays rejected");
    CHECK(nd_lut2_asm_ok(&bad, 0, rows) == 0, "the predicate itself rejects it (no divergence)");

    /* 3. reset then the SAME address holding different data must give the fresh verdict.
     *    This is the model-close/reopen case: the tier can hand a later open the same
     *    pointer for a different tensor, and a stale false positive would change arithmetic
     *    (the asm walker cannot see the encoding the C walker handles), so it is the only
     *    stale case that can hurt. */
    uint16_t *reuse = norms_clean;                 /* same pointer value, new contents */
    for (i = 0; i < rows * ngroup; i++) reuse[i] = norms_bad[i];
    nd_lut2_ctx after = ctx_of(reuse, rows, ngroup);
    CHECK(lut2_asm_usable(&after, packed_clean, rows) == 1,
          "without reset a reused address keeps the cached verdict (documented immutability)");
    nd_cq_asmemo_reset();
    CHECK(lut2_asm_usable(&after, packed_clean, rows) == 0,
          "after nd_cq_asmemo_reset the same address is re-walked and rejected");

    /* Restore the clean fixture: case 3 deliberately dirtied it in place. */
    for (i = 0; i < rows * ngroup; i++) norms_clean[i] = (uint16_t)(0x3C00u + (i % 7u) * 0x800u);

    /* 4. stronger key: same blob, same rows, different ngroup must NOT reuse the entry. */
    nd_cq_asmemo_reset();
    nd_lut2_ctx g6 = ctx_of(norms_clean, rows, 6u);
    nd_lut2_ctx g4 = ctx_of(norms_clean, rows, 4u);
    CHECK(nd_lut2_asm_ok(&g6, 0, rows) == 1, "ngroup=6 geometry accepted by the predicate");
    CHECK(lut2_asm_usable(&g6, packed_clean, rows) == 1, "ngroup=6 memo entry created");
    CHECK(lut2_asm_usable(&g4, packed_clean, rows) == nd_lut2_asm_ok(&g4, 0, rows),
          "different ngroup on the same blob recomputes rather than reusing");

    /* 5. geometry veto survives memoisation. */
    nd_cq_asmemo_reset();
    nd_lut2_ctx wide = ctx_of(norms_clean, rows, 6u);
    wide.g = 64u;
    CHECK(lut2_asm_usable(&wide, packed_clean, rows) == 0, "non-128 group is rejected through the memo");
    CHECK(lut2_asm_usable(&wide, packed_clean, rows) == 0, "and the rejection is cached");

    /* 6. capacity: more distinct blobs than slots must never produce a wrong verdict - the
     *    probe falls through to a recompute. 200 blobs, half of them dirty. */
    /* Rejection at the other exponent extreme (Inf/NaN encoding) too. */
    nd_cq_asmemo_reset();
    nd_lut2_ctx hi = ctx_of(norms_hi, rows, ngroup);
    CHECK(lut2_asm_usable(&hi, packed_clean, rows) == 0, "exponent-31 norm is rejected through the memo");
    CHECK(lut2_asm_usable(&hi, packed_clean, rows) == 0, "and that rejection is cached");

    /* Capacity: 96 genuinely distinct, stable keys (distinct blob addresses) over 64 slots,
     * alternating clean/reject, every verdict compared against the predicate. Rows stay inside
     * the fixture. */
    nd_cq_asmemo_reset();
    for (r = 0; r < 96u; r++) {
        uint8_t *blob = blobs[r];
        uint16_t *ns = (r % 3u == 0u) ? norms_bad : ((r % 3u == 1u) ? norms_hi : norms_clean);
        nd_lut2_ctx c = ctx_of(ns, 8u + (r % 5u), 6u);
        int memo = lut2_asm_usable(&c, blob, 8u + (r % 5u));
        int real = nd_lut2_asm_ok(&c, 0, 8u + (r % 5u));
        if (memo != real) { printf("GUARD_FAILED capacity r=%u memo=%d real=%d\n", r, memo, real); failures++; break; }
    }
    CHECK(failures == 0, "96 distinct keys over 64 slots, every verdict equals the predicate");

    printf("ASM_GUARD passes=%d failures=%d\n", passes, failures);
    return failures ? 1 : 0;
}
