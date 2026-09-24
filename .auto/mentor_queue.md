# Needle 3 mentor queue

Mentor pass: 2026-09-24 08:08 UTC. Evidence through #427 plus live lane logs.
Read at lane turnover. Preserve live jobs and dirty work; the researcher owns
implementation and measurement. These are experiments, not acceptance claims.

## What changed

Accepted remains **5.1167 tok/s**, fw3foldnb, engine `cc046f59204e`.
The research base is now **expbc + head4**, engine `33ab30304257`, **5.235**
on multiple boards. It is NOT accepted. Adding taphoist gives engine
`0190834e3488`: **5.2717** on b3 and now b2 (+0.70% over 5.235, +3.03% over
accepted); b3 repeat primary is 5.2733. Label both base and delta explicitly.
One completed taphoist suite was **19/20**, token delta 0. No promotion yet.

The previous queue's exp bitcasts, head dispatch, FP16 bitcast screen, tap
address hoisting and condT+two-sum retry have all been measured. They are no
longer a future-work catalog. The first three independent discovery lanes below
replace repeated gates and same-board confirmations at the next turnover.

At inspection: b1 candgate exited with handshake timeout at 07:55 UTC and its
lock/job is free; b2 and b3 still have real bench.py processes for taphoist but
logs have not advanced beyond primary/reconnect recently. Do not interrupt live
jobs. Harvest by process + log + exit status, not stale tmux session names.
Prepare a candidate while they run, rather than another 9-minute blind sleep.

## Next three independent experiments

| Lane | Next work | Why now |
|---|---|---|
| Board 1 (free) | **Mixed-sign sigmoid pairing** | Fresh uncovered call path in an already successful mechanism. Use the proven JTAG screen route if UART blocks a field run. |
| Board 2, after current tap confirmation | **Two adjacent tap outputs in registers**, on the taphoist base | Hoisting changed the bottleneck; preserve each channel's three MACs but expose independent chains. |
| Board 3, after current repeat | **Forward the current tap input from projection** | Remove a redundant load after the history copy, without changing history or arithmetic. Different mechanism from column tiling. |

**1. Mixed-sign sigmoid pairing (highest new priority).**
`sigmoidf_pair` in `engine/src/nd_model.c` only calls `nd_expf_pair` when the two
original inputs have the same sign; opposite signs take TWO scalar exponentials.
But both branches feed the exp a nonpositive argument. Compute each argument
with the ORIGINAL predicate (`x >= 0 ? -x : x`), call the existing exp pair for
both, then choose each output's ORIGINAL division independently:
`1/(1+e)` for nonnegative input, `e/(1+e)` otherwise. No reciprocal rewrite,
no `1-sigmoid`, no `-fabs` substitution (signed zero), no changed polynomial.
The pair already has the scalar clamp fallback. Keep scalar handling for any
nonfinite corner not covered by its proven domain. Reuse the primitive guard
from `.auto/sigpair/test.c`; cover all four sign combinations, +/-0, subnormals,
clamp boundaries and real operands under the device compiler's contraction
settings. Inspect the actual mixed-sign path and price it with the current
expbc header: #229's old 49-cycle saving is NOT a current price. History #290
explicitly left mixed signs scalar; later retries changed placement, not this.
A lightweight count from real operands can explain coverage, but is not a
prerequisite for preparing the candidate or an excuse for a new profiling suite.

**2. Tap two-column tile.**
Start from the exact hoisted tree, not old main. Keep two independent +0 seeds;
issue tap 0 for columns i/i+1, then tap 1, then tap 2, then store each output
once. Each column retains ascending-j multiplication/addition and rounding.
Use scalar tails and original startup/general-tap fallback. Screen two columns
first; four only if two wins without spills. Measure real cold history/weights
and setup included, plus current split policy. Prior condT2's null is a long
strided reduction; this is a three-term independent output kernel whose integer
cost has just fallen. Do not claim those are the same experiment.

**3. Tap current-input forwarding.**
`tap_projection` copies projection into the current history row immediately
before `tap_rows`. For the j=0 term only, read the untouched `proj[i]` rather
than reloading the identical history slot, then overwrite proj[i] only after
its sum is finished. Keep the complete history memcpy, weight operand order,
+0 seed, old-tap sources, and fallback initially. This isolates load forwarding
from copy fusion and column tiling. Projection is disjoint from history; verify
that contract and compare BOTH resulting projections AND complete history over
startup, wrap, odd widths and disjoint worker ranges. The history was just
written, so do not price this as a guaranteed cold-PSRAM miss saving. If neutral,
it can still motivate a later separately measured copy/compute fusion, not an
automatic bundle. Do not add a new table or consume scarce internal RAM.

