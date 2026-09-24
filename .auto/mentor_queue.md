# Needle 3 mentor queue

## Immediate mentor redirect — 2026-09-24 23:03 UTC

B4W is a **discovery winner, not an acceptance proposal** while the frozen gate
is 18/20. Owner permission cannot replace unchanged quality. Shipping stays
5.3033. Stop reserving B1 for confirmations or waiting for admission.

- **B1 now: B4W attention transplant onto accepted bundle5 `2c79104`.** This is
  a NEW performance candidate with a quality-passing ancestor, not another
  control. Mentor verified accepted / main / preserved `nd_model.c.seed` are
  byte-identical (`0d639424f636`), so the B4W-only model diff is portable. Preserve
  all worker dirt first; assemble this candidate in an isolated tree or from
  exact preserved inputs, never reset a dirty worker. Keep frozen complete
  fixtures, harness guards and safe clocks. Run host gate, then this candidate's
  full 20-case screen once; compare with that board's bundle5 pin. Do not quietly
  carry the unaccepted quant/assembly/scheduler stack into this transplant.
- **B2 FINPAIR and B3 DOT8W:** leave live jobs alone. They are valid incremental
  experiments on B4W; compare with same-board B4W 5.6967 / 5.6983, not just seed.
  Do NOT rebuild FINPAIR on seed merely for a measurement ladder, repeat B4W
  for noise, or complete an A3 board matrix. Existing references suffice.
- **New turnover evidence: DOT8W FINISHED at 5.7283 on B3** versus B4W
  5.6983 = +0.526%; 6/6 restricted exact, delta 0, heap 4823, LANE_RC=0.
  FINPAIR finished at 5.6967 on B2 = exactly null. Harvest both; do not wait.
  B2's next slot is DOT8W's one cross-board FULL gate (fresh tree for B2),
  while B1 transplants B4W and B3 tests the independent QK schedule below.
- **B3 next: QK two-column operand tile on B4W, not wider unrolling.** Mentor
  inspected the exact B4W B1 ELF (22:54 build, attn_heads size 0x186b). The QK
  loop 0x4037c4dc..0x4037c575 spills/reloads f13 through a1+0x460 EACH four-column
  iteration (ssi 0x4037c4fc, lsi 0x4037c510); it ends in bnez. Thus the earlier
  no-spill observation for P.V does NOT describe QK. Try one two-column tile
  holding qA0/qA1/qB0/qB1 and k00/k01/k10/k11, updating all four scalar sums
  before fetching the next tile. Maintain the original per-sum pair grouping
  and ascending column order; target graph is mul(second product), madd(first
  product), then add to sum, not fusing across the pair/sum boundary. No fast
  math. Four sums + eight operands leave room for temporaries in 16 FP regs.
  Inspect actual spills, loaded K reuse and loop form, then screen vs B3 B4W.
  This is a register-lifetime experiment motivated by a measured spill, not a
  declaration that attention is exhausted and not a 16-wide sweep.
