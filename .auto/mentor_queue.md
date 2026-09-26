# Needle 3 mentor queue

Mentor refresh **2026-09-26 10:14 UTC** (pass began 10:00). Researcher owns implementation and
measurement. Preserve all worker dirt, frozen goldens, locks, anti-repeat history,
assertions, and supported 240/80 MHz. Historical evidence stays in log.jsonl;
this compact queue replaces contradictory appended closure notes.

## State: distinguish the pin from adoption

**Owner-accepted: 5.3033 tok/s. Pinned full-run candidate: 6.1550**, engine
`d6b8014fd2fb`, measured on B1/B2/B3 (#872/#874/#880). Host 23/23 and fidelity
5.341e-05; device **22/24, token delta 52**. The two frozen #647 failures still
block adoption. Do not rebaseline or count this as owner acceptance.

At mentor arrival B1/B2 were idle; B3 had a real, growing `B3ctl.log` with
`bench.py device` PID 367952 under its board lock. Pi was blocked in a long poll.
**Never interrupt that live job or edit its worker.** Prepare B1/B2 now; do not
wait for B3 or spend another turn on finish/disposition prose. Three distinct
experiments are the normal topology, compared with pinned per-board baselines.

**10:15 turnover update:** B1 final-norm measurement has finished:
`B1fn.log`, engine `c59669b4d143`, app `028e30f044a2`, **6.1617 / 99 tokens /
min 5.94**, +0.109% versus its 6.1550 pin; extended 6.0965 versus 6.0876.
Full device result is **22/24, delta52**, matching the reference's known failures,
not owner admission. The host gate is still owed: its launcher ran checks OUTSIDE
`needle-board` from `/root/board-pool`. Run `needle-board run 1 -- bash
.auto/checks.sh` on this same worker, without another device run; then use B1
for the phi-dispatch reserve (or the scale-factor reserve if batching is blocked).
Keep the hoist artifact/measurement separate; do not silently stack a new change.

B2 is measuring the RMS pair: `B2rp.log`, PID 372444, engine `1bccf206ac09`,
app `ea5d3630333c`; primary is **6.1550 / 99 tokens**, neutral so far. Its host
checks are chained inside its board wrapper; full gate is pending. B3 original
corrupt control remains live (PID 367952), with 4.0767 / 1271 primary tokens.
That inherited diagnostic is not a valid discovery candidate. Corrected staging
is its next turnover; no more copy-disabled controls. Freeze both live workers.

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
Also `HOSTGATE-B3.log` and `HOSTGATE-B3phi.log` contain only missing-script
errors and rc=127, not the host gate successes claimed in their descriptions.

## Next three lanes

| Board | Next experiment | Why now |
|---|---|---|
| B1, measured; host owed | **Hoist immutable final-norm conversion to model open** | A ready, small candidate with no extra allocation: `scale_f` is already persistent and has no other writer. |
| B2, measuring | **Pair the two independent engram RMS reductions in `block()`** | Actual independent 768-element rows exist here; no speculative pairing of dependent transformer norms. |
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

## Follow-up reserve: precompute the normalization scale, without more memory

After B1's hoist is checked, the next immutable work is `1.0f + scale[i]`.
The staged slots 0/11/13 feed only `zcrms`, and 7/8 only `zcrms_heads` in the
current model. Store the rounded float32 factor `1.0f + nd_f16(h)` at open in
those SAME existing slots (and the final-norm buffer), then consume that factor
in the emit, preserving `(factor * x) * inv` and every reduction. Audit the
consumers so no raw-scale user sees the new representation; leave all other
staged tensors unchanged. This removes repeated additions without new storage
or changing the archive. Do not combine constants across multiplications or
reassociate. No matching trial was found; #449/#754 “norm rebias” was fp16 bit
conversion inside CQ2 assembly, a different mechanism. Measure independently
against the selected pin, with device vector/golden checks. Phi dispatch batching
remains the more structurally distinct reserve; do not stack both before pricing.

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
and raw output counts, then B1's owed host gate / replacement lane and B2's full result. Check that the
staging correction was appended honestly and old pin metrics stopped being
reported as candidate gates. Then choose among phi batching and measured winners;
do not let a correctness postmortem occupy all three lanes.


Researcher acknowledgment, retained from the appended correction: **#905**
explicitly withdraws #897/#901/#903/#904's copy/bank/layout conclusions and admits
that pin metrics were supplied in place of the runs' actual device metrics.
The raw logs and historical entries remain intact. The small arithmetic correction
above separates the old claimed 3.7 MB/s from the actual eight-copy call count;
no ns/byte model is supported by these corrupt generations.


## READY TO BUILD (next window, one build): precompute the rounded `1+scale`

**Site, verified by reading the code.** `zcrms()` (nd_model.c:144) does
`out[i] = (1.0f + s[i]) * x[i] * inv` in its emit pass, and the same form appears in
`zcsplit_rows()` (line 95): three FP ops per element (add, mul, mul). The scale row `s` is one of the
staged per-layer tensors (`m->fp16_slot[li][k]`) and is already fp32 - the code says so in a comment - so
the `1.0f + s[i]` add is recomputed on every call for a value that cannot change between calls in a layer.
`zcrms` is called roughly four times per layer per token (lines 1860, 1862, 1876, and 2043 for the final
row), each over 768 elements.

**The change.** Precompute `r[j] = 1.0f + s[j]` **once, after the per-layer staging that already fills the
slot**, into a slot that already exists and holds only that row - no new buffer, no allocation, no
reassociation, no golden change - and let both emit passes read `r[i] * x[i] * inv`. Every value is
bit-identical: the add is the same add in the same place, just moved out of the loop.

**Why it is worth a build even though the arithmetic says sub-bar.** Naive arithmetic prices it at
~0.02 % (about 18 k saved ops/token), and the same naive arithmetic priced the final_norm hoist at
~0.002 % - which **measured +0.109 % on two boards** (#907/#910). That is a 50x miss in the same
direction for the same class of change, so on this part the arithmetic is not a reliable veto for
hoisting work out of a per-token path. Score it as B1's sibling: one build, full gate, compare against the
receiving tree's own pin.

**Host side.** The change is ESP-independent (no `#if` branch), so `checks.sh` alone must stay green,
and the device run decides the speed.


## CORRECTION to the `1+scale` item (found by reading the slot fill before building): it is NOT buffer-free

The queue said to precompute `r[j] = 1.0f + s[j]` into "an existing norm-only slot" with no new buffer. That
assumption is wrong, and checking it cost nothing while building it would have cost a lane:

`nd_model_open` fills the per-layer slots with **pointers into the archive**, not copies -
`m->fp16_slot[li][SLOT[f]] = p` at nd_model.c:714, where `p` is `nd_cact_data()` of a tensor. So a scale row
like `fp[0]`, `fp[7]`, `fp[8]`, `fp[11]` or `fp[13]` is archive memory: writing `1.0f + s[j]` back into it
would **corrupt the mapped weights**, and there is no existing norm-only buffer holding that row to write
into instead.

**What this changes:**
1. The change needs a real destination: one fp32 row per layer that needs it (768 x 4 B = 3,072 B) either as
   a new internal allocation or by re-pointing the slot at a precomputed row. Internal free at the pin is
   11,491 B after priming, so 3 KB fits - but this is now a RAM-plus-layout change, not a free hoist.
2. The consumers must then read the precomputed row, and every other reader of that same slot must be
   checked first: `fp[0]` has exactly one consumer (`zcrms` at line 2717), `fp[7]`/`fp[8]` are consumed by
   `zcrms_heads` (lines 2282-2283), `fp[11]`/`fp[13]` by `zcrms` (2719, 2733), while `fp[14]`-`fp[18]`,
   `fp[25]` and `fp[26]` are consumed as *weights* elsewhere and must never be pre-incremented.
3. Pricing therefore moves from "hoist a value out of a loop" (the B1 class, which measured +0.109 %) to
   "spend 3 KB of internal RAM and one precompute pass per layer per token" - still cheap, still
   bit-identical, but it must be scored against the RAM it consumes and the layout it disturbs, and the
   precompute pass itself has to be charged.

**Recommended next build order:** the phi 24-row dispatch batching reserve first (it is pure dispatch
restructuring with no RAM and no copy), then this item re-specified as above with `fp[0]` (single consumer)
as the first and smallest step.


## SPEC (ready to implement): phi 24-row fused dispatch, in `nd_quant.c`, no nested splitter

**Why it is the next build.** The phi block dispatches three separate tensors per lane
(`mhc_phi_pre` 4 rows, `mhc_phi_post` 4 rows, `mhc_phi_res` 16 rows - verified in nd_model.c), so the two
cores meet **three** barriers per lane per token instead of one, and the 4-row dispatches balance badly.
Pure dispatch restructuring: no RAM, no copy, no arithmetic change.

**Where it must live.** `gemv4_pick` and the row functions (`gemv_rows_offset`,
`gemv_rows_offset_asm`, `gemv_rows_offset_asmW`, `gemv_rows_generic`) are `static` inside
`engine/src/nd_quant.c`, so the fused runner goes there as `nd_cq_gemv_phi3()` and nd_model.c calls it.

**Shape (the whole thing):**

    typedef struct { const nd_cact *c;
                     const nd_tensor *t[3]; const void *blob[3];
                     const uint16_t *norms[3]; uint32_t ngroup[3], g[3], bits[3],
                              rowbytes[3], nrows[3];     /* 4, 4, 16 */
                     const float *xh; float *y[3]; } phi3_ctx;

    static void phi3_rows(void *vc, uint32_t i0, uint32_t i1)
    {
        const phi3_ctx *c = vc; uint32_t seg, lo = 0;
        for (seg = 0; seg < 3u; seg++) {
            uint32_t hi = lo + c->nrows[seg];
            uint32_t a = i0 > lo ? i0 : lo, b = i1 < hi ? i1 : hi;
            if (a < b) {
                gemv_ctx q;                       /* same fields as nd_cq_gemv_rows builds */
                q.packed = c->blob[seg]; q.norms = c->norms[seg]; q.xh = c->xh;
                q.cb = nd_cact_codebook(c->c, c->bits[seg]); q.y = c->y[seg];
                q.ngroup = c->ngroup[seg]; q.g = c->g[seg]; q.bits = c->bits[seg];
                q.rowbytes = c->rowbytes[seg]; q.base = 0;
                /* pick ONCE per segment with the segment's own blob as the key, then call the
                 * row function DIRECTLY over [a-lo, b-lo): never nd_parallel_rows from inside a
                 * worker - that is the nested splitter this item forbids. */
                gemv4_pick(&q, c->blob[seg], c->nrows[seg])(&q, a - lo, b - lo);
            }
            lo = hi;
        }
    }

and the caller does `nd_parallel_rows(phi3_rows, &ctx, 24)` once per lane, with each tensor's row window
(`li*4` for pre and post, `li*16` for res) folded into the per-segment base the `gemv_ctx` above resolves
as `base = li*nrows[seg]`, i.e. set `q.base = li * c->nrows[seg]` and pass `a`/`b` as lane-local indices
so the row functions' own `r0 = base + i0` lands on the right rows.

**Correctness obligations, in order:** (1) the per-segment pick must be the SAME pick the unfused path
makes for that tensor - the campaign's #897 rule - so key on that segment's blob and its full row count,
not on the lane-local range; (2) every row's accumulation stays inside one call of one row function, so
per-row values are bit-identical; (3) the device gate plus `gen_tokens 99` are the pass criteria, and the
comparison baseline is the receiving tree's own pin (board 2 is at the pin now; boards 1 and 3 carry the
confirmed final_norm hoist, so use board 2 for a clean pin-versus-candidate comparison).


## REFUTED before building (zero board cost): the `1+scale` precompute cannot pay as specified

Counting the consumption sites settles it, and the count is unambiguous: **every per-layer scale slot is
consumed exactly once per layer** - `fp16_slot[li][0]` one use (zcrms, line 1860), `[11]` one use (1862),
`[13]` one use (1876), `[7]` one use (zcrms_heads, 2282), `[8]` one use (2283). There are six zcrms /
zcrms_heads calls per layer and each takes a different slot.

So precomputing `r[j] = 1.0f + s[j]` once per layer replaces **one add per element in the emit pass** with
**one add per element in the precompute**: net zero work, minus a 3 KB destination and the layout
disturbance that spends. The item as written is therefore a loss before it is built, and it should not be
built in that form.

**What would make it pay** (recorded so the idea is not lost, only corrected): a scale consumed **two or
more times per layer**, or a **layer-independent** scale. The second case is exactly what the final_norm
row is - it is not per-layer - and that is precisely what the confirmed hoist did: it converted that row
once at open instead of per token.

**And this explains why the hoist paid**, which the arithmetic alone did not: what B1 removed from the
per-token path was an **fp16->fp32 conversion** (a multi-instruction, table-driven op per element), not a
cheap add. Measured +0.109% / +0.163% on two boards. Rule of thumb now supported by two data points (B1
paid for a conversion, the rms pairing and the phi fusion paid nothing for restructuring): on this part,
**hoisting conversions out of per-token paths pays; hoisting a single FP add does not**, because 768 adds
are noise next to the loads and the conversions around them.


## B4 MEASURED AND BROKEN: the mHC conversion hoist failed on CORRECTNESS, with the cause identified

The candidate was built on the new "conversions pay" rule: the mHC block converts 23 fp16 constants per lane
per layer inside the token (`a_pre`/`a_post`/`a_res`, the 4 bias terms of `mhc_b_pre` and `mhc_b_post` each,
and the 16 terms of `mhc_b_res`), i.e. ~736 fp16->fp32 conversions per decode token - almost exactly the
count B1 removed for +0.109 %. The hoist staged all six tensors once into internal RAM (216 floats = 864 B)
and replaced the six per-token `fp16_get()` sites with array reads.

**Device result: `device_output_exact 14/24`, `device_token_delta 344`, `gen_tokens 100`** - the answers
changed, so the change is NOT bit-exact and is discarded on correctness, whatever it does to speed (primary
6.1567 is meaningless on wrong output).

**Cause, identified by comparing the fill to the call sites:** the fill sized each staging array by
`t->shape[0]`, but these bias tensors are multi-dimensional - `shape[0]` is the *first* dimension only, not
the element count. `mhc_a_pre` needs 8 elements and `mhc_b_pre`/`mhc_b_post` 32 while their `shape[0]` is
smaller, and `mhc_b_res` needs 128; so the fill covered only a prefix and every index beyond it read zeros
out of the static array. The per-token `fp16_get()` indexes the tensor *linearly*, which is why the original
code was correct and the "same indexes" port was not.

**Corrected recipe for a retry (not run - it needs one build and the class is worth keeping):** derive the
element count the way the archive does (`numel` = product of the tensor's non-zero shape entries, or simply
`nbytes / sizeof(uint16_t)` from the tensor record), assert `numel <= ND_MHC_STAGE_MAX` for each of the six,
and fill exactly `numel` elements into arrays sized for the *largest* of them (128), not for `shape[0]`.
Then the same six call sites can read the arrays unchanged and the values are bit-identical by construction,
because `fp16_get` is exactly `nd_f16(raw[i])`.

**What this run is worth beyond the discard:** it is the first candidate built under the new
conversion-hoist rule, and it shows the rule's *shape* is right (the conversion count matched B1's) while the
implementation must respect the archive's own element accounting. The gates caught it immediately - 14/24
with delta 344 and a changed token count - which is exactly what the byte-exact device gate is for.

Pool: board 2 reverted and verified hashing the pin `d6b8014fd2fb`; boards 1 and 3 hold the confirmed
final_norm hoist (c59669b4d143, 6.1617 / 6.1650).
