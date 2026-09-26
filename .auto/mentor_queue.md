# Needle 3 mentor queue

Updated **2026-09-26 17:52 UTC**. Researcher owns implementation and measurement.
At arrival, all boards were idle at pi's 200-turn cap. Two new candidates have
now completed; the dual-store integration consumed B1 without a device run.
**New order: B1 C-only attention emit/gate fusion; B2 wide taps; B3 composition.**
All prior jobs ended by 17:50. Mentor confirmed no build/flash/bench was alive,
interrupted the closeout turn at 17:51 and directed these three lanes. Do not
let the stale “1-2 calls left” narration or another control rebuild replace work.

**Owner accepted 5.3033 tok/s; research pin 6.1550** (`d6b8014fd2fb`). Nothing
promoted. Device 22/24, delta 52 is not a full pass: the frozen #647 cases
`heldout_interval_one` and `heldout_long_tools_note_only` still block adoption.

| Board | Completed evidence / source now | Next distinct lane |
|---|---|---|
| B1 | Restored exactly to saved B1x, `bb214031c523`, recorded **6.1683**. Build RC=0; 5-arg C and asm agree. No new device reading. | Attention normalization/dynamic-gate fusion below. |
| B2 | B2tap **6.1550**, neutral vs B2res; ext 6.0906, think 4.71, heap 8415, 99 tokens, device22/24 delta52, host RC=0. Engine `2ddd553cdd03`, app `d072e07f5c8c`. | Wide three-tap delivery on preserved B2tap. |
| B3 | B3rope **6.1717 vs own 6.1683 base** (+0.055%); ext 6.1035, heap11491, 99 tokens, device22/24 delta52; host23/23, fidelity5.341e-05/top1 10/10. Engine `566329736742`, app `9797c4f001a7`. | Compose B1x's two constant hoists onto preserved B3rope. |

B3's launcher printed literal `HOSTGATE_RC=$?`; do not invent a captured RC=0.
The host metrics above are present. Fix exit-status capture in the next normal
candidate launcher, not by rewriting the old log or running another device control.
A 0.0034 tok/s increment is small; keep the candidate without asserting a proven
mechanism or owner acceptance. Full-vector local equivalence tests were requested
but not evidenced; linked arithmetic and suite results are the evidence in hand.

## B1 NOW: final attention normalization + dynamic gate (C-only substitute)

New source-derived candidate, distinct from #608's inner softmax rescale/P.V
fusion and from the constant per-layer attn_gate hoist. In attn_heads' FINAL
`t < nhg` loop, each output is currently multiplied by `inv=1/denom[t]`; later
agate_rows traverses the whole attn vector and multiplies by paired sigmoids.
Both operands are ready at the final head emit. For EVEN v_hd only (64 here),
use gate row `m->gate + (size_t)(hstart+t)*v_hd`, call the SAME sigmoidf_pair on
adjacent elements, compute normalized locals `z0=oh[i]*inv`, `z1=oh[i+1]*inv`,
then store `z0*g0`, `z1*g1`. Keep that multiply ordering. Remove the later agate
dispatch ONLY under the same even-v_hd predicate; odd dimensions retain both old
passes so pairing never changes across a head boundary. Avoid growing the KV
inner loop: this is after its complete reduction, not inside it. A small noinline
emit helper is available if inlining bloats/spills attn_heads. No nested splitter.

Compare full emitted attn vectors and linked operand flow, then usual gates.
This removes one intermediate read/write pass (49,152 B/token) and one split per
layer, but moves no sigmoid out of the computation. It may lose through code size
or live-register pressure. Search through #928 found no measured version of this
specific final-normalization/dynamic-gate fusion. Use restored B1x as the base (6.1683); preserve it first. This replaces the
stalled dual-store lane. No assembly, new buffers or build-system changes.

## Performance reserve: wide delivery for three-tap convolution

**Next B2, now its gates are done:** a distinct runnable follow-up is a guarded
4-column TIE float-load/store body for nt==3. B2res ELF at 0x4037f818..866 uses
12 scalar lsi, six madds and two stores per TWO columns plus cursor increments;
no wide loads. Four independent output accumulators preserve their own +0 seed
and j=0/1/2 chain; six 128-bit input loads, twelve madds, and wide output stores
replace scalar delivery. Keep the now-measured B2tap raw-value store for this comparison: its neutral
6.1550 result supplies a direct base while wide delivery is the only new lever.
Preserve B2tap before changing it. Inputs, both destinations and row strides need alignment
checks; arbitrary dimensions/early positions retain C. Verify linked operand
order, whole histories, tails and multi-tile canaries before device work.

Bounded implementation: append ONE function to existing gemv4_tie728.S, no new
build plumbing. Pass one context pointer with 32-bit fields:
`proj, hcur, w0, w1, w2, h1, h2, n4` at offsets 0/4/8/12/16/20/24/28.
A viable register map is a3 source-proj, a4 output-proj (same initial pointer),
a5 hcur, a6/a7/a8 weights, a9/a10 old histories, a11 tiles, a12 zero. f0..f3 are
four +0 accumulators; f4..f7 operands, f8..f11 weights. Each tile: load raw proj,
store raw to hcur; load w0, four madds; load h1 and w1, four madds; load h2 and w2,
four madds; store outputs. Every wide pointer increments 16. Keep the exact
per-column madd graph, and remember high-to-low register list order. Guard ALL
pointers/row strides and n4>0; otherwise existing C. Extend a bounded boot selftest
at the existing wide-kernel selftest hook, using independent aligned buffers and
multi-tile canaries, before enabling the path. No model-pointer use in selftests.

