#!/usr/bin/env python3
"""Candidate `asmemo`: remember the immutable 2-bit assembly verdict per tensor, not per call.

`nd_lut2_asm_ok()` walks EVERY group norm of the row range - by design, because the specialised
kernel cannot see the subnormal and inf/nan encodings that `nd_f16()`'s inline path mishandles - and
the 2-bit projection wrapper remembered only ONE (blob, rows) verdict, a design unchanged since run
#137. A decode token touches 44 distinct field blobs (q/k/v/gate/out_proj per layer plus the engram
projections), one per call, so the cache missed on nearly every call and re-walked 130,560 immutable
PSRAM halfwords per token on the calling core, before any row is dispatched. The kbench screens that
priced the kernel chose their function pointer *outside* the timing loop, so this wrapper cost was in
none of the campaign's numbers.

Four properties, three inherited and one new:
  * the verdict is still produced by `nd_lut2_asm_ok()` itself, so a hit and a fresh walk cannot
    disagree - this is memoisation, not a relaxed predicate;
  * 64 slots for at most 46 CQ2 tensors, open-addressed with linear probing, so every tensor keeps a
    slot; probing past a full table recomputes rather than guessing;
  * the key is blob + rows + ngroup + g, strictly stronger identity than the blob + rows the single
    entry used;
  * blob pointers belong to the mapped archive and the PSRAM tier, and a later open can hand those
    same addresses to different tensors, so `nd_model_close()` clears the table. Pointer stability
    inside one open is not lifetime correctness. `nd_model_open`'s failure paths route through close.

Safety, stated exactly: the two row walkers are bit-equivalent ONLY WHEN `nd_lut2_asm_ok()` is true.
A stale FALSE NEGATIVE is merely slower; a stale FALSE POSITIVE - the memo says the blob is clean
while it carries a norm whose exponent field is 0 or 31 - would run the specialised walker over an
encoding only the C path handles, and that changes arithmetic. So the memo's identity key, its
clear-on-close, and `.auto/exp77/test_asmemo.c` (which includes this candidate file and calls the
static predicate directly) are the guard, and the golden suite is not: the host build compiles the
engine with the asm off, so it never instantiates this code path.
"""
import hashlib
import os
import sys

BASES = {
    "engine/src/nd_quant.c": "a485999e5c91",        # accepted (run #431 bundle5)
    "engine/src/nd_model.c": "0d639424f636",
    "engine/include/nd_quant.h": "57a0fa8af900",
}

OLD_QUANT = '''#if ND_LUT2_ASM
/* nd_lut2_asm_ok() walks every norm in the row range, and a projection runs once
 * per token, so asking it on every call would cost more than the kernel saves.
 * Its answer depends only on the geometry and on the packed/norm blob - both set
 * in stone once the model is mapped - so it is remembered per (blob, rows). */
static const void *s_asm_blob;
static uint32_t    s_asm_rows;
static int         s_asm_ok;

static int lut2_asm_usable(const nd_lut2_ctx *c, const void *blob, uint32_t rows)
{
    if (blob != s_asm_blob || rows != s_asm_rows) {
        s_asm_ok   = nd_lut2_asm_ok(c, 0, rows);
        s_asm_blob = blob;
        s_asm_rows = rows;
    }
    return s_asm_ok;
}
'''

