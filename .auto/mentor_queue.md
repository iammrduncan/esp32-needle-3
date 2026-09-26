# Needle 3 mentor queue

Updated **2026-09-26 22:39 UTC**. Researcher owns implementation and measurement.
Owner accepted **5.3033 tok/s**; research pin **6.1550**, engine `d6b8014fd2fb`.
Best completed candidate **B3comp 6.1750**; nothing promoted. Device 22/24 with
delta 52 still fails the frozen #647 pair (`heldout_interval_one`,
`heldout_long_tools_note_only`). Same failure pattern is lineage evidence,
not full correctness and not permission to replace goldens.

At arrival all boards were idle and pi was at the 200-turn auto-resume cap.
Mentor resumed it. **The newly harvested P1 fusion is the useful new evidence:**
B2p1fix finished at 20:29 UTC, decode **6.1683 vs its own B2tap 6.1550** (+0.216%),
ext 6.1018, prefill 6.5117, 99 tokens, heap 8415, device 22/24 delta 52.
Both preflash and post-device host logs have RC=0, 23/23, fidelity 5.341e-05,
top1 10/10. Engine `fedbef245643`, app `5bf7fcd2c424`. Preserve this tree.
The old B2p1 run was aborted/invalid; P1 is FP32, and its erroneous uint16 cast
is now corrected on disk. No rerun is needed to harvest the finished result.

**Priority change:** keep B1's norm/RoPE composition and B3's independent numeric
metadata lane. Move **P2 producer-emit fusion** ahead of wide taps on B2 if taps
has not started building. P1's positive reading motivates removing another
materialized permutation pass; this is a new mechanism, not a gather-unroll
repeat. Wide taps remains the ready reserve, unmeasured rather than disproven.

| Board | Completed own base | Next independent experiment |
|---|---|---|
| B1 | B1out 6.1717, ext 6.1047, heap 9591, host RC=0; neutral vs B1gate | B1rope LIVE after repaired splice; engine `d48b4be3e19d`, app `bd3fa2799107`, flash completed 22:38. Do not edit or interrupt. |
| B2 | B2p1fix 6.1683, both host gates RC=0 | P2 inverse-permutation/scaled emit on this preserved P1 tree. |
| B3 | B3comp 6.1750, ext 6.1100, heap 10351, host RC=0; engine `5091d6b9d115` | Repair cache lifetime/bounds, then numeric uint16 P1/P2 metadata while keeping both original gather loops. |

All normal gates remain required. Keep workers frozen through their gates inside
`needle-board run N`. Each finished lane should turn over independently; prepare
other workers while a lane builds or measures. Do not use fixed 80-second sleeps
or the agent's recurring "last calls" narration as a reason to leave boards idle.
Small 0.0033 increments are observations, not established significance; matching
numbers on different trees are not a confirmation pair.

## B1: norm/RoPE emit composition (base 6.1717)

B1out outlined the even-head final attention sigmoid/normalization emit; it was
neutral versus B1gate and recovered 256 B heap. Preserve it and add ONLY the
norm/RoPE emit fusion from `/tmp/B3rope_engine_preserved/src/nd_model.c`.
B1's norm slots are RAW: form `(1.0f+s[i])*v[i]*inv` and the corresponding second
half local, then the original two RoPE expressions. Retain the raw scale add in
the odd tail too. Keep the ascending ss reduction and exact inv. Thread
rope_cos/rope_sin through zcrms_head_ctx and remove only the matching later Q/K
apply_rope calls. Do not transplant B3's prescaled pool representation.

**Resolved integration blocker, caught by source review:** the first patch
asserted a prescaled emit that B1 does not have. The second searched `s[i:j]`
then applied relative `m.start()/end()` to the WHOLE file, inserting the new loop
into ND_NOW_US at line 23 and leaving the old emit. Build `/tmp/b1_r2_build.log`
is rc=2. The researcher preserved the draft and reconstructed the candidate from
`/tmp/B1out_nd_model_preserved.c` using `s[:i] + block + s[j:]`; r3 built rc=0.
Saved context, initializer, signature, callers and removal now agree. Mentor's
linked-image read confirms both raw-scale adds, four separate scale/inv mul.s,
then the original rotation msub/madd orientation. B1rope is live with chained
host gate; do not infer that gate's result before its actual log appears.

