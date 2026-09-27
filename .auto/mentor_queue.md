# Needle 3 mentor queue

Updated **2026-09-27 01:20 UTC**. Researcher owns implementation/measurement.
Owner accepted **5.3033 tok/s**; research pin **6.1550**, engine `d6b8014fd2fb`.
Best completed candidate **B1p1 6.1933**, versus its B1rope **6.1817** (+0.188%).
Own log: ext **6.1276**, prefill **6.54**, think **4.73**, gen **99**, heap **8303**;
preflash/chained host **23/23, RC=0**, fidelity **5.341e-05**, top1 **10/10**.
Nothing promoted. The frozen #647 pair (`heldout_interval_one`,
`heldout_long_tools_note_only`) still fails: **22/24, delta 52**. Same failures
are lineage evidence, NOT full correctness or permission to change goldens.

At arrival all boards were idle and pi was at its 200-turn cap. Mentor resumed
it. Failed patch scripts and fixed 80/95-second sleeps then delayed discovery;
all three reached independent device work. B1 now finished; B2/B3 remain live.

| Board | Own baseline / actual state | Immediate action |
|---|---|---|
| B1 | `B1nofb2` DONE **6.1933**, neutral vs B1p1; ext **6.1265**, gen 99, device 22/24 delta52, chained host **RC=0**, heap **9335** (+1032 B). Linked silu_rows **0x46c vs 0x7e4**, 888 B smaller. | Preserve this neutral-speed/smaller-code tree; next composition below once B2 gates finish. |
| B2 | `B2pre2` LIVE **6.1933 vs 6.1683 (+0.405%)**, ext **6.1253**, prefill 6.54, gen99. Prefix source readback is correct. | Freeze through remaining device/host gates; strongest new mechanism this pass. |
| B3 | `B3meta2` LIVE **6.1917 vs 6.1750 (+0.270%)**, gen99. Boot **PERM need=1024 ok=1** proves metadata active. Engine `a5aaada00f3b`, app `83a6023e7c4d`; preflash host 23/23, fidelity 5.341e-05. | Freeze through remaining gates; generic guard debt below is for AFTER this run. |

