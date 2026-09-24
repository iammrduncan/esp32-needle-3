#!/usr/bin/env python3
"""Candidate `silu4`: the two big elementwise sigmoid loops step four elements per iteration.

`silu_rows` (8,192 elements per token, in the Hadamard MLP) and `agate_rows` (6,144, the attention
output gate) are the largest remaining elementwise mass, and both already pair their exponentials
through `sigmoidf_pair` - the unpaired half of that family is what run #290 shipped. This is the
mentor queue's reserve #2: the only 2-way batching left in the engine.

Bit-exact by construction in the strongest sense available here: the pairing is unchanged, so each
element still goes through the identical `sigmoidf_` expression with the identical sign test and the
identical division form. Four-wide simply calls the SAME pair twice per iteration, so no element's
value can move - only the loop control, the address arithmetic and the scheduler's freedom change.
"""
import hashlib, sys
PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "0d639424f636"           # accepted (run #431) engine/src/nd_model.c

EDITS = [
    ("SILU", """    for (i = lo; i < hi; i += 2) {
        float z0 = c->d2[i] * c->sc[i] * c->a[i] + c->b2[i];
        float z1 = c->d2[i + 1u] * c->sc[i + 1u] * c->a[i + 1u] + c->b2[i + 1u];
        float s0, s1;

        sigmoidf_pair(z0, z1, &s0, &s1);
        c->a[i]        = z0 * s0;
        c->a[i + 1u]   = z1 * s1;
    }""", """    for (i = lo; i + 3u < hi; i += 4) {
        float z0 = c->d2[i] * c->sc[i] * c->a[i] + c->b2[i];
        float z1 = c->d2[i + 1u] * c->sc[i + 1u] * c->a[i + 1u] + c->b2[i + 1u];
        float z2 = c->d2[i + 2u] * c->sc[i + 2u] * c->a[i + 2u] + c->b2[i + 2u];
        float z3 = c->d2[i + 3u] * c->sc[i + 3u] * c->a[i + 3u] + c->b2[i + 3u];
        float s0, s1, s2, s3;

        /* Two calls to the same pair the 2-wide loop used, on the same adjacent
         * pairs, so every element's sigmoid is the identical expression; four
         * loads are issued before any store because `a` is read-modify-written. */
        sigmoidf_pair(z0, z1, &s0, &s1);
        sigmoidf_pair(z2, z3, &s2, &s3);
        c->a[i]        = z0 * s0;
        c->a[i + 1u]   = z1 * s1;
        c->a[i + 2u]   = z2 * s2;
        c->a[i + 3u]   = z3 * s3;
    }
    for (; i < hi; i += 2) {
        float z0 = c->d2[i] * c->sc[i] * c->a[i] + c->b2[i];
        float z1 = c->d2[i + 1u] * c->sc[i + 1u] * c->a[i + 1u] + c->b2[i + 1u];
        float s0, s1;

        sigmoidf_pair(z0, z1, &s0, &s1);
        c->a[i]        = z0 * s0;
        c->a[i + 1u]   = z1 * s1;
    }"""),
    ("AGATE", """    for (i = lo; i < hi; i += 2) {
        float g0, g1;

        sigmoidf_pair(c->gate[i], c->gate[i + 1u], &g0, &g1);
        c->attn[i]        *= g0;
        c->attn[i + 1u]   *= g1;
    }""", """    for (i = lo; i + 3u < hi; i += 4) {
        float g0, g1, g2, g3;

        sigmoidf_pair(c->gate[i], c->gate[i + 1u], &g0, &g1);
        sigmoidf_pair(c->gate[i + 2u], c->gate[i + 3u], &g2, &g3);
        c->attn[i]        *= g0;
        c->attn[i + 1u]   *= g1;
        c->attn[i + 2u]   *= g2;
        c->attn[i + 3u]   *= g3;
    }
    for (; i < hi; i += 2) {
        float g0, g1;

        sigmoidf_pair(c->gate[i], c->gate[i + 1u], &g0, &g1);
        c->attn[i]        *= g0;
        c->attn[i + 1u]   *= g1;
    }"""),
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
    if new == text or new.count("sigmoidf_pair(z0, z1, &s0, &s1);") != 2 \
        or new.count("sigmoidf_pair(c->gate[i], c->gate[i + 1u], &g0, &g1);") != 2:
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print("APPLIED target=" + PATH + " md5=" + hashlib.md5(new.encode()).hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main())
