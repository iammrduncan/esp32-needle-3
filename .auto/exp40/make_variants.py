#!/usr/bin/env python3
"""Experiment 40 - three distinct, bit-exact candidates on the FWHT path.

Run #364's closure said the paired-butterfly transform "measured +36.75 % in
isolation and delivered +0.000 % on device" and blamed cache warmth. Experiment 38
measured that diagnosis and it does not hold: on an internal-SRAM operand - which
is what the field actually transforms, because fwht_rows works on c->xh, allocated
with ND_ALLOC_FAST - emptying the cache with a 160 KiB PSRAM sweep changes the cost
by EXACTLY NOTHING (47,470 cycles warm and cold alike), and even a PSRAM-resident
operand pays only +9.0 % when cold. Warmth cannot account for a 36.75-point
isolated delta, so the honest reading is that the wide-load form's win lived in the
bench's instruction mix, not in operand delivery.

Two consequences, both of which re-open exactly one thing each:
  * the transform is not immune to optimization - the rescale unroll (#362,
    +0.332 %) is a transform-loop change that DID transfer;
  * the mechanism that DID transfer was giving the scheduler independent work,
    and the third category this campaign has repeatedly under-priced is call
    structure: run #333 shipped the phi kernel because dot_group was "a function
    call per group", and fwht_rows calls nd_fwht once per group (~96 calls per
    decode token).

So screen three things, one lever each:
  40A  butterfly inner loop unrolled by 2   (isolated +17.95 % bit-exact, #360)
  40B  butterfly inner loop unrolled by 4   (the curve point; unroll-8 lost -28 %
       in the rescale, so the register-file ceiling is a known risk here)
  40C  nd_fwht's body inlined into fwht_rows, removing a call per group

Bit-exactness, for all three, is structural rather than empirical: butterflies in
one stage are disjoint index pairs, each element is read and written once per
stage, so grouping two or four of them moves no value, no operand and no order.
40C changes no code at all - it has ONE definition, a static inline helper that
both the exported nd_fwht and the hot caller use, because a hand-typed second copy
of the transform is precisely the bug that made run #363 diverge on device.
"""
import pathlib
import sys

SRC = pathlib.Path("engine/src/nd_quant.c")
body = SRC.read_text()

FWHT_OLD = """ND_HOT void nd_fwht(float *x, uint32_t n)
{
    uint32_t len;


    for (len = 1; len < n; len <<= 1) {
        uint32_t i;
        for (i = 0; i < n; i += len << 1) {
            uint32_t j;
            for (j = i; j < i + len; j++) {
                float a = x[j];
                float b = x[j + len];
                x[j]       = a + b;
                x[j + len] = a - b;
            }
        }
    }
}"""
assert body.count(FWHT_OLD) == 1, "shipped nd_fwht not found verbatim"

# Two sites call nd_fwht: fwht_rows (the hot parallel path, ~96 calls per token)
# and the dequant/prepare walk. Only the hot one is inlined - one lever, and the
# assert is what caught that a naive single-replacement would have silently hit
# the first match rather than the intended site.
CALL_OLD = """        float   *blk = xh + (size_t)gi * g;
        nd_fwht(blk, g);"""
assert body.count(CALL_OLD) == 1, "fwht_rows call site not unique"
assert body.count("        nd_fwht(blk, g);") == 2, "expected exactly two call sites"

DISJOINT = """        /* Butterflies inside one stage are disjoint pairs - j and j+len differ
         * for every j - so grouping K of them reads and writes each element
         * exactly as often, in the same per-element order. Bit-exact by
         * construction, and this is the same lever run #362's rescale unroll
         * shipped (+0.332 %), applied to the loop that precedes it. */
"""


def unroll(k: int) -> str:
    step = "    " * 0
    # Plain declarations, no braces: the writes below are in the same scope, and
    # wrapping the loads in a nested block was the bug that made the first
    # generated variant fail to compile ('a0' undeclared).
    vec = "\n".join(
        f"            float a{o} = x[j + {o}u], b{o} = x[j + {o}u + len];"
        for o in range(k)
    )
    writes = " ".join(
        f"x[j + {o}u] = a{o} + b{o}; x[j + {o}u + len] = a{o} - b{o};" for o in range(k)
    )
    return f"""ND_HOT void nd_fwht(float *x, uint32_t n)
{{
    uint32_t len;

{DISJOINT}    for (len = 1; len < n; len <<= 1) {{
        uint32_t i;
        for (i = 0; i < n; i += len << 1) {{
            uint32_t j = i, jend = i + len;
            for (; j + {k - 1}u < jend; j += {k}u) {{
{vec}
                {writes}
            }}
            for (; j < jend; j++) {{
                float a = x[j];
                float b = x[j + len];
                x[j]       = a + b;
                x[j + len] = a - b;
            }}
        }}
    }}
}}"""


HELPER = """/* One definition of the transform, used by the exported nd_fwht and inlined at
 * the hot per-group call site in fwht_rows. There is deliberately no second
 * copy of the butterfly loop anywhere: a hand-typed transcription of this
 * transform is what made run #363 diverge 4/17 on device while its benchmark
 * reported diff=0. */
static inline void fwht_inplace(float *x, uint32_t n)
{
    uint32_t len;

    for (len = 1; len < n; len <<= 1) {
        uint32_t i;
        for (i = 0; i < n; i += len << 1) {
            uint32_t j;
            for (j = i; j < i + len; j++) {
                float a = x[j];
                float b = x[j + len];
                x[j]       = a + b;
                x[j + len] = a - b;
            }
        }
    }
}

ND_HOT void nd_fwht(float *x, uint32_t n)
{
    fwht_inplace(x, n);
}"""

out = {
    "fw2": body.replace(FWHT_OLD, unroll(2), 1),
    "fw4": body.replace(FWHT_OLD, unroll(4), 1),
    "fwinline": body.replace(FWHT_OLD, HELPER, 1).replace(CALL_OLD, """        float   *blk = xh + (size_t)gi * g;
        fwht_inplace(blk, g);""", 1),
}
for name, text in out.items():
    pathlib.Path(f"/tmp/ndq_{name}.c").write_text(text)
    print(f"wrote /tmp/ndq_{name}.c  ({len(text) - len(body):+d} bytes)")

# The launcher syncs /tmp/ndq_<variant>.c alongside /tmp/ndm_<variant>.c, and it
# takes the model file from the accepted copy, so give it a no-op model file that
# is byte-identical to the accepted engine to keep the lane honest.
acc = pathlib.Path("/tmp/ndm_bundle.c").read_text()
for name in out:
    pathlib.Path(f"/tmp/ndm_fw{name}.c").write_text(acc)
print("model file for all three lanes = the accepted bundle engine")
