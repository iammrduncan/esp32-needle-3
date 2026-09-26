# Needle 3 mentor queue

Updated **2026-09-26 22:47 UTC**. Researcher owns implementation and measurement.
Owner accepted **5.3033 tok/s**; research pin **6.1550**, engine `d6b8014fd2fb`.
Best completed candidate **B1rope 6.1817**; nothing promoted. Device 22/24 with
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

**Current order:** B1 now composes the strongest measured P1 win onto the new
6.1817 tree; B2 tests a W3 residual emit with NO metadata allocation; B3 finishes its bounded
metadata/cache repairs and measures numeric indices with both gathers retained.
W3 emit moves above inverse-P2 because the metadata integration has consumed
several idle-lane cycles. It tests the same useful removal of an intermediate
with fewer moving parts. Inverse-P2 and wide taps remain unmeasured reserves.

| Board | Completed own base | Next independent experiment |
|---|---|---|
| B1 | B1rope DONE 6.1817 vs B1out 6.1717; ext 6.1147, prefill 6.5267, 99 tokens, heap 9335, host 23/23 RC=0; engine `d48b4be3e19d`, app `bd3fa2799107` | Preserve full engine, then compose the measured P1 gather/SiLU fusion here. This ready lane moves ahead of further B3 build-only repair cycles. |
| B2 | B2p1fix 6.1683, both host gates RC=0 | W3 completed-reduction emit directly into the existing d4/residual update; no allocation. |
| B3 | B3comp 6.1750, ext 6.1100, heap 10351, host RC=0; engine `5091d6b9d115` | Repair cache lifetime/bounds, then numeric uint16 P1/P2 metadata while keeping both original gather loops. |

All normal gates remain required. Keep workers frozen through their gates inside
`needle-board run N`. Each finished lane should turn over independently; prepare
other workers while a lane builds or measures. Do not use fixed 80-second sleeps
or the agent's recurring "last calls" narration as a reason to leave boards idle.
Small 0.0033 increments are observations, not established significance; matching
numbers on different trees are not a confirmation pair.

At 22:44 all real jobs had finished (B1 host RC=0, B3 draft build rc=0).
Mentor interrupted only the stuck agent turn and redirected to the ready B1/P1
composition, then B2's producer emit while it runs (W3 now moves ahead of inverse-P2). B3's current draft is preserved
but still incomplete; use the explicit remaining corrections below, not another
claim that compiling proves they landed.

## B1: compose the measured P1 gather/SiLU fusion (base 6.1817)

Preserve the FULL B1rope engine first. Use B2p1fix's silu_ctx/silu_rows and its
hadamard call-site change as the reference, not a whole-file transplant.
Append `const float *src, *perm` to the context; initialize src=hada_b, perm=p1,
and retain a=hada_a. Load BOTH gathered x0/x1 before stores; feed the same
`d2*sc*x+b2` expressions, sigmoidf_pair and z*sigmoid. Remove only the first
serial P1 copy. Retain scale-row construction, P2/d3 and all Kron reductions.
P1 is FP32; never cast its storage to uint16. No new allocation or assembly.
This is composition onto B1's final attention gate plus norm/RoPE fusions, not
a repeat of B2's tree. Compare against B1rope 6.1817.

22:48: B1 composition built and preflash host gate is RC=0, 23/23, fidelity
5.341e-05. Launch it and leave its tree frozen through measurement. Interpretation
note for turnover: this port added a null-perm branch plus a duplicate old SiLU
body, unlike B2p1fix. Linked silu_rows is 0x7e4 vs B2's 0x470 (884 B larger).
All actual callers supply P1. If composition loses or is neutral, removing that
unneeded duplicate path is a concrete changed-premise follow-up; do not conclude
the original fusion failed to compose from a differently shaped callback.

B1rope's proof/context: raw `(1.0f+s)` factors are retained in both half-pair
locals and odd tail, with four separate scale/inv mul.s before the original
RoPE msub/madd orientation. Mentor checked the linked body and the saved-source
diff; only the intended norm/RoPE context/emit/call changes were present.
The earlier relative-offset splice corrupted ND_NOW_US; researcher preserved
the draft and rebuilt from `/tmp/B1out_nd_model_preserved.c`. For new edits,
replace inside a bounded function block, reconstruct the file with absolute
anchors, and re-read saved source before build/host gate/flash.

