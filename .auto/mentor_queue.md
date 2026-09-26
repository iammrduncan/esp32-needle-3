# Needle 3 mentor queue

Updated **2026-09-26 20:23 UTC**. Researcher owns implementation/measurement.
Owner accepted **5.3033 tok/s**; research pin **6.1550**, engine d 6b 8014fd 2fb.
Nothing promoted: the frozen #647 cases heldout_interval_one and
heldout_long_tools_note_only still fail (22/24, delta 52). This pattern is
lineage evidence, NOT full correctness or permission to replace goldens.

At arrival pi was idle at its 200-turn cap; all three boards were idle. Mentor
resumed it. Current order: **B 1 norm/RoPE composition; B 2 corrected P 1 gather/SiLU fusion;
B 3 integer permutation metadata on repaired composition.** Wide taps moves down
because it remained unstarted across passes; it is unmeasured, not disproven.

**B 2 INVALID RUN:** the 20:16 draft mistakenly declared perm as uint 16_t*
and cast p 1. Despite the queued warning it flashed before consuming that message.
P 1 is FP 32; that cast COMPILES and gives wrong/out-of-range indices. Pi then
killed B 2p 1 despite the no-kill directive; its 162-byte log has no result.
Its next repair did NOT write: it replaced old strings in memory, then tested
for those now-absent strings before writing. #932's claim of corrected on-disk
source is false as of 20:22. Mentor verified all jobs/builds ended, interrupted
the stuck turn at 20:21 and directed an unconditional write and saved-file read.
Use const float *perm and pass p 1 without any cast. Integer metadata on B 3
requires actual numeric conversion at open; never reinterpret FP 32 storage.

| Board | Own completed base / current work | Next action |
|---|---|---|
| B 1 | B 1out DONE 6.1717 (neutral vs B 1gate); ext 6.1047, heap 9591 (+256), host 23/23 RC=0; app dc 13d 8bb 3cab. | Preserve it; next RoPE composition below, no outline repeat. |
| B 2 | B 2tap 6.1550 neutral; ext 6.0906, heap 8415, host RC=0; engine 2ddd 553cdd 03. B 2p 1 aborted/invalid; type repair is first priority. | Verify saved FP 32 pointer, host-gate, launch uniquely named B 2p 1fix. |
| B 3 | B 3comp DONE 6.1750 vs B 3rope 6.1717; ext 6.1100, prefill 6.52, think 4.72, heap 10351, 99 tokens, device 22/24 delta 52; host 23/23, fidelity 5.341e-05/top 1 10/10, RC=0. Engine 5091d 6b 9d 115, app 23ab 6a 0002d 7. | Preserve it; repair cache lifetime/bounds and try integer permutation metadata. |

B 1gate and B 3rope happen to read 6.1717 on different trees: not a confirmation
pair. Increments of 0.0033 are small; retain evidence without claiming proven
significance. B 3rope's old host launcher wrote literal HOSTGATE_RC=$?;
its metrics exist, but no captured RC may be invented or old log rewritten.
New script launchers capture the actual exit code.

## B 1: outlined final attention gate emit

Source-derived hypothesis: B 1gate fused the final head normalization and dynamic
sigmoid gate, removing an intermediate pass and split, but enlarged attn_heads
to 0x 1bd 7. The pair loop repeatedly materialized frame offsets 0x 488/48c/490/494
around __divsf 3. Outline ONLY that even-head loop into:
static ND_HOT __attribute__((noinline)) void attn_gate_emit(float *oh,
const float *gp, float inv, uint 32_t n).
Same sigmoidf_pair, z 0=oh[i]*inv, z 1=oh[i+1]*inv, then z 0*g 0 and z 1*g 1.
One call/head, caller computes inv once; KV reduction and odd-head fallback
unchanged; later agate dispatch remains skipped for even heads.

**Linked B 1out verified:** helper 0x 35e bytes, frame 64; caller 0x 1783. Spill loads
are now direct offsets 0/4/8/16, and four mul.s preserve the normalization-then-
gate graph. This is the intended mechanism; device speed decides call overhead.
Compare with its B 1gate 6.1717, not B 1x or another board. B 1gate model source:
 /tmp/B 1gate_nd_model_preserved.c. Keep full engine snapshots at turnover.

