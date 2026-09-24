#!/usr/bin/env python3
"""Candidate `tapbund`: the two measured tap-loop levers as ONE change (the #371 precedent).

`tap2col` (+0.094 pct) walks two columns per iteration over the hoisted rows; `tapfwd` (+0.062 pct)
lets tap j=0 read the current input instead of the history slot the caller just copied it into.
Both are individually below the 0.2 pct bar and both act on the SAME phase - the qkv tap
convolution - which is the case the campaign bundled before (run #371: two bar-marginal halves of
one phase that only clear the bar together). Independent mechanisms: one removes loop control per
column, the other removes a PSRAM sweep per projection, so there is nothing double-counted.

Bit-exact: every column keeps its own accumulator, seeded at +0.0f, and its own ascending-tap
product order, and j=0's operand is the byte-identical value the caller memcpy'd, read before that
column is stored. `.auto/exp57/test_tap_equiv.c` carries the combined variant and reports
mismatches=0 against the shipped form over 87,836 comparisons.
"""
import hashlib, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "53553a3507c5"           # accepted+expbc+head4+taphoist engine/src/nd_model.c

EDITS = [
    ("FWD", """        const float *h0 = c->hist + (size_t)( c->pos        % c->taps) * dim;""", """        /* j = 0 is the row the caller memcpy'd out of c->proj immediately before this call,
         * so read the source: same bytes, same order, one PSRAM sweep fewer per projection. */
        const float *h0 = c->proj;"""),
    ("TILE", """        for (i = lo; i < hi; i++) {
            float value = 0.0f;
            value += w0[i] * h0[i];
            value += w1[i] * h1[i];
            value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;""", """        for (i = lo; i + 1u < hi; i += 2u) {
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
        return;"""),
]


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    for name, old, _ in EDITS:
        if text.count(old) != 1:
            print(f"{name}_ANCHOR count={text.count(old)} -> refusing"); return 2
    new = text
    for _, old, repl in EDITS:
        new = new.replace(old, repl, 1)
    if (new == text or new.count("const float *h0 = c->proj;") != 1
            or new.count("i + 1u] * h0[i + 1u]") != 1):
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print("APPLIED target=" + PATH + " md5=" + hashlib.md5(new.encode()).hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main())
