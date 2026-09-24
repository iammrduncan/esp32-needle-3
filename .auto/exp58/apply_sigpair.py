#!/usr/bin/env python3
"""Candidate `sigpair`: stop `sigmoidf_pair` from throwing away the pairing when the two
arguments have opposite signs.

The shipped guard runs the interleaved pair only `if ((x0 >= 0) == (x1 >= 0))`, otherwise two
scalar `sigmoidf_` calls. But the pairing never needed the signs to agree: the pair's exp chains
are fed the sign-normalized magnitudes (`-x` when x >= 0, `x` otherwise) and the two divisions stay
scalar, so a mixed-sign pair can still share one `nd_expf_pair` and only select the division per
element. ~14,400 gate sigmoids per token run through here, and the gate values are a random-ish
sign mix, so a large share of pairs currently pays for two serially-dependent degree-5 Horner
chains where one interleaved pair exists.

Bit-exact by construction, and the reason it is admissible: each element's exponential argument is
exactly the one the scalar path passes; `nd_expf_pair` is documented and proven bit-identical to
two scalar calls (and out-of-range arguments still fall back to two scalar `nd_expf` calls); each
division keeps the shipped scalar expression. No rounding point moves.
"""
import hashlib, sys
PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "53553a3507c5"   # accepted+expbc+head4+taphoist engine/src/nd_model.c

OLD = """    if ((x0 >= 0.0f) == (x1 >= 0.0f)) {
        float e0, e1;

        nd_expf_pair(x0 >= 0.0f ? -x0 : x0,
                     x1 >= 0.0f ? -x1 : x1, &e0, &e1);
        if (x0 >= 0.0f) {
            *y0 = 1.0f / (1.0f + e0);
            *y1 = 1.0f / (1.0f + e1);
        } else {
            *y0 = e0 / (1.0f + e0);
            *y1 = e1 / (1.0f + e1);
        }
    } else {
        *y0 = sigmoidf_(x0);
        *y1 = sigmoidf_(x1);
    }
}"""

NEW = """    /* The pair needs matching EXPONENTS, not matching signs: both chains are fed the
     * sign-normalized magnitude exactly as the scalar path would, so the sign only selects
     * which of the two shipped scalar division forms each element uses. The shipped code
     * required the signs to agree and otherwise ran two serially-dependent scalar chains. */
    float e0, e1;

    nd_expf_pair(x0 >= 0.0f ? -x0 : x0,
                 x1 >= 0.0f ? -x1 : x1, &e0, &e1);
    if (x0 >= 0.0f) *y0 = 1.0f / (1.0f + e0);
    else            *y0 = e0 / (1.0f + e0);
    if (x1 >= 0.0f) *y1 = 1.0f / (1.0f + e1);
    else            *y1 = e1 / (1.0f + e1);
}"""

def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    if text.count(OLD) != 1:
        print(f"ANCHOR count={text.count(OLD)} -> refusing"); return 2
    new = text.replace(OLD, NEW, 1)
    if new == text or new.count("sigmoidf_(x") != 0:
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print(f"APPLIED target={PATH} md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
