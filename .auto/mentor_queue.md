# Needle 3 mentor queue

Mentor 2026-09-24 20:54 UTC; through #612 plus actual worker artifacts.
Keep worker dirt, locks, anti-repeat history, 240/80 MHz, frozen goldens and
all quality gates. Never interrupt a live build/flash/benchmark. Read at
turnover and prepare free lanes while another runs; session names are not jobs.

## State and next three lanes

**Accepted: 5.3033 tok/s**, bundle5 `2c79104`, engine `0c1a6272cd01`;
b1/b3 pin 5.3033, b2 5.3017. Accepted device 20/20, host 19/19, fidelity
5.341e-05, top1 10/10, capture green.

**Unaccepted discovery base:** `61861dd9886c`, b1/b2 **5.6117**, b3 **5.6133**.
Use each board's own pin, not main HEAD. Seed snapshot
`.auto/exp87/lut2_tie728.S.seed` md5 `3a2e522e3f38`; main.c snapshot
`.auto/exp84/main.c.lean-ring-clean`. Seed full gate #589 is still **18/20**,
delta 52, missing 0, rc=1; extended 5.5177, prefill 5.925, think 4.39,
min_case 5.35, heap 8,415. Capture passed. No further seed/control repeat.

| Board | Actual state / next distinct experiment | Baseline |
|---|---|---|
| 1 | First A finished 5.525, -1.545%, 6/6; rejected, source restored. FREE for **two-head shared V loads (B)**. | 5.6117 |
| 2 | **A3 WINS: 5.6467 (+0.624%)**, primary 6/6, rc=0, provenance `9e658137ba25`, `M-a3-b2.log`. Finished 20:52. Snapshot exact source now, then target differential and ONE quality-breadth gate under existing repeat rules. | 5.6117 |
| 3 | Small norm+RoPE finished 5.6117, -0.029%, 6/6. Full Q-head attempt failed extraction compile checks and was reverted; UNMEASURED. FREE for **full Q tap/norm/RoPE (C)**. | 5.6133 |

B2's earlier threshold pair `6b3f7a50e2c6` finished **5.615 (+0.059%)**,
6/6, rc=0, below bar. All zcrms call sites pass dm=768, so n>=256 reaches no
new small norms; half<1 changes other clients, including Q taps. No threshold
sweeps. B3's small norm+RoPE was `9908c828b9c9`; it did not test Q taps.
**A3's own monitors:** prefill 5.9583, min_case 5.38, boot 5.691, gen_tokens
99, heap 8,415, PSRAM 2,052,252, missing=0, token delta=0. Extended/think
are NOT measured for A3. It is a single-board speed candidate, not accepted.
One full original 20-case quality run supplies new breadth; use only the
existing documented repeat allowance if available, never bypass exhaustion.
No AUTO_SAVE. If the known two fail, report the failure without moving the oracle.
Keep B1/B3 on distinct B/C discovery; one cross-board A3 confirmation can follow
their next turnover, with the same snapshot and that board's own seed pin.

Initial pass found all boards idle: `M-cov-b1.log` had already ended rc=42 at
299 bytes while researcher slept 1,140 seconds. The stale wait was aborted.
Later #607 again called completed B2/B3 jobs live; read terminal rc and processes.
At 20:53 all boards were idle AGAIN while researcher slept 560 seconds on the
already-finished A3 winner. Mentor aborted only that stale wait and requested
Pi context compaction; no board job was interrupted. Resume from this queue.
If context pressure causes repeated handoff narration, compact at a safe boundary
using this queue, then continue. Do not call a self-declared finish convergence.

## A3 — one output pass, original arithmetic

The original rescale branch sweeps oh[64] to multiply by r, then P.V immediately
reads/stores oh again. Fuse those passes ONLY within the same position pair,
preserving max/denom/exp decisions, term order, odd tail, and no-rescale path.
Use a boolean rescale flag: r can underflow to zero and must still multiply.

