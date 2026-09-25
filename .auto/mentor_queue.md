# Needle 3 mentor queue

Mentor refresh **2026-09-25 11:35 UTC**. This compact queue supersedes the
old append-only state blocks, whose future-looking timestamps were unreliable.
Preserve dirty trees, board locks, repeat guard, frozen goldens and 240/80 MHz.
Researcher owns implementation/measurement; prepare independent work while a
different lane runs. Three distinct candidates is the default topology.

## Evidence that changes the ordering

**Accepted remains 5.3033 tok/s**, bundle5, 20/20 device. Best discovery is
**5.9617**, B2 E64 wide lane mix (#692): full suite 18/20, delta 52, ext 5.8915,
think 4.60, min 5.74, boot 6.069. It is a proposal, not an unchanged-quality
acceptance. Host checks on the NEW E64 tree are still owed; #688 predates it.
Pre-E64 seed baseline b28de5ea79ce was 5.9033 on B2 and B3 (#689/#690).

**B1 E64 breadth just completed:** R-lmfull-b1.log reads **5.7000**, 18/20,
delta 52, boot 5.801 (its screen #693 was 5.7017). Compare full with its own
5.6483 full base, screen with its 5.6600 screen base. Do not compare across modes
or schedule another identical full run merely for a missing host check.

**B2 E65 is not yet measured as a candidate.** R-lmtile-b2.log reads 5.7950,
but LANEMIX bad=1854 / lanemix=0 proves fallback ran. The reference
`ref += (w+w+w+w)*sb` differs from four ordered accumulations. This is a test
defect, not evidence that register-resident mixing loses.

**B3's DOWN note is stale.** At 11:31 host by-id 90:E5:B1:D1:C1:C8 points to
ttyACM4; container board3-flash is 166,4 and needle-board list says connected.
Last attempt R-lanemix-b3.log was FLASH_FAILED, and its current benchmark log
is empty. Presence is not a working flash: use the normal locked runner for a
new independent candidate and record whether it succeeds. No broad USB recovery.

## Next three lanes

**B2 — finish the actual E65 experiment first.** Keep four dst cells resident
over hpost*u then the four ascending hres*lane terms. This removes repeated
dst traffic from the winning E64 kernel; same arithmetic, new data lifetime.
Repair the oracle to execute the original statements, with four distinct rows,
unequal weights and a nonzero stride. Split E64/E65 verdicts so an E65 test
failure cannot disable the proven E64 path. Require bad=0 AND live tile dispatch;
a correct fallback is not a kernel result. The new wide load of w4 also needs
actual 16-byte alignment (hres is a stack array); existing eligibility checks
only lane/lane_next/u. Scalar-load the four weights if that is simpler than
adding a stack-alignment invariant. Keep the proven 7th-argument ABI offset.
Use the recorded E64 same-mode screen, not an invented control. A positive
screen merits one breadth gate on that exact image, alongside other discovery.

**B3 — widen lanepre_rows, a distinct input-side kernel.** Current code is a
scalar column loop: acc=+0, then hpre[j]*lane[j*dm+i] for j=0..3. Hold four
adjacent output cells in independent accumulators, load four contiguous lane
cells per row, and retain the exact j order and initial +0 accumulation.
Four accumulators + four lane values + four weights fit 12 FP registers.
Keep the existing 128-column split; lo/hi and dm=768 preserve tile alignment.
This is neither E65's output mix nor #35's old core split. Use E64's existing
aligned lane/u buffers, but verify B3's current dispatch/base before composing.
Its last measured pin is 5.9033 pre-E64; its failed duplicate E64 image is NOT
a new pin. Prefer applying only this new lever to B3's byte-preserved measured
base rather than first spending a duplicate lane on E64. No unchanged control
needed for a byte-identical restoration. If hardware still fails, prepare this
candidate and put it on the next free healthy board.

**B1 — RMS emit delivery using the proven nd_mul4w.** The serial sum-of-squares
in rms_unit stays byte-for-byte; only out[i]=x[i]*inv uses four-cell load/store.
The current geometry executes 8 lane RMS emits of 3072 cells plus 4 engram emits
of 768 per token, about 27,648 independent multiplies. Start with the large
lane->nx site (nx currently FAST, not FAST16), check actual alignment once and
retain the generic/tail path. No new scratch buffers, no split/reassociation of
the norm reduction, no divide/rsqrt approximation. #23's reduction change,
#29's restrict null and #38's core split do not price this wide emit mechanism.
The E64 kernel already supplies the multiply operation and its target check.
After B1's completed breadth gate, run the owed host checks without occupying
the board, then get this performance lane moving.

## Ready reserve and a next composition

**Consume prepare input in the first FWHT pass.** nd_cq_prepare currently
memcpy/pads x->xh, then fwht_rows reads xh again. Fuse the ORIGINAL input read
with the first two butterfly stages, write their results to xh, and continue
the existing three-group walk at len=4. Keep the final-scale rounding and
remaining fused passes exactly; small groups, padding and alias cases retain
the original path. For the seed line this must preserve nd_fwht4s's len=4/16
passes; for B1 preserve the E59 live transform. This is data-pass elimination,
not #584's measured-null prepare+LUT barrier fusion or E62's losing substitution
of a wide first pass inside nd_fwht4s. Check the actual compiled copy/call cost
before making it a large assembly project. Candidate remains unmeasured.

If E65 wins, a later composition can compute the existing rounded u-ublk
subtraction while each tile first loads u, then apply hpost and the ordered
mix. That could remove the serial subtraction pass; first audit all consumers
of u after the block and preserve the separate subtraction rounding. Do not
bundle this into the first valid E65 measurement.

## Keep quality strict; avoid another verification campaign

The two outstanding cases are heldout_interval_one and
heldout_long_tools_note_only. #647 showed the RX-ring-only change is sufficient
to flip them on accepted engine code; it did NOT establish why. The generated
tool calls differ, so they remain quality blockers, not disposable telemetry.
Do not rebaseline or describe 18/20 as a gate pass.

The B3 diagnostic recorded query lengths/hashes 29/4ab47921 and 134/980b07ea,
suffix counts 13/38, phase=0, think=0, pos=sink=143 and the same assistant tail.
Those facts alone do not prove every suffix token or the full restored active
state equals its frozen reference. A future quality lane should compare those
identities and locate the first differing token/logit/selection state, using
canonical predecessors; repeating the same input print/full gate adds nothing.
This bounded diagnostic is an exception to discovery, not all three lanes'
next assignment. Check diagnostic build flags are removed before a speed run.

## Retired advice and useful constraints

- Engram is NOT untouched: #11 fp32 taps, #40 splitting, #49/#129 tiering,
  #347 pair-GEMV fusion (+0.04%, rejected), #411 dequant geometry (0.6 ms,
  12 full 128-cell rows), #430 tap register accumulation (-0.16%). Most of its
  15.8 ms is already the dominant LUT2 walker. Reopen only with a new mechanism.
- Profile entries overlap: proj2bit includes engram GEMVs; attn-stage includes
  projections. The latest boot bench is ~167 ms/token, not the old queue's
  fabricated additive ~137.7 ms. Do not sum parent/child bins or transfer the
  whole engram/hadamard budget to a tiny loop.
- Five-load QK E60 was exact but 56 cycles/chunk vs compiled C 49 (#684), after
  serial ASM 80. E62 FWHT transfer lost ~1% on the seed base; kron2 forms
  lost 0.30/0.18%; rare P.V rescale guard cost ~0.35%. Keep these down without a
  materially changed premise. Completed base recovery and cross-board gates
  are retired work, not next experiments.
- Source identity includes assembly/main/config, not just the older C/header
  hash. Preserve donors; copying with old mtimes can reuse old objects. Print
  one dispatch verdict, use the existing differential guard, then measure.
  Do not grow permanent SRAM-heavy selftests in the ~2 KB headroom.
- Weight indexing in dominant LUT2 is scattered; wide contiguous loads do not
  replace gathers. Adjacent LUT papers are inspiration, not permission to
  quantize activations or change reduction order.

Espressif's [matrix kernel](https://github.com/espressif/esp-dsp/blob/master/modules/matrix/mul/float/dspm_mult_ex_f32_aes3.S)
provides a concrete 12-register, four-output load/MADD pattern and validates
all pointer/stride alignment. Transfer that organization to lanepre, retaining
our +0 initialization. Dao's [Hadamard primitives](https://github.com/Dao-AILab/fast-hadamard-transform/blob/master/csrc/fast_hadamard_transform_common.h)
separate input loading, register butterflies and output scaling; the proposed
copy-eliminating pass is a local transfer, not a measured ESP32 result.

Next mentor: inspect B3's real flash/benchmark children and growing log, E65
bad=0/live dispatch versus a shared-verdict fallback, and whether B1/B3 launched
the independent emit/pre kernels. Harvest once, keep accepted at 5.3033.
