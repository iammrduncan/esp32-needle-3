#!/usr/bin/env python3
"""Candidate `condT2`: channel-major `cond_v` staging (run #423's condT, +0.196 %) plus two
independent accumulator chains per channel pair inside `cond_rows`.

Why retry the interleave that #380/#381 rejected: those runs interleaved chains over the STRIDED
`cond_v` layout, which is exactly the operand that bound the loop (-0.394 %, -0.427 %, monotonically
worse with more chains). condT changes the delivery - channel ch is one contiguous 3 KB row - so
two sequential rows are now resident operands, the case the campaign's sharpest mechanism pays for.
The hypothesis is specifically "resident operand + pure arithmetic pays"; this is that rule tested
on the layout that makes the operands resident, not another unchanged gate attempt.

Bit-exact by construction: each accumulator sees only its own channel's products, in ascending i,
with one accumulator, so every partial sum is the shipped rounding sequence. `.auto/exp49/
test_pair_equiv.c` proves it over 200 random d_model x 8 cases (mismatches=0) - that guard exists
because #363/#380 both produced a literal channel offset inside a channel loop.

Pins and asserts its BASE: a stale base silently yields a candidate identical to accepted (#379).
"""
import hashlib
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "5c646468b78f"           # accepted engine/src/nd_model.c (tree cc046f59204e)

EDITS = [
    # (name, old, new)
    ("FILL", """                    m->fp16_slot[li][SLOT[f]] = p;
                    for (k = 0; k < n; k++)
                        p[k] = nd_f16(h[k]);
""",
     """                    m->fp16_slot[li][SLOT[f]] = p;
                    if (SLOT[f] == 25 && n == m->d_model * 8u) {
                        /* cond_v: [d_model][8] in the archive, read one column at a time by
                         * cond_rows, so store it [8][d_model]. Same pool, same element count,
                         * same values - only the fill order - and the reader's term order is
                         * untouched, so every sum is bit-identical. */
                        uint32_t r, cc;
                        for (r = 0; r < m->d_model; r++)
                            for (cc = 0; cc < 8u; cc++)
                                p[cc * m->d_model + r] = nd_f16(h[r * 8u + cc]);
                    } else {
                        for (k = 0; k < n; k++)
                            p[k] = nd_f16(h[k]);
                    }
"""),
    ("LOOP", """    for (ch = c0; ch < c1; ch++) {
        float acc = 0.0f;
        for (i = 0; i < c->dm; i++)
            acc += c->x[i] * c->cv[(size_t)i * 8 + ch];
        c->dst[ch] = acc;
    }
""",
     """    /* Two channels at a time over the staged channel-major rows. Each accumulator sees
     * only its own channel's terms, in ascending i, in a single accumulator, so both sums
     * keep the shipped rounding sequence. Two sequential resident operands per slot instead
     * of one strided sweep per channel. */
    for (ch = c0; ch + 1u < c1; ch += 2u) {
        const float *ca = c->cv + (size_t)ch * c->dm;
        const float *cb = ca + c->dm;
        float        a = 0.0f, b = 0.0f;
        for (i = 0; i < c->dm; i++) {
            float xi = c->x[i];
            a += xi * ca[i];
            b += xi * cb[i];
        }
        c->dst[ch] = a;
        c->dst[ch + 1u] = b;
    }
    if (ch < c1) {
        const float *ca = c->cv + (size_t)ch * c->dm;
        float        a = 0.0f;
        for (i = 0; i < c->dm; i++)
            a += c->x[i] * ca[i];
        c->dst[ch] = a;
    }
"""),
]


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing to edit")
        return 2
    text = data.decode()
    for name, old, _ in EDITS:
        n = text.count(old)
        if n != 1:
            print(f"{name}_ANCHOR count={n} -> refusing")
            return 2
    new = text
    for _, old, repl in EDITS:
        new = new.replace(old, repl, 1)
    if new == text:
        print("NO_CHANGE")
        return 2
    if (new.count("c->cv + (size_t)ch * c->dm") != 2 or new.count("cc * m->d_model + r") != 1
            or "i * 8 + ch" in new):
        print("MARKER_ASSERT_FAILED")
        return 3
    open(PATH, "wb").write(new.encode())
    print(f"APPLIED target={PATH} md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
