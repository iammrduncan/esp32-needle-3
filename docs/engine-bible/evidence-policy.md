# Evidence policy and provenance

The central lesson of this project is that a fast number without provenance is
not a result. ESP32 builds, board assignments, serial state, compile-time CMake
options, host fallbacks, and frozen outputs can each make an invalid experiment
look plausible. This document defines the evidence standard used throughout the
bible.

## Evidence classes

### Product evidence

Product evidence exercises behavior a user can observe:

- a full firmware request through the serial protocol;
- the seven-scenario `make capture` path through the HTTP bridge;
- route choice, number of inference passes, tool arguments, execution result,
  telemetry progression, cadence change, and timer expiry;
- a sustained-session or repeated-prompt soak.

Product evidence can detect failures that logits or kernel tests cannot, but it
does not isolate a mechanism by itself.

### Model-quality evidence

The campaign used complementary oracles:

- frozen host outputs, currently 19/19 on the final trees;
- frozen device outputs, 20 cases in the breadth suite;
- token-count delta and missing-golden counts;
- a ten-step, 8,192-logit fidelity probe with a maximum-delta budget of
  `2e-3` and a 10/10 top-1 check;
- host/device agreement on prompts outside the frozen suite;
- grammar and prefix-isolation C tests.

Text equality and numeric fidelity answer different questions. A kernel can
retain top-1 while moving logits, and a two-core race can be invisible in a
host build that uses the serial fallback.

### Mechanism evidence

A performance hypothesis is stronger when the claimed mechanism moves too:

- phase counters change in the predicted phase;
- an ELF contains the intended symbol or instruction sequence;
- `compile_commands.json` contains the intended macro;
- a kernel differential runs the same real tensor geometry and both relevant
  memory placements;
- an isolated microbenchmark and end-to-end decode agree in direction;
- a no-op, fallback, or stale-image explanation is excluded by hashes.

Disassembly and raw bytes outrank comments about what a compiler “should” do.

### Performance evidence

A speed claim needs:

- the exact board/tree/image identity;
- the same frozen prompt set and decoder behavior;
- device-reported prefill/decode timing, not host wall time;
- a same-board pin or simultaneous controls when board spread matters;
- a non-profiled image for promotion;
- all applicable quality gates.

The primary metric is the mean device-reported decode rate over the frozen
primary prompts. Secondary monitors include prefill, extended, reasoning,
boot-bench, worst-case prompt rate, generated-token count, and free internal and
external memory. A change that improves only a diagnostic microbenchmark is not
an engine win.

## State labels used in this bible

| Label | Required interpretation |
|---|---|
| Accepted | Promoted comparison pin; required gates green on that exact tree. |
| Candidate | Real measured improvement, but a named decision/gate remains. |
| Kept mechanism | Valid and preserved on at least one measured lineage; it may not be in the checkout's current engine files. |
| Diagnostic | Valid mechanism or capacity information; its tok/s is not promotable. |
| Rejected | Built/measured and lost, diverged, or violated a constraint. |
| Closed by budget | Not built because a conservative byte/cycle bound already proves it cannot meet the bar. |
| Vendor-blocked | Hardware supports or suggests a mode, but the pinned SDK/module configuration does not support a shippable use. |
| Unverified | Narrated or prepared, but no adequate artifact proves the outcome. |

## Provenance keys

The campaign evolved from weak file hashes to a shipping signature. A robust
record includes:

- Git commit and dirty diff;
- a combined hash of all engine sources, headers, assembly, and app sources;
- built application hash;
- model manifest revision and SHA-256;
- resolved `sdkconfig`, not only `sdkconfig.defaults`;
- CMake cache options and compile definitions;
- board identity and serial/flash paths;
- prompt/golden hashes and exact group selection;
- whether the image was profiled, diagnostic, or kbench-only.