**Next B 1: compose the norm/RoPE emit fusion onto B 1out.** This is an unmeasured
combination with the dynamic gate/outlined emit, not another run of B 3rope.
Reference ONLY the zcrms_head_ctx/rows/heads fusion in
/tmp/B 3rope_engine_preserved/src/nd_model.c. B 1's scale slots are RAW: retain
the (1.0f+s[i]) addition in BOTH normalized half-pair locals and the odd tail;
blindly copying B 3's prescaled s[i] formula would be wrong. Keep ascending ss
and exact inv; form both normalized locals, then the original RoPE expressions.
Pass rope_cos/rope_sin through the head context and remove only corresponding
later Q/K apply_rope calls. Do not port the norm-pool representation in this
lane. Verify the same separate scale/inv multiplies and rotation contraction
orientation in the ELF, full head vectors, and normal gates. Own base 6.1717.
Do not reopen #615's sequential tap/norm/RoPE callback: that was already measured;
this composition specifically removes the normalized intermediate emit.

## B 2: P 1 gather inside the existing SiLU callback

Preserve B 2tap first (model copy /tmp/B 2tap_nd_model_preserved.c).
hadamard_mlp_unscaled writes kron_apply's output into hada_b, serially copies
hada_a[i]=hada_b[(uint 32_t)p 1[i]], then silu_rows reads/overwrites hada_a.

Append **const float *src, *perm** to silu_ctx; initialize src=m->hada_b,
perm=p 1, retaining a=m->hada_a. In the existing pair loop load BOTH
x 0=src[(uint 32_t)perm[i]], x 1=src[(uint 32_t)perm[i+1]] before output writes.
Use those locals in the SAME d 2*sc*x+b 2 expressions, sigmoidf_pair and z*sigmoid.
Remove ONLY the first serial P 1 copy. Keep scale-row construction, P 2/d 3 pass
and all Kron reductions. The two arrays stay distinct until the callback joins.

Saves one intermediate write/read (65, 536 B/token for 1024*8), while putting the
gather in an existing two-core split. Extra live pointers/gathers may lose.
Check full emitted vectors against saved producer+consumer and linked FMA
operand order, then normal gates. No new allocation/assembly. #369's four-way
gather unroll lost 0.133%; it did not fuse the consumer or remove this temporary.
No prior measured P 1-consumer fusion found through #931.

## B 3: cache repairs, then immutable integer permutation metadata

B 3comp added B 1x's mHC constant and per-layer attn_gate hoists to B 3rope.
The graft omitted BOTH invalidation calls. Mentor warning was queued before
launch but consumed after flash; its run was left untouched and is now done.
Preserve this measured tree before repairs. Add mhc_cache_invalidate() and
attn_gate_cache_invalidate() after nd_model_open's memset.

Pi's assertion that fixed arrays provide bounds fallback is false. Current
mhc_const_fill copies fixed 32/128/8 counts without shape gating; attn_gate_of
caps filling at 64 but returns s_agate[li] unguarded. The 8-layer/4-lane blob fits.
Require actual layer/lane/tensor consumer bounds and original fp 16_get fallback
outside them; copy only valid elements. Host goldens did not prove reopen or
general-shape correctness. Neither this defect nor the frozen pair is waived.

**Distinct next candidate:** decode P 1/P 2 indices numerically ONCE at open.
Mentor read archive records 226/227: each FP 32[1024], all integral unique 0..1023.
The linked serial loops load FP 32 then utrunc.s for every index, every layer.
Use ONE zeroed model-owned uint 16_t *perm 16 for two adjacent tables (4096 B on
this blob). Guard hada_n<=65536 and both tables' finite/integral/in-range values.
Allocate optional PSRAM metadata at the END of nd_model_open, after tensor
binding and hot scratch/selftests, before open's own final return. Allocation
failure or unsupported input leaves the original FP 32 paths. Free at close.
Do not allocate at initial memset, before tensor pointers exist.

Branch outside the two gather loops. Keep BOTH original gather loops on B 3;
do not copy B 2's fusion. This removes 16, 384 float-to-index conversions/token,
not any floating arithmetic. Compare against B 3comp 6.1750 and disclose the
accompanying cache repairs. Do not promise a large gain from instruction count.
No prior metadata-cache measurement found. CQ 2 expanded-offset streams are a
different representation with a different bandwidth cost.

