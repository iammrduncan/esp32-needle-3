#!/usr/bin/env python3
"""Experiment 35 lane 3: hoist the fwht_ctx reads out of fwht_rows' loops.

`c` is `const fwht_ctx *`, but const is not `restrict`: the loop writes through
`blk` (which is `c->xh` + offset) and the compiler cannot prove that does not
alias `c->scale` or `c->g`, so both are candidates for a reload inside the
innermost loop - `c->scale` is read once PER ELEMENT in the rescale that run
#362 just shipped.

Same mechanism that paid +0.332 %, one level up, and bit-exact for the same
reason: identical values, identical expressions, identical order.
"""
import pathlib

SRC = pathlib.Path("/tmp/ndq_rescale_only.c")
body = SRC.read_text()

OLD = """static ND_HOT void fwht_rows(void *vc, uint32_t g0, uint32_t g1)
{
    const fwht_ctx *c = (const fwht_ctx *)vc;
    uint32_t        gi;
    for (gi = g0; gi < g1; gi++) {
        float   *blk = c->xh + (size_t)gi * c->g;
        nd_fwht(blk, c->g);"""

NEW = """static ND_HOT void fwht_rows(void *vc, uint32_t g0, uint32_t g1)
{
    const fwht_ctx *c = (const fwht_ctx *)vc;
    uint32_t        gi;
    /* `const fwht_ctx *` is not `restrict`: the rescale below writes through
     * blk, which the compiler cannot separate from c->g and c->scale, so both
     * are loop-variant for it and `c->scale` is read once per element. Taking
     * them once is the same values in the same order. */
    const uint32_t g      = c->g;
    const float    scale  = c->scale;
    float         *xh     = c->xh;
    for (gi = g0; gi < g1; gi++) {
        float   *blk = xh + (size_t)gi * g;
        nd_fwht(blk, g);"""

assert body.count(OLD) == 1, "fwht_rows changed shape"
out = body.replace(OLD, NEW, 1)
for a, b in (("for (; j + 3u < c->g; j += 4u)", "for (; j + 3u < g; j += 4u)"),
             ("b4[j] = v0 * c->scale; b4[j + 1u] = v1 * c->scale;", "b4[j] = v0 * scale; b4[j + 1u] = v1 * scale;"),
             ("b4[j + 2u] = v2 * c->scale; b4[j + 3u] = v3 * c->scale;", "b4[j + 2u] = v2 * scale; b4[j + 3u] = v3 * scale;"),
             ("for (; j < c->g; j++) b4[j] *= c->scale;", "for (; j < g; j++) b4[j] *= scale;")):
    assert out.count(a) == 1, a
    out = out.replace(a, b, 1)

pathlib.Path("/tmp/ndq_fwathoist.c").write_text(out)
print("wrote /tmp/ndq_fwathoist.c")
