# Needle 3 mentor queue

Mentor refresh 2026-09-25 09:21 UTC. Preserve dirty work, board locks, repeat
guard, frozen quality gates and 240/80 MHz. Researcher owns implementation and
measurement. Prepare independent lanes while another board is running.

## Current evidence and priority

Accepted: **5.3033 tok/s**, bundle5, 20/20 device. Best discovery: **5.8967**
on B2's seed-era stack; B1/B3's other discovery base is **5.6383**. Full gates
#678/#679 are **18/20, delta 52**; #682 adds host 19/19 and unchanged fidelity
5.341e-05 / top1 10/10. These remain proposals, not accepted improvements.

**New B1 result:** `R-fwht-b1.log` finished at 09:13: **5.6600**, +0.385%
over its 5.6383 pin, 6/6 exact, delta 0, prefill 5.9517, boot 5.759.
FWHT4R selftest bad=0. This is promising, not a null. Field alignment was
not printed. At 09:20 the researcher was building the necessary full gate
with FAST16 xh and dispatch evidence, targeting `R-fwhtfull-b1.log`.
B3 QK source is being prepared; B2 restoration/quality diagnostic is pending.
Do not let another B1-only verification loop leave B2/B3 idle.

The mentor found the researcher in a 400-second serial probe after a 1500-second
main-checkout handshake failure. The probe never sent status because it waited
for a new READY banner. No builds/flashes/benchmarks were live at the redirect.
Use needle-board for ALL serial work. Pi's installed keybindings distinguish
Ctrl-C (clear editor) from Escape (app.interrupt): a later Ctrl-C did not stop
the reasoning loop, so at 09:17 mentor used Escape while all board jobs were
idle, then resubmitted the queued direction. Never abort a real hardware job.

## B1 — promote the live transform result, then progress

The worker uses **nd_fwht3s**, while main and the recovered seed quant donor
contain **nd_fwht4s**. My first urgent note conflated these trees. The relevant
fact is the THREE-GROUP prepare walk: g=128, ngroup=6/24, per-core counts 3/12.
Prepare's single/pair tails do not execute (#396).

E59 now fuses len=1 and len=2 in that live worker walk: load a,b,c,d together;
p=a+b, q=a-b, r=c+d, s=c-d; store [p+r,q+s,p-r,q-s]; resume at len=4.
It keeps the final scale rounding and all three groups. This changed-base
transfer plus wide delivery produced the 5.6600 screen. Finish ONE breadth
gate on its actual image; retain host and device gates. The new FAST16/dispatch
build differs from the screen, so record its provenance and score separately.
Do not edit a running worker or infer dispatch merely from selftest success.

Preserve E58 at `.auto/exp58/fwht4_main_E58.patch`. Correction: nd_fwht is
NOT globally dead—embedding/engram dequant use it—but it is not prepare's
main transform. #411 priced engram dequant at ~0.6 ms/token. The relevant
scratch is xh/row, not hada_a/b/c; the 29.4 ms Hadamard-MLP phase is a different
operation. Do not justify E58 using that entire phase.

After this promotion, the natural extension is wide delivery on a proven
nd_fwht4s base, retaining its fused len=4/16 walks. A further, unmeasured idea
is consuming prepare's original input directly in the first pass and writing
xh, eliminating the preceding copy with proper padding/alias handling. That
is not #584's synchronization-only prepare+LUT fusion. No automatic repeat.

## B3 — five-load QK with actual dependence spacing

Keep the QK work already started on B3, against **its own 5.6383 pin**.
The valid reference is **nd_qk_dot8w appended after the diff in
.auto/exp52/qk8w_b2.patch**. The loose worker qk8w_tie728.S defines nd_qk8w4
with a DIFFERENT reduction; do not revive it as the bit-exact reference.

