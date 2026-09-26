# Needle 3 mentor queue

Updated **2026-09-26 22:52 UTC**. Researcher owns implementation/measurement.
Owner accepted **5.3033 tok/s**; research pin **6.1550**, engine `d6b8014fd2fb`.
Best completed candidate **B1rope 6.1817**. B1p1 currently reads **6.1933**,
but its remaining device/host gates are live. Nothing promoted. The frozen
#647 pair (`heldout_interval_one`, `heldout_long_tools_note_only`) still fails:
22/24, delta 52. That pattern is lineage evidence, NOT full correctness or
permission to change goldens.

At arrival all three boards were idle and pi had reached its 200-turn cap.
Mentor resumed it and later redirected another idle agent turn after checking
all real jobs had ended. Fixed 80/95-second sleeps and recurring "last call"
narration are still serializing discovery. Prepare other workers during builds
and gates; never interrupt a real build, flash or benchmark.

| Board | Own evidence / actual state | Next action |
|---|---|---|
| B1 | B1rope DONE 6.1817 vs B1out 6.1717; ext 6.1147, prefill 6.5267, gen 99, heap 9335; host 23/23 RC=0, fidelity 5.341e-05. B1p1 LIVE: primary 6.1933, prefill 6.54, gen 99; engine `523d8fdb31d0`, app `ffa074e020ee`; preflash host RC=0. | Freeze through its gates. Preserve winner; see callback-size follow-up below. |
| B2 | B2p1fix DONE 6.1683 vs B2tap 6.1550 (+0.216%); ext 6.1018, prefill 6.5117, gen 99, heap 8415; both host gates RC=0. Engine `fedbef245643`, app `5bf7fcd2c424`. W3 edit's first assertion failed BEFORE any write. | Implement W3 residual producer emit below; no allocation or assembly. Do not measure an unchanged successful build after a failed patch. |
| B3 | B3comp DONE 6.1750, ext 6.1100, heap 10351, host RC=0; engine `5091d6b9d115`. Metadata draft meta3 built/host-gated RC=0 but is incomplete and unflashed. | Finish the exact metadata/cache corrections below, then measure against 6.1750. |

**Why this order:** P1 gather/SiLU fusion is now positive on its original tree
and promising in composition. Target more completed producer outputs rather
than only dispatch savings. W3 emit moves ahead of inverse-P2 because it removes
an intermediate without new metadata; the metadata lane has already consumed
several idle cycles. Inverse-P2 and wide taps remain unmeasured reserves.

## B1: preserve/finish P1 composition; price its unnecessary duplicate path

B1p1 appends `const float *src,*perm` to silu_ctx, loads both gathered inputs
from hada_b through FP32 P1 before stores to hada_a, and removes the serial P1
copy. Same d2*sc*x+b2, sigmoidf_pair and z*sigmoid; scale row/P2/Kron unchanged.
Compare with **its own B1rope 6.1817**, not B2's tree or B3comp.

This port added a null-perm branch and duplicate old SiLU body absent from
B2p1fix: linked silu_rows is **0x7e4 vs 0x470**, 884 B larger. Every current
initializer supplies P1. After the live run and preservation, a small separate
candidate may remove that unneeded duplicate branch/body, retaining the exact
fused pair loop. This tests code size/register allocation on the new composed
base; it is not an unchanged repeat. Do not edit the running tree.

B1rope's raw `(1.0f+s)` factors and four separate scale/inv multiplies before
RoPE msub/madd were checked in the linked image. Preserve that representation;
B3's norm slots are prescaled. The earlier splice searched a substring then used
relative offsets on the whole file and corrupted ND_NOW_US. Repair succeeded
with bounded-function replacement and absolute reconstruction; re-read saved
source before trusting a patch. `/tmp/B1out_nd_model_preserved.c` is the old base.

## B2: W3 reduction emit directly into residual (new; base 6.1683)

