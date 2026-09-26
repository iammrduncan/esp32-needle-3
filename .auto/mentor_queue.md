# Needle 3 mentor queue

Updated **2026-09-26 20:19 UTC**. Researcher owns implementation/measurement.
Owner accepted **5.3033 tok/s**; research pin **6.1550**, engine d6b8014fd2fb.
Nothing promoted: the frozen #647 cases heldout_interval_one and
heldout_long_tools_note_only still fail (22/24, delta52). This pattern is
lineage evidence, NOT full correctness or permission to replace goldens.

At arrival pi was idle at its 200-turn cap; all three boards were idle. Mentor
resumed it. Current order: **B1 outlined gate emit; B2 P1 gather/SiLU fusion;
B3 integer permutation metadata on repaired composition.** Wide taps moves down
because it remained unstarted across passes; it is unmeasured, not disproven.

**B2 INVALID RUN:** the 20:16 draft mistakenly declared perm as uint16_t*
and cast p1. Despite the queued warning it flashed before consuming that message.
P1 is FP32; that cast COMPILES and gives wrong/out-of-range indices. Leave the
live B2p1 job/gates untouched; no speed conclusion is valid from this attempt.
Use const float *perm and pass p1 without any cast. Integer metadata on B3
requires actual numeric conversion at open; never reinterpret FP32 storage.

| Board | Own completed base / current work | Next action |
|---|---|---|
| B1 | B1gate 6.1717 vs B1x6.1683; ext6.1041, heap9335, host RC=0. B1out finished device6.1717 (neutral), ext6.1047, heap9591 (+256); host gate pending; app dc13d8bb3cab. | Preserve it; speed mechanism is neutral, no repeat. Next reserve after gates. |
| B2 | B2tap6.1550 neutral; ext6.0906, heap8415, host RC=0; engine2ddd553cdd03. B2p1 is live with the pointer-type bug above. | Let guards finish, mark invalid; fix type and host-gate before corrected flash. |
| B3 | B3comp DONE6.1750 vs B3rope6.1717; ext6.1100, prefill6.52, think4.72, heap10351, 99 tokens, device22/24 delta52; host23/23, fidelity5.341e-05/top1 10/10, RC=0. Engine5091d6b9d115, app23ab6a0002d7. | Preserve it; repair cache lifetime/bounds and try integer permutation metadata. |

B1gate and B3rope happen to read6.1717 on different trees: not a confirmation
pair. Increments of0.0033 are small; retain evidence without claiming proven
significance. B3rope's old host launcher wrote literal HOSTGATE_RC=$?;
its metrics exist, but no captured RC may be invented or old log rewritten.
New script launchers capture the actual exit code.

## B1: outlined final attention gate emit

Source-derived hypothesis: B1gate fused the final head normalization and dynamic
sigmoid gate, removing an intermediate pass and split, but enlarged attn_heads
to0x1bd7. The pair loop repeatedly materialized frame offsets0x488/48c/490/494
around __divsf3. Outline ONLY that even-head loop into:
static ND_HOT __attribute__((noinline)) void attn_gate_emit(float *oh,
const float *gp, float inv, uint32_t n).
Same sigmoidf_pair, z0=oh[i]*inv, z1=oh[i+1]*inv, then z0*g0 and z1*g1.
One call/head, caller computes inv once; KV reduction and odd-head fallback
unchanged; later agate dispatch remains skipped for even heads.

**Linked B1out verified:** helper0x35e bytes, frame64; caller0x1783. Spill loads
are now direct offsets0/4/8/16, and four mul.s preserve the normalization-then-
gate graph. This is the intended mechanism; device speed decides call overhead.
Compare with its B1gate6.1717, not B1x or another board. B1gate model source:
 /tmp/B1gate_nd_model_preserved.c. Keep full engine snapshots at turnover.

## B2: P1 gather inside the existing SiLU callback

Preserve B2tap first (model copy /tmp/B2tap_nd_model_preserved.c).
hadamard_mlp_unscaled writes kron_apply's output into hada_b, serially copies
hada_a[i]=hada_b[(uint32_t)p1[i]], then silu_rows reads/overwrites hada_a.

Append **const float *src, *perm** to silu_ctx; initialize src=m->hada_b,
perm=p1, retaining a=m->hada_a. In the existing pair loop load BOTH
x0=src[(uint32_t)perm[i]], x1=src[(uint32_t)perm[i+1]] before output writes.
Use those locals in the SAME d2*sc*x+b2 expressions, sigmoidf_pair and z*sigmoid.
Remove ONLY the first serial P1 copy. Keep scale-row construction, P2/d3 pass
and all Kron reductions. The two arrays stay distinct until the callback joins.

Saves one intermediate write/read (65,536 B/token for1024*8), while putting the
gather in an existing two-core split. Extra live pointers/gathers may lose.
Check full emitted vectors against saved producer+consumer and linked FMA
operand order, then normal gates. No new allocation/assembly. #369's four-way
gather unroll lost0.133%; it did not fuse the consumer or remove this temporary.
No prior measured P1-consumer fusion found through #931.

## B3: cache repairs, then immutable integer permutation metadata

B3comp added B1x's mHC constant and per-layer attn_gate hoists to B3rope.
The graft omitted BOTH invalidation calls. Mentor warning was queued before
launch but consumed after flash; its run was left untouched and is now done.
Preserve this measured tree before repairs. Add mhc_cache_invalidate() and
attn_gate_cache_invalidate() after nd_model_open's memset.