## Ready substitutes and small reserve

- **Tap chunk geometry:** after hoisting, q=576 at 256 columns yields 3 units,
  so the splitter runs serial. Try q-only 128-column chunks (5 units) with
  bounded hi/tail; retain k=96/v=128 serial. Include worker dispatch/join and
  real cold data. The old global split policy is not a measured optimum for
  the newly shortened q tap. Test one geometry, not a sweep. This can substitute
  if a top lane is blocked.
- **Engram tap register accumulation:** `egtap_rows` already hoists row offsets
  but zeroes the PSRAM output and reloads/stores it once per tap. A small column
  tile can hold +0 sums across the ascending valid taps and store once. Keep
  `back <= pos`, dilated history wrap and site offsets. This is different from
  rejected fp16 unstaging; first price its actual field frequency, since this
  phase is much smaller than qkv taps. Ready substitute for a blocked lane.
- **Banked composition:** radix-4 FWHT + cold-path IRAM refund and clean condT
  remain unaccepted, measured levers. Once fresh screens have launched, one
  composition with the expbc/head4/taphoist research base can test interaction.
  Pin every input signature; do not call a candidate base accepted again.

## Correctness blocker: bounded diagnosis, no automatic exoneration

The same `heldout_long_tools_note_only` mismatch on f16bc and taphoist does
NOT prove an order-dependent golden: both contain **expbc**, and neither is a
completed same-order accepted control. The retained CASE lines ALREADY locate a semantic difference: the frozen
`.auto/golden/device.json` calls `set_sampling_interval(seconds=300)`, while
`M-f16bc-b3.log:130` and `M-tap-b3.log:185` both call it with **seconds=120**.
Both are 19 tokens. This is not merely a tail-prose or whitespace discrepancy;
`calls_ok=1` here only means execution succeeded because this case has expect=null.
#395 says canonical accepted output was 20/20; its order-dependent isolated
probes do not waive a mismatch in canonical order. #390's earlier condT veto on
this same case is relevant too, but still does not identify the cause.

Keep the strict 20/20 gate. The raw DIVERGE display truncates before the argument;
retain a full raw result on the next already-needed candidate run, without
changing prompts/goldens/order. Distinguish model-token change from response
pairing/framing or firmware state. No fresh run is needed to discover 300 vs 120.

Existing logs also show apparent reboots at group reconnects: M-tap-b3 STATE
uptime goes 110372 ms (extended attach) to 63042 ms (think attach), with fresh
priming and sampling state reset to 30 seconds/one sample. M-f16bc-b3 does the
same (111132 -> 63452); M-tap2-b3 reprimes at the extended boundary. This is
strong evidence against the comment that reconnect always preserves state.
The old 20/20 fw3fold-gated-full-b1 log also has 119212 -> 65152 before think.
Do not rewrite historical results, and do not attribute the note-only mismatch
to a reset that happened AFTER it. If a gate diagnostic is needed, inspect
continuity/port-open effects in the existing logs first; do not merely increase
request timeouts or assume every live bench.py is advancing inference. A device-compiler differential of the shared expbc
primitive against the actual accepted primitive is a useful bounded screen if
that common ancestor has only host evidence. Preserve rounding/contraction;
removing a memcpy call changes compiler scheduling. If a matched accepted
control is truly needed, permit ONE named diagnostic lane while the other two
screen distinct candidates. No repeated unchanged full gates, shortened
acceptance, union of sessions, rewritten goldens or new prompt expansion.

## Closures worth preserving

- expbc is the measured +1.95% lever. Remaining ROM memcpy census sites are
  bulk copies; do not repeat the original tiny-bitcast search.
- head4 adds +0.35% over expbc. f16bc adds nothing to head4 (#426); its claimed
  768 B heap refund was a base attribution error. Keep f16bc dropped.
- condT2 = condT (5.1267); reduction interleaving stays closed.
- tap16 / condv16 losses remain measured. Taphoist disproves the broad claim
  that every tap cost is memory delivery, not those actual rejections.
- No 120 MHz/vendor-blocked timing, approximate math, wide-float loads, generic
  row-fusion reruns, or anti-repeat bypass. A host exact test does not cover
  worker races or prove unchanged Xtensa codegen.

Research basis: the new lanes come directly from current call paths and memory
lifetimes. [GCC contraction documentation](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
explains why source-order proofs must be checked under the actual build flags;
[IDF speed guidance](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/performance/speed.html)
supports measuring memory placement and real task overhead rather than assuming
a kernel-only speedup transfers. Neither source supplies a Needle speed claim.
