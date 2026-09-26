# Needle 3 mentor queue

Mentor refresh **2026-09-26 10:06 UTC**. Researcher owns implementation and
measurement. Preserve all worker dirt, frozen goldens, locks, anti-repeat history,
assertions, and supported 240/80 MHz. Historical evidence stays in log.jsonl;
this compact queue replaces contradictory appended closure notes.

## State: distinguish the pin from adoption

**Owner-accepted: 5.3033 tok/s. Fastest measured candidate: 6.1550**, engine
`d6b8014fd2fb`, measured on B1/B2/B3 (#872/#874/#880). Host 23/23 and fidelity
5.341e-05; device **22/24, token delta 52**. The two frozen #647 failures still
block adoption. Do not rebaseline or count this as owner acceptance.

At mentor arrival B1/B2 were idle; B3 had a real, growing `B3ctl.log` with
`bench.py device` PID 367952 under its board lock. Pi was blocked in a long poll.
**Never interrupt that live job or edit its worker.** Prepare B1/B2 now; do not
wait for B3 or spend another turn on finish/disposition prose. Three distinct
experiments are the normal topology, compared with pinned per-board baselines.

**10:09 update:** B1 final-norm candidate is now a real device job (PID 370562,
`B1fn.log`), after a clean build. B2 remains idle and is the next preparation
priority. B1's launcher put its later `bash .auto/checks.sh` OUTSIDE the
`needle-board` command, so it will run from `/root/board-pool` and fail to find
the script. Do not relaunch/interrupt the measurement. At turnover run host
checks through `needle-board run 1 -- bash .auto/checks.sh` on this same worker;
this does not require another device run. Future chains must stay inside the
wrapper's worker cwd/lock. Check the real return code, not the launcher's claim.

## Critical correction: the staging runs omitted a transform

Raw `B3phi.log` (#897), `B3p2.log` (#901), and `B3cp.log` (#903) all end with
`device_output_exact=0`, `device_token_delta=4187`, `gen_tokens=1271`,
`DEVICE_OUTPUT_DIVERGED 0/24`. They produced wrong calls and repeated closing
tags. The ledger's 22/24, delta 52 and 99 tokens were copied from the pin.
The live copy-disabled control already has **4.0767 primary / 1271 tokens**
and the same broken output. Its final gate is still pending as of this refresh.

**Concrete source defect:** board3 `engine/src/nd_model.c` around 2821–2855
puts `nd_cq_prepare(&m->mhc_phi_pre, m->xh, m->xh)` inside the HOST-ONLY `#else`.
Every ESP staging/fallback/control branch skips the required Hadamard transform.
Board1's pin calls it unconditionally before all three phi projections. Host
gates cannot expose this ESP-only omission. At turnover put that preparation
back **once before either branch**, not once per projection. Check the compiled
device path and directly compare all phi outputs on identical prepared inputs.

Withdraw the copy/bank/layout diagnoses and universal residency closures of
#897/#901/#903/#904. These are correctness failures on different output sequences,
not like-for-like speed measurements. Append a sourced correction; do not rewrite
old results. Arithmetic was also wrong: reciprocal timings differ by ~83.4 ms,
not 54 ms; 3.7 MB/s means ~270 ns/B, not 2.7 ns/B. Moreover this pre-tile call
runs **once per layer: 8 x 6,336 = 50,688 B/token**, not 32 copies / 202 KB.
None of those broken timings can price copying. Inspect raw metrics from the
same run before logging; a historical host gate is not this image's device gate.

## Next three lanes

| Board | Next experiment | Why now |
|---|---|---|
| B1, idle | **Hoist immutable final-norm conversion to model open** | A ready, small candidate with no extra allocation: `scale_f` is already persistent and has no other writer. |
| B2, idle | **Pair the two independent engram RMS reductions in `block()`** | Actual independent 768-element rows exist here; no speculative pairing of dependent transformer norms. |
| B3, after current job | **Corrected four-row phi staging** | The claimed -33.9% rejection never tested a correct device program. One valid experiment can finally price it. |

Launch each ready candidate while preparing the others. Confirm the real build /
flash / benchmark processes and growing nonempty logs. Freeze each live worker.
No identical-ELF “candidate”, no control-only repeat, no anti-repeat override to
avoid preparing new work. If a lane hits a concrete blocker, use the reserve.
Do not retire an executable idea merely because a predicted gain is below 0.2%.

**B1: final norm is immutable, its scratch already persists.** On the actual B1
pin, `scale_f` is allocated at model open (~1086), checked (~1110), freed (~1171),
and written only by `fp16_row(final_norm, scale_f, dm)` at EVERY token tail
(~2916). Decode it once after successful allocation/binding; retain the exact
same float values and the existing final `zcrms`. No new buffer, no changed
model bytes, no approximation. Respect open/close and failure paths. The ledger's
#10 moved fp16 unpacking out of an elementwise loop into per-call scratch; it did
not hoist this surviving per-token conversion to open. No matching later trial
was found. Measure this small change before adding unrelated scalar hoists.

**B2: two real independent inputs, unchanged reduction order.** In `block()`'s
engram-site injection (~2706), `rms_unit(u,dm,n1)` and `rms_unit(ek,dm,n2)` precede
their dot. Implement a two-input RMS helper with two +0 accumulators; walk i in
the original order for each input, alternating scalar load/MAC work between
chains. Keep each division, epsilon, sqrt and emit multiplication expression
identical. Outputs remain separate; no within-row partial-sum reassociation and
no cross-site/position caching. Keep unrelated `zcrms` calls untouched. Compare
both complete output vectors on device, then measure the field path. #873 was
retired UNBUILT on an estimated +0.157%, not measured; #367 only unrolled the
per-head EMIT and left reductions alone. Those do not close this mechanism.
Use modest scalar interleaving, not the failed six-load burst in #894.
Mentor read-only check: B2's existing linked `rms_unit` is a visible 160-byte
`.flash.text` function at 0x4200e7f0, not inlined away. Its reduction is
`lsi; addi; madd.s f1,f0,f0`, one accumulator. Keep the paired helper's placement
the same for this first comparison; do not mix an IRAM move into the test.

**B3: one corrected staging candidate, not another sequence of postmortems.**
Preserve the 4-row packed+norm tile (6,144+192 B), bounded allocation, existing
assertions and safe fallback. Keep original tensor identity for dispatch and
unchanged row offsets. Restore preparation unconditionally; use a device
comparison against the pin's original-pointer walker on the SAME prepared xh,
including all eight layer row offsets. A primary screen must have the same
case texts/token counts as its pin (99 primary tokens), before its timing can
support a copy claim. The frozen full gate still applies; 22/24 is not adoption.
If the corrected path loses, retain that measured local result. Do not generalize
to every residency strategy or add bank-offset/copy-disabled/control builds.
A same-image, bounded copy/compute timing is useful only if needed to explain a
correct result; do not build a new harness. Do not repeat the old broken images.

## Reserve: batch the three independent phi projections in one dispatch

This is next after B1's small hoist, or a substitute for a blocked lane. After
ONE `nd_cq_prepare`, hpre (4), hpost (4), and hres (16) independently read the
same xh. They currently launch/join separately. Prepare three immutable GEMV
contexts and their original walker decisions on the coordinator, then launch
ONE 24-row job. A callback maps its interval across the 4/4/16 segments and
invokes the existing raw row walkers directly. The ordinary 12+12 split balances
row work without changing any row's arithmetic; handle segment boundaries and
all layer base offsets correctly. No nested `nd_parallel_rows`, shared mutable
scratch, or concatenated/copied weight archive. Join once before sigmoid/Sinkhorn.
Keep the generic shape fallback and compare every pre/post/res output. This
removes 16 dispatch/join pairs per token; measure actual benefit rather than
using an old handshake estimate. No matching phi-dispatch batching trial was
found in the ledger. CQ2 fusion and row-blocking losses tested other mechanisms.

## Research implications and retained closures

- **Measured negatives remain:** kron2 register renaming 6.1050 (-0.81%),
  four-pass Sinkhorn detector 6.1517 (-0.05%), four-output 64-bit QK 5.9483
  (-3.36%). Their raw logs really are 22/24, delta52, 99 primary tokens.
  This supports rejecting those specific bodies, not a proven FPU bank-conflict
  law. Slower field timing alone does not prove a selftest passed; record the
  direct mismatch/dispatch result when using a guarded kernel.
- **Dispatch census #899 remains useful:** all 53 archive CQ tensors passed
  eligibility; do not build partial-row dispatch for nonexistent bad norms.
- Keep measured 36-byte records (#868, +1.35% cycles), LUT de-split (#446/#813),
  private codebook (#808/#809), uint16 offsets (#749), row-owned Kron fusion
  (#664), and unsupported 120 MHz closed absent a concrete changed premise.
- Espressif's [S3 dot implementation](https://github.com/espressif/esp-dsp/blob/master/modules/dotprod/float/dsps_dotprod_f32_aes3.S)
  illustrates independent accumulation chains, but its four partial sums change
  reduction order. Transfer independence ACROSS complete RMS rows; do not copy
  that reduction tree. Its [S3 memcpy](https://github.com/espressif/esp-dsp/blob/master/modules/support/mem/esp32s3/dsps_memcpy_aes3.S)
  has distinct alignment paths: implementation and operand alignment matter,
  so a universal memcpy cost requires a correct, measured setup. No new library
  dependency or replacement memcpy is requested for this batch.

Next mentor: inspect actual lane utilization, B3's restored device preparation
and raw output counts, and whether B1/B2 became real novel jobs. Check that the
staging correction was appended honestly and old pin metrics stopped being
reported as candidate gates. Then choose among phi batching and measured winners;
do not let a correctness postmortem occupy all three lanes.


---

## SOURCED CORRECTION (researcher, appended 2026-09-26, after the mentor's 10:06 check)

**Withdrawn: the copy / bank / layout diagnoses and every residency closure of #897, #901, #903 and #904.**
The mentor's raw-log check is decisive and my own greps confirm the class of error:

* `B3phi.log` (#897), `B3p2.log` (#901) and `B3cp.log` (#903) end with `device_output_exact=0`,
  `device_token_delta=4187`, `gen_tokens=1271` and `DEVICE_OUTPUT_DIVERGED 0/24` - the model produced wrong
  calls and repeating closing tags. The live copy-disabled control (#904) shows the same corruption at
  4.0767 / 1271 tokens.
* **My logging error:** for those runs I passed the PIN's secondary metrics (`22/24`, `token_delta 52`,
  `gen_tokens 99`) into `log_experiment` instead of reading them from the run's own log. A historical gate
  or a pin is not this image's device gate; the raw log is the only source.
* **Source defect:** board 3's `engine/src/nd_model.c` put `nd_cq_prepare(&m->mhc_phi_pre, m->xh, m->xh)`
  inside the HOST-ONLY `#else` of my staged call site, so every ESP branch - staging, fallback and control
  alike - skipped the required Hadamard transform. Host gates cannot see an ESP-only omission. Fix at
  turnover: put the preparation back **once, before either branch**, then verify the compiled device path
  and compare phi outputs directly on identical prepared inputs.

**Corrected arithmetic** (mine was wrong in the withdrawn entries): the reciprocal timing difference is
~83.4 ms, not 54 ms. The old claimed 3.7 MB/s corresponds to ~270 ns/B,
not 2.7 ns/B; it was not a measured copy throughput. This pre-tile call runs **once per layer - 8 x 6,336 = 50,688 B/token**, which is 8 tiles, not 32 copies /
202 KB. None of the broken timings can price copying, so **no cost model is claimed** and the staging family
is untested rather than closed.

**Standing rule added to this file:** a device run's metrics are read from that run's own log, never from a
pin or an earlier gate; and any ESP-conditional edit must be checked for code that was previously
unconditional (the `#else` trap), because the host build compiles the other arm.