[T-MAC](https://arxiv.org/html/2407.00088v 1) motivates immutable index-layout
preprocessing, but its SIMD byte tables/quantization are not an exact transfer.
This blob's CQ 2 centroids (-.13317214,-.03990209,.04003528,.13346045) are NOT
sign mirrors; mirror-table compression therefore lacks an exact premise.

## Reserve: wide three-tap delivery (still unmeasured)

On preserved B 2tap, specialize nt==3 with four independent +0 accumulators,
each keeping j=0/1/2 madd order. Six 128-bit loads and wide output stores replace
scalar delivery; retain B 2tap's raw projection snapshot before overwriting it.
Guard all pointers, row strides, tile count and alignment; startup/tails retain C.

Append one function to existing gemv 4_tie 728.S; no new build plumbing.
One context pointer,32-bit fields: proj,hcur,w 0, w 1, w 2, h 1, h 2, n 4 at offsets
0/4/8/12/16/20/24/28. Candidate map: a 3 raw source, a 4 output (same initial
projection), a 5 hcur, a 6/a 7/a 8 weights, a 9/a 10 histories, a 11 tiles, a 12 zero;
f 0..f 3 sums, f 4..f 7 values, f 8..f 11 weights. Per tile load/store raw to hcur,
load w 0/madd, h 1+w 1/madd, h 2+w 2/madd, store output; pointers advance 16.
Remember high-to-low TIE register lists. Compare complete projection/history,
multi-tile canaries, startup/wrap/odd sizes/nonzero split starts; current slot
must start stale in the oracle (old exp 57 pre-copied it and masked this class).

Transfer [Espressif's float FIR delivery](https://github.com/espressif/esp-dsp/blob/master/modules/fir/float/dsps_fir_f 32_aes 3.S),
not its horizontal reduction, which changes our rounding. Do not let this
integration monopolize a board if the ready C candidates remain unmeasured.

## Retained negative evidence and parked work

- Residual-difference fusion lost 6.1683->6.1650. Raw Q/K norm residency and raw
  tap-history snapshot were neutral 6.1550. The snapshot saves source reads,
  not history stores or a PSRAM write sweep.
- Correct phi staging #911 lost 0.378%; missing-prepare staging readings were
  invalid and withdrawn. #915 dispatch/RMS pairing was neutral.
- Keep kron 2 renames, four-pass Sinkhorn exit, four-output wide QK,36-byte
  records, LUT de-split, private codebook, expanded CQ 2 uint 16 offsets and
  row-owned Kron fusion closed unless the premise changes.
- Dual-store remains unmeasured/parked after ABI/selftest integration failures.
  Later recipe: scope nd_lanepre 4w through its own .size; sixth destination a 7,
  move tile-zero to a 12, second wide store; update context, initializer, BOTH
  callers, C prototype, scalar path and both-output alignment. Independent
  aligned selftest buffer/canaries, never a model pointer in selftest. Remove
  only snapshot memcpy after both paths work. Saved B 1x:
  /tmp/B 1x_engine_snapshot_1512. Keep later block-difference subtraction.
- CQ 2 PMU stays below runnable work. Two-wide issue-floor claims mixed two-core
  work with wall time; #745 measured 34 cycles/word. Installed core-isa.h has
  FLIX 3=0, one load/store unit,4-byte fetch; do not infer per-core IPC from
  phase totals. API is real/per-core ERI, two counters/core:
  [perfmon](https://docs.espressif.com/projects/esp-idf/en/v 4.4.2/esp 32s 3/api-reference/system/perfmon.html).
  Future bounded screen: caller-select one 576-row Q pointer, per-core cycles+
  one event around real row callbacks, row/byte/init/overflow fields, print
  after join. Draft /tmp/nd_quant.c.pmu-draft-1525 is not evidence.

## Execution and next mentor check

Preserve every dirty artifact; main has an older lineage, never reset workers
from main. Keep each worker frozen through its host gate INSIDE needle-board
run N. Preserve goldens, anti-repeat history, locks, assertions and 240/80MHz.
Inspect live child processes AND growing non-empty logs. Never interrupt real
build/flash/bench work. A model's repeated “1-2 calls left” narration is not a
reason to guess types, leave ready boards idle, or log another finish.

Next mentor: B 1out's own 6.1717 comparison; B 2's FP 32 pointer correction and
real P 1 measurement; B 3's actual cache repairs and metadata lane. Monitor the
200-turn cap. A small speed increase with 22/24 is still not owner acceptance.
