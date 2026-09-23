#!/usr/bin/env python3
"""Candidate: stop splitting jobs too small to pay for the cross-core handshake.

`rows_dual_core()` in esp32/main/main.c already runs tiny jobs serially, but the
threshold is `half < 2`, i.e. nrows <= 3. That is right for the phases it was tuned
on (attention splits 8 heads of ~10K MACs each) and wrong-shaped for the jobs the
request-path harvest found: `nd_cq_prepare` splits `ngroup` = 8 independent FWHT
groups of 128 floats - about 3.5k butterflies, ~7k cycles of work - against a
handshake the same comment prices at ~15 us (~3.6k cycles) *when the worker is hot*,
and run #343 measured one prepare at 0.156 ms with ~12 prepares per decode token.

The guard goes in the shared helper rather than at a call site because that is where
every split is decided, and it is bit-exact by construction: `fn(ctx, 0, nrows)`
computes exactly the rows the two halves would have computed, in the same per-row
order. The engine's shared-state audit (run #162) established that every kernel run
through this splitter is pure per row, and the host build has always taken the serial
path (`rows_serial`), which is why the host goldens cannot move.

Default is the shipping value, so the macro alone cannot change an image: the
candidate is built by adding -DND_SPLIT_SERIAL_HALF=N on a FRESH build dir
(N=5 makes jobs of <=8 units serial, which is `nd_cq_prepare` exactly; N=9 also
covers the sampler gather at mean_n 6.7). Verify the define in
compile_commands.json before believing a number - the campaign's build-integrity
rule - and expect the boot bench to move, because prepare is inside the bench (unlike
the sampler work in runs #340/#341, which the bench structurally cannot see).
"""
import sys

p = "esp32/main/main.c"
s = open(p).read()

old = """    uint32_t half = nrows / 2;

    /* Below this the handshake costs more than the work it saves. Attention
     * splits only 8 heads at a time, but each head is ~10K MACs, far above the
     * ~15 us handshake. */
    if (half < 2 || !s_go) {"""

new = """    uint32_t half = nrows / 2;

    /* Below this the handshake costs more than the work it saves. Attention
     * splits only 8 heads at a time, but each head is ~10K MACs, far above the
     * ~15 us handshake. The threshold is a macro because the right value depends
     * on what a *parked* worker costs to wake, which is measured in
     * esp32/main/kbench.c's bench_wake() (KB WAKE) and not by the hot-loop figure
     * above: nd_cq_prepare hands this splitter ngroup = 8 groups of a 128-point
     * FWHT (~7k cycles of work), so it is on the wrong side of a threshold tuned
     * for 10K-MAC heads. Shipping default 1 keeps the original half < 2. */
#ifndef ND_SPLIT_SERIAL_HALF
#define ND_SPLIT_SERIAL_HALF 1u
#endif
    if (half < ND_SPLIT_SERIAL_HALF || !s_go) {"""

assert s.count(old) == 1, "rows_dual_core changed shape; re-read it before patching"
open(p, "w").write(s.replace(old, new, 1))
print("serial-threshold guard inserted; build the candidate with"
      " -D CMAKE_C_FLAGS=-DND_SPLIT_SERIAL_HALF=5u on a fresh dir;"
      " revert with git checkout esp32/main/main.c")
sys.exit(0)
