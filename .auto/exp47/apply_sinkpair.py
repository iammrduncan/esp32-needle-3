#!/usr/bin/env python3
"""Candidate `sinkpair`: interleave the two independent exponentials per pair inside
Sinkhorn's row and column log-sum-exp loops.

Why this is the next thing worth a board. The campaign's sharpest mechanism, measured
four times now, is "put independent work in flight inside a serial dependency chain
whose operands are already resident": two FWHT groups per stage walk (+0.492 %, #379),
butterflies unrolled within a stage (+0.461 %, #378), and the attention softmax pair
(+0.48 %, #229). It explicitly did NOT pay on the model's long reductions (cond2
-0.394 % / cond4 -0.427 %, #380/#381), where the loop is bound by strided loads.

Sinkhorn's sums are the winning shape and nothing has ever paired them: 20 iterations
x (4 rows + 4 columns) x 4 terms x 8 layers, each term a degree-5 Horner chain
(`nd_expf`, ~99 cycles of latency, ~11 FP ops of work) with nothing else in flight, on
a 4x4 matrix that lives in one cache line. n == 4 so each sum becomes exactly two
pairs, and #229's measured cost for the pair is 198.04 cycles against 247.42 for two
scalar calls.

Bit-exact by construction, with two constraints honored:
  * `nd_expf_pair` computes each argument's own chain in `nd_expf`'s own order - proven
    bit-identical over 4,000,000 pairs and shipped (#229) - so the values cannot move;
  * the two partials are ADDED one at a time in the shipped position (j, then j+1), so
    the accumulator's sequence of roundings is unchanged. That is the condition the
    ideas backlog put on this exact idea ("bit-exact if the partials are added back one
    at a time in the shipped order, which the loop already does").
The run #291 exact-zero literal is kept per element, and the pair call is only used
when NEITHER argument is +-0, so the shortcut's semantics are untouched. Order of the
four branches is chosen so the common cases stay branch-predictable; #292 measured that
a guard around this call costs real time when the loop is the scheduler's best block, so
this candidate's verdict is the measurement of that trade in the loop where the trade
already measured favourable.

Pins its BASE file (#379's rule: a stale base yields a byte-identical "candidate").
"""
import hashlib
import sys

MODEL = "engine/src/nd_model.c"
BASE_MD5 = "5c646468b78f"          # accepted engine/src/nd_model.c

HELPER = """
/* Two terms of a Sinkhorn log-sum-exp, in the shipped order.
 *
 * The values come from `nd_expf_pair`, whose two interleaved chains are each
 * `nd_expf`'s own operation sequence (proven bit-identical over 4,000,000 pairs and
 * shipped in run #229), and the partials are added one at a time at the positions the
 * shipped loop used, so the accumulator's rounding sequence does not move.
 *
 * The exact-zero literal from run #291 is preserved per element: the pair is used only
 * when neither difference is +-0.0f. Both-zero is listed first because in a converged
 * Sinkhorn pass it is the most common pair shape (the row maximum and any entry that
 * has fallen below nd_expf's clamp both contribute exactly 1.0f and 0.0f terms). */
static ND_HOT inline void sink_term_pair(float d0, float d1, float *sum)
{
    if (d0 == 0.0f) {
        *sum += 1.0f;
        *sum += (d1 == 0.0f) ? 1.0f : nd_expf(d1);
    } else if (d1 == 0.0f) {
        *sum += nd_expf(d0);
        *sum += 1.0f;
    } else {
        float w0, w1;
        nd_expf_pair(d0, d1, &w0, &w1);
        *sum += w0;
        *sum += w1;
    }
}

static void sinkhorn(float *a, uint32_t n)"""

ROWS_OLD = """            for (j = 0; j < n; j++) {
                /* The row maximum contributes exp(0), and `nd_expf` returns
                 * exactly 1.0f for both signed zeros, so that one term per pass
                 * is the literal rather than a 99-cycle degree-5 call: a quarter
                 * of this kernel's ~5,120 exponentials per decode token. Testing
                 * the difference (not the index) keeps it exact by construction:
                 * only equal finite operands can subtract to +-0, so NaN and
                 * inf-inf still fall through to `nd_expf` as before. The partial
                 * is added in the shipped position, so the sum's bits cannot
                 * move. Verified bit-identical to the shipped loop over 3.2 M
                 * elements in `.auto/sinkzero/test.c`. */
                float d = a[i * n + j] - mx;
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);
            }"""
ROWS_NEW = """            /* Two at a time. n is the mHC lane count (4 here), so this is two
             * pairs per sum; the odd tail keeps the literal-only scalar form for
             * an odd lane count so the function stays correct for other n. */
            for (j = 0; j + 1u < n; j += 2u)
                sink_term_pair(a[i * n + j] - mx, a[i * n + j + 1u] - mx, &sum);
            for (; j < n; j++) {
                float d = a[i * n + j] - mx;
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);
            }"""

COLS_OLD = """            for (i = 0; i < n; i++) {
                float d = a[i * n + j] - mx;   /* same exact-zero shortcut */
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);
            }"""
COLS_NEW = """            for (i = 0; i + 1u < n; i += 2u)
                sink_term_pair(a[i * n + j] - mx, a[(i + 1u) * n + j] - mx, &sum);
            for (; i < n; i++) {
                float d = a[i * n + j] - mx;   /* same exact-zero shortcut */
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);
            }"""


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else MODEL
    data = open(path, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing to edit")
        return 2
    text = data.decode()
    for name, old in (("HELPER_ANCHOR", "static void sinkhorn(float *a, uint32_t n)"),
                      ("ROWS", ROWS_OLD), ("COLS", COLS_OLD)):
        if text.count(old) != 1:
            print(f"{name}_ANCHOR count={text.count(old)}")
            return 2
    new = text.replace("static void sinkhorn(float *a, uint32_t n)", HELPER, 1)
    new = new.replace(ROWS_OLD, ROWS_NEW, 1).replace(COLS_OLD, COLS_NEW, 1)
    if new.count("sink_term_pair(") != 3:      # definition + two call sites
        print(f"WIRING count={new.count('sink_term_pair(')} want=3")
        return 2
    open(path, "w").write(new)
    print(f"applied md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