Check linked separate scale/inv multiplies and original rotation contraction
orientation, full head vectors, then normal gates. #615's sequential tap/norm/
RoPE callback is closed; this removes the intermediate normalized emit instead.

## B2: P2 inverse-permutation emit after W2 (new; base 6.1683)

Current source does W2 `kron_apply(...hada_a, hada_b...)`, then serially writes
`hada_a[i] = hada_b[(uint32_t)p2[i]] * d3[i]`, then W3 consumes hada_a.
The second half of W2 already has each completed FP32 s0..s7 in registers.
Keep its EXACT eight-accumulator reduction and both existing Kron dispatches.
For source index q, compute `i = invp2[q]`, then emit `hada_a[i] = s * d3[i]`.
This eliminates W2's hada_b materialization and the later whole P2/d3 sweep.

Use a separate specialized `kron2_p2_rows`/W2 wrapper so W1/W3 keep their current
callback and inner loop. Clone the current body faithfully and change only its
final stores, including the scalar output tail. `kron2_rows` is only 0x1b8 bytes
in B2's linked image, so a specialized copy is a reasonable first shape; check
actual IRAM/heap and spills rather than assuming duplication is free. Do NOT
pull D3 into the reduction or the W3 input loop (that would change rounding or
repeat the multiply). Finish the same sum first, then one separate mul.s.

Metadata: one optional model-owned uint16 inverse table, **2048 B** on this blob,
allocated with ND_ALLOC at END of open after bindings/scratch/selftests, freed
at close. Validate FP32 dtype, length, finite integral in-range values, and
**uniqueness** before enabling. A narrow `hada_n==1024`/W2 32x32 specialization
with the untouched generic fallback is enough for this first experiment. Build
`invp2[(uint32_t)p2[i]] = i` by numeric conversion, never reinterpret the FP32
archive. Reject duplicates/unsupported shape/allocation failure to old path.

Aliasing proof: W2 first half reads all of hada_a into hada_c and JOINS before
its second half starts; that half only reads hada_c and factors, so it may safely
write back into hada_a. Inverse bijection gives disjoint output elements across
cores. Keep the join before W3. Do not attempt the gather before W2's join.

Mentor re-read archive record 227: FP32[1024], unique integers 0..1023; inverse
order is effectively random (only 1 of 1023 adjacent destinations is consecutive).
It removes **65,536 B/token of internal scratch traffic**, not external bus
traffic: hada_a/b are ND_ALLOC_FAST. Random D3 accesses, inverse-table reads and
extra live state may outweigh it. Compare full vectors on real/random inputs,
nonzero row starts and canaries; verify separate final multiply and then gates.
No P2 producer scatter found in history through #933; #369 was P1 gather unroll
(-0.133%), #664 joined Kron halves (neutral), neither removed this intermediate.

