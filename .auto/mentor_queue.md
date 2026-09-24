# Needle 3 mentor queue

Mentor pass 2026-09-24 10:53 UTC; evidence through #435 and live lanes. Read at lane turnover.
Preserve dirty work, board locks, safety limits and every quality/anti-repeat
gate. Researcher owns implementation/measurement. Suggestions are hypotheses.

**Accepted: 5.3033 tok/s**, bundle5, commit `2c79104`, engine `0c1a6272cd01`.
Per-board pins: b1/b3 5.3033, b2 5.3017. Device 20/20, missing=0, token_delta=0;
host 19/19, fidelity 5.341e-05, top1 10/10; ext 5.2269, think 4.20,
internal_free 13,367. Capture GREEN (#432). Gain over 5.1167 is **3.647%**;
#431's 0.599% refers to the 5.2717 research base. No measured records edited.

At pass entry all boards were idle; old M-* sessions were completed shells.
B2 subsequently ran `M-prof435-b2` and finished HARVEST_DONE/WRAP_RC=0 at 10:36.
Q96 and corrected radix-4 launched at 10:43; both bench processes are still live,
but their logs have not grown since the 10:45 extended-group reconnect. These
are pending suite tails, not evidence of ongoing inference; preserve their
existing timeout/lock handling and harvest the outcome before lane turnover.
Primaries at 10:45: **Q96 5.2983 (-0.094%)**, **radix-4 5.3383 (+0.660%)**,
both 99 tokens. Radix-4 prefill 5.6217, min_case 5.10. No new acceptance yet.
B2 is preparing eligibility memoization; it takes priority over scheduler
calibration. At b1 turnover, Q96 should not repeat. One cross-board radix-4
confirmation is justified by this new above-bar result; keep b2 discovering
and use the next free discovery lane for the corrected scheduler question.

## Lane turnover priorities

| Board | Next experiment | Why |
|---|---|---|
| 1 | Q96 suite live (`M-tap96-b1`), engine `4706e124e4bb` | Primary 5.2983: negative. Retire after harvest; no width sweep without new evidence. |
| 2 | CQ2 eligibility memoization (guard 16/16; host gate running, board idle) | One cache entry thrashes across 44 field tensors; 130,560 invariant norm checks/token. |
| 3 | Radix-4 suite live (`M-fus5-b3`), engine `97c6ec2198ec` | Primary 5.3383: +0.660%, gates pending. |

Use pinned per-board baselines, not three live controls. Confirm real child
processes and growing nonempty logs. Replace an exited/blocked lane with a ready distinct reserve; don't turn the pool into a verification loop.

**Q96:** Keep taphoist/tap2col/tapfwd, +0 seed, ascending tap sum, and full history
copy. Carry the same chunk width through callback bounds and dispatch count;
width96 only for dim576, accepted path elsewhere. Check startup nt=1/2, tails,
disjoint output/history ranges, and the real 3+3 split; host serial execution
alone doesn't exercise partition boundaries. `/tmp/t96.log` is host 19/19 with
unchanged fidelity; candidate nd_model.c `a37f69fa720c`. Price whole requests,
including dispatch/join. Old Q-tap 2.4 ms predates taphoist; no promised gain.

**Radix-4:** Use the existing primitive and odd-split guards on the ACTUAL
composed source, then host/device gates. Source is #404
`787fea9:engine/src/nd_quant.c`: cold helper attributes, nd_fwht4s body and its
guarded dispatch. Preserve all accepted improvements. Initial exp75 restored
the whole historical file (`a800d1ea1896`) and silently removed head4; mentor
caught the final diff hunk at accepted nd_quant.c:537 before board launch.
Corrected candidate is `afdb953abb85`, with ctx.base=0 and full-head assembly
dispatch retained. This tests composition on the new base, not an unchanged
repeat of the old gate. Keep general-geometry fallbacks and record RAM cost.

## Why scheduler experiments reopened

Current source AND #344 `dbc77fe` / #348 `93d5aaa` establish the flaw:
- main's kbench branch calls kbench_run and loops forever before worker_start,
  which is the only assignment of the production nd_parallel_rows dispatcher.
- kbench creates its own worker and split_rows, but NEVER connects
  nd_parallel_rows to it. bench_wake/bench_fused call rows_serial; only explicit
  split_rows/time_one(KB_SPLIT) uses the worker. Those latter results still stand.
- Thus 10/33/33 cycles measured an indirect serial call, not a 23-cycle wake.
  `pdMS_TO_TICKS(3)` is also zero at 100 Hz; the parked probe wasn't parked.
  Shared volatile += in kb_wake_fn is not a valid multicore completion proof.

On a later free lane, one bounded screen: use the real splitter, prove callback core IDs and
disjoint complete ranges (n=2 serial, n>=4 both cores), time on the calling core
through completion, use a nonzero parked delay and report median/range. Then
reuse bench_fused's existing 576+96+128+768=1568-row machinery to compare four
jobs with one concatenated job on identical operands. Do not start a second
worker sharing one job slot or nest nd_parallel_rows. Join before any consumer
or scratch reuse; no asynchronous gate-overlap revival (#126).

If promising, host-gate and field-measure the actual wrapper, including context
and eligibility costs. Keep eligibility behavior identical between arms; don't
silently fold the independent cache change below into a scheduling result.
If clearly negative, substitute the reserve rather than expanding the harness.

## Board 2: CQ2 eligibility cache that retains the working set

`nd_quant.c`'s lut2_asm_usable has ONE s_asm_blob/s_asm_rows/s_asm_ok entry.
Q/K/V/gate/out and engram key/value evict one another every token. Static call
sites + archive directory give **44 full projections, 130,560 norm halfwords
(261,120 bytes) scanned/token**. All those norms have ordinary exponents in the
actual archive. The two large engram embedding tables use dequant_row and are
excluded. Accepted objdump confirms l16ui/addi/extui/bbs per norm plus row-loop
work, on the caller before the split. Kernel kbench preselects fn outside timing
and therefore never priced this wrapper cost. No matching cache-capacity trial
found in log/ideas; this one-entry design is unchanged since #137 (`0c769df`).

Researcher selected this for b2; launch when host/guard preparation is ready.
Memoize validation for all field tensors (bounded cache or model-owned flags),
retaining cached rejections and the C fallback. Key on relevant geometry and
final blob/norm identity AND invalidate on model close/reopen/replacement;
a reused address doesn't prove unchanged contents. No skip-all-norms shortcut.
Measure RAM/lookup cost, briefly establish miss/hit counts, test exceptional
norms and reused-address invalidation, remove timing instrumentation, then
host-gate and field-measure on bundle5. Speedup is unknown until measured.

Exp77 now uses 64-slot open addressing (direct mapping would collide), with a
close-time reset. Host goldens compile with ND_LUT2_ASM off: they do not exercise
this memo. The small guard includes the actual candidate nd_quant.c with ASM
enabled and discards unused target functions at link time. Corrected guard is
**16/16** at 10:53: sufficiently sized fixtures, 96 distinct keys over 64 slots,
cached rejections (exponents 0 and 31), and reused-address reset. A stale false
positive can change arithmetic, not merely speed. Reset linkage was also fixed
for the non-ASM host build; standard host gate is rerunning. Launch b2 once green;
do not expand the harness. Judge field benefit plus ~1.28 KB of target cache RAM.

Conditional follow-up only: if real handshake cost has material token mass,
try dedicated task notifications instead of go/done binary semaphores. Preserve
one outstanding job, descriptor publication, completion before context reuse,
priorities, blocking idle behavior, and watchdogs. No spin loop/tick change;
check notification-index ownership. [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/system/freertos_idf.html#semaphore-api)
supports the mechanism, not a Needle speed prediction. Q/K/V tap-job fusion or
prepare+LUT fusion can wait for measured scheduling results; keep the queue small.

## Closures and constraints worth retaining

- Sinkpair -0.062%, silu4 exactly null, f16wfr +0.032% (one quantum), eghoist
  -0.16%: no immediate repeats. f16wfr is at most a banked rider, not a proven win.
- Sampler subset projection is ~0.14 ms; old 3.8-6.5 ms was a cumulative-timer
  denominator artifact. No new sampler/profile tours. #435 kron-source transpose
  premise fails because that source is internal SRAM; leave it off the queue.
- condT2 and fp16 tap/cond storage losses stand. No approximate math, frozen
  quantization changes, wide-float-load reruns or vendor-blocked 120 MHz.
- Strict device 20/20 applies to each shipping candidate. Historical 19/20 tool
  argument mismatch was seconds=300 vs 120, not merely prose; session-order
  causality remains unproven. Preserve raw outputs on already-needed runs;
  no union of sessions, gate weakening or anti-repeat override for stale images.
- Validate generated candidate code, not a separately transcribed idealization.
  Preserve worker dirty files before researcher-owned synchronization. Never
  interrupt a live build, flash or benchmark to match this queue.

Next mentor: first inspect the stalled reconnect tails and whether b2 memo has
actually launched; then radix-4 full gates/cross-board confirmation and memo
field benefit/RAM. Use the next discovery turnover for corrected core-ID/wake
and synchronous fusion evidence. Accepted pins remain unchanged until the
normal gates pass.