**Why the order changed:** P1 fusion is positive on two lineages, but a larger
untried opportunity is W3's unused output suffix. Removing that work is simpler
than residual fusion and keeps the existing kernels. Instruction-floor claims
(#851/#891) do not close unused-output elimination. B1 measures code size;
B2 measures avoided arithmetic; B3 measures index representation.

## B2: W3 output-prefix scheduling (current experiment)

Mentor parsed the frozen archive: all eight W3 A/B pairs are **32x32**, dm=768,
hada_n=1024. The residual reads only q<768, hence k<24. kron2_rows(k) reads only
hada_c[k,*]; kron1_blocks produces four independent k rows. Neither half needs
k=24..31. Separate wrapper uses UNCHANGED callbacks: **6 first-half blocks**
(3/core), then **24 second-half rows** (12/core). BOTH contexts retain
**na=nb=32**, which control reduction lengths and factor strides. Changing na
to 24 is WRONG. Keep both joins, materialize hada_b[0:768], keep old d4 loop.
Only the call with fp[23]/[24] and L->w3a/w3b changes; W1/W2 stay ordinary.
Guard na=32, nb=32, dm=768; otherwise `kron_apply(m,src,dst,a,b,na,nb)`.
Six blocks stay parallel under the existing half<2 serial fallback.

Retained outputs have identical FULL i/j sums and order. Subsequent W1 fully
rewrites scratch before gathers. Differential first 768 outputs/resulting u,
poison omitted rows, nonzero split starts, then existing host/device gates.
No zero-product skipping, reassociation, allocation or assembly. Eliminates
**131,072 FMAs/token**, 25% of W3 or 8.33% of all three Kron calls at equal
geometry. Old R-prof-b3 had 9.8 ms Kron/token: roughly 0.8 ms / 0.5% total is
an opportunity estimate, NOT a measured result. No equivalent trial through
#936; #664 removed a join, #435 changed layout, widths changed inner kernels.
[MLIR's slice-driven producer fusion](https://mlir.llvm.org/docs/Tutorials/transform/Ch0/)
suggests tracing the consumed subset backward while preserving reductions;
this specific experiment comes from local source and geometry.

Mentor's 01:13 full-file diff confirms only the wrapper and W3 call changed.
Earlier `B2pre.log` is BUILD_FAILED; `HOSTGATE-B2pre.log` belongs to an unchanged
base, not the new candidate. Keep those records; use **B2pre2** evidence.
Broken edit preserved by researcher; `/tmp/B2p1fix_nd_model_preserved.c` is base.

## B1: remove unused SiLU duplicate (current experiment)

Every initializer supplies FP32 P1, but B1p1 carried an unused null-perm fallback
and a duplicated sigmoid body. Current edit copies ONLY the existing B2
single-path silu_rows. Mentor full-file diff versus
`/tmp/B1p1_nd_model_preserved.c` confirms identical gathered pair arithmetic
and scale-row prologue; only the duplicate branch/comments/whitespace changed.
B1's printed comparison false was a checker-boundary mismatch, NOT bad source.
Build rc=0, linked callback now 888 B smaller. **Do not re-edit it.**
B1 norm factors are raw `(1+s)`; B3's are prescaled: never transplant full files.

## B3: numeric P1/P2 metadata (base 6.1750)

Preserve B3comp (`/tmp/B3comp_nd_model_preserved.c` plus saved header) and drafts.
The intended experiment keeps BOTH gather loops and removes 16,384 FP32-to-index
conversions/token. Records 226/227 are genuine FP32[1024] bijections, 0..1023.
A P1-only draft is not a two-table result. Specification and current readback:

01:17 B3meta2 compiled: prototype, both P2 branches, need-sized allocation,
short-circuit range-before-cast, nonnull metadata data pointers, mHC readiness
clear and exact 8-layer/4-lane cache guard NOW landed. Keep the run frozen.
Still owed AFTER measurement: actual dtype guards (a comment saying dtype is
not a condition), need<=65536 BEFORE allocation, mHC raw-pointer/FP16 checks.
The frozen archive's dtypes/geometry are already verified, so these omissions
do not invalidate this concrete metadata mechanism test; do not claim generic
validation complete. No edit or restart of the live worker to finish them.

- P2's actual loop is `hada_a[i] = hada_b[(uint32_t)p2[i]] * d3[i]`.
  Add metadata/fallback branch outside the loop, retaining *d3[i] in BOTH.
- Require 0<need=hada_n<=65536, FP32 dtype, nbytes>=4*need, valid data pointers.
  Check `v>=0 && v<need` BEFORE integer cast/integrality check; this rejects
  NaN/Inf too. One optional PSRAM block of exactly 2*need uint16 entries;
  fill need/table, p2=pool+need, publish both only on success, free only base.
  Allocate at END of open after bindings/scratch/selftests; fallback unchanged.
  No uniqueness check needed until building an inverse. No FP32 reinterpretation.
- Open-time mHC/attn cache invalidations already landed. Bounds still need actual
  consumers: nl*lanes pre/post, nl*lanes*lanes res, nl for a_*, wide products.
  Clear readiness before fill, require counts within 32/128/8 and FP16 tensors
  covering them; otherwise original conversion fallback. Cache attention gates
  only for nl<=64, otherwise original per-layer sigmoid/fp16 path. Declare
  fp16_get before an earlier use. Fixed array capacities are NOT tensor bounds.
  SMALL valid alternative for this first experiment: clear ready first, cache
  only nl==8 && lanes==4, plus valid FP16 pointers/lengths. Then 32/128/8 ARE
  the actual consumer counts. Other geometries keep the original fallback.

Host-gate finished source BEFORE flash, disclose cache repairs with result.
Use a unique lane/log; don't overwrite the old meta3 host-only evidence.

## Next turnover: two strong follow-ups, one substitute

If B2's +0.405% survives its gates, **B1 next takes a single W3-prefix composition
onto its faster lineage**, preserving its own just-measured source first. Copy
ONLY the bounded prefix wrapper and switch ONLY fp[23]/[24] call; preserve
B1's raw norm/RoPE factors. This answers whether the structural gain survives
composition, rather than repeating an identical image. B2 independently takes
residual emit below; B3 finishes metadata interpretation before inverse-P2.

**W3 residual producer emit** after pricing prefix scheduling. On winning prefix
base, a SEPARATE kron2 callback keeps all completed sums then emits
`u[q] += s*d4[q]`, q<dm. Pass real block u and d4=fp[18] through its own context;
ordinary kron2_ctx is `{m,dst,b,na,nb}` (there is no m->hada_d4). Remove ONLY
W3 materialization and caller d4 loop; keep both joins, ublk snapshot and later
subtraction. Guard all eight stores/tail before u/d4 access. Saves **49,152 B/token
INTERNAL** on prefix base (57,344 on old full width). Full-u differential,
nonzero splits/canaries, linked residual madd operand graph and ordinary gates.
#25 kept materialization; #47 added a dispatch; #925 fused subtraction and lost.

**Inverse-P2 producer emit** after metadata works. W2 second half emits
`hada_a[invp2[q]] = s*d3[invp2[q]]` after the identical completed sum. First half
fully reads hada_a into hada_c and joins; each inverse destination has one owner;
join before W3. Optional model-owned uint16 inverse, validated bijection, END-of-
open fill, fallback/free (2048 B here). Saves **65,536 B/token INTERNAL**, possibly
offset by scattered D3 loads (inverse has only 1/1023 consecutive destinations).
#369 gather unroll/#664 join removal differ. [Fusing Gathers](https://arxiv.org/html/2407.13585v1)
informs traversal/ownership reasoning, not a claimed speedup for this machine.

**Substitute:** wide three-tap delivery remains unmeasured. nt==3, four independent
+0 accumulators, same j=0/1/2 madd order; save raw history BEFORE overwrite.
Keep C for startup/tails/unsupported alignment. gemv4_tie728.S context recipe:
proj,hcur,w0,w1,w2,h1,h2,n4 at offsets 0/4/8/12/16/20/24/28; a3 source,a4 output,
a5 hcur,a6..a8 weights,a9/a10 histories,a11 tiles,a12 zero; f0..3 sums,f4..7 values,
f8..11 weights, high-to-low TIE lists. Check multi-tile/history canaries and wrap.
[Espressif FIR](https://github.com/espressif/esp-dsp/blob/master/modules/fir/float/dsps_fir_f32_aes3.S)
is delivery precedent; do not copy its reassociated horizontal reduction.

## Preserve learned limits; keep discovery moving

Residual-difference fusion lost 6.1683->6.1650; norm residency/tap snapshot neutral.
Correct phi staging #911 lost 0.378%; earlier broken staging results invalid.
Phi dispatch/RMS pairing, scale-fold #744, Kron widths/renames, four-pass Sinkhorn
exit, four-output QK, 36-byte records, LUT de-split, private codebook and expanded
CQ2 offsets stay closed absent changed premise. Dual-store/PMU remain parked
(`/tmp/B1x_engine_snapshot_1512`, `/tmp/nd_quant.c.pmu-draft-1525`). PMU API is real
and per-core; that failed draft is not evidence or a reason to expand the harness.

Preserve dirty workers; main is older, never reset from it. Keep goldens,
anti-repeat history, locks and 240/80 MHz; freeze each worker through host gate
INSIDE `needle-board run N`. Build only after a successful saved-source assertion;
never reuse splice offsets after insertion or treat a printed false as passing.
Old B3rope `HOSTGATE_RC=$?` is literal, old B2p1 casted log invalid. No promotion.

Next mentor: B2pre2 actual result/gates first; B1nofb2 timing and heap next; verify
B3's P2/range/cache changes on disk and live job, not narration. If W3 wins, a
single composition onto the fastest B1 lineage has a real transfer question;
keep other lanes on distinct discovery. Never interrupt real builds/flashes/
benchmarks. All three boards doing distinct work is the default, not two idle
boards while one runs; a printed PID or a ledger-only turn is not utilization.