Pi's assertion that fixed arrays provide bounds fallback is false. Current
mhc_const_fill copies fixed32/128/8 counts without shape gating; attn_gate_of
caps filling at64 but returns s_agate[li] unguarded. The8-layer/4-lane blob fits.
Require actual layer/lane/tensor consumer bounds and original fp16_get fallback
outside them; copy only valid elements. Host goldens did not prove reopen or
general-shape correctness. Neither this defect nor the frozen pair is waived.

**Distinct next candidate:** decode P1/P2 indices numerically ONCE at open.
Mentor read archive records226/227: each FP32[1024], all integral unique0..1023.
The linked serial loops load FP32 then utrunc.s for every index, every layer.
Use ONE zeroed model-owned uint16_t *perm16 for two adjacent tables (4096 B on
this blob). Guard hada_n<=65536 and both tables' finite/integral/in-range values.
Allocate optional PSRAM metadata at the END of nd_model_open, after tensor
binding and hot scratch/selftests, before open's own final return. Allocation
failure or unsupported input leaves the original FP32 paths. Free at close.
Do not allocate at initial memset, before tensor pointers exist.

Branch outside the two gather loops. Keep BOTH original gather loops on B3;
do not copy B2's fusion. This removes16,384 float-to-index conversions/token,
not any floating arithmetic. Compare against B3comp6.1750 and disclose the
accompanying cache repairs. Do not promise a large gain from instruction count.
No prior metadata-cache measurement found. CQ2 expanded-offset streams are a
different representation with a different bandwidth cost.

[T-MAC](https://arxiv.org/html/2407.00088v1) motivates immutable index-layout
preprocessing, but its SIMD byte tables/quantization are not an exact transfer.
This blob's CQ2 centroids (-.13317214,-.03990209,.04003528,.13346045) are NOT
sign mirrors; mirror-table compression therefore lacks an exact premise.

## Reserve: wide three-tap delivery (still unmeasured)

On preserved B2tap, specialize nt==3 with four independent +0 accumulators,
each keeping j=0/1/2 madd order. Six128-bit loads and wide output stores replace
scalar delivery; retain B2tap's raw projection snapshot before overwriting it.
Guard all pointers, row strides, tile count and alignment; startup/tails retain C.

Append one function to existing gemv4_tie728.S; no new build plumbing.
One context pointer,32-bit fields: proj,hcur,w0,w1,w2,h1,h2,n4 at offsets
0/4/8/12/16/20/24/28. Candidate map: a3 raw source, a4 output (same initial
projection), a5 hcur, a6/a7/a8 weights, a9/a10 histories, a11 tiles, a12 zero;
f0..f3 sums, f4..f7 values, f8..f11 weights. Per tile load/store raw to hcur,
load w0/madd, h1+w1/madd, h2+w2/madd, store output; pointers advance16.
Remember high-to-low TIE register lists. Compare complete projection/history,
multi-tile canaries, startup/wrap/odd sizes/nonzero split starts; current slot
must start stale in the oracle (old exp57 pre-copied it and masked this class).

Transfer [Espressif's float FIR delivery](https://github.com/espressif/esp-dsp/blob/master/modules/fir/float/dsps_fir_f32_aes3.S),
not its horizontal reduction, which changes our rounding. Do not let this
integration monopolize a board if the ready C candidates remain unmeasured.

## Retained negative evidence and parked work

- Residual-difference fusion lost6.1683->6.1650. Raw Q/K norm residency and raw
  tap-history snapshot were neutral6.1550. The snapshot saves source reads,
  not history stores or a PSRAM write sweep.
- Correct phi staging #911 lost0.378%; missing-prepare staging readings were
  invalid and withdrawn. #915 dispatch/RMS pairing was neutral.
- Keep kron2 renames, four-pass Sinkhorn exit, four-output wide QK,36-byte
  records, LUT de-split, private codebook, expanded CQ2 uint16 offsets and
  row-owned Kron fusion closed unless the premise changes.
- Dual-store remains unmeasured/parked after ABI/selftest integration failures.
  Later recipe: scope nd_lanepre4w through its own .size; sixth destination a7,
  move tile-zero to a12, second wide store; update context, initializer, BOTH
  callers, C prototype, scalar path and both-output alignment. Independent
  aligned selftest buffer/canaries, never a model pointer in selftest. Remove
  only snapshot memcpy after both paths work. Saved B1x:
  /tmp/B1x_engine_snapshot_1512. Keep later block-difference subtraction.
- CQ2 PMU stays below runnable work. Two-wide issue-floor claims mixed two-core
  work with wall time; #745 measured34 cycles/word. Installed core-isa.h has
  FLIX3=0, one load/store unit,4-byte fetch; do not infer per-core IPC from
  phase totals. API is real/per-core ERI, two counters/core:
  [perfmon](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-reference/system/perfmon.html).
  Future bounded screen: caller-select one576-row Q pointer, per-core cycles+
  one event around real row callbacks, row/byte/init/overflow fields, print
  after join. Draft /tmp/nd_quant.c.pmu-draft-1525 is not evidence.

## Execution and next mentor check

Preserve every dirty artifact; main has an older lineage, never reset workers
from main. Keep each worker frozen through its host gate INSIDE needle-board
run N. Preserve goldens, anti-repeat history, locks, assertions and240/80MHz.
Inspect live child processes AND growing non-empty logs. Never interrupt real
build/flash/bench work. A model's repeated “1-2 calls left” narration is not a
reason to guess types, leave ready boards idle, or log another finish.

Next mentor: B1out's own6.1717 comparison; B2's FP32 pointer correction and
real P1 measurement; B3's actual cache repairs and metadata lane. Monitor the
200-turn cap. A small speed increase with22/24 is still not owner acceptance.