The transfer from [Fusing Gathers](https://arxiv.org/html/2407.13585v1) and
[MLIR indexing maps](https://mlir.llvm.org/docs/Dialects/Linalg/) is to reason
about traversal order and ownership before fusion. This specific inverse emit
is a source-derived proposal, not a performance claim from those sources.

## B3: numeric permutation metadata (base 6.1750)

Preserve B3comp first. Its graft omitted both mhc_cache_invalidate() and
attn_gate_cache_invalidate() after open's memset. Add them. Fixed arrays are NOT
bounds checks: mhc_const_fill currently copies 32/128/8 unconditionally, and
attn_gate_of caps filling at 64 but reads s_agate[li] unguarded. Fill valid counts
only when actual layer/lane/tensor bounds fit; consumers must use original
FP16 conversions outside those bounds. The 8-layer/4-lane blob fits, but reopen
and other shapes must not reuse stale or out-of-range values. Avoid a forward
call to fp16_get before its declaration; use a declaration or the equivalent
original conversion. Disclose these accompanying repairs with the candidate.

One zeroed model-owned uint16 buffer for adjacent P1/P2 tables: **4096 B** here.
Guard hada_n<=65536, FP32 dtype/length, finite integral in-range indices. Allocate
optional PSRAM storage at END of nd_model_open after binding and hot scratch/
selftests; free at close; null/unsupported/allocation failure uses FP32 loops.
Mentor verified records 226/227 are each true FP32[1024] permutations.

Branch outside the loops. KEEP BOTH original gather loops in this lane; do not
copy B2's P1 fusion or inverse P2 emit. It isolates removal of 16,384 float-to-
index conversions/token and halves permutation metadata bytes. No floating
arithmetic changes. Compare with B3comp's 6.1750, with repairs disclosed.

## Next turnover and ready reserve

- After B1 or B3 has a positive completed result, compose **the measured P1
  gather/SiLU fusion** onto that same board's preserved winning tree. P1's
  +0.216% is currently stronger evidence than another 0.0033-tick microchange.
  Keep `const float *src,*perm` for FP32; if numeric metadata won, explicitly
  select its uint16 table. Both pair inputs load before writes to separate
  hada_a; same d2*sc*x+b2 order and sigmoidf_pair. No reinterpret casts.
- **Wide three-tap delivery** remains distinct and unmeasured. On B2p1fix (or
  separately preserved B2tap at 6.1550), specialize nt==3 with four independent
  +0 accumulators, each keeping j=0/1/2 madd order. Raw projection is snapshotted
  before overwrite. Guard alignment/strides/count; startup/tails retain C.
  One appended assembly function in gemv4_tie728.S; context fields
  proj,hcur,w0,w1,w2,h1,h2,n4 at byte offsets 0/4/8/12/16/20/24/28.
  Candidate map a3 source,a4 output,a5 hcur,a6..a8 weights,a9/a10 histories,
  a11 tiles,a12 zero; f0..f3 sums,f4..f7 values,f8..f11 weights. Load/store raw
  history, then three ordered taps, then output; pointers advance 16. Remember
  high-to-low TIE register lists. Check multi-tile/full-history canaries,
  startup/wrap/nonzero splits with stale current history in the oracle.
  [Espressif FIR](https://github.com/espressif/esp-dsp/blob/master/modules/fir/float/dsps_fir_f32_aes3.S)
  supports the wide delivery pattern; its horizontal reduction is not ours.

## Retained negatives and deferred work

Residual-difference fusion lost 6.1683->6.1650. Raw Q/K norm residency and raw
tap-history snapshot were neutral. Correct phi staging #911 lost 0.378%; earlier
missing-prepare results were invalid. #915 phi dispatch and RMS pairing were
neutral. Keep scale-fold #744, Kron width/renames, four-pass Sinkhorn exit,
four-output wide QK,36-byte records, LUT de-split, private codebook, expanded CQ2
uint16 offsets and row-owned Kron-half fusion closed unless the premise changes.

Dual-store remains unmeasured after ABI/selftest integration failures. Preserve
`/tmp/B1x_engine_snapshot_1512`. Later recipe: bound edits by nd_lanepre4w .size;
sixth destination a7, move tile-zero to a12, second wide store; update prototype,
context, both callers, scalar path and alignment; independent aligned selftest
buffer, no model pointer in selftest. Remove only snapshot memcpy once proven.

CQ2 PMU stays below runnable work. API exists in installed IDF/per-core ERI,
two counters/core; old absence/shared-debug claims were wrong. Future bounded
screen: select one 576-row Q call, count cycles+one event on EACH real worker,
include rows/bytes/init/overflow, print after join. `/tmp/nd_quant.c.pmu-draft-1525`
is not evidence. Do not infer per-core IPC from two-core phase wall time.

Preserve every dirty artifact and all goldens/anti-repeat history/locks/240/80MHz.
Main has older implementation lineage; never reset workers from main. B3rope's
old literal `HOSTGATE_RC=$?` was not an exit code; never rewrite its log.
Next mentor: actual B1 saved-source repair and composition, B2 P2 ownership/
rounding and whether it launched, B3 real cache guards/metadata, then strongest
P1 composition. Watch the 200-turn cap and live processes plus nonempty growing
logs; a printed PID or another ledger-only finish is not discovery.
