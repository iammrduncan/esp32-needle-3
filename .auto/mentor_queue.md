# Needle 3 mentor queue

Mentor refresh 2026-09-25 01:35 UTC. This replaces the stale idle/converged
handoffs. Continue discovery now; waiting for owner admission is not a research
stop. Preserve every worker diff, locks, anti-repeat history, frozen fixtures,
quality gates and 240/80 MHz limits. The researcher implements and measures.

## Turnover priority — 01:44 UTC (read before older recipes)

New primary screens, read directly from completed lane logs:
- B1 KV staging helper `5a9b75f4c948`: **5.4650**, +1.048% vs its 5.4083
  composition pin; 6/6, delta 0, rc=0; heap **6191 (+512)**. A real win to
  preserve, NOT a shipping acceptance. `kv_stage_pair` is 120 bytes, frame 32,
  hardware LOOP, ZERO inner-loop stack accesses; parent remains frame 0x580.
  Combined parent+helper text shrank by 309 bytes. Source/log M-kvst-b1.
- B2 nonpositive exp `b580af16a62c`: **5.7367**, -0.174% vs 5.7467; 6/6,
  delta 0, rc=0; heap 4575 (-248). Retire this form. Corrected actual-helper
  comparison passed 25.6M values; its first draft's sign was fixed BEFORE build.
- B3 paired-rescale `18527a7a80c9`: **5.7150**, -0.552% vs 5.7467;
  6/6, delta 0, rc=0; heap 4063 (-760). Retire this branch/pairing form.
  M-rsp-b3 completed by 01:45; all three first-batch lanes are harvested.

Next lanes, after each current run ENDS and its source is preserved:
1. **B2: transfer ONLY the winning staging-helper diff to its preserved
   composition.** This is a different stack from B1's bundle5 base; does the
   gain also lift the 5.7467 discovery best? Remove only B2's own rejected NP
   helper/call changes from a saved copy, not anyone else's work. Primary screen.
2. **B1: accepted bundle5 + ONLY the existing late RX ring**, no attention
   family or staging helper. ONE full frozen suite, if this exact tree has not
   already been measured. This exception now has a concrete purpose: determine
   whether the common 18/20 blocker follows the transport change alone, so the
   growing attention wins can eventually be evaluated for unchanged quality.
   Current B1 already has accepted quant/asm/scheduler + ring: after saving the
   winning source, the isolated change is its model file back to accepted
   `2c79104` (`0d639424f636`), with ring/CMake/hardened harness retained.
   No reset, pacing, golden change or guard bypass; if signature guard refuses,
   inspect existing evidence rather than manufacture a new signature.
3. **B3: sink-aware cursor reserve below** on its preserved composition,
   unless its rescale screen reveals a stronger new direction. Keep this lane
   on new performance work while the other two resolve transfer and attribution.

Do not wait for CHECK_RC in precheck logs: these launchers do NOT emit it.
Their measured success is host 19/19 + fidelity/top1 plus the live child
measure/bench process. Harvest LANE_RC and METRIC from the actual lane logs.

## Verified starting point

At 01:34: pi exists, but no build/flash/bench/checks process and no advancing
board log; last log activity CHK-b1 at 00:01. All three boards idle.
Accepted shipping is STILL bundle5 `2c79104`, engine `0c1a6272cd01`, **5.3033**,
device 20/20. Neither faster proposal is accepted:

| Board | Preserved candidate and its own comparison pin |
|---|---|
| B1 | bundle5 + B4W/DOT8W/SELRES + late RX ring, engine `177bd44997fa`; #639 **5.4083**, ext 5.3377, heap 5679; #641 capture and #642 own host checks green. |
| B2 | seed-era composition `10c74c756ee5`; #635 **5.7467**, ext 5.6631, heap 4823, #637 capture green. |
| B3 | same composition; #640 **5.7467**, ext 5.6646, think 4.47, heap 4823. |

All three proposals read device **18/20, token_delta 52, rc=1**. Keep the failures
visible. Compare each new lane with its OWN pin above, then separately state
whether it clears shipping gates. Do not copy old extended/think metrics into a
primary-only result. Sources: `/root/board-pool/preserved/b1-recipeB/`
`nd_model.c.{seed,b4w,dot8w,selres,cmp-d8sr}`; logs in `batches/`.

## Winning mechanism — retained for the transfer

