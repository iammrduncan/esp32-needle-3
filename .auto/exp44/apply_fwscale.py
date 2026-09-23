#!/usr/bin/env python3
"""Candidate `fwscale`: fold the per-group rescale into the transform's LAST stage.

Mechanism, and why it is different from the accepted one. fwht_rows currently walks
each group's seven FWHT stages and then makes a SEPARATE pass over all g elements to
multiply by scale (#362 shipped that pass unrolled by 4, #371 hoisted its invariants).
The last stage (len == n/2) is special: its n/2 butterflies write every element of the
group exactly once. So the multiply can happen at the store, and the whole pass - g
loads and g stores per group - disappears instead of merely getting a cheaper loop.
That is the distinction from run #385's fw3res (which merged the loop boundaries and
left the pass intact).

Bit-exact by construction, and the claim is narrow enough to check by eye: each
element's value after the final stage is identical to what the shipped code holds
before fw_scale, and `v * scale` is the same single rounding the shipped pass does.
No accumulation order exists here to move.

The generator PINS ITS BASE and asserts it, because once a candidate is accepted the
tree no longer contains the text a generator matches, and a stale base silently
produces a "candidate" identical to the accepted code (run #379's harness fact).
"""
import hashlib
import subprocess
import sys

TARGET = "engine/src/nd_quant.c"
# md5 of the file this was written against; refresh only together with a re-read.
BASE_MD5 = "bdcf0c7c8b0c"

NEW_FN = """
/* nd_fwht2 with the group rescale folded into the final stage.
 *
 * The final stage of a length-n FWHT is len == n/2: it has exactly n/2 butterflies,
 * each writing one untouched pair, so between them they write all n elements exactly
 * once. Multiplying by `scale` at that store replaces the separate fw_scale() pass
 * over the group (g loads + g stores) with work the transform already does, and every
 * element still gets exactly one multiply by the same `scale`, so the result is
 * bit-identical. Earlier stages deliberately do NOT scale: their outputs are inputs to
 * the next stage, and scaling them would change every later butterfly's operands. */
static ND_HOT void nd_fwht2s(float *x0, float *x1, uint32_t n, float scale)
{
    uint32_t len, step, base, j;

    for (len = 1u; len < n; len <<= 1) {
        step = len << 1;
        const int last = (step == n);      /* the stage that writes every element */

        for (base = 0u; base + step <= n; base += step) {
            const uint32_t half = base + len;
            for (j = base; j + 1u < half; j += 2u) {
                float a0 = x0[j], b0 = x0[j + len], a1 = x0[j + 1u], b1 = x0[j + 1u + len];
                float c0 = x1[j], d0 = x1[j + len], c1 = x1[j + 1u], d1 = x1[j + 1u + len];

                if (last) {
                    x0[j] = (a0 + b0) * scale;            x0[j + len] = (a0 - b0) * scale;
                    x0[j + 1u] = (a1 + b1) * scale;       x0[j + 1u + len] = (a1 - b1) * scale;
                    x1[j] = (c0 + d0) * scale;            x1[j + len] = (c0 - d0) * scale;
                    x1[j + 1u] = (c1 + d1) * scale;       x1[j + 1u + len] = (c1 - d1) * scale;
                } else {
                    x0[j] = a0 + b0;          x0[j + len] = a0 - b0;
                    x0[j + 1u] = a1 + b1;     x0[j + 1u + len] = a1 - b1;
                    x1[j] = c0 + d0;          x1[j + len] = c0 - d0;
                    x1[j + 1u] = c1 + d1;     x1[j + 1u + len] = c1 - d1;
                }
            }
            if (j < half) {
                float a = x0[j], b = x0[j + len];
                float c = x1[j], d = x1[j + len];

                if (last) {
                    x0[j] = (a + b) * scale; x0[j + len] = (a - b) * scale;
                    x1[j] = (c + d) * scale; x1[j + len] = (c - d) * scale;
                } else {
                    x0[j] = a + b; x0[j + len] = a - b;
                    x1[j] = c + d; x1[j + len] = c - d;
                }
            }
        }
    }
}

static ND_HOT void fwht_rows(void *vc, uint32_t g0, uint32_t g1)"""

CALLS = """        nd_fwht2s(blk, blk2, g, scale);
        /* No fw_scale(): the final stage folded it in. The single-group remainder
         * still needs its own pass, which is why fw_scale is not deleted. */"""

GUARD = "        if (last) {"


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else TARGET
    data = open(path, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing to edit")
        return 2
    text = data.decode()
    anchor = "static ND_HOT void fwht_rows(void *vc, uint32_t g0, uint32_t g1)"
    if text.count(anchor) != 1:
        print("ANCHOR_MISSING")
        return 2
    text = text.replace(anchor, NEW_FN, 1)
    old_calls = ("        nd_fwht2(blk, blk2, g);\n"
                 "        fw_scale(blk,  g, scale);\n"
                 "        fw_scale(blk2, g, scale);\n")
    if text.count(old_calls) != 1:
        print("CALL_SITES_NOT_ONE")
        return 2
    text = text.replace(old_calls, CALLS, 1)
    if text.count(GUARD) != 2:
        print("GUARD_COUNT_UNEXPECTED")
        return 2
    open(path, "w").write(text)
    print(f"applied md5={hashlib.md5(text.encode()).hexdigest()[:12]} "
          f"nd_fwht2s_present={text.count('nd_fwht2s')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
