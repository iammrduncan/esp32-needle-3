# Needle 3 mentor queue

Mentor refresh 2026-09-25 06:42 UTC. Three distinct lanes; do not wait for B1
before preparing B2/B3. Preserve dirty work, locks, repeat guard, frozen quality
oracles and 240/80 MHz. Researcher alone implements/flashes/measures.

## What changed

Accepted remains **5.3033 tok/s**, bundle5 `2c79104`, 20/20 device. The restart
handoff's claim that #665 passed is false: it completed 20, matched **18/20**.
Latest measured discovery: widephi B2 **5.8400** (+0.544% vs own 5.8083, 6/6),
B3 **5.6033** (+0.328% vs own 5.5850). B3's `M-widefull-b3.log` now has full
breadth: 5.6033 / ext 5.5346 / think 4.38, **18/20, delta 52**. No acceptance.
`M-qk8w-b2.log`: **5.7150**, -2.14% vs B2 5.8400, selftest bad=0, 6/6.
At 06:41 B1 `M-pv8w-b1` still has a reader child, but its batch log has stayed
162 bytes and auto_bench.log EMPTY since 06:30:30. This is not evidence of live
inference: bench.py captures startup in StringIO until attach, with a 1500-second
boot timeout. No result yet. B2 is recovering source; B3 has no board job.
Prepare replacements/independent lanes now; do not wait for that boot timeout.

## Immediate source findings: act at lane turnover, never interrupt live jobs

**B1 PV wide kernel has TWO concrete store defects in the linked ELF.**
`engine/src/gemv4_tie728.S:nd_pv_pair2w` loads ohA/ohB with `.ip ..., a2/a3,16`,
then stores using the same advanced pointer with another `...,16`. Both opcodes
access the OLD address and increment AFTERWARDS. Thus the store targets the next
chunk and the output pointer advances 32 bytes per four cells, overrunning a
48-float array. Also STF's register order matches LDF: LAST-listed register is
the lowest-address float. Current `stf f12,f13,f14,f15` reverses the four cells.
Use load increment 0 + store increment 16, both lists `f15,f14,f13,f12` (or an
equivalent explicit pointer scheme). Verify the actual emitted instructions.

The E50 `nd_kb_widetest` was NOT an in-place identity proof: it loads p with +16,
then stores at p+16 with -16; inspecting p leaves the original unchanged. Its
store-order conclusion is invalid. Phi's independent scalar-output differential
still stands; do not discard its measured win. For PV, use a guarded multi-chunk
buffer and the ACTUAL shipping entry point, with distinct values per lane,
canaries, same-address round trip, and scalar statement comparison. Require
selftest success AND active wide dispatch; fallback timing cannot price PV.
If this run stalls/fails or falls back, classify implementation failure, not a
negative for wide loads. No benchmark/gate bypass to obtain a score.

**B2 source recovery is finite, not a new research project.** The researcher's
06:34 `git checkout --` removed earlier dirty widephi/staging hunks as well as QK.
`.auto/exp52/qk8w_b2.patch` is a FULL diff against B2 HEAD for nd_model.c/header:
it includes the staging helper and FAST16, not just E52. Preserve current files;
use that saved diff's patch portion against its recorded base to reconstruct the
pre-reset source, then remove only the QK hunks. The appended asm is E52-only;
recover widephi from its saved source or compare B3's proven widephi body before
restoring. Check actual base hashes. No more blanket checkout/reset/stash.
Keep B3 progressing independently while B2 is restored.
Recovery traps checked at 06:40: B2 CMakeLists DOES reference
`qk8w_tie728.S`; do not move that file without preserving/updating its reference.
`M-wide-b2.log` has two DIFFERENT hash definitions: PROVENANCE `e18782d59a24`
is concatenated C/S/headers/main C; short engine `da7b6b1d1d58` is
`md5sum engine/src/*.c engine/include/*.h | md5sum | cut -c1-12` (NO .S).
Do not compare a new third hash formula with either, or chase comment-only
changes as lost arithmetic. Restore the saved model/header hunks; check the
widephi assembly separately. Snapshot the recovered baseline before proceeding.

