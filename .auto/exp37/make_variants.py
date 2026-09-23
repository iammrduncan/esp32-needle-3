#!/usr/bin/env python3
"""Experiment 37 - the two un-unrolled independent loops inside the Hadamard MLP.

Run #362 shipped the only instruction-level win this window found (the FWHT rescale
unrolled by 4, +0.332 %), and runs #365/#366 showed the lever is narrow: aliasing
hoisting is worth zero, and the ZCRMS emit loops unroll to -0.13 % / +0.03 %. What
the one win had in common with nothing else was a plain loop of independent
per-element arithmetic over a few thousand values, sitting in a phase big enough to
notice.

The Monarch MLP (24.4 ms of a 197 ms token) still has exactly two loops of that
shape, both once per layer:

    for (i = 0; i < dm; i++) m->hada_a[i] = d1[i] * x[i];                 768/layer
    for (i = 0; i < n; i++)  m->hada_a[i] = m->hada_b[(uint32_t)p1[i]];  1024/layer

The first is one independent multiply per element (6,148 elements/token). The second
is an independent dependent-load gather per element (8,192/token) - unrolling gives
the scheduler several outstanding loads instead of one, which is the only cure this
core has since it has no prefetch (#300).

Both are bit-exact by construction: one output element per iteration, no
accumulation, no order to move. The accumulation loops in the same function
(the 8-way conditioning reductions, the softmax sums) are deliberately untouched.
"""
import pathlib

SRC = pathlib.Path("engine/src/nd_model.c")
body = SRC.read_text()

SCALE_OLD = "    for (i = 0; i < dm; i++) m->hada_a[i] = d1[i] * x[i];"
GATHER_OLD = "    for (i = 0; i < n; i++) m->hada_a[i] = m->hada_b[(uint32_t)p1[i]];"
assert body.count(SCALE_OLD) == 1, "scale loop not unique"
assert body.count(GATHER_OLD) == 1, "gather loop not unique"

NOTE = """    /* Unrolled by 4: one independent result per element, so grouping four moves
     * no value, operand or order - bit-exact by construction. The conditioning
     * reductions and softmax sums in this function are accumulations and are
     * left alone for that reason. Same lever as run #362's rescale (+0.332 %);
     * runs #365/#366 showed the lever is narrow, so this is measured, not assumed. */
"""

SCALE_NEW = NOTE + """    {
        float *restrict a4 = m->hada_a;
        uint32_t j = 0;
        for (; j + 3u < dm; j += 4u) {
            float x0 = x[j], x1 = x[j + 1u], x2 = x[j + 2u], x3 = x[j + 3u];
            float k0 = d1[j], k1 = d1[j + 1u], k2 = d1[j + 2u], k3 = d1[j + 3u];
            a4[j] = k0 * x0; a4[j + 1u] = k1 * x1; a4[j + 2u] = k2 * x2; a4[j + 3u] = k3 * x3;
        }
        for (; j < dm; j++) a4[j] = d1[j] * x[j];
    }"""

GATHER_NEW = NOTE + """    {
        float        *restrict a4 = m->hada_a;
        const float  *restrict b4 = m->hada_b;
        /* p1 holds the permutation as FLOATS and the index is a truncating cast,
         * so it is read as float and cast exactly as the shipped line does -
         * reinterpreting the buffer as uint32_t would read different numbers. */
        const float    *restrict q4 = p1;
        uint32_t j = 0;
        for (; j + 3u < n; j += 4u) {
            float g0 = b4[(uint32_t)q4[j]], g1 = b4[(uint32_t)q4[j + 1u]],
                  g2 = b4[(uint32_t)q4[j + 2u]], g3 = b4[(uint32_t)q4[j + 3u]];
            a4[j] = g0; a4[j + 1u] = g1; a4[j + 2u] = g2; a4[j + 3u] = g3;
        }
        for (; j < n; j++) a4[j] = b4[q4[j]];
    }"""

# The gather's tail is written with p1's own element width in the original, so keep
# the original expression there and only unroll the vector part.
GATHER_NEW = GATHER_NEW.replace("for (; j < n; j++) a4[j] = b4[q4[j]];",
                                "for (; j < n; j++) a4[j] = m->hada_b[(uint32_t)p1[j]];")

pathlib.Path("/tmp/ndm_hadascale.c").write_text(body.replace(SCALE_OLD, SCALE_NEW, 1))
pathlib.Path("/tmp/ndm_hadagather.c").write_text(body.replace(GATHER_OLD, GATHER_NEW, 1))
print("wrote /tmp/ndm_hadascale.c and /tmp/ndm_hadagather.c")
