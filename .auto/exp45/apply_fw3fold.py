#!/usr/bin/env python3
"""Candidate `fw3fold`: three groups per stage walk, with the rescale folded into the
final stage - and the final stage PEELED, not flagged.

Two measured sub-bar halves of one phase, applied as a single change (run #371's
precedent: two levers of ~+0.1 % each that no board would keep separately):

  * the three-wide walk, +0.098 % on three readings (run #384 twice, run #387 once),
    because ngroup 6 split 3+3 gives each core exactly three groups, so this is the
    whole per-core transform and the single-group path stops running in the field;
  * deleting the rescale pass by folding the multiply into the transform's last stage,
    which writes every element exactly once (#361 measured that pass at 18.3 % of
    prepare's cycles, and #362 made it 3.6x cheaper by unrolling, so removing it is
    worth roughly the unroll's gain).

Why the fold is PEELED. The first attempt (run #388, `fwscale`) put `const int last =
(step == n)` in the stage loop and branched on it per butterfly: it measured 5.0983,
i.e. -0.13 %. That is run #292's mechanism re-confirmed - a data-independent branch
inside the hottest straight-line block in the firmware costs more than the work it
skips. Here the flag is also unnecessary: the last stage is one iteration of the stage
loop, so it is written once, after the loop, with no condition anywhere in it.

Bit-exact, and the claim is mechanical: a group's stages complete in its own order
(nothing is shared between the three chains), butterflies within a stage are disjoint
pairs, and the final stage's n/2 butterflies write all n elements exactly once, so
each element still receives exactly one multiply by the same `scale` - the same single
rounding the separate pass performed. `.auto/exp45/run_equiv.sh` proves it against the
exported nd_fwht() over g = 2..256 including scale 0, 1/sqrt(g), +-0, and random
mantissas and exponents.

The generator pins its BASE file and asserts it: once a candidate is accepted the tree
no longer contains the text a generator matched, and a stale base produces a
"candidate" byte-identical to the accepted code (run #379's harness fact).
"""
import hashlib
import sys

BASE_MD5 = "bdcf0c7c8b0c"          # accepted engine/src/nd_quant.c at write time
ANCHOR = "static ND_HOT void fwht_rows(void *vc, uint32_t g0, uint32_t g1)"

