#!/usr/bin/env python3
"""Build the Experiment 35 variants of engine/src/nd_quant.c.

Same lever that shipped in run #362 - an elementwise loop whose cycles the
compiler was leaving on the table - applied to the other hot per-cell loop in the
same phase: `lutb_rows`, which builds the 2-bit pair table.

Why this loop: every pair reads `c->cb[0..3]`, and `c->lut` is *written* inside
the same loop. GCC cannot prove `cb` does not alias `lut`, so the four codebook
values are loop-variant as far as the optimizer is concerned although they are
constant. Hoisting them is bit-exact by construction: same four values, same
expressions, same order, same per-entry arithmetic.

  hoist   = codebook hoisted, one pair per iteration
  hoist2  = hoist plus two pairs per iteration (two independent chains to interleave)

The host compiles this same C, so `checks.sh` on a variant is a real byte-exact
verdict, not a proxy. Run it before any board time.
"""
import pathlib

SRC = pathlib.Path("engine/src/nd_quant.c")
ACQUIRED = pathlib.Path("/tmp/ndq_rescale_only.c")     # the accepted shipping tree
body = ACQUIRED.read_text()

FUNC_START = "static ND_HOT void lutb_rows(void *vc, uint32_t p0, uint32_t p1)"
FUNC_END = "\n}\n"
start = body.index(FUNC_START)
end = body.index(FUNC_END, start) + len(FUNC_END)

SHARED_HEAD = """static ND_HOT void lutb_rows(void *vc, uint32_t p0, uint32_t p1)
{
    const lutb_ctx *c = (const lutb_ctx *)vc;
    uint32_t        p;
    /* Each pair's 16 entries depend only on that pair's two activation values,
     * so pairs split across cores and every entry is the same expression,
     * computed the same way, as in the sequential build. */
    /* The codebook is constant for the whole build, but `c->lut` is written in
     * this loop and GCC cannot prove `c->cb` does not alias it, so it reloads
     * all four values for every pair. Reading them once is the same four values
     * in the same order - bit-exact by construction. */
    const float cb0 = c->cb[0], cb1 = c->cb[1], cb2 = c->cb[2], cb3 = c->cb[3];
"""

ONE = SHARED_HEAD + """    for (p = p0; p < p1; p++) {
        float  x0 = c->xh[2 * p], x1 = c->xh[2 * p + 1];
        float  a0 = cb0 * x0, a1 = cb1 * x0;
        float  a2 = cb2 * x0, a3 = cb3 * x0;
        float  b0 = cb0 * x1, b1 = cb1 * x1;
        float  b2 = cb2 * x1, b3 = cb3 * x1;
        float *T  = c->lut + (size_t)p * 16;
        T[0]  = a0 + b0; T[1]  = a1 + b0; T[2]  = a2 + b0; T[3]  = a3 + b0;
        T[4]  = a0 + b1; T[5]  = a1 + b1; T[6]  = a2 + b1; T[7]  = a3 + b1;
        T[8]  = a0 + b2; T[9]  = a1 + b2; T[10] = a2 + b2; T[11] = a3 + b2;
        T[12] = a0 + b3; T[13] = a1 + b3; T[14] = a2 + b3; T[15] = a3 + b3;
    }
}
"""

TWO = SHARED_HEAD + """    for (p = p0; p + 1 < p1; p += 2) {
        float  x0 = c->xh[2 * p],     x1 = c->xh[2 * p + 1];
        float  y0 = c->xh[2 * p + 2], y1 = c->xh[2 * p + 3];
        float  a0 = cb0 * x0, a1 = cb1 * x0, a2 = cb2 * x0, a3 = cb3 * x0;
        float  b0 = cb0 * x1, b1 = cb1 * x1, b2 = cb2 * x1, b3 = cb3 * x1;
        float  d0 = cb0 * y0, d1 = cb1 * y0, d2 = cb2 * y0, d3 = cb3 * y0;
        float  e0 = cb0 * y1, e1 = cb1 * y1, e2 = cb2 * y1, e3 = cb3 * y1;
        float *T  = c->lut + (size_t)p * 16;
        float *U  = T + 16;
        T[0]  = a0 + b0; T[1]  = a1 + b0; T[2]  = a2 + b0; T[3]  = a3 + b0;
        T[4]  = a0 + b1; T[5]  = a1 + b1; T[6]  = a2 + b1; T[7]  = a3 + b1;
        T[8]  = a0 + b2; T[9]  = a1 + b2; T[10] = a2 + b2; T[11] = a3 + b2;
        T[12] = a0 + b3; T[13] = a1 + b3; T[14] = a2 + b3; T[15] = a3 + b3;
        U[0]  = d0 + e0; U[1]  = d1 + e0; U[2]  = d2 + e0; U[3]  = d3 + e0;
        U[4]  = d0 + e1; U[5]  = d1 + e1; U[6]  = d2 + e1; U[7]  = d3 + e1;
        U[8]  = d0 + e2; U[9]  = d1 + e2; U[10] = d2 + e2; U[11] = d3 + e2;
        U[12] = d0 + e3; U[13] = d1 + e3; U[14] = d2 + e3; U[15] = d3 + e3;
    }
    if (p < p1) {                     /* odd tail: the shipped single-pair body */
        float  x0 = c->xh[2 * p], x1 = c->xh[2 * p + 1];
        float  a0 = cb0 * x0, a1 = cb1 * x0;
        float  a2 = cb2 * x0, a3 = cb3 * x0;
        float  b0 = cb0 * x1, b1 = cb1 * x1;
        float  b2 = cb2 * x1, b3 = cb3 * x1;
        float *T  = c->lut + (size_t)p * 16;
        T[0]  = a0 + b0; T[1]  = a1 + b0; T[2]  = a2 + b0; T[3]  = a3 + b0;
        T[4]  = a0 + b1; T[5]  = a1 + b1; T[6]  = a2 + b1; T[7]  = a3 + b1;
        T[8]  = a0 + b2; T[9]  = a1 + b2; T[10] = a2 + b2; T[11] = a3 + b2;
        T[12] = a0 + b3; T[13] = a1 + b3; T[14] = a2 + b3; T[15] = a3 + b3;
    }
}
"""

for name, fn in (("hoist", ONE), ("hoist2", TWO)):
    out = pathlib.Path(f"/tmp/ndq_{name}.c")
    out.write_text(body[:start] + fn + body[end:])
    print(f"wrote {out}")