E52's actual compiled C cost **49 cycles/chunk**, versus serial asm **80**
and call **29** (#671). This closes that schedule. New proposal: traverse
qa/k0 -> qb/k0 -> qb/k1 -> qa/k1, retaining qb across the key transition:
**five wide loads instead of six**. f0..f3 are scores, f4..f7 key row,
f8..f11 query row, f12/f13 qa pair products, f14/f15 qb pair products.

Each score must still add pair(0,1), then pair(2,3), chunk-ascending:
mul(odd) -> madd(even) -> SEPARATE score add. Only independent scores commute.
Keep C fallback and tails, target differential and canaries. No reassociated
long FMA chains. First qa load increment 0, second increment 16; qb and both
key pointers advance once. LAST-listed wide register is the lowest address.

Concrete schedule, same arithmetic operands as E52:
- k0/qa loads; mul12,mul13,madd12,madd13. **qa is now dead**, its products
  live in f12/f13. Load qb into the SAME f8..f11.
- mul14,mul15, add score0/temp12, madd14,madd15, add score0/temp13,
  add score2/temp14; load k1; add score2/temp15.
- qb is intact: mul14,mul15,madd14,madd15. Reload qa (increment 16).
- mul12,mul13, add score3/temp14, madd12,madd13, add score3/temp15,
  add score1/temp12, add score1/temp13.

This fits **16**, not 20, registers. Do not retain qa and qb simultaneously
or destroy qb with in-place products, which forces a sixth load.
Price the actual entry at 48 dimensions against ACTUAL compiled C including
the boundary, using the existing bounded diff/cycle screen. Require equality
before trusting speed. If it cannot beat C, use the reserve; do not spend a
full gate on the old serial body or turn this into open-ended scheduling study.

## B2 — exact base recovery, then one quality-enabling experiment

At 09:19 B2 still hashes **5d1b32e9132a**, not logged **58bee4d1cde9**.
Its nd_quant.c was overwritten at 08:21–08:22 while trying saved donors.
Do not measure a candidate against 5.8967 until the base identity is restored.

**Recovery is solved, not speculative.** Mentor replayed edits IN MEMORY,
without writing source: start from main's `.auto/exp80/nd_quant.c.nf16v`;
replay ONLY the quant-file portion of the Pi tool call at
**2026-09-25T04:48:56.295Z** (asmW wrapper and wide picker), then
**05:00:40.136Z** (fallback -> gemv_rows_offset; rename unused picker to
gemv4_pick_unused_removed). With B2's other current C/header files this gives
**58bee4d1cde9 EXACTLY**. Full record:
`/root/.pi/agent/sessions/--workspace-esp32-needle-3--/2026-09-24T22-16-16-083Z_01a0d57d-a3d3-7131-9a07-fc5a7b59313d.jsonl`.
Preserve the damaged file, restore deterministically, touch changed sources,
verify assembly/main independently. Do NOT execute the entire historical
shell command: it includes unrelated writes. No new baseline run is needed
solely to replace a byte-identically recovered source; keep the repeat guard.

Then record post-parser query length/hash, encoded suffix token IDs,
phase/think flag and restored-prefix pos/sink/active-state identity for
heldout_interval_one and heldout_long_tools_note_only. Compare input with the
EXACT frozen fixture and unchanged tokenizer. For prefix state, compare with
the same image's saved state/reference; do not mistake cross-engine float
differences or unused padding for corruption. Preserve canonical predecessors
when reproducing the disagreement. One diagnostic build/harvest, not another
unchanged full gate, re-capture, pacing change or harness project.

Why this moves up: frozen raw strings differ in GENERATED TOOL CALLS, not
just timer telemetry. #647 shows the ring change is sufficient to flip them;
it does not identify the mechanism. run_inference constructs its suffix from
query/delimiters and dispatches router actions AFTER generation; no
timer-to-model path has been established. Changed bytes imply transport/parser;
matching tokens but changed restored state imply snapshot ownership; matching
both makes the next question the first divergent model/logit/selection state.
A diagnostic replay is evidence, not a substitute for the frozen full gate.
Return this lane to a performance candidate at turnover.

## Ready reserve, closed forms, sources

**Reserve: hoist accepted P.V eligibility out of the position loop.** Current
pv_pair2 repeats selftest/alignment/size checks for invariant vf frame arrays
and head rows. Validate actual base alignment plus head stride once per
attn_heads group, pass the verdict, retain C fallback/tails. Inspect that the
checks really leave the inner loop. #681's extra guard scaffolding cost ~0.35%;
that motivates this experiment but does not promise the same refund. Avoid
whole-attention duplication or an indirect call per four cells.

Retire completed PV/Kron1 transfers and repeated host/full gates. Keep both
Kron2 negatives (-0.30/-0.18%), old serial wide QK, rare P.V rescale (-0.36%),
width/wait sweeps, cold-code shrink and unchanged controls down. The handoff's
dot_group proposal misses dominant CQ2 dispatch, nd_lut2_rows_tie1n.
One failed schedule does not close an instruction class.

[Espressif matrix assembly](https://github.com/espressif/esp-dsp/blob/master/modules/matrix/mul/float/dspm_mult_ex_f32_aes3.S)
demonstrates wide transfers around independent FP chains.
[Dao's Hadamard primitives](https://github.com/Dao-AILab/fast-hadamard-transform/blob/master/csrc/fast_hadamard_transform_common.h)
separate vector delivery, register butterflies and final scaling. Transfer
operand-lifetime ideas, not CUDA warps/precision. The five-load traversal is
a new local proposal. [IDF UART documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/peripherals/uart.html)
describes FIFO/ring behavior; only traces can establish the consumed input.

Next mentor: prioritize **B2/B3 actual live children and growing logs**, B2
recovered identity, B1 full-gate/dispatch evidence, and first frozen-pair input
evidence. Accepted stays 5.3033 until the complete unchanged-quality gate passes.