## B2: W3 reduction emit directly into the residual (new; base 6.1683)

Use the preserved B2p1fix tree. Current hadamard_mlp_unscaled ends with W3
`kron_apply(...hada_a, hada_b, fp[23],fp[24]...)`; its ONLY caller then runs
`u[i] += hada_b[i]*d4[i]` for i<dm. A separate W3 `kron2_res_rows` callback can
keep the exact eight-accumulator reduction and, AFTER each sum is complete,
emit `u[q] += s*d4[q]` for q=k*nb+l < dm. For q>=dm, do not touch u or d4.
Keep both Kron dispatches and their joins, the original output-width/tail loops,
and W1/W2 unchanged. The inputs to W3 are hada_a/hada_c, distinct from u; the
conditioning work has already consumed its input before any residual writes.
Each output index has one owner, so no atomics or new synchronization are needed.

Pass u into the MLP/W3 wrapper, replace ONLY W3's second-half callback, and remove
ONLY the caller's old d4 residual loop. No new allocation, permutation table,
assembly or reduction reassociation. Preserve the original `u + s*d4` madd
operand graph, fully rounded W3 sums first. Keep the later block-difference
subtraction and ublk snapshot unchanged. Check whole u against the old W3 plus
residual pipeline, nonzero row starts, dm boundary/tails and canaries; then
linked output madd, host and device gates. Specialized kron2 copy starts from
only 0x1b8 linked bytes, so check actual IRAM/heap rather than predicting spills.

Removes W3's 1024-float store plus the 768-float read per layer: **57,344 B/token
of internal scratch traffic**, and folds a serial update into an existing split.
No external-bandwidth saving is claimed. It can still lose to code layout or
register pressure. History: #25 fused d4 with residual but left W3 materialized
in hada_b; #47 added a separate residual dispatch; #925 fused the later block
subtraction and lost. None tried this producer emit, through #934.

### Reserve: inverse-P2 emit (still worthwhile, deferred behind the simpler W3 lane)

W2 currently writes hada_b then serially gathers/scales into hada_a before W3.
A specialized W2 kron2 callback can instead write
`hada_a[invp2[q]] = s*d3[invp2[q]]` AFTER the identical completed sum. Keep both
Kron dispatches: W2's first half fully consumes hada_a into hada_c and joins,
so writing back into hada_a during its second half is safe; join again before W3.
Validate a bijection and build one optional model-owned uint16 inverse table at
END of open in PSRAM, 2048 B here, with shape/dtype/range/failure fallback and
free at close. No FP32 reinterpret casts, no D3 inside the reduction or W3 loads.
Mentor verified record 227 is a true FP32[1024] permutation; its inverse is
random (only 1/1023 adjacent destinations consecutive). Removes 65,536 B/token
of INTERNAL scratch traffic; inverse reads/random D3 accesses may erase it.
History #369's P1 gather unroll and #664's Kron-half joining are different.

The transfer from [Fusing Gathers](https://arxiv.org/html/2407.13585v1) and
[MLIR indexing maps](https://mlir.llvm.org/docs/Dialects/Linalg/) is to reason
about traversal order and ownership before fusion. These specific producer
emits are source-derived proposals, not speed claims from those sources.

## B3: numeric permutation metadata (base 6.1750)

**22:46 draft still needs work:** meta3 built and host-gated RC=0, but P2's
regex missed its actual `* d3[i]` suffix, so only P1 uses metadata. Keep that
multiply in BOTH P2 branches. Validation now casts `(int)v` BEFORE checking
range; instead require `need<=65536`, FP32 dtype/length, and short-circuit
`v>=0 && v<need` BEFORE `(uint32_t)v`/integrality comparison (range rejects
NaN/Inf). Actual cache bounds are still unchanged. Single PSRAM allocation,
one free, all-or-none publication and P1's off-loop branch are repaired.
No metadata device result exists yet. Do not call this a two-table result.

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
Next mentor: actual B1 saved-source repair and composition, B2 W3 output ownership/
rounding and whether it launched, B3 real cache guards/metadata, then strongest
P1 composition. Watch the 200-turn cap and live processes plus nonempty growing
logs; a printed PID or another ledger-only finish is not discovery.
