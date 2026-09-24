#!/usr/bin/env python3
"""Candidate `taphoist`: resolve the tap row pointers once per call instead of once per element.

`tap_rows` runs the q/k/v tap convolution over a column range, and its inner statement computes
`(pos - j) % taps` - a `remu` plus the address arithmetic - for EVERY output column: 3 taps x dim
columns per call, 3 calls (q,k,v) x 8 layers per token, i.e. ~55K redundant remainder+multiply
operations per decode token for a quantity that is fixed for the whole call. The disassembly the
mentor quoted is exactly this (`remu` immediately followed by `mull` inside the tap/channel nest).

The candidate computes the tap history rows once per call and walks the columns with three
pointers. What is deliberately NOT changed:
  * the MAC order stays ascending in j, one accumulator seeded at +0.0f, so every partial sum is
    the same sequence of roundings (the -0.0/+0.0 seed behaviour is preserved rather than folded
    into `a*b + c*d + e*f`, whose association would be the same but whose seed is not);
  * the startup guard `j <= pos` becomes `nt = min(taps, pos+1)`, which is the same predicate;
  * everything that is not the archive's 3-tap geometry (a short history at pos < 2, a different
    tap count) falls through to the ORIGINAL loop, unchanged.

Base pinned; the archive header says 3 taps with q/k/v widths 576/96/128 (#379 rule: a stale base
silently produces a candidate identical to accepted).
"""
import hashlib
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "5c646468b78f"           # accepted engine/src/nd_model.c

OLD = """    const tap_ctx *c = (const tap_ctx *)vc;
    uint32_t        i, j, dim = c->dim;
    uint32_t        lo = b0 * 256, hi = (b1 * 256 < dim) ? b1 * 256 : dim;
    for (i = lo; i < hi; i++) {
        float value = 0.0f;
        for (j = 0; j < c->taps && j <= c->pos; j++) {
            uint32_t prior = (c->pos - j) % c->taps;
            value += c->w[(size_t)j * dim + i] *
                     c->hist[(size_t)prior * dim + i];
        }
        c->proj[i] = value;
    }
}"""

NEW = """    const tap_ctx *c = (const tap_ctx *)vc;
    uint32_t        i, j, dim = c->dim;
    uint32_t        lo = b0 * 256, hi = (b1 * 256 < dim) ? b1 * 256 : dim;
    uint32_t        nt = c->taps <= c->pos + 1u ? c->taps : c->pos + 1u;

    /* The valid tap count and the history rows are fixed for the whole call, but the
     * shipped loop derived `(pos - j) % taps` and the row address per column - a
     * remainder per tap per column. Resolve them here; the column loop below then only
     * loads, multiplies and adds. `nt` is exactly the shipped `j < taps && j <= pos`. */
    if (nt == 3u) {
        const float *h0 = c->hist + (size_t)( c->pos        % c->taps) * dim;
        const float *h1 = c->hist + (size_t)((c->pos - 1u)  % c->taps) * dim;
        const float *h2 = c->hist + (size_t)((c->pos - 2u)  % c->taps) * dim;
        const float *w0 = c->w, *w1 = c->w + dim, *w2 = c->w + 2u * dim;
        for (i = lo; i < hi; i++) {
            float value = 0.0f;
            value += w0[i] * h0[i];
            value += w1[i] * h1[i];
            value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;
    }
    for (i = lo; i < hi; i++) {
        float value = 0.0f;
        for (j = 0; j < nt; j++) {
            uint32_t prior = (c->pos - j) % c->taps;
            value += c->w[(size_t)j * dim + i] *
                     c->hist[(size_t)prior * dim + i];
        }
        c->proj[i] = value;
    }
}"""


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else PATH
    data = open(path, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing to edit")
        return 2
    text = data.decode()
    if text.count(OLD) != 1:
        print(f"ANCHOR count={text.count(OLD)} -> refusing")
        return 2
    new = text.replace(OLD, NEW, 1)
    if (new == text or new.count("% c->taps) * dim") != 3
            or "j < c->taps && j <= c->pos" in new):
        print("MARKER_ASSERT_FAILED")
        return 3
    open(path, "wb").write(new.encode())
    print(f"APPLIED target={path} md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