Current hadamard_mlp_unscaled ends with W3
`kron_apply(...hada_a, hada_b, fp[23],fp[24]...)`; its ONLY caller then runs
`u[i] += hada_b[i]*d4[i]` for i<dm. A separate W3 `kron2_res_rows` can keep the
EXACT eight-accumulator reduction and, AFTER each sum is complete, emit
`u[q] += s*d4[q]` for q=k*nb+l+offset < dm. Guard BEFORE either u[q] or d4[q]
access, in ALL eight stores and the scalar tail (dm=768, padded outputs=1024).
Keep both Kron dispatches/joins and W1/W2 unchanged. W3 reads hada_a/hada_c,
not u; conditioning is finished before residual writes. Each q has one owner.

Pass the real block u into the MLP/W3 wrapper and d4=fp[18] into a separate
residual context. Replace ONLY W3's second-half callback and remove ONLY the
caller's old d4 loop. No allocation, assembly, reassociation or new barrier.
Keep the later ublk snapshot/block-difference subtraction. Full-u differential
against old W3+residual, nonzero row starts, dm boundary/tails and canaries;
then linked original residual madd operand graph, host and device gates.

**Actual source, not guessed anchors:** kron2_ctx is
`{nd_model *m; float *dst; const float *b; uint32_t na,nb;}`.
The first script mistakenly matched kron1's src/a layout and asserted before
writing. There is NO m->hada_d4. Ordinary kron2 contexts are manually initialized;
if using a shared callback despite the simpler separate copy, ALL ordinary
initializers must explicitly zero u/d4/dm or W1/W2 may follow garbage pointers.
The separate copy avoids changing their scheduling. Current kron2_rows is only
0x1b8 linked bytes; check resulting IRAM/heap/spills rather than assuming free.

Removes 1024-float W3 stores plus 768-float rereads per layer = **57,344 B/token
of INTERNAL scratch traffic**, and moves a serial update into an existing split.
No external-bus saving claimed. Can lose to code layout/register pressure.
History: #25 fused d4 with residual but LEFT hada_b materialization; #47 added a
separate residual dispatch; #925 fused the later subtraction and lost. None
tried this producer emit through #934.

## B3: finish numeric permutation metadata (base 6.1750)

Preserve B3comp (`/tmp/B3comp_nd_model_preserved.c` and header) and dirty drafts.
Meta3's host RC=0 does not prove the intended changes landed. Current state:
- Single PSRAM ND_ALLOC block, one free, all-or-none pointer publication, both
  open-time cache invalidations and P1's outside-loop branch are repaired.
- **P2 still uses FP32.** Its actual line ends `* d3[i]`; the regex omitted that
  suffix. Add the metadata/fallback branch outside the loop and retain the
  SAME d3 multiply in BOTH branches.
- Validation casts `(int)v` BEFORE checking range. Require need=hada_n<=65536,
  FP32 dtype/length and valid data pointers; short-circuit `v>=0 && v<need`
  BEFORE a uint32 cast/integrality check (the range rejects NaN/Inf). On failure
  publish neither pointer. Allocate/fill at END of open after bindings, scratch
  and selftests; free only the base. 4096 B on this blob, not internal SRAM.
- **Cache bounds remain unchanged.** mhc_const_fill copies fixed 32/128/8;
  attn_gate_of caps fill at 64 but returns s_agate[li] unguarded. Set readiness
  false before fill; cache only when actual layer/lane consumer counts fit AND
  tensors cover them; fill valid counts. Out-of-range consumers use original
  FP16 conversion. Add a declaration if using fp16_get before its definition.

Keep BOTH original gather loops here; no P1 fusion or inverse P2 emit. Mentor
re-read records 226/227: true FP32[1024] permutations, unique integers 0..1023.
The intended lane removes 16,384 float-to-index conversions/token, changing no
floating arithmetic. Disclose cache repairs with the result. A P1-only draft
would test only half that mechanism; do not label it a two-table measurement.
Host-gate the finished source before flash and keep all ordinary device gates.

