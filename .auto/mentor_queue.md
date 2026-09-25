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

---

## RESEARCHER STATE -- 2026-09-25 ~09:35Z (runs #683-#685, all three lanes harvested)

**B1: live FWHT delivery KEPT (#683).** Fused len=1+len=2 radix-4 first pass with
128-bit loads inside the ACTIVE `nd_fwht3s` three-group walk (`nd_fwht4r`); per
4-cell block 3 loads + 24 fp + 3 stores for three groups against 12+24+12 scalar.
Selftest 64/64 bit-exact, and the boot line proves field dispatch:
`FWHT4R xh_align=0` + `FWHT4R live=1 n=128 align=0` (m->xh is now FAST16). FULL
20-case gate: **5.6483 (+0.18% vs 5.6383)**, ext 5.5815, think 4.4, 18/20 = the two
#647 goldens. The screen had read 5.6600 (+0.385%) - breadth is the honest number.
B1 tree = 5.6483 state (engine `986dc8d5f2e8` before the run; re-hash at turnover).

**B3: QK reschedule CLOSED with a price (#684).** The five-load order (k0, qa
increment 0, qb into qa's registers, k1, qa increment 16, no rewind) with split head
temporaries is bit-exact (256/256) and priced **56 cycles/chunk against the compiled
C body's 49** - E52's serial body was 80, so the reschedule recovered 24 of the
31-cycle defect and still loses end-to-end (5.5983 = -0.71%). Hand-written asm for
this loop is closed on a measured floor, not on one negative. B3 tree restored to
`24d6ce2ce19b` and rebuilt after the discard.

**B2: base restored EXACTLY, then one hoist KEPT (#685).** The mentor's recipe
(exp80/`nd_quant.c.nf16v` + the two recorded quant-file edits) reproduced
`58bee4d1cde9` byte-for-byte; the damaged file is preserved at
`/root/board-pool/preserved/nd_quant.c.b2_damaged_5d1b32e9132a`. On that base, E61
hoists the P.V kernel's eligibility verdict out of `pv_pair2` to one decision per
attn_heads group: **5.9033 = +0.11%** (boot 6.007, min_case 5.69, 6/6 exact). Sub-bar
alone, kept as a refund of scaffolding the E53 kernel introduced (#681 priced the
un-hoisted shape at ~0.35% on a rarer path).

**Open at turnover.** (1) The E59 FWHT lever has not been transferred to the seed
line: B2's 5.9033 base has no FWHT delivery change, so it is the natural next
candidate there (+0.18% on B1). (2) B3 is clean and free - the same transfer, or the
frozen-pair diagnostic the queue asks for. (3) `ohp[t]` head rows and the PV verdict
are now computed per group; if a future candidate changes v_hd or the attn base
alignment, that verdict must move with it. (4) Host gates were last run on the
5.8967/5.6383 trees (#682); the FWHT and hoist trees owe `checks.sh` before any
owner submission.

---

## RESEARCHER STATE -- 2026-09-25 ~10:25Z (runs #686 + one diagnostic harvest)

**E62 (FWHT radix-4 delivery on the seed line) DISCARDED at -1.02% (#686).**
B2's live transform is `nd_fwht4s`, whose first fused pass (len=1) has the same
contiguous four-cell shape as B1's kernel, so the verified kernel was wired in with
the walk resuming at len=4. Byte-exact 6/6 and dispatch proven
(`FWHT4R live=1 n=128 align=0`), decode 5.8433 vs the 5.9033 base, boot 5.945 vs
6.007. Same kernel, +0.18% inside B1's `nd_fwht3s` and -1.02% inside B2's
`nd_fwht4s`: the win belongs to the surrounding C walk, not to the kernel, and
`nd_fwht4s`'s later fused passes are already cheaper than a wide delivery of the
first one. Two integration attempts failed before the arithmetic was right - skipping
the remaining fused passes made every output wrong (0/6) while the kernel's own
selftest stayed green, because it cannot see call-site bookkeeping; only the device
gate saw it. B2 restored byte-exactly to `b28de5ea79ce` (its 5.9033 base).

**Frozen-pair diagnostic HARVESTED (B3, one diagnostic build).**
Request-time firmware prints never reach the run log (bench.py closes its echo while
a request is in flight), so the record came from a raw console harvest run inside
`needle-board`: `/root/board-pool/diag_pair.py`, output at
`/root/board-pool/batches/R-diagharv-b3.log`. The hook is `ND_REQ_DIAG` in
`esp32/main/main.c`, enabled through `.auto/diag_build_cfg =
-DCMAKE_C_FLAGS=-DND_REQ_DIAG=1` (note: a bare `-DNAME=1` there sets a CMake cache
variable and never reaches the compiler).

| case | qlen | qhash | nt | phase | think | pos | sink | first four ids | last four ids |
|---|---|---|---|---|---|---|---|---|---|
| heldout_interval_one | 29 | 4ab47921 | 13 | 0 | 0 | 143 | 143 | 38, 8129, 4040, 3628 | 38, 4, 421, 38 |
| heldout_long_tools_note_only | 134 | 980b07ea | 38 | 0 | 0 | 143 | 143 | 38, 1077, 309, 3628 | 38, 4, 421, 38 |

Both queries arrive with their full frozen length and both encode to a suffix whose
LAST FOUR token IDs are identical (38, 4, 421, 38 = the assistant-template tail),
and the restored prefix identity is the same for both cases (pos = sink = 143). The
extended-group diagnostic run earlier in the same image read 11/13 exact with the same
two failures, so the divergence is downstream of input arrival: the next question is
the first divergent model/logit/selection state for these two, not the transport.
The raw query strings are 29 and 134 bytes and their FNV-1a hashes are recorded above
so a later run can prove the fixture did not move.

**Trees now:** B1 = `986dc8d5f2e8` (5.6483 full gate), B2 = `b28de5ea79ce` (5.9033
restricted), B3 = clean 5.6383 base with the diagnostic hook present but the diag
build config still in `.auto/diag_build_cfg` - remove that file before any speed
measurement on B3 (it is a diagnostic image and its metrics are marked
diagnostic-only).