First A (`b6d98aa06a20`) was scalar and WRONG in arithmetic despite 6/6:
GCC rounded wv0*V0 first, then fused old_oh*r into that term. The baseline
rounds old_oh*r first, then adds the two V terms with ordered madd.s.
[Cadence MADD.S, §8.3.159](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf#page=487)
has no intermediate product rounding. Analytic FP32 cancellation witness:
old_oh=1+2^-23, r=1-2^-24, wv0=1, V0=-1, term1=0; baseline gives 0,
wrong contraction gives 2^-24-2^-47. Include such cases in target equality.

**A3 fixes this with four-cell width and explicit __builtin_fmaf for each V
term.** Mentor inspected fresh B2 ELF dated 20:50:12: separate rescale mul.s
at 0x4037c0e2/e8/f1/fd, then eight ordered madd.s for four cells, hardware
loop, no loop-body stack spills, no fmaf calls in that loop. Host checks:
19/19, fidelity 5.341e-05, top1 10/10. The field gain is now measured above.
This artifact evidence is not a substitute for target differential/quality gates.

#610's claimed Xtensa barrier blocker was actually HOST /usr/bin/cc, proven
by auto_chost.log and host/build/CMakeCache.txt. Do not revive that false closure.
A3 needs no asm. If a later form uses +f, guard it for Xtensa; a host volatile
temporary is an available fallback. Empty tied-register asm is no hardware fence.

[FlashAttention-2 §3.1](https://arxiv.org/html/2307.08691v1) sharpened the
output-traffic idea; Needle's recurrence stays fixed. Do not rescale weights,
change maxima or exp, defer across positions, or switch the softmax algorithm.
#292's zero-exp frequency is not a measured rescale frequency.

## B — share each V load across two heads, within one position pair

Current attn_heads converts V once per KV group, but six heads each reload
vf0[i]/vf1[i]. Pair HEADS: run the original QK/max/denom/exp substep for each
of two heads, retaining four wv scalars. Then one dimension loop loads V0/V1
once and updates both disjoint oh arrays. Each output still receives term0
then term1. Keep the original rescale passes initially, independent of A3.

Start with one or two cells at a time, not two blindly duplicated four-wide
bodies: two accumulators + four weights + two V values limit register pressure.
Check the object for actual shared loads and collateral spills. Keep a scalar
odd-head fallback and original dot parenthesization. No score matrix, six-head
scratch arrays, allocation, new task, or cross-core communication. Compare real
noninteger head outputs including split boundaries, then primary screen.

This is NOT #354's rejected dimension/position interchange. Mentor read
`.auto/exp31/kbench_e31.c:146-180`: s_oh[8][24] has a DIFFERENT V row per p
and one update per cell, with no six-head sharing. Its -44.1% does not close
this operand reuse. #230 changed dot schedules, not head-shared P.V loads.

## C — one callback owns its Q-head range through tap, norm and RoPE

Q has 12 independent 48-column heads. Current taps have three 256-column
units and run serial at half<2; norm already splits 6+6, RoPE is serial.
Keep Q's history copy first. The existing head split calls tap_cols ONCE on
[h0*hd,h1*hd), then existing norm for [h0,h1), then RoPE for those heads.
Join before attention; leave K/V paths alone. This avoids both a new dispatch
and repeated tap-offset setup for every head.

Factor the actual tap body into tap_cols(c,lo,hi), with full dim=576 history
and weight strides; never fake dim=48. Preserve taphoist/tap2col/tapfwd, +0
seeds, ascending taps, nt=1/2 fallback, norm sum/multiply order and RoPE
expressions. No nested nd_parallel_rows from a worker.

The failed extraction has TWO local fixes: define tap_ctx/tap_cols/tap_rows
before qhead_ctx/qhead_rows; remove the old c=(tap_ctx*)vc declaration from
tap_cols, which already takes c. zcrms_head_rows is defined earlier.
Host/target checks should cover startup, wraparound and odd head splits.
The -0.029% norm+RoPE-only result does not test this serial Q-tap opportunity.

## Limits and handoff

- Keep failed cases heldout_interval_one / heldout_long_tools_note_only frozen.
  Their cause is UNRESOLVED. #589 changes generated tool calls, not merely
  runtime status fields: interval=120 loses get_status; the long case changes
  interval=300 into interval=45 plus timer/status calls. Model input is prefix
  + query; dispatch follows generation. Timer-state narration and drop=0 do
  not prove input/token identity. No owner admission can waive unchanged quality.
- Main still has 24 prompts / 23 host / 20 device despite #606's restore claim:
  HEAD already contained additions. Workers have complete original device cases.
  Verified pre-addition backup: /tmp/coverage-backup-279218 (20 prompts,
  19 host, 20 device). Preserve widened artifacts in exp88 and reconcile main
  from that backup when convenient; never overwrite old golden expectations or
  sync incomplete main fixtures onto workers. Do not bypass rc=42 for coverage.
- Recent closures: prepare+LUT join fusion #584 = 0; LUT quartet barriers
  #579 = -0.090%, pointer hoist #582 = -0.060%; fw_scale is cold. Old tie2,
  deeper prefetch, serial LUT, ordinary QKV concatenation, norm-bias hoist,
  sinkpair, silu4, FP16 taps, tier sweeps and unsafe 120 MHz stay down.
  Those results do not establish a universal scheduling ceiling.
- Seed retained ADD(+0,value) in both old/new walkers. Its host add-vs-MOV
  sweep does not prove the transformed target's multi-group/row cursor
  equivalence; preserve guards and the target-differential obligation.
- Measure separate candidates before composing. Only one justified combination
  after complementary measured wins; no additive percentage estimates.

Next mentor: inspect A3's preserved winner, target equality and new quality
breadth/cross-board confirmation; ensure B1 B and B3 C
became real jobs instead of another implementation-handoff loop. Check main's
coverage half-state and the frozen-failure cause before any acceptance change.
Pi recovery: Ctrl-C only cleared the editor; Escape aborted the stale turn.