NEW_QUANT = '''/* One verdict per TENSOR, not per call (kept outside the ND_LUT2_ASM guard because
 * nd_model_close() clears it, and the host build of the engine has the asm off): 64 slots for the at-most-46 CQ2 blobs a decode
 * token visits, so the norm walk runs once per tensor per open instead of once per projection.
 * Keyed on blob + rows + ngroup + g, which is strictly stronger identity than the single entry
 * replaced here, and a probe that finds no free slot recomputes the verdict instead of guessing.
 * Written only from the core that runs the dispatch - the row split happens after this returns. */
#define ND_ASM_MEMO_SLOTS 64u
typedef struct { const void *blob; uint32_t rows, ngroup, g; int ok; } asm_memo;
static asm_memo s_asm_memo[ND_ASM_MEMO_SLOTS];

void nd_cq_asmemo_reset(void)
{
    uint32_t i;

    for (i = 0; i < ND_ASM_MEMO_SLOTS; i++)
        s_asm_memo[i].blob = 0;       /* the blob pointer is the validity tag */
}

#if ND_LUT2_ASM
static int lut2_asm_usable(const nd_lut2_ctx *c, const void *blob, uint32_t rows)
{
    uint32_t h = (uint32_t)((((uintptr_t)blob >> 4) * 2654435761u) >> 26)
                 & (ND_ASM_MEMO_SLOTS - 1u);
    uint32_t p;

    for (p = 0u; p < ND_ASM_MEMO_SLOTS; p++, h = (h + 1u) & (ND_ASM_MEMO_SLOTS - 1u)) {
        asm_memo *m = &s_asm_memo[h];

        if (m->blob == blob && m->rows == rows && m->ngroup == c->ngroup && m->g == c->g)
            return m->ok;
        if (m->blob == 0) {
            m->blob   = blob;
            m->rows   = rows;
            m->ngroup = c->ngroup;
            m->g      = c->g;
            m->ok     = nd_lut2_asm_ok(c, 0, rows);
            return m->ok;
        }
    }
    return nd_lut2_asm_ok(c, 0, rows);   /* full table: recompute, never guess */
}
'''

HDR_OLD = "int nd_lut2_asm_ok(const nd_lut2_ctx *c, uint32_t r0, uint32_t r1);"
HDR_NEW = HDR_OLD + """

/* Forget every remembered nd_lut2_asm_ok() verdict. Called from nd_model_close: the blob
 * pointers belong to the mapped archive and the PSRAM tier, and a later open can reuse those
 * addresses for different tensors, so a memo entry must not outlive its model. */
void nd_cq_asmemo_reset(void);
"""

CLOSE_OLD = """void nd_model_close(nd_model *m)
{
    if (!m)
        return;
"""
CLOSE_NEW = """void nd_model_close(nd_model *m)
{
    if (!m)
        return;
    nd_cq_asmemo_reset();   /* blob identity is only valid for this open */
"""

EDITS = [
    ("engine/src/nd_quant.c", "QUANT", [(OLD_QUANT, NEW_QUANT)]),
    ("engine/include/nd_quant.h", "HDR", [(HDR_OLD, HDR_NEW)]),
    ("engine/src/nd_model.c", "CLOSE", [(CLOSE_OLD, CLOSE_NEW)]),
]


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_quant.c"
    root = os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(target))))
    texts = {}
    for rel, _name, _pairs in EDITS:
        path = os.path.join(root, rel)
        data = open(path, "rb").read()
        got = hashlib.md5(data).hexdigest()[:12]
        if got != BASES[rel]:
            print(f"BASE_MISMATCH {rel} got={got} want={BASES[rel]} -> refusing")
            return 2
        texts[rel] = data.decode()
    for rel, name, pairs in EDITS:
        for old, _new in pairs:
            if texts[rel].count(old) != 1:
                print(f"{name}_ANCHOR count={texts[rel].count(old)} -> refusing")
                return 2
    for rel, _name, pairs in EDITS:
        for old, new in pairs:
            texts[rel] = texts[rel].replace(old, new, 1)

    quant = texts["engine/src/nd_quant.c"]
    if ("s_asm_blob" in quant
            or quant.count("s_asm_memo") != 3
            or quant.count("nd_cq_asmemo_reset") != 1
            or texts["engine/include/nd_quant.h"].count("nd_cq_asmemo_reset") != 1
            or texts["engine/src/nd_model.c"].count("nd_cq_asmemo_reset();") != 1):
        print("MARKER_ASSERT_FAILED")
        return 3
    for rel, _name, _pairs in EDITS:
        open(os.path.join(root, rel), "wb").write(texts[rel].encode())
        print("PATCHED " + rel + " md5=" + hashlib.md5(texts[rel].encode()).hexdigest()[:12])
    print("APPLIED asmemo root=" + root)
    return 0


if __name__ == "__main__":
    sys.exit(main())