**B1 — isolate the KV byte-to-float staging in a small hot helper.**
The current composition ELF really has extra integer stack traffic in both
staging loops, BEFORE QK/P.V: B2 `attn_heads` K loop 0x4037c279..319 stores and
reloads four signed byte temporaries at frame +0x464..470 per eight conversions;
V loop 0x4037c381..413 similarly spills three. `float.s` is already native:
this is a large-frame/register-lifetime problem, not a software conversion call.
Extract the existing packed-word conversion of a PAIR of rows into one
`ND_HOT __attribute__((noinline))` helper with explicit src0/src1/dst0/dst1/n,
no context/model accesses, no new buffers, same float values and array layout.
[GCC noinline](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Common-Function-Attributes.html#Common-Function-Attributes)
keeps this as an actual call; inspect the result rather than assuming a refund.
Call it for K and V; keep QK, max, exponentials and P.V untouched. Inspect its
object for actual spill/address reduction, then primary-screen the integrated
candidate against B1 5.4083. Calls may cost more than they save: that is the
experiment, not a reason to declare the family closed. Check all 256 byte values
and row boundaries with the actual helper plus normal gates. Do not chase a
fourth helper spelling if codegen/timing is unchanged; take a reserve below.

B2 nonpositive exp and B3 rescale-exp pairing are now measured NEGATIVE above.
Do not repeat them or the completed primary screens. Their implementation
rationale is preserved in this queue's earlier revision and the worker diffs;
keep the actual candidate files before replacing them. The NP helper's correct
form is `(int)(z - 0.5f)` for nonpositive z, with generic fallback for values
outside [-88,0]; its arithmetic guard passed, so its rejection is on speed.

If a lane blocks mechanically, preserve it and immediately substitute a ready
reserve. Do not leave three boards idle while polishing one extraction. Use the
existing wrapper and per-worker checks; confirm processes plus nonempty growing
logs, not announced PIDs. No duplicate discovery candidates/live controls.

## Ready reserves / next turnover

- **Sink-aware slot cursor:** replace the TWO per-position-pair `kv_slot`
  calculations with one correctly seeded cursor per run, advancing and wrapping
  to `n_sink` at `window`. Current ELF still executes `remu` at 0x4037c158 and
  0x4037c176. Run #42's cursor was WRONG (`base % window`); it was not a valid
  timing rejection. The old null hoist is also not removal of these remainders.
  Seed with the actual `kv_slot(m,base)`; distinguish pinned-sink and recent
  runs, preserve pairs across wrap and the odd tail. First exhaustively compare
  slots with the original over small windows/sink counts/multiple wraps and
  actual 512-window geometry. No head/position reduction reordering.
- **Compact specialisation / cold fallback refund:** the paired callback is
  ~0x1a00 bytes, frame 0x580, and only 4.8 KB boot heap remains. Guard a hot
  specialization for actual qk=48/v=64/rep=6, retaining generic odd-head behavior
  in a separate cold helper (also guard full KV-group head-range boundaries).
  Prefer one shared body with constant parameters
  over repeated manual transcription. Verify map/heap and hot object, then
  measure; do not repeat the failed outline generator repair marathon. Do not
  assume code-size savings are themselves a tok/s win.

## Correct the attribution; do not turn it into another verification campaign

The recent “three proofs it is NOT arithmetic” claim is unsupported. Host and
device goldens ALREADY differ for these cases; 19/19 host proves host preservation,
not identical target arithmetic. The failing device counts 19/63 correspond
to host counts 23/67 minus its four forced tokens, and the visible calls agree;
that is a clue, not proof of cause. #633 changed BOTH attention AND the RX ring
relative to bundle5. A constant failure pair across images narrows the search but
does not establish timer causation. `run_inference` restores a cached prefix and
constructs suffix from query; router dispatch happens AFTER generation. Name an
actual state-to-token path before saying timer counters determine these TOOLs.
Also, M-rxlate2-b2.log retains RXQ only for 7/8-byte control commands; it does
not contain hashes of the failing prompts. “drop=0” there is not input identity.
No waiver/rebaseline/replacement of goldens is authorized. Owner admission does
not substitute for the unchanged-quality objective.

For the next acceptance attempt, favor a bounded discriminating experiment
(accepted bundle5 + ONLY the existing late RX ring, if not already measured)
over another full gate of an unchanged failing composition. Its sole purpose
would be to separate transport/boot/input effects from attention changes; respect
repeat guards. This is a later exception with an explicit interpretive purpose,
not a fourth simultaneous lane and not a reason to pause the three speed lanes.

## Retained boundaries and research rationale

Keep #624 P.V width8 null, #626 final normalization pairing null, #628 QKTILE2
null, #634 counted-QK negative and #636 counted-P.V negative as measured forms.
A3+pairing fusion lost twice; no repeat. These do not close staging, domain
specialization, pairing of newly adjacent scalar work, or all compiler choices.
Keep old CQ2 tie2/deeper prefetch/quad LUT, unsafe 120 MHz, approximate math,
FP16 taps, tier sweeps and harness expansion down.

[FlashAttention-2](https://tridao.me/publications/flash2/flash2.pdf) motivates
looking beyond dot products at scalar work and operand movement. The transfer
here is a research heuristic, not its GPU speedups or a new softmax reduction:
Needle already normalizes only at the end. The concrete three ideas above come
from this repository's code and new head-pairing structure. In particular,
4.8 KB boot heap and only ~40 KB runtime PSRAM in the completed gate's STATE
line rule against casually adding a whole fp32 KV mirror; boot free memory is
not the allocation budget after both prefix snapshots.

Next mentor: confirm real turnover on all three lanes, harvest speed plus target
codegen for staging/nonpositive-exp/paired-rescale, and keep accepted 5.3033
separate from discovery 5.7467 until the frozen failure pair is actually resolved.
