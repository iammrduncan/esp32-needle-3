#!/usr/bin/env python3
"""Candidate `tap2col`: two output columns per iteration inside the hoisted 3-tap path.

`taphoist` (#427, +0.70 %) proved the tap loop has slack that the precision experiments never
found: it is not only delivery-bound, it also paid loop control per column. This walks columns in
pairs so one iteration issues two independent loads/accumulator chains over the same three hoisted
rows, and keeps a single-column tail.

Bit-exact by construction: each column keeps its OWN accumulator, seeded at +0.0f, and its own
ascending-tap product order, so every stored value is the same sequence of roundings. Proven on
host by `.auto/exp57/test_tap_equiv.c` (which now checks this variant too) over dims 96/128/576/
768/33, taps 1-4, every startup position and 1-3 column blocks.
"""
import hashlib, sys
PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "53553a3507c5"

OLD = """        for (i = lo; i < hi; i++) {
            float value = 0.0f;
            value += w0[i] * h0[i];
            value += w1[i] * h1[i];
            value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;"""

NEW = """        for (i = lo; i + 1u < hi; i += 2u) {
            /* Two columns, two accumulators: each keeps its own ascending-tap order and
             * its own +0.0f seed, so both stored sums are the shipped rounding sequence. */
            float a = 0.0f, b = 0.0f;
            a += w0[i] * h0[i];
            a += w1[i] * h1[i];
            a += w2[i] * h2[i];
            b += w0[i + 1u] * h0[i + 1u];
            b += w1[i + 1u] * h1[i + 1u];
            b += w2[i + 1u] * h2[i + 1u];
            c->proj[i] = a;
            c->proj[i + 1u] = b;
        }
        if (i < hi) {
            float value = 0.0f;
            value += w0[i] * h0[i];
            value += w1[i] * h1[i];
            value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;"""

def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    if text.count(OLD) != 1:
        print(f"ANCHOR count={text.count(OLD)} -> refusing"); return 2
    new = text.replace(OLD, NEW, 1)
    if new == text or new.count("i + 1u] * h0[i + 1u]") != 1:
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print(f"APPLIED target={PATH} md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