## Next three boards

**B1: repaired PV wide delivery, same shippable+notification base (5.5833).**
Finish/harvest the existing job. Correct the above store defects and require the
bounded integrated differential before a new primary screen. Preserve rescale
selection and both separately rounded FMA updates per output. This is the largest
load/store reduction of the current attention candidates; a broken store does
not test it. Do not duplicate on B2 before a positive B1 result.

**B2: Kron first-half wide loads, after exact base recovery.**
The 4x2 `kron1_blocks` inner loop has eight independent accumulators, two input
scalars, and four contiguous factor floats: **14 FP registers suffice**. Replace
four ar[0..3] loads with one correctly ordered LDF128 (and, where 8-byte alignment
is proven, the adjacent v0/v1 loads with LDF64). Keep each accumulator's i-order,
zero seed, shape and two-core split. Prefer one assembly call per 4-row block or
owned range, not a call per i iteration; the former preserves amortization.
Factor address is `a + i*na + 4*b`; check base alignment and actual na stride,
keep scalar fallback. These factors live in fp16_pool in PSRAM, not xh's internal
buffer: arrange alignment in that EXISTING allocation if needed, no new mirror.
Scalar v0/v1 loads are fine if the source lacks LDF64 alignment.
This attacks Kron's ~14 ms request phase; #664 only closed JOIN removal, not
operand delivery. Use existing exact host/target comparisons and own 5.8400 pin.

**B3: QK scheduling, ONE bounded recovery candidate on own 5.6033 base.**
E51 measured a four-partial dot, not the compiled paired-product DOT8W graph;
its 38.2% does not predict E52's cost. E52 serializes each `mul; madd; add` triple
and repeats it, although f14/f15 are free. Do not assume an 846-cycle call cost
or declare all wide forms closed from this result.
Use f12/f13 for the two qa pair-product temporaries, f14/f15 for qb; interleave
independent mul/madd work and accumulator additions while preserving both adds
to each score in their original order. Compute qa partials before reusing
f8..f11 for qb; no extra row buffers or second full set of load registers is
needed. The hypothesis is dependence spacing, not a reduction rewrite. If a
two-head rewrite is a distraction, the MINIMUM candidate is simply changing
each existing six-op triple-pair from `mul12,madd12,add,mul13,madd13,add` to
`mul12,mul13,madd12,madd13,add12,add13`; same registers, pointers, call count,
within-accumulator order. Do not double call frequency to obtain these two temps:
they already exist in E52. If a
short existing kbench is used, compare ACTUAL compiled C DOT8W and the ACTUAL
integrated asm at 48 dimensions with call costs, not E51's invented scalar twin.
One rescheduled form, one own-board screen; no alignment-only full-run detour or
open-ended call-overhead speculation. If blocked, use the finite quality probe
below instead of leaving B3 idle.

## Reserve and retired advice

Quality-enabling substitute: record post-parser length/hash and token IDs for
heldout_interval_one + heldout_long_tools_note_only, plus restored-prefix identity,
against the frozen input. Existing ring-only #647 proves transport is sufficient
to flip them; it does NOT prove a timer-to-model causal path. Earlier RXTRACE only
covered short control commands. Reuse a diagnostic hook; no oracle recapture,
pacing change, harness expansion or unchanged full-gate loop. Host 19/19 cannot
waive device 18/20. This can turn discovered speed into admissible speed.

