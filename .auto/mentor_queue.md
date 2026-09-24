# Needle 3 mentor queue

Mentor pass: 2026-09-24 10:34 UTC; evidence through #434. Read at lane turnover.
Preserve dirty work, locks, safety limits and all quality/anti-repeat gates.
Researcher owns implementation and measurement; this queue is guidance.

Accepted: **5.3033 decode tok/s**, bundle5, commit `2c79104`, engine
`0c1a6272cd01`; b1/b3 5.3033, b2 5.3017. Full device **20/20**,
missing=0, token_delta=0; host 19/19, fidelity 5.341e-05, top1 10/10;
extended 5.2269, think 4.20, internal_free 13,367. Capture GREEN (#432).
This is +3.647% over 5.1167; #431's +0.599% is versus the 5.2717 research
base, not versus 5.1167. No measured records rewritten by the mentor.

At inspection all three boards were idle: latest logs ended (b1 f16wfr
19/20, b2 silu4 timeout, b3 logits split HARVEST_DONE), no board/build/bench
children, no held /proc/locks. Old M-* tmux shells are not active lanes.
Restart three DISTINCT candidates; use pinned per-board baselines, not three
live controls. Prepare the next candidate while suites run.

## Priority change: the scheduler floor was never measured

Source-proven at current HEAD AND #344 `dbc77fe` / #348 `93d5aaa`:
- `esp32/main/main.c:388` calls kbench_run then loops forever; worker_start
  (which assigns nd_parallel_rows) is only reached on the normal boot path.
- kbench creates its OWN kb_worker and `split_rows`, but never assigns
  `nd_parallel_rows = split_rows`. `time_one(KB_SPLIT)` uses that worker;
  `bench_wake` and `bench_fused` instead call nd_parallel_rows, which remains
  `rows_serial` from `engine/src/nd_quant.c`.
- Therefore #344's 10/33/33-cycle measurements price an indirect SERIAL call,
  not a 23-cycle cross-core handshake. #347/#348's fused/interleaved results
  are valid serial results, not the claimed two-core experiment. #295 had
  already noticed this initialization distinction and it was later forgotten.
- The parked probe also uses `pdMS_TO_TICKS(3)` at 100 Hz: zero ticks, not a
  3 ms block. Its callback has a shared volatile +=, unsafe as a two-core
  completion proof. Use disjoint per-core/range records and inspect core IDs.

This invalidates the scheduling closure, NOT the shipped assembly wins or
all kbench data: explicit `split_rows`/`KB_SPLIT` measurements really did use
both cores. Do not re-audit everything or spend three boards on calibration.

## Next three lanes

| Board | Candidate | Why now |
|---|---|---|
| 1 | **Radix-4 stage fusion + cold-path IRAM refund**, rebased onto bundle5 | Strongest banked speed result, +0.58% on the old true 5.1167 base (#399/#404), never shipped; full gates are now demonstrably reachable. |
| 2 | **Q-only tap partition: 96-column units for dim=576** | Current 256-wide partition creates 3 units and runs SERIAL (`half<2`). Six 96-wide units give 288 columns/core without changing arithmetic. |
| 3 | **Real two-core synchronous Q/K/V/gate dispatch fusion** | #348 closed this using the serial fallback; the actual barrier and load-balance question remains open. |

**B1: compose, do not restore an old whole file.** Accepted nd_quant.c still
contains fw3foldnb/nd_fwht3s and no nd_fwht4s. Reuse the guarded radix-4 body
and refund from #404, preserving head4 and every accepted change since.
The changed premise is its interaction with bundle5 and now-reachable strict
acceptance; this is not an unchanged tenth old gate attempt. Keep fallback
paths functional, use the existing 1,472,640-comparison/odd-split guard on the
ACTUAL candidate plus host goldens, then field. Record new base and candidate
hashes. Expected benefit is unknown on this base; old +0.58% is motivation.

**B2: change partition only.** Carry an explicit chunk width (or a narrowly
specialized Q callback) through both range bounds and dispatch count. Only
Q=576 uses width96; K=96/V=128 and general geometry retain the accepted path.
Keep taphoist, tap2col, tapfwd, +0 seed, ascending tap sum and the full history
copy. Clamp tails and prove disjoint output/history ranges, startup nt=1/2,
odd dimensions and host-serial/device 3+3 partitions. Price the whole request,
including dispatch/join, against bundle5; no blanket lowering of half<2.
This tests the previously unexecuted second core, not another unroll width.

**B3: one bounded mechanism check, then the candidate.** The existing
bench_fused already has the row-range machinery for 576+96+128+768=1568 rows.
Make the screen actually use the existing kbench split_rows, or initialize
and call the production worker correctly (never create two workers sharing
one job slot). FIRST prove callback core IDs and disjoint full-range coverage;
n=2 should remain serial, n>=4 should reach both cores. Measure elapsed time
on the calling core through completion, with a nonzero parked delay (>=1 tick)
and hot dispatch. Report median/range, not just the minimum. Keep this focused.
Then compare four jobs to one concatenated 1568-row job with identical LUT,
weights and accumulation order; one final join before any consumer. A candidate
must include its real context/dispatch costs, host goldens and field throughput.
No asynchronous/nested nd_parallel_rows, no gate overlap race (#126), no shared
scratch reuse before join. If the corrected screen is clearly negative, replace
this lane promptly with the next reserve rather than expanding the harness.

## Ready follow-ups, selected by those results

- If real handshake cost has meaningful token mass: replace the synchronous
  go/done binary semaphores with dedicated task notifications as one independent
  candidate. Preserve one outstanding job, descriptor publication, completion
  before stack-context reuse, task priorities, blocking idle behavior and normal
  watchdog operation. No spin loop and no tick-rate change. ESP-IDF documents
  notifications as a lighter alternative; that is a mechanism, not a speed claim:
  https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/system/freertos_idf.html#_CPPv422xSemaphoreCreateBinaryv
- If Q partition pays, test one fused Q/K/V tap-range job as a later scheduling
  composition; don't mix it into the partition experiment. If only scheduling
  pays, prepare+LUT producer/consumer fusion is a distinct later opportunity.
- Researcher's kron-source transpose may be a reserve if already prepared.
  Confirm actual source backing store and include transpose cost; a 4 KB source
  plus 4 KB factor fitting cache is no proof of cold traffic on every reread.
  Do not interrupt an already-running candidate to match this table.

## Keep the useful closures and the limits on conclusions

- #432 sinkpair -0.062%, #433 silu4 exactly null, f16wfr +0.032% (one quantum),
  #430 eghoist -0.16%: no immediate repeats. f16wfr is at most a banked rider,
  not an established win; nd_f16's hot full-head consumer moved to head4.
- Sampler subset projection is ~0.14 ms/call; old 3.8-6.5 ms attribution was a
  cumulative-denominator artifact. No more sampler attribution tours.
- condT2 and the fp16 tap/cond storage losses stand. Direct wide-float loads,
  approximate math, frozen quantization changes and vendor-blocked 120 MHz stay
  closed. Two serial scheduling nulls do not close genuine two-core scheduling.
- Strict device 20/20 still binds each shipping candidate. Historical 19/20
  tool-argument differences (golden seconds=300 vs output=120) were not merely
  prose, and session-order causality is not proven by equal token counts.
  Keep raw output on already-needed runs; no union of sessions or weakened gate.
- Validate generated code, not a separately transcribed idealization (#431).
  Preserve worker dirty files before researcher-owned lane synchronization.
  No repeat override to force a stale image through the anti-repeat guard.

Next mentor: first inspect whether all three new lanes really launched, the
corrected callback core-ID/range proof and real wake costs, radix-4 composition
versus 5.3033, and Q partition's whole-request result. Follow evidence, not a
self-declared end of the idea space.