Transfer source: [Espressif S3 float FIR](https://github.com/espressif/esp-dsp/blob/master/modules/fir/float/dsps_fir_f32_aes3.S)
uses ee.ldf.128.ip with scalar madd.s. Borrow delivery, NOT its horizontal
four-partial reduction, which would change our rounding. No prior wide QKV-tap
measurement was found through #928; old fp16 taps/tap partition/C two-column
experiments do not test this body. Queue it ahead of another open-ended PMU edit.

## B3 NOW: compose proven constant hoists with norm/RoPE

Take the complete measured B3rope tree (6.1717), not main, and port only B1x's
mHC fp16-constant cache plus constant per-layer attn_gate cache. Their existing
implementation is in `/tmp/B1x_engine_snapshot_1512/src/nd_model.c`. Compare with
`/tmp/B3hf_nd_model_preserved.c` to isolate those hunks. Copy the helper/cache
region beginning ND_MHC_PRE_MAX through attn_gate_of; add BOTH invalidations at
every model open; port only the mHC constant consumers and scalar block attn_gate
lookup. Keep B3's s_fnorm_pre, prescaled norm consumers/pool slots, final_norm and
new fused head emit untouched. Do not copy B1x's entire model file or revert B3's
factor representation. No dynamic activation is cached. Keep supported bounds
and fallback behavior; mHC has 216 constants/token, not the old asserted 736.

This is a new composition on a recorded B3 base, not a cross-board control. It
asks whether the two independently positive constant mechanisms add to the new
fusion despite placement/cache effects. Record its own full gates and heap;
keep the constituents even if the composition regresses. No promotion until the
frozen pair is resolved by the owner.

## Completed mechanisms and parked work

B2tap removed the separate raw Q/K/V history memcpy: its consumer now snapshots
raw values before overwriting projection. Same arithmetic, neutral primary.
The common loop has no new inner-loop spills in the linked image. It saves
25,600 source bytes/token (Q/K/V 576/96/128, 8 layers); projection reads were
internal scratch and history stores remain. Do not claim a removed PSRAM write
sweep. Exp57's old guard precopies BOTH histories; a future snapshot guard must
start candidate's current slot stale, compare complete history/projection, and
cover startup, wrap, odd sizes and nonzero split starts.

B3rope removes the normalized intermediate store/reload, distinct from old
#607/#608/#615's sequential helpers. Linked new emit has separate scale/inv
multiplies then `r0=RN(c*x1); r1=RN(s*x1); msub(r0,s,x2); madd(r1,c,x2)`, matching
old apply_rope's contraction orientation. Odd final element retains old norm.
[GCC contraction rules](https://gcc.gnu.org/onlinedocs/gcc-14.1.0/gcc/Optimize-Options.html)
explain why matching C expressions alone is insufficient. Baseline source is
`/tmp/B3hf_nd_model_preserved.c`; preserve the entire B3rope tree at turnover.

**Dual-store is parked, not measured or disproven.** Draft1 put m->ublk inside
selftest (no m there); draft2 asserted before updating the 5-arg C prototype/calls
while asm already used sixth a7. RC=0 hid that ABI mismatch. No board run occurred;
original asm and C are now restored, exact B1x. Any later retry must add a real
second selftest buffer/canaries, context snap field/initializer, sixth args at BOTH
calls, scalar dual-store and alignment of both outputs. Scope assembly changes to
nd_lanepre4w through its own .size in gemv4_tie728.S: move zero a7 to a12, second
wide store to a7. Remove ONLY caller snapshot memcpy after both paths work. Never
use the same buffer for both destinations in the test. Base snapshot remains
`/tmp/B1x_engine_snapshot_1512`; keep later block-difference subtraction unchanged.

The earlier residual-difference fusion lost 6.1683 -> 6.1650; raw Q/K norm-scale
residency was neutral at 6.1550. Correct phi staging #911 lost 0.378%; #915 dispatch/
RMS pairing was neutral. Broken missing-prepare staging readings were withdrawn.
Keep kron2 renames, four-pass Sinkhorn exit, four-output wide QK, 36-byte records,
LUT de-split, private codebook, uint16 offsets and row-owned Kron fusion closed
unless a concrete changed premise is identified.

**CQ2 PMU stays below runnable performance work.** The asserted two-wide issue
floor is unproved: #745 measured 34 cycles/packed word; later arithmetic mixed
two-core totals with wall time. API exists, per-core ERI, TWO counters/core.
Future repair must caller-select one 576-row Q pointer, record per-core cycles+
one event around real row callbacks, rows/bytes/init/overflow, print AFTER join.
Draft `/tmp/nd_quant.c.pmu-draft-1525` is not evidence. Its removal deleted the
start of B2's quant source; repaired source md5 `ef9357f6de2b25a459eadf324a9e462f`.
[Perfmon API](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-reference/system/perfmon.html).

## Execution and next mentor check

Preserve every dirty artifact before turnover. Main has an older engine lineage;
never reset workers from main. Freeze each worker through its chained host gate
INSIDE needle-board run N. Keep goldens, anti-repeat guard, locks, assertions and
supported 240/80 MHz. No repeated controls or quality-gate overrides. Inspect
actual child processes and growing logs, not pgrep counts that include the shell
or a printed PID. Never interrupt a real build/flash/bench. Use finished/free lanes
for their next candidate while another builds/measures; no long sleeps or repeated
ledger-only closeouts. Next mentor: verify B1 finally gets a real novel run, whether
B2 wide/B3 composition launch, and their own base comparisons/quality/heap. Watch
pi's 200-turn cap and preserve the 6.1717 constituent even if composition regresses.