BODY = """
/* Three groups per stage walk, rescale folded into the (peeled) final stage.
 *
 * Three is not an arbitrary width: ngroup is 6 and the device splits it 3+3, so a
 * three-wide step is one call's entire workload and the paired and single paths below
 * stop executing on device. It is also the width at which the transform's serial
 * butterfly chains - the reason this phase responded to ILP at all (runs #378/#379),
 * while the model's long reductions did not (runs #380/#381) - have three deep chains
 * in flight instead of one.
 *
 * The rescale is folded into the final stage instead of run afterwards. n/2 butterflies
 * cover all n elements exactly once, so `* scale` at the store is the same single
 * rounding as the separate pass, and g loads plus g stores per group disappear.
 * Earlier stages must NOT scale: their outputs feed the next stage.
 *
 * Valid for the same domain as nd_fwht2 (power-of-two n >= 2); n == 1 has no stage
 * that writes anything in either form. */
static ND_HOT void nd_fwht3s(float *x0, float *x1, float *x2, uint32_t n, float scale)
{
    uint32_t len, step, base, j;

    /* Every stage except the last: len runs 1, 2, ... while 2*len < n. */
    for (len = 1u; len < (n >> 1); len <<= 1) {
        step = len << 1;
        for (base = 0u; base + step <= n; base += step) {
            const uint32_t half = base + len;
            for (j = base; j + 1u < half; j += 2u) {
                float a0 = x0[j], b0 = x0[j + len], a1 = x0[j + 1u], b1 = x0[j + 1u + len];
                float c0 = x1[j], d0 = x1[j + len], c1 = x1[j + 1u], d1 = x1[j + 1u + len];
                float e0 = x2[j], f0 = x2[j + len], e1 = x2[j + 1u], f1 = x2[j + 1u + len];

                x0[j] = a0 + b0;          x0[j + len] = a0 - b0;
                x0[j + 1u] = a1 + b1;     x0[j + 1u + len] = a1 - b1;
                x1[j] = c0 + d0;          x1[j + len] = c0 - d0;
                x1[j + 1u] = c1 + d1;     x1[j + 1u + len] = c1 - d1;
                x2[j] = e0 + f0;          x2[j + len] = e0 - f0;
                x2[j + 1u] = e1 + f1;     x2[j + 1u + len] = e1 - f1;
            }
            if (j < half) {
                float a = x0[j], b = x0[j + len];
                float c = x1[j], d = x1[j + len];
                float e = x2[j], f = x2[j + len];

                x0[j] = a + b; x0[j + len] = a - b;
                x1[j] = c + d; x1[j + len] = c - d;
                x2[j] = e + f; x2[j + len] = e - f;
            }
        }
    }

    /* The final stage, written once with no `last` flag anywhere in it: len == n/2,
     * one base, and between the butterflies every element is stored exactly once. */
    len = n >> 1;
    for (j = 0u; j + 1u < len; j += 2u) {
        float a0 = x0[j], b0 = x0[j + len], a1 = x0[j + 1u], b1 = x0[j + 1u + len];
        float c0 = x1[j], d0 = x1[j + len], c1 = x1[j + 1u], d1 = x1[j + 1u + len];
        float e0 = x2[j], f0 = x2[j + len], e1 = x2[j + 1u], f1 = x2[j + 1u + len];

        x0[j] = (a0 + b0) * scale;        x0[j + len] = (a0 - b0) * scale;
        x0[j + 1u] = (a1 + b1) * scale;   x0[j + 1u + len] = (a1 - b1) * scale;
        x1[j] = (c0 + d0) * scale;        x1[j + len] = (c0 - d0) * scale;
        x1[j + 1u] = (c1 + d1) * scale;   x1[j + 1u + len] = (c1 - d1) * scale;
        x2[j] = (e0 + f0) * scale;        x2[j + len] = (e0 - f0) * scale;
        x2[j + 1u] = (e1 + f1) * scale;   x2[j + 1u + len] = (e1 - f1) * scale;
    }
    if (j < len) {                        /* only reachable when n/2 is odd, i.e. n == 2 */
        float a = x0[j], b = x0[j + len];
        float c = x1[j], d = x1[j + len];
        float e = x2[j], f = x2[j + len];

        x0[j] = (a + b) * scale; x0[j + len] = (a - b) * scale;
        x1[j] = (c + d) * scale; x1[j + len] = (c - d) * scale;
        x2[j] = (e + f) * scale; x2[j + len] = (e - f) * scale;
    }
}

"""

LOOP = """    /* Three groups per stage walk for the whole per-core call: with ngroup 6 split
     * 3+3, one three-wide step consumes everything this core was given, so the
     * paired and single paths below run only in splits the field does not produce.
     * The guard for an ng-wide step is `gi + ng-1 < g1`, never `gi + ng < g1` - the
     * latter is never true when ng equals the per-core count, which is how a four-wide
     * experiment once re-measured the accepted path without executing at all.
     * No fw_scale() here: the final stage folded the rescale in. */
    for (gi = g0; gi + 2u < g1; gi += 3u) {
        float *blk  = xh + (size_t)gi * g;
        float *blk2 = blk + g;
        float *blk3 = blk2 + g;

        nd_fwht3s(blk, blk2, blk3, g, scale);
    }
"""


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_quant.c"
    data = open(path, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing to edit")
        return 2
    text = data.decode()
    if text.count(ANCHOR) != 1:
        print("ANCHOR_MISSING")
        return 2
    text = text.replace(ANCHOR, BODY + ANCHOR, 1)
    # insert the three-wide loop immediately before the accepted pair loop
    pair = "    for (gi = g0; gi + 1u < g1; gi += 2u) {"
    if text.count(pair) != 1:
        print("PAIR_LOOP_NOT_ONE")
        return 2
    pair_cont = "    for (; gi + 1u < g1; gi += 2u) {"
    # The accepted pair loop INITIALISES `gi = g0`. Leaving that initialiser in place
    # after a three-wide loop is a double transform: the three-wide step advances gi
    # past its groups, the pair loop then rewinds to g0 and transforms them a second
    # time. .auto/exp41/test_odd_split.c caught exactly this on 4 of 5 splits, with
    # first_cell=384 (= group 3's first element) on the 5+1 split, which is how the
    # arithmetic confirmed the mechanism. The three-wide loop owns the initialiser.
    text = text.replace(pair, LOOP + pair_cont, 1)
    if text.count("nd_fwht3s(") != 2:
        print("WIRING_UNEXPECTED")
        return 2
    open(path, "w").write(text)
    print(f"applied md5={hashlib.md5(text.encode()).hexdigest()[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