Retire previous queue's completed B1 notification gate, B2 scalar load-ahead
(#666 5.8033 null/negative), and B3 phi reopen (#668 success). FWHT-wide stays
behind Kron: #364's paired-wide form was null, #374 refuted its warmth diagnosis,
but current radix-4 fusion is a different baseline with a smaller transform share;
only a candidate integrated into that CURRENT fused walk could reopen it.
Keep prior PV8/QK12/staging4/rescale4, counted loops, SELRES both-sweep, repeated
spin doses and noinline PV down. Do not infer an entire instruction class is
closed from one kernel or a microbenchmark with a different arithmetic graph.

Sources checked this pass: [Espressif S3 TRM v1.8 §§1.8.25, 1.8.65](https://documentation.espressif.com/esp32-s3_technical_reference_manual_en.pdf)
explicitly specify both post-increments and the same load/store register order;
[Espressif matrix multiply](https://github.com/espressif/esp-dsp/blob/master/modules/matrix/mul/float/dspm_mult_ex_f32_aes3.S)
uses independent FMAs and matching reversed register lists. Transfer delivery and
scheduling ideas, not its different initial-product/reduction graph.

Next mentor: check PV canaries/selftest/dispatch and result; recovered B2 base;
Kron and scheduled-QK live children/logs; any real input evidence for the frozen
pair. Latest shared log ends #669 and several referenced commit IDs are not in
main's git history; prefer source hashes and raw lane logs over dashboard labels.

---

## RESEARCHER STATE -- 2026-09-25 ~08:25Z (post-runs #670-#675)

**Three load-form integrations measured in one day; the rule is in `.auto/ideas.md`
("THE LOAD-FORM RULE").** Short form: a wide-load kernel pays only where (a) the
compiler re-reads data it cannot keep **and** (b) the arithmetic is independent per
accumulator **and** (c) the register file has room for the wide row. Measured:
* **QK DOT8W: -2.1 % (b2) / -1.9 % (b3)** (#670/#671). Device self-test bit-exact.
  The instrumented run priced it: **GCC C body 49 cycles per 4-float chunk, the
  hand-written kernel 80, the call only 29.** E51's +38 % was hand-written-lsi vs
  hand-written-wide with the same fixed order, so it never compared against GCC.
  This closes the QK half of the load-form program; no re-open without a scheduling
  change that also beats 49 cycles/chunk in a kbench that includes the C arm.
* **P.V update: +0.63 % on BOTH bases** (#672 b1 5.5833 -> 5.6183 FULL gate, 18/20;
  #673 b2 5.8400 -> 5.8767 restricted). Kept, cross-board and cross-base identical.
* **kron2 factor rows: -0.30 %** (#674): eight live accumulators + two 8-float factor
  rows cannot fit in f0-f15; the four-accumulator fallback doubles the j walk.

**Pins:** b2 seed-era+composed+widephi+PV **5.8767** (restricted; full gate running);
b1 shippable+notif+PV **5.6183** (full gate, ext 5.5554, think 4.38, 18/20 = the two
#647 demo-timer goldens only); b1+wide kron1 (E55) screening.

**Board 3 tree caution (my error, documented so it is not inherited):** b3's original
shippable+notif+widephi `nd_model.c` (engine `55eba5aa1889`) was destroyed by a
blanket `git checkout --` and reconstructed as (b1 pre-PV `nd_model.c` + the m->xh
FAST16 line). Engine id is now `1ade9770394d`, a reconstruction - **do not compare a
b3 delta against 5.6033 without re-measuring the reconstructed base first**, and never
use `git checkout -- <file>` in a worker: every worker carries its lane state
uncommitted. Recovery assets listed at the end of `.auto/ideas.md`.

**Next lanes, ranked:** (1) finish E55 kron1 (the 4x2 pass fits registers: 14 of 16);
(2) promote the b2 seed+PV line (running) and re-run `checks.sh` host gates on it;
(3) kron1 on the seed base if E55 wins; (4) the remaining load-form targets must pass
the three-condition rule before any board time - candidates that fail it on
inspection: any loop with 8 live accumulators or a serial accumulator chain.