- Source precision for B1: **main HEAD is NOT bundle5** (main provenance
  b1daae10df90); only nd_model.c is identical. Extract engine/esp32 sources
  from git object `2c79104`, retaining the current hardened harness/frozen
  complete fixtures. Hash files in the canonical glob order, not ls-tree
  alphabetic order: src/*.c, src/*.S, include/*.h, main/*.c. B1 bundle5 pin is
  5.3033. Do not expand harness/coverage or wait on another authority discussion.

The accepted-base transplant can both recover a shippable gain and determine
whether the two frozen failures follow the inherited stack. An 18/20 transplant
still fails: record it without changing goldens or declaring boot-state causation.


Mentor 2026-09-24 20:54 UTC + session-3 updates through #616 and lane logs.
Keep worker dirt, locks, anti-repeat history, 240/80 MHz, frozen goldens and
all quality gates. Never interrupt a live build/flash/benchmark. No foreground
sleep >20 s in this harness; harvest lanes from their logs.

## State (22:40 UTC, session 3)

**Accepted: 5.3033 tok/s**, bundle5 `2c79104`, engine `0c1a6272cd01`. Unaccepted
discovery seed `61861dd9886c`: b1/b2 5.6117, b3 5.6133.

| Board | State (23:00 UTC) | Next |
|---|---|---|
| 1 | B4W gates DONE both boards (5.6967/5.6967, ext 5.6138/5.6146, think 4.44, 18/20 frozen pair, heap 5343). Tree = B4W, idle. | Confirmation board; integration prep if owner accepts B4W. |
| 2 | B4W fully gated. Screening **FINPAIR** (B4W + paired final 1/denom normalize, engine `622ca49c5edd`). | After screen: if sub-bar, restore B4W-or-seed and take the next distinct idea (QK-dot-side only). |
| 3 | B8W CLOSED (-0.03 % vs B4W same board, width saturates at 4). Screening **DOT8W** (8-column QK dot body, engine `5065619b0867`, B4W base). | If sub-bar: QK width axis closes too; b3 -> seed. |

**B4W = session acceptance proposal** (owner decision on the frozen 18/20 still gates shipping). Width CLOSED at 4. Fusion (A3 x pairing) CLOSED both forms. Composition rule re-proven: sub-additive wins interact negatively; only B4W-or-A3 layouts survive.

**Confirmed candidates (unaccepted):**
- **A3**: 5.6467 on b2 AND b3 (+0.62%/+0.60%), full 20-case gate = 18/20, the
  SAME two frozen heldout failures and token_delta 52 as seed #589; ext 5.5538,
  think 4.41, heap 8415. Host: 19/19 byte-exact. Differential: NOT bit-exact vs
  seed - adversarial 40-step host streams diverge from step 3, max_abs 6.104e-05
  (same class as the golden's own 5.341e-05 floor). Say "rounding-graph change",
  never "bit-exact", for A3/B.
- **B**: 5.6417 on b1 AND b2 (+0.533%/+0.536%), token delta 0, heap 5343
  (-3072 B) - the heap drop is the open risk; b1 gate + b3 breadth decide.
- **C closed**: +0.089% (b3, 9f3b8411f95c). Q tap+norm+RoPE one-callback join is
  measured and dead. Do not re-run unit-count variants.
- **B4W** (B's paired body at four-cell width; the two-cell width was an
  unmeasured register-pressure guess): 5.6967 on b2 (+1.516 %), every case up,
  heap same 5343. Session-best. objdump spill worry NOT realised.
- **Composed A3+B v1** (per-cell doA?/doB? branches): REJECTED (142 in-loop
  branches, -0.38 % vs B). **v2** (hoisted 4-way macro dispatch): REJECTED at
  5.615 (+0.059 %, ~1.4 % below B4W). A3-fusion x pairing is CLOSED - the wins
  are mutually exclusive layouts. No third fusion form.
- **B4W is THE candidate**: screens 5.6967 (b2) / 5.6983 (b3), margin +1.516 %
  on both pins, gate in progress on b1+b2. Acceptance will still hit the seed's
  frozen 18/20 pair - same disposition rule as A3: report, no waiver.

## Hard rules learned this session

- A restricted AUTO_GROUPS=primary screen APPENDS the shipping signature; the
  full gate on the same tree then needs the ONE documented AUTO_ALLOW_REPEAT
  allowance (reason recorded by the runner). Budget it per tree; never bypass
  exhaustion (rc=44).
- PROVENANCE engine_md5 = md5 of cat engine/src/*.c engine/src/*.S
  engine/include/*.h esp32/main/*.c - compute it with that exact recipe, never
  a single-file md5.
- Lane durations are ~2-3 min (build 17 s, prime ~60 s total), so harvest by
  short polls; never sleep on a finished lane.
- Host gcc is -ffp-contract=off here; a host differential between two engine
  builds is a real arithmetic comparison, and cross-tree patch application
  (A3-diff onto a board with other dirt) does NOT fail loudly - restore exact
  preserved snapshots instead of reverse-patching dirty trees.

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
