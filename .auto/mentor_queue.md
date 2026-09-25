# Needle 3 mentor queue

Mentor refresh **2026-09-25 11:51 UTC**. Preserve dirty work, board locks,
repeat guard, frozen quality gates and 240/80 MHz. Researcher owns implementation
and measurement. Guidance applies at turnover, never by aborting a live build,
flash or benchmark. When hardware permits, use three different experiments.

## State and immediate ordering

Accepted: **5.3033 tok/s**, bundle5, 20/20 device. Best discovery: **5.9617**,
B2 E64 wide lane mix (#692), full suite **18/20, delta 52**, ext 5.8915,
think 4.60, min 5.74, boot 6.069. This is still a proposal. Host checks on the
E64 tree are owed; #688 predates it.

B1's E64 full run #694 / R-lmfull-b1.log: **5.7000**, ext 5.6400, think 4.44,
min 5.50, boot 5.801, 18/20, delta 52; its same-mode screen pin is **5.7017**.
The E66 RMS-emit screen R-rms-b1.log also read 5.7017. RMS dispatch/alignment
was not printed, so this does not close wide emit. Raw ready heap is 2231 B;
do not carry older 4191 B heap figures forward.

**B1 E67 just completed:** R-lanepre-b1.log reads **5.7100**, 6/6 exact,
delta 0, LANEPRE bad=0 and lanepre=1. That is **+0.146%** against its own
5.7017 screen pin: a valid small result, below the campaign's 0.2% solo bar.
Raw ready heap is now 1871 B. Bank it for a measured composition rather than
spend an immediate full gate or repeat on this small standalone lever.
Next use the nx/xh lifetime experiment below on this pinned 5.7100 screen tree.

**B2 has a stalled readiness reader**, not proven decode work. Repaired E65
image c6a5b01357ed / R-lmtile2-b2.log flashed at 11:36, but after >12 minutes
its benchmark file was still empty and process read/write counters were static.
bench.py buffers boot text in StringIO until Device attach returns, so empty
output proves no completed handshake, not zero emitted serial bytes. Leave
the locked job alone; do not wait serially behind it or open a second console.

**B3 is still unable to flash.** Its node reappeared at 11:31, but the NEW
E67 attempts R-lanepre-b3.log failed with
`Could not configure port: (5, 'Input/output error')` on
`/dev/needle-pi/board3-flash`. Mere presence was not recovery. Preserve the
prepared E67 tree. No repeat USB loop or broad recovery; use healthy turnover
for its candidates. Once B3 can flash again, assign a DISTINCT reserve, not a
duplicate of B1's E67.

## B1 now: four-column input mix, then in-place phi preparation

**E67 is a real new kernel**, not #35's old split: lanepre_rows currently does
acc=+0 then hpre[j]*lane[j*dm+i], j=0..3. Four independent output columns keep
their exact j order and initial +0 accumulation; four accumulators + four lane
values + four weights fit 12 FP registers. Keep the 128-column work split and
actual lane/u alignment. Require LANEPRE bad=0 plus lanepre=1 before pricing it.
If a composition clears the bar, run one full gate plus host checks alongside
discovery. E67 itself is now screened, not yet fully gated or accepted.

The researcher fixed two mentor-found draft defects: use four independent row
pointers advancing 16 bytes/tile (the first draft advanced four full rows per
tile), and scalar-load stack hpre weights instead of assuming 16-byte alignment.
The N=16 differential covers multiple tiles with a small static footprint.
B3 additionally carried the OLD E64 lazy shared-buffer selftest in lanemix_rows;
that callsite was disabled there to isolate E67. Do not reintroduce that race
when using the preserved B3 donor. B1 has the fixed E64 verdict at model open.

**Next: remove nx through in-place phi preparation.** Source audit finds nx
has one producer (lane RMS), one consumer (the immediately following phi
prepare), and no prefix/snapshot ownership. Emit the SAME rounded RMS vector
directly into existing aligned xh; then transform xh in place. Explicitly skip
memcpy when x==xh, preserve padding, and remove nx allocation/check/free
consistently. Avoid alias-induced double frees in close/failure cleanup.

This can remove **eight 12,288-byte copies per token** and reclaim **12,288 B
internal SRAM** without moving scaling across butterflies or changing the
serial RMS reduction. It also gives the existing nd_mul4w RMS emit an aligned
destination. ELF inspection found that emit still costs lsi/addi/mul/ssi/addi
per scalar cell; about 27,648 emit multiplies occur per token (8x3072+4x768).
Use a one-time dispatch/alignment verdict. Speed remains unmeasured; RAM saved
is not itself tok/s. No matching nx lifetime experiment was found in history.
This is smaller than inventing a general memory arena or rewriting FWHT.

## B2 next valid turnover: actually measure the tiled output mix

E65 keeps four dst cells resident through hpost*u and four ascending hres*lane
terms, eliminating E64's repeated dst traffic. **It has no valid speed result
yet.** First R-lmtile-b2.log read 5.7950 with bad=1854 / lanemix=0, so C fallback
ran: its reference used (w+w+w+w)*sb instead of four ordered accumulations.
Researcher repaired the reference with distinct rows/weights/stride and split
E64/E65 verdicts. Preserve those fixes.

The repaired ELF added **1600 B permanent db/sb/lb test storage** against an E64
runtime with only 2087 B at ready and 1039 B after attach. Investigate allocation
failure from the buffered boot evidence at normal turnover; it is a strong
hypothesis, not an observed panic. Use bounded existing scratch before model
reset or a small multi-tile differential, not another permanent slab.
The new wide w4 load also needs alignment: hres is on stack, while existing
eligibility only checks lane/lane_next/u. Scalar-load four weights if simpler.
Keep the proven seventh-argument ABI and require bad=0 AND actual tile dispatch.

Compare with the RECORDED E64 same-mode pin; do not invent a control. If B2
remains blocked, E65 can take a healthy B1 turnover after its current candidate,
with local base/mode recorded. The nx reclamation could remove the test's
memory obstacle without changing any arithmetic.

## B3 when usable / ready reserve

**Consume original prepare input in the first FWHT pass.** nd_cq_prepare
currently copies/pads x->xh, then fwht_rows reads xh again. Fuse the input read
with the first two butterfly stages, write xh, then resume the SAME live
three-group walk at len=4. Retain final scaling, padding/alias fallback and all
later stages: seed nd_fwht4s must still execute its len=4/16 fused passes;
B1 uses the E59 nd_fwht3s-based path. Inspect the actual compiled copy/call
cost before making this a large assembly project.

This removes data traffic; #584 only fused prepare+LUT barriers (null), and
E62 replaced the seed's first pass with a wide kernel (~1% slower). Neither
measured copy elimination. Prefer the smaller nx-only experiment first.

If E65 wins, a later composition may load u and ublk, perform the existing
rounded subtraction, then hpost multiply and ordered mix in one tile. Audit
all later u consumers and preserve the separate subtraction rounding. Keep
this out of E65's first valid measurement.

## Constraints that matter at the next turnover

- Same-board, same-mode comparisons. B3's 5.9033 pin is FULL (#690); B2's
  matching screen does not create a B3 screen pin. B1 screen/full have differed.
- The two frozen failures remain heldout_interval_one and
  heldout_long_tools_note_only. #647's ring-only change is sufficient to flip
  them on accepted engine code, but does not establish why. Generated tool
  calls differ; 18/20 is not a gate pass and no rebaseline is authorized.
- Existing input diagnostic: lengths/hashes 29/4ab47921 and 134/980b07ea,
  suffix counts 13/38, phase=think=0, pos=sink=143, same assistant tail.
  This does not prove all suffix tokens/restored active state match reference.
  A future bounded quality lane should compare those identities and locate
  the first divergent model/logit/selection state with canonical predecessors.
  Do not repeat the same input print/full gate or devote every lane to it.
- Engram is not untouched: #11/#40/#49/#129 already optimized it; #347 pair
  fusion +0.04% rejected, #411 dequant 0.6 ms and correct 128-cell rows,
  #430 tap register accumulation -0.16%. Most of 15.8 ms is existing LUT2.
- Profile bins overlap: proj2bit includes engram GEMVs; attn-stage includes
  projections. Latest boot bench is ~167 ms/token; do not sum parent/child bins.
- Keep QK E60 (56 cycles/chunk vs compiled C 49), both kron2 negatives,
  rare P.V rescale guard cost and completed recovery/verification down.
  #407 closes phi streaming residency's free-arrival assumption; more RAM
  alone does not reopen it.
- Preserve donors and provenance including assembly/main/config. Print one
  dispatch verdict, use existing differential/quality gates, then measure.
  Do not expand a harness or weaken the anti-repeat guard.

Useful transfers: Espressif's [matrix assembly](https://github.com/espressif/esp-dsp/blob/master/modules/matrix/mul/float/dspm_mult_ex_f32_aes3.S)
shows the 12-register four-output load/MADD pattern; our +0 initialization stays.
TFLM's [memory planning](https://github.com/tensorflow/tflite-micro/blob/main/tensorflow/lite/micro/docs/memory_management.md)
supports reuse of non-overlapping lifetimes; nx/xh is the concrete local proposal.
Dao's [Hadamard primitives](https://github.com/Dao-AILab/fast-hadamard-transform/blob/master/csrc/fast_hadamard_transform_common.h)
separate loads, register butterflies and final scale; no CUDA precision or warp
semantics transfer here.

Next mentor: inspect B1's nx/xh follow-through on the 5.7100 E67 screen,
B2 readiness/heap evidence, and B3's actual ability to flash. Restore three
distinct lanes when hardware permits. Accepted stays 5.3033.

---

## RESEARCHER STATE -- 2026-09-25 ~12:00Z (runs #692-#695, two lanes banked/hardware-blocked)

**Kept this window: mHC lane-mix wide delivery (E64).** B2 **5.9617 full gate** (+0.99 %,
the campaign's biggest single lever since P.V) and B1 **5.7000 full gate** (+0.92 % on
the shippable line) - the same lever, both bases, agreeing to a tenth of a percent.
Seed line stands at **+12.41 %** over the owner's 5.3033 pin; shippable at **+7.48 %**.

**Banked below the bar (code present in the trees, not claimed):**
* **E67 wide lanepre** on B1: 5.7100 = +0.146 % vs its own same-mode 5.7017 screen,
  selftest 64/64, dispatch live, 6/6 exact. No repeat, no full gate, per instruction.
  * **E65 tiled mix** (dst resident across the four input rows) - oracle fixed and
  split from E64's verdict, but B2's lane never completed its handshake after flashing.
  **B2's locked job is still running and has not been touched**; its log is empty and
  its reader counters stopped. Treat the tile as unmeasured, not as a negative, and
  check boot/allocation evidence (the E65 test slab was 1600 B of permanent .bss
  against ~2 KB free internal RAM) before re-running it.
* **E66 aligned RMS emit** - measured as a null because `m->nx` is still
  `ND_ALLOC_FAST`, so the guard silently kept the C path (ELF confirms lsi/addi/mul/ssi).

**Hardware:** B3's flash node returned but flashing still fails with EIO; no further USB
loops were attempted. B1 is healthy and free; B2 is locked by the stalled job.

**Next (B1, when free): the nx/xh lifetime candidate** - audit says `nx` has one
producer (lane RMS) and one consumer (phi prepare) and no prefix ownership, so emitting
the RMS directly into an aligned `xh` and skipping the prepare memcpy for the in-place
case removes 8x12,288 B copies per token and frees 12,288 B of internal SRAM with
identical arithmetic; it also makes the RMS emit guard live (E66's blocker). Judge the
composition, not the pieces. **Do not** spend a board on an E67 repeat or on engram
(#347/#411/#430 already price its ideas), and do not read B2's silence as a tile result.

---

## RESEARCHER STATE -- 2026-09-25 ~12:35Z (run #696: a structural win)

**nx/xh lifetime KEPT (#696, B1): +0.85% and 9,976 B of internal SRAM back.** The lane
RMS now emits straight into `xh` (the phi prepare's own output buffer) and
`nd_cq_prepare` skips its copy when input and output are the same pointer, so the
12,288 B memcpy per layer (8 per token) disappears with identical arithmetic; `nx` is
retired from allocation, null check and free. B1: decode **5.7583** vs its same-mode
5.7100 screen (+0.85%), boot 5.859, min_case 5.55, 6/6 exact, `internal_free` 4191 ->
**14167**. The boot line also closes E66's story: `lane_align=0 xh_align=0`, so the RMS
emit's wide path is live - E66 read as a null only because its destination (`nx`) was
never 16-byte aligned and the guard silently kept the C loop. The measured change is a
composition (copy removal + live emit), judged as one.

**This is the shape to look for again:** a *lifetime* defect, not an instruction-count
defect - a scratch buffer that only exists because one producer and one consumer did not
share storage. The audit found it by following `nx`'s single producer/consumer and the
absence of prefix ownership; the same question ("who owns this buffer, and does anything
need the copy?") has not been asked of `n1`, `n2`, `ublk`, `tmp2`, `row` or the
`fp16_slot` staging rows.

**Board state:** B1 carries E68 (5.7583 screen, full gate + host gates owed). B2 is
still locked by its stalled E65 job (untouched, as instructed). B3's flash still fails
with EIO. **Next: port E68 to the seed line the moment B2 frees** (it is a pure
lifetime change with no base dependency), then judge it as a composition there; and in
parallel audit `n1`/`n2`/`ublk`/`tmp2` for the same single-producer/single-consumer
pattern. Do not spend a board on E67 repeats or engram.

---

## RESEARCHER STATE -- 2026-09-25 ~12:45Z (runs #696-#697: the lifetime family pays twice)

**E68 confirmed on BOTH stacks, with ~10 KB of internal SRAM back on each.**
* B1: 5.7100 -> **5.7583** (+0.85 %), internal_free 4191 -> 14167 (+9,976 B)
* B2: 5.9617 -> **5.9917** (+0.50 %), internal_free 2087 -> 12531 (+10,444 B)
  seed line = **+12.96 %** over the 5.3033 pin (restricted; full gate owed), shippable
  line's last full gate 5.7000 (its E68 tree now reads 5.7583 restricted).

Both trees keep E65's tiled mix compiled but DISABLED (its lane never handshook, so it
is unmeasured, not negative) - no unmeasured lever is inside the numbers above.

**The lesson worth repeating: this was a LIFETIME defect, not an instruction-count
defect.** `nx` existed only because a producer (lane RMS) and a consumer (phi prepare)
did not share storage; following its single producer/consumer and the absence of prefix
ownership found ~98 KB/token of pure copying plus a guard that could never fire (E66's
null). The same question has not been asked of the other scratch rows: `n1`, `n2`,
`ublk`, `tmp2`, `row`, `m->u` and the `fp16_slot` staging (`nd_cq_prepare` is also
called with a scratch in the dequant path - check that call site for the same shape).
That audit is the highest-value next step; it needs no board and it has already paid
once at +0.5-0.85 % per stack.

**Board state:** B1 `0f0d3668d71f` (5.7583 screen, E68 + banked E67; full gate + host
gates owed), B2 `1f17fa25e179` (5.9917 screen; full gate + host gates owed), B3
`b255280ba9a9` (E67 only, E64 disabled; flash still EIO). All boards idle; no job
running. Next lanes in order: (1) the scratch-lifetime audit above and its first
candidate; (2) full gates for whichever line the owner wants to submit (both owe them);
(3) B3 when its USB returns.

---

## RESEARCHER STATE -- 2026-09-25 ~13:15Z (run #698: best line breadth-gated again)

**Seed line FULL GATE: 5.9917 / ext 5.9215 / think 4.61 / min 5.77 / boot 6.10 / 18-20
(delta 52 = the two #647 goldens) - +12.96 % over the owner's 5.3033 pin.** The image
carries the nx/xh lifetime change (+0.50 % on breadth, exactly the restricted screen
value) and E69.

**E69 = null on speed, real on resources.** The dequant path had the same lifetime
defect E68 removed in the phi path: `nd_cq_dequant_row` transformed into a scratch
(`m->row`) and then copied the row to its destination. It now writes straight into the
destination when the caller passes it as both (both field call sites do), and `m->row`
is retired: `internal_free` 12,531 -> **15,607 B**. Predicted null was correct - the
copy is ~19 KB/token - so it is banked as a resource change, not a speed claim.

**Lifetime audit result (the family is now empty of *copies*, but not of *buffers*):**
`memcpy/memset` in the hot path is gone except the prefix snapshot/restore (per
request, outside the timed region) and the PSRAM staging at open. Remaining scratch
rows are `n1`, `n2`, `ublk`, `tmp`, `tmp2`, `u`, `gate`, `aout`, `y` - none of them is
copied, so the next question is not "can this copy go" but "can two of these rows share
storage without changing arithmetic" (the same reasoning that retired nx and row, now
applied to whether `ublk` could be folded into its producer, which WOULD change the
subtract order - do not do that without a differential test).

**Board state:** B1 `0f0d3668d71f` (5.7583 screen; full gate + host gates owed),
B2 `31e8d04c8120` (5.9917 full gate; host gates owed), B3 `b255280ba9a9` (E67 only,
flash EIO). All idle. With ~15.6 KB internal free on the seed line, the long-blocked
RAM-gated ideas come back into range (16 KiB private LUT2 tables; phi row residency
~18 KB/core is still out of reach) - worth re-pricing before building anything new.

---

## RESEARCHER STATE -- 2026-09-25 ~13:50Z (runs #699-#700: both lines gated, 6.0 crossed)

**Two lines, both with device breadth evidence:**
* **seed line 5.9917** (ext 5.9215, think 4.61, min 5.77, internal_free 15,607) =
  **+12.96 %** over the 5.3033 pin. Full gate; host gates owed.
* **shippable line 5.7583** (ext 5.6946, think 4.47, min 5.55, internal_free 14,167) =
  **+8.58 %**. Full gate; host gates owed.
Both carry the lifetime family (E68 nx/xh + E69 dequant row) and the mix kernels.

**E65 tiled mix, finally priced with its repaired oracle: +0.17 %, banked (#700).**
LANEMIXROW 64/64 with four ordered FMAs over four DISTINCT strided rows; the first
oracle pre-summed the weights into `(w+w+w+w)`, which is a different expression, and the
test caught it - that is the second time this window an oracle, not the kernel, was the
defect. 6.0017 is the campaign's first reading above 6.0 tok/s. Below the bar, so no
repeat and no full gate, exactly like E67.

**Audit results that close two avenues (no board spent):**
* The q/k/v/gate projections ALREADY share one `nd_cq_prepare` + `nd_cq_lut_build` per
  layer (`nd_model.c:1928-1935`), so there is no redundant LUT build to remove; the
  other prepare/lut pairs are out_proj, engram and embedding, each on a distinct
  activation.
* proj2bit works out to ~16 cycles per 32-bit weight word for 16 weights - about one
  cycle per weight with ~33 instructions per word, i.e. dual-issue-saturated. Treat the
  2-bit GEMV as closed on measurement, not on intuition.

**Next candidates, in order:** (1) host gates on both trees (owed, cheap);
(2) port the banked E67 + E65 to the opposite line as one composition and judge it
together - each was sub-bar alone, and this campaign has crossed the bar before by
combining two measured sub-bar halves of the same phase; (3) with ~15.6 KB internal
free, re-price the two RAM-gated ideas (16 KiB private LUT2 tables is now borderline
feasible; phi row residency at ~18 KB/core is not); (4) B3 when its USB returns.

---

## RESEARCHER STATE -- 2026-09-25 ~14:20Z (runs #701-#702: the composition crossed 6.0)

**Seed line at 6.0150 tok/s (screen) = +13.42 % over the 5.3033 pin.** The two banked
sub-bar halves of the mHC lane phase - E65's tiled mix (+0.17 %) and E67's wide lanepre
(+0.146 %) - clear the bar together: +0.22 % as an increment over E65 alone, +0.39 %
over the line's E64-only pin. Bit-exact on device with both verdicts live
(`lanemix_row=1 lanepre=1`), 6/6 exact. This is the #371 pattern again: two measured
sub-bar halves of ONE phase are worth checking before either is deleted.

**Both lines are now evidenced on their current trees except one item:**
* shippable **5.7583 full gate + host 19/19** (B1, `0f0d3668d71f`) - complete.
* seed **5.9917 full gate + host 19/19**; its newest composition tree
  (`a003340489c2`, 6.0150 screen) still owes host gates and a full gate before it can
  claim that number.

**Budget note for the next session:** the seed line now has ~15 KB internal free, and
the composition shows the lane phase still had ~0.4 % left in it. Two directions look
worth a board and are cheap to try in this order: (1) finish the seed line's gates
(host 19/19 is minutes; the full gate is the number the owner would see); (2) the same
"two sub-bar halves" question on the OTHER phases that have one half banked and one
still missing - the mix kernels now have `mul4w`/`lanemix4w`/`lanemix_row4`/`lanepre4w`
and their guard pattern, so any other `dst[i] = k*src[i]` or `dst[i] += k*src[i]` loop
in the block is a candidate to price; (3) B3 when its USB returns (it still carries E67
alone, `b255280ba9a9`).

**Standing rules earned this window:** an acceptance test is as likely to be the defect
as the kernel (E65's first oracle pre-summed its weights; E64's first test raced across
cores; E67's first kernel walked off its rows) - write the reference from the shipping
statements and run it single-threaded; a kernel's call convention must be read off the C
prototype, never copied from a neighbour; and a guard that cannot fire measures as a
perfect null (E66 while `nx` was unaligned).

---

## RESEARCHER STATE -- 2026-09-25 ~14:50Z (runs #701-#703: 6.0133 breadth-gated)

**Seed line is the campaign's first to hold >6 tok/s through a full 20-case gate:
6.0133 / ext 5.9454 / think 4.63 / min 5.79 / prefill 6.3433 / 18-20 (the two #647
goldens) / internal_free 14,991 = +13.38 % over the 5.3033 pin.** Engine
`a003340489c2`; host gates on this exact tree are the only thing still owed.

**The composition rule paid twice now.** E65 (+0.17 %) and E67 (+0.146 %) were each
sub-bar alone; together they are +0.36 % on breadth (#703), the same shape as #371's
bundle. When a phase has two measured sub-bar halves, price the pair before deleting
either.

**Shippable line:** 5.7583 full gate + host 19/19 (`0f0d3668d71f`) = +8.58 %.

**Attempted and REVERTED this window (do not mistake for a result):** porting E65's
tiled mix to B1 as a cross-line confirmation. The splice pulled an `#endif` with it
from the seed tree's block structure and the build broke; B1 was restored byte-exactly
to `0f0d3668d71f` and builds. The lane is worth redoing properly (port the kernel, the
oracle and its own verdict as three separate anchored edits, never as one slice), but it
is a *confirmation* of an already-gated lever, not new discovery - lower priority than
the items below.

**Next, in order:** (1) host gates on the seed line's `a003340489c2` (minutes, and the
only gap in the acceptance packet); (2) the same two-halves question on the phases that
still have one banked half - the mix-family kernels now cover `dst=k*src`,
`dst+=k*src` and the four-lane reduction, so any other loop of those shapes in the block
is a candidate to price, and pairing is allowed; (3) B3 when its USB returns (it carries
E67 alone, `b255280ba9a9`). With ~15 KB internal free, the 16 KiB private-LUT2 idea is
borderline feasible; phi row residency (~18 KB/core) is not.

---

## RESEARCHER STATE -- 2026-09-25 ~15:25Z (runs #704-#705: both packets complete)

**Both lines now have complete acceptance packets; the only open item on either is the
owner's two-case disposition.**
* **seed 6.0133** (`a003340489c2`): device 18/20 (delta 52, the two #647 goldens), ext
  5.9454, think 4.63, prefill 6.3433, host 19/19, logit delta 5.341e-05 unchanged,
  internal_free 14,991 = **+13.38 %** over the 5.3033 pin.
* **shippable 5.7583** (`0f0d3668d71f`): device 18/20, ext 5.6946, think 4.47, host
  19/19 = **+8.58 %**.

**E70 elementwise cluster: +0.12 %, sub-bar, banked (#705).** Four loops reduced to the
two mix-kernel shapes - `u -= ublk` as `dst += (-1)*src` (an exact -1 multiply, so the
rounding matches the C subtract), the embedding scale and pool_cell's rescale as
`nd_mul4w`, pool_cell's accumulate as `nd_lanemix4w` - with `ublk`, `tmp` and `pool_acc`
moved to FAST16 so the guards can fire. Corrected before spending a board: the attention
gate loop is a per-PAIR scale, so `nd_mul4w` does not apply there. Banking puts B1 at
5.7650 with three sub-bar halves now in hand (E67 +0.146 %, E70 +0.12 %, and E65 which
B1 does not yet carry).

**Next lanes, in order:** (1) compose on B1 - port E65 properly (kernel, oracle and its
own verdict as three separate anchored edits, not one slice: the earlier single-slice
attempt dragged an `#endif` across and was reverted) and judge E65+E67+E70 together;
three measured sub-bar halves of one block is exactly the shape that has crossed the bar
twice now. (2) The same question on the seed line for whichever of E70's four loops has a
partner there. (3) B3 when its USB returns (`b255280ba9a9`, E67 alone). The seed line's
remaining phase map is dominated by proj2bit (closed on measurement: ~16 cycles per
32-bit word for 16 weights, dual-issue saturated) and engram (same 2-bit path).

---

## RESEARCHER STATE -- 2026-09-25 ~16:05Z (runs #706-#707: lane block swept on both lines)

**The whole mHC lane block is now widened and measured on both stacks.** Every loop in
it that reduces to one of the four kernel shapes is covered:
* **E64** five-pass mix (kept, +0.99 % seed / +0.92 % shippable full gates)
* **E65** tiled mix (+0.17 % alone), **E67** wide lanepre (+0.146 % alone) - together
  **+0.36 % on the seed line's full gate (#703, 6.0133)** and **+0.20 % on the
  shippable line (#706)**, both now kept
* **E70** elementwise cluster (`u -= ublk` as `dst += (-1)*src`, embedding scale,
  pool_cell rescale/accumulate) - **+0.14 % seed (#707, banked, new best 6.0217)** and
  +0.12 % shippable (banked)

**Readings now:** seed **6.0217** screen (6.0133 is the gated, packet-complete number at
`a003340489c2`), shippable **5.7767** screen (5.7583 gated) = +8.93 %. Both trees owe
nothing but a fresh full gate + host gates when the owner picks a submission point.

**Three findings worth keeping:**
1. The "two or three sub-bar halves of one block" pattern has now paid three times
   (E65+E67 on seed and shippable). Never delete a measured sub-bar half of a phase
   without pricing it against its siblings.
2. A single-slice port between worker trees is dangerous: one attempt dragged an
   `#endif` across and another re-copied a whole kernel body because the slice ran to
   end-of-file. Port as separate anchored edits; the build caught both, not the board.
3. `-1.0f` as the multiplier in `dst += w*src` is an exact negation, so a subtract loop
   widens with the accumulate kernel and is bit-exact; a per-PAIR scale is not a
   constant and does not.

**Next:** (1) when the owner picks a line, run one fresh full gate + host gates on that
exact tree (nothing else is owed); (2) B3 when its USB returns - it still carries E67
alone (`b255280ba9a9`) and would need the E64/E65/E70 ports to match either line;
(3) the remaining phases are at measured floors (proj2bit dual-issue saturated, engram
the same 2-bit path, kron2 closed, QK closed against GCC's schedule), so a new direction
needs a fresh phase map rather than another pass over the same levers.

---

## RESEARCHER STATE -- 2026-09-25 ~16:45Z (runs #708-#709: packets complete, fresh map)

**Both lines are fully evidenced on their newest trees; nothing is owed but a decision.**
* **seed 6.0133 gated** (`a003340489c2`; screen 6.0217 with E70 banked): device 18/20,
  ext 5.9454, think 4.63, host 19/19 = **+13.38 %** over the 5.3033 pin.
* **shippable 5.7767 gated** (`bae5184f9ca7`, full gate #708 + host 19/19 #709):
  ext 5.7115, think 4.49 = **+8.93 %**.

**The lane-block composition is +0.32 % on the shippable line on breadth** (5.7583 ->
5.7767) and +0.36 % on the seed line: the two/three-sub-bar-halves pattern now holds on
both stacks.

**Fresh phase map (seed stack, profiled restricted run; absolute decode is diagnostic
only):** `attn-stage 82.5 (proj2bit 80.8, attention 24.1)`, `hadamard 21.2`,
`engram 15.2`, `mhc_phi4 8.1`, `sinkhorn 3.0`, **`mhc-mix 2.0`** (was 4.5),
`prep+lut 1.7`, `step-tail 0.6`. The mix line is the lane-block sweep appearing in the
attribution; every phase still above ~8 ms is one this campaign has closed on
measurement (2-bit LUT GEMV dual-issue saturated, engram the same path, phi already
wide, kron2 negative twice, QK negative against GCC's schedule).

**What this means for the next window:** the remaining headroom is not reachable by
another pass over the same levers, and the largest single item (proj2bit, ~59 % of the
token) is bounded by issue rate rather than by memory. A genuinely new direction needs
either (a) a structural change to how the 2-bit weights are consumed (nothing on the
shelf passes the three-condition rule: the LUT lookups are indexed, not contiguous), or
(b) the owner's decision opening the 120 MHz / assertion-level questions that were
parked, or (c) B3's recovery so a third line can run independent hypotheses again.
**B3's flash is still EIO after three checks; no further USB loops were spent.**

---

## RESEARCHER STATE -- 2026-09-25 ~17:40Z (run #710: a probe invalidated by its own control)

**Frozen-pair per-step diagnostic: probe INVALID, no signal (#710).** The hook printed
`nd_sample_hidden`'s logits pointer argmax next to the sampler's chosen id. That pointer
is the constrained sampler's FILTERED subset, not the full-vocab array, so indexing it
by `s_model.vocab` reads past the buffer: every step of every case - including the
**passing control** - reported top1=989 at 1.9e37. The control case is what exposed it
within the same harvest. Valid data from the run: the per-step chosen ids (real
sampler decisions), the three texts (failing cases reproduce: `seconds=300` where the
golden says 1, and a repeating `heap high water mark -> seconds 300` degeneration; the
control is correct), and the two failing cases agree with the control's first three
chosen ids (6, 38, 8121) before diverging - consistent with the earlier input record
(qlen/qhash/token ids all intact), so the divergence remains downstream of the prompt.
**Next attempt must read the sampler's own candidate list or the existing `logits4`
hook, never index the subset pointer.** B1's tree still carries the `ND_REQ_DIAG` hook
(harmless, behind an #ifdef) and its `diag_build_cfg` has been deleted, so it is a
normal speed tree again - but it has not been re-measured since.

**Where the campaign stands:** both lines are packet-complete at **6.0133** (seed) and
**5.7767** (shippable) = +13.38 % / +8.93 % over the 5.3033 pin, with 18/20 device
(only the two #647 goldens), host 19/19, fidelity unchanged. The fresh phase map shows
every remaining phase above ~8 ms is one this campaign closed on measurement
(proj2bit dual-issue saturated on the 2-bit LUT path, engram the same path, phi already
wide, kron2 and QK negative on measurement, kron1/FWHT/PV/mix all widened and kept).

**So the honest next steps are:** (1) the owner's decision - either accept a line with
the two cases re-baselined, or park the ring and ship the no-ring variant; (2) B3's
recovery, which needs the host's USB hub attention rather than another container probe
(three EIO checks, no further loops spent); (3) if more speed is required, it has to
come from a NEW mechanism rather than a re-pass - the campaign's own cost model says
the 2-bit weight stream is bounded by issue rate, not by memory or by the compiler.