`.auto/measure.sh` now refuses a previously measured shipping signature unless
an explicit, one-use repeat reason is supplied. It asserts an expected engine
hash, records app/engine hashes, checks board config drift, and only records a
signature after the benchmark and hard device gate complete.

## Known corrections and retractions

These are not footnotes; they are reusable warnings about how evidence failed.

1. **The initial 1.22 tok/s reading was real.** The later `2.44` “session
   baseline” begins after run #2's FP32 staging. State which denominator is in
   use.
2. **A build option in the CMake cache is not proof it reached C.** Several
   kbench runs silently compiled the previous body until macros were forwarded
   and checked in `compile_commands.json` and the boot banner.
3. **A new object file is not proof its kernel linked.** Linker garbage
   collection removed an unreferenced assembly candidate while the shipping
   signature still changed. Require the symbol/instructions in the ELF.
4. **Host output cannot validate target-only assembly or two-core safety.** The
   host compiles C fallbacks and uses serial `nd_parallel_rows`.
5. **A passing comparison can be vacuous.** Missing golden entries once counted
   as exact; the harness now reports and gates `golden_missing`.
6. **A restricted six-case lane once reported exactness without enforcing it.**
   The device gate now fails restricted runs too.
7. **The folded 4-bit codebook looked green on three boards but was racy.** Two
   cores wrote one static table; timing happened to hide the race. The explicit
   row-split contract now forbids shared scratch.
8. **The apparent stdout-stall diagnosis was retracted.** `_line_quiet()` hid
   diagnostic lines during requests. A later length correlation and RX-ring
   experiment localized the request ceiling to transport.
9. **A `5 ms` FreeRTOS delay became zero ticks at 100 Hz.** Always reason in
   resolved ticks after changing `CONFIG_FREERTOS_HZ`.
10. **A candidate base was contaminated by an unaccepted fusion tree.** Worker
    checkouts were restored from their own HEAD instead of the canonical base.
    Deltas were relabeled, not silently retained.
11. **A synthetic oracle can validate the wrong implementation.** At least one
    FWHT experiment compared against a locally retyped “reference” instead of
    the shipping function. Use the actual callable reference whenever possible.
12. **Profiled images are diagnostic.** Per-phase timers add work and their
    absolute tok/s cannot be promoted.
13. **The boot benchmark omits grammar sampling.** It correctly reported
    sampler time as zero; real requests were roughly 4% slower. The request-path
    profile was added to close that blind spot.
14. **Board state is part of the result.** A kbench image, stale worker tree,
    alternate config, console owner, or post-reset timer state can invalidate a
    later lane even when source hashes look right.

## Resolving contradictions

Use the following procedure:

1. Identify whether the records describe the same source, config, model,
   prompts, board, boot, and phase.
2. Prefer a raw artifact with a hash over prose.
3. Check whether the later statement explicitly retracts or scopes the earlier
   one.
4. Separate observation from causal inference. A timeout is an observation;
   “stdout deadlock” was an inference later retracted.
5. Preserve both results when the base changed. Composition and memory pressure
   make a result legitimately base-dependent.
6. If the quality oracle itself is state-dependent, stop and request an owner
   decision; do not edit the oracle during a performance lane.

## What “bit-exact” means here

The phrase is scoped:

- **kernel bit-exact**: output float bit patterns match the reference kernel for
  the tested shapes and operand placements;
- **host byte-exact**: generated raw text and token counts match frozen host
  outputs;
- **device byte-exact**: the real firmware request outputs match frozen device
  outputs in that boot/session order;
- **behaviorally equivalent**: routes/tools/results match even when the raw
  rendering differs.

Never promote one scope into another. All three durable candidate trees have
host, device-breadth, numeric/top-1, and own-tree CQ2 evidence. B1 alone has the
unseen-prompt host/device cross-check; B1 and B3 have 9/9 behavioral captures;
and B3 has the ten-request repeat soak. Every tree's device breadth remains
18/20 until the owner resolves the two transport/state cases.
