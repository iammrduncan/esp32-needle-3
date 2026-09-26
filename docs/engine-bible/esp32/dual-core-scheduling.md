# Dual-core scheduling

Matrix-vector output rows are independent, making row partitioning the safest
coarse parallel unit on the ESP32-S3. The firmware installs a target-specific
`nd_parallel_rows` callback and keeps one pinned helper task on the second core.
The implementation is in [`esp32/main/main.c`](../../../esp32/main/main.c); the
portable engine remains usable without it.

## Execution contract

For a call `(fn, ctx, nrows)`:

1. choose a row split appropriate to the operator/call shape;
2. publish function, context, and worker row range;
3. publish a new sequence number only after the payload is visible;
4. core 0 computes its range while core 1 computes the other;
5. wait for the worker's completion sequence before consuming outputs.

Outputs must be disjoint, inputs immutable for the duration, and every scratch
write either range-private or synchronized. Publication/completion ordering is
the correctness boundary.

## Why a persistent worker works

Creating a task per GEMV would dominate small operations. A persistent pinned
worker amortizes task setup. The retained hybrid briefly spins on the common
path, then uses a notification rather than burning a core indefinitely. The
spin is a latency optimization only; sequence/notification ordering must remain
correct if the worker sleeps immediately.

At the 100 Hz FreeRTOS tick, a nominal 5 ms tick-based delay rounds to zero.
Sub-tick behavior needs a cycle/timer/spin primitive or a different tick rate.
This corrected several misleading scheduler experiments.

## Split policy

A 50/50 row count is correct only when both halves have equal delivery and
compute cost. Use measured completion time for the exact operator and placement.
Small output counts should often stay on one core because publish/wake/wait
cost exceeds parallel work. Attention can split only eight heads, but each head
is large enough in this model to qualify; tiny norms/elementwise loops are not.

If one core receives staged operands and the other streams them, balance time,
not rows. The late one-core phi staging idea estimated about a 54.5/45.5 split,
but the token-wide upper bound was only ~0.22% with no engineering margin, so it
was priced rather than built.

## Measured overhead and failed overlap

The campaign measured tiny async split/notification costs around
16.6–19.2 microseconds. That is cheap for a large projection but larger than
many proposed pieces of independent work.

MimiModel-inspired cross-operator overlap did not transfer. This runtime's
worker slot already belonged to the synchronous row splitter. Reusing it for
LUT preparation collided with in-flight work and produced thousands of
tripwire hits. More importantly, the preparatory work was only about 2 µs while
the following GEMVs were ~30 µs: after safe orchestration there was too little
useful work to hide.

The lesson is architectural: overlap requires an ownership model, dependency
graph, and queueing protocol. An idle-looking second core is not an unowned
executor.

## Shared-state hazard: folded CQ4 table

A folded four-bit codebook initially appeared positive. Its 8 KiB
function-static scratch table was built by both cores. Host execution serialized
calls and hid the race; device results exposed it. Moving scratch to caller/
worker ownership restored safety but lost performance.

Hard rule: writable function-static kernel scratch is forbidden in a concurrent
row callback unless protected or provably single-owner. Protection can itself
make the optimization unprofitable.

## What worked

- Persistent, core-pinned helper rather than task creation per operator.
- Disjoint output-row splitting for large GEMVs/heads.
- Sequence-based publication and completion, with notification fallback.
- Shape/operator thresholds based on integrated timing.
- Per-worker scratch and immutable shared LUTs.
- Measuring both worker finish times before changing split ratios.

## What did not

- Parallelizing tiny operators or ever-finer chunks: handshake cost dominated.
- Assuming two cores double a shared external-memory bus.
- Reusing the worker slot for asynchronous inter-operator overlap.
- Static writable scratch inside a supposedly pure row kernel.
- Host-only validation of concurrency.
- Treating spin-budget changes as computational speedups without liveness and
  power/idle consequences.

## Verification checklist

1. Run scalar single-core reference and dual-core output differentials.
2. Exercise odd row counts, zero/one row, thresholds, and both range orders.
3. Poison or guard per-core scratch to expose overlap.
4. Stress repeated requests and cache restore, not one boot benchmark.
5. Use target race tripwires/counters; host tests cannot schedule the same way.
6. Measure core completion skew and total wall time.
7. Inspect behavior when the helper sleeps, wakes late, or receives back-to-back
   sequence values.
8. Re-run full device goldens after any scheduling or placement change.