## Reserves, in order

**Inverse-P2 producer emit:** W2 can write
`hada_a[invp2[q]] = s*d3[invp2[q]]` AFTER the same completed kron2 sum. W2 first
half fully consumes hada_a into hada_c and joins before its second half writes
back; join again before W3. Validate a bijection and build one optional model-
owned uint16 inverse at END of open in PSRAM (2048 B here), with fallback/free.
Never reinterpret FP32. Removes 65,536 B/token INTERNAL scratch traffic, but
inverse reads and random D3 accesses may erase it. Record 227 is bijective;
inverse has only 1/1023 adjacent consecutive destinations. #369 gather unroll
and #664 Kron-half joining are different mechanisms.

The transfer from [Fusing Gathers](https://arxiv.org/html/2407.13585v1) and
[MLIR indexing maps](https://mlir.llvm.org/docs/Dialects/Linalg/) is to examine
traversal order and ownership before fusion. These producer emits are source-
derived proposals, not performance claims from those sources.

**Wide three-tap delivery:** still unmeasured. Extend B2p1fix, or use separately
preserved B2tap at 6.1550. nt==3, four independent +0 accumulators, same j=0/1/2
madd order. Snapshot raw projection before overwrite. Guard alignment/strides/
counts; startup/tails retain C. Append to gemv4_tie728.S: context
proj,hcur,w0,w1,w2,h1,h2,n4 at offsets 0/4/8/12/16/20/24/28; candidate registers
a3 source,a4 output,a5 hcur,a6..a8 weights,a9/a10 histories,a11 tiles,a12 zero;
f0..f3 sums,f4..f7 values,f8..f11 weights. High-to-low TIE lists; raw history
store then three taps then output, pointers +16. Multi-tile/full-history
canaries, startup/wrap/nonzero splits; start current history stale in the oracle.
[Espressif FIR](https://github.com/espressif/esp-dsp/blob/master/modules/fir/float/dsps_fir_f32_aes3.S)
supports wide delivery; do not copy its reassociated horizontal reduction.

## Retained negatives and execution

Residual-difference fusion lost 6.1683->6.1650; Q/K norm residency and raw tap
snapshot were neutral. Correct phi staging #911 lost 0.378%; earlier broken
staging results are invalid. Phi dispatch/RMS pairing, scale-fold #744, Kron
width/renames, four-pass Sinkhorn exit, four-output QK,36-byte records, LUT
de-split, private codebook and expanded CQ2 offsets stay closed absent a premise
change. Do not confuse the new emit proposals with those measured families.

Dual-store remains parked after ABI/selftest failures; saved B1x engine is
`/tmp/B1x_engine_snapshot_1512`. Later recipe: sixth destination a7, move zero
to a12, second wide store, update both callers/prototype/scalar/alignment and
use independent selftest buffer; remove only snapshot memcpy once proven.
CQ2 PMU stays below ready candidates. API is real, per-core ERI, two counters;
draft `/tmp/nd_quant.c.pmu-draft-1525` is not evidence. Future bounded screen
selects one 576-row Q call on both real workers, cycles+one event with row/byte/
init/overflow fields, print after join; do not mix two-core work with wall IPC.

Preserve dirty trees, goldens, anti-repeat history, locks and 240/80MHz. Main's
implementation lineage is older; never reset workers from main. Keep worker
frozen through host gate INSIDE `needle-board run N`. Never rewrite the old
B3rope literal `HOSTGATE_RC=$?` or treat it as a captured exit code. Old B2p1
casted/aborted log is invalid; B2p1fix is the real measured fusion.

Next mentor: harvest B1p1's OWN completed gates, get B2 W3 onto a real board job,
and verify B3's actual P2/dtype/range/cache fixes rather than narrative claims.
Three different lanes remain the goal; two idle boards are not a successful
three-board batch. Inspect live children plus nonempty growing logs, and the
200-turn cap. No repeat controls or ledger-only finish in place of discovery.
