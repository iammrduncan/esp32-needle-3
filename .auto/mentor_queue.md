# Needle 3 mentor queue

Updated **2026-09-27 03:41 UTC**. Researcher owns code and measurement.
Owner accepted **5.3033 tok/s**; research pin **6.1550**, engine `d6b8014fd2fb`.
Best completed research reading **6.2200**. Nothing promoted: the frozen #647
pair still fails **22/24, token delta 52**. Same failures are lineage evidence,
not full correctness or permission to change goldens.

## Evidence and immediate lanes

Arrival: pi was stopped at the 200-turn cap; no board/build jobs were alive.
Mentor resumed it. Completed own-log results, all host **23/23, RC=0**, fidelity
**5.341e-05**, top1 **10/10**, gen99, same device failures:

| Tree | Decode / own baseline | Meaning |
|---|---|---|
| B1nofb2 | **6.1933 / 6.1933**, ext 6.1265, heap 9335 | SiLU dead fallback removal neutral, callback 888 B smaller; retain. |
| B2pre2 | **6.1933 / 6.1683 (+0.405%)**, ext 6.1253, heap 8415 | W3 output-prefix scheduling is the strongest new mechanism. |
| B3meta2 | **6.1917 / 6.1750 (+0.270%)**, ext 6.1259, heap 10327 | Both numeric metadata tables active (`PERM need=1024 ok=1`). Host evidence is HOSTGATE-B3meta3dev.log. |
| B1w3 | **6.2200 / 6.1933 (+0.431%)**, ext 6.1500, prefill 6.5683, think 4.75, heap 9335 | Prefix gain survives composition. Device same pair; chained host RC=0, 23/23. Full-file diff is only wrapper + W3 call. |

**Actual 03:43 state:** B1w3 is DONE, including chained host gate. Preserve it.
B2resid2 has a REAL live build/host-gate chain (PID 477065); freeze board 2.
B3inv was a
failed guard-only build, not inverse-P2; broken draft preserved and metadata
base restored. A script or a printed launch is not proof of device work.

Next three lanes, prioritized for implementation readiness:
1. **B1:** preserve the completed **6.2200** tree; next W1 zero-tail/replay
   proof and candidate below. No further prefix transfer/control now.
2. **B2:** finish C-only W3 residual producer emit on B2pre2; base **6.1933**.
3. **B3:** repair metadata guards and **fuse the P1 gather into SiLU using the
   uint16 metadata**; base **6.1917**. This ready substitute moves ahead of
   inverse-P2 because that candidate is unwritten and guard repair alone is not
   discovery. Keep P2's gather and `*d3[i]` unchanged for this lane.

No control repeats or guard-only timing laps. A new user turn resumes research;
self-declared "final call" or a previously reached cap does not close the queue.
Use fresh log names; confirm processes and nonempty growing logs after launch.

## B2: exact residual data flow and wiring

**03:43 readback:** missing launcher, missing u/dm signatures and duplicate dm
argument are now repaired. Full-file diff matches the intended clone, 9 guards,
6/24 schedule, MLP u threading and fallback q<dm. B2resid2 is REAL and frozen.
Old B2resid.log is build-failure evidence, not a timing result.

`kron1_blocks` writes **m->hada_c**, at its two eight-result/scalar store sites.
`kron2_rows` reads that same **m->hada_c**; its `b` is the factor, not activation.
Keep the prefix wrapper's first half unchanged: kron1 context `{m,src,a,na,nb}`,
**6 blocks**, then the existing join. Use a SEPARATE second-half callback/context
`{m,u,d4,b,na,nb,dm}` over **24 rows**. Keep na=nb=32 for strides/reductions.
Each of its eight completed sums and scalar tail emits `u[q] += sum*d4[q]`
only after **q<dm**. Pass block's real u and fp[18] through the MLP call.
Remove only W3 materialization and caller d4 loop; keep ublk and later subtraction.

Guard 32/32/768. The unsupported-geometry branch must run ordinary kron_apply
**AND the original residual loop**, since its caller no longer performs that
loop. W1/W2 keep the ordinary callback. Retain both joins. Expected avoided
traffic on the prefix base: **49,152 B/token INTERNAL**, not PSRAM bandwidth.

`/tmp/patch_b2_resid.py` is an INCOMPLETE clone generator: its `or True` is
vacuous; exit only checks clone existence, ignores guarded-store failure, and
wrapper/caller wiring is absent. Do not mistake running it for an experiment.
Read back finished source; require 9 guarded stores, correct context cast,
6/24 scheduling, W3-only call, u/d4 arguments, old loop absent, fallback complete.
Use full-u differential, nonzero splits/canaries, then preflash host and device
checks. Build only if every saved-source check passes.

## B3: metadata + P1 consumer fusion, then inverse-P2

Preserve broken B3inv draft; `/tmp/B3meta2_nd_model_preserved.c` is the measured
base. Repair a bounded whole stanza, not repeated splices through stale offsets.
Actual enums in nd_cact.h: **ND_DT_FP32=2, ND_DT_FP16=1**. Divisibility of nbytes
is a length check, NEVER a dtype check. Require tp1/tp2 dtype FP32, valid data,
0<need<=65536 BEFORE allocation, nbytes>=4*need; range BEFORE numeric cast,
then integrality; one optional PSRAM block, both pointers published together,
base freed at close. mHC fill clears ready first; guard exact 8 layers/4 lanes,
FP16 dtype, nonnull raw pointers, lengths covering 32/128/8 consumers. Keep
open-time cache invalidation and unsupported-geometry fallback.

P1 fusion: retain B3's scale-row prologue and pair arithmetic. On the metadata
path read x0/x1 from **hada_b[p1_meta[i/i+1]]** before writing hada_a. Remove
its preceding P1 whole-row gather. Select metadata/FP32 fallback outside the
pair loop; do not reinterpret the archive float pointer as uint16. A small
separate metadata callback plus existing gather/SiLU fallback avoids duplicating
arithmetic in the hot path. All initializers must supply the intended source.
B1/B2 P1 fusion already won on FP32; this asks whether it composes with the
numeric-index win, saving another **65,536 B/token INTERNAL** without allocation.
B3 norm factors are prescaled; B1's are raw (1+s). Never transplant whole files.

03:43 draft is not yet active: `perm` is still float*, initializer is NULL,
and both old P1 gathers remain. Make perm **const uint16_t***, initialize it
from **m->p1_meta** without a cast, keep only `if (!m->p1_meta)` FP32 gather.
Its existing callback branch is already outside the pair loop. Read back all
three changes plus real dtype guards before treating it as a candidate.

**Next after this:** W2 producer writes `hada_a[invp2[q]] = sum*d3[invp2[q]]`.
Validate a bijection, optional model-owned 2048 B inverse here, fallback/free.
First half reads all hada_a into hada_c and MUST join before any aliased output
writes. Each inverse destination has one owner; join again before W3. Keep
completed sums/order and C kernels; save 65,536 B/token INTERNAL. The random
D3 reads are the uncertainty. If scatter loses while arithmetic/correctness hold,
one follow-up can prearrange D3_emit[q]=D3[invp2[q]] at open (32 KiB PSRAM for
8 layers) to distinguish scale delivery from fusion overhead; no archive edit.

## New follow-up: W1's known zero input suffix, with exact replay

This is DISTINCT from W3 dropping unused outputs and from #400's FWHT copy.
W1 writes hada_a[768:1024]=+0, then its first half reduces 32 input rows.
Explore reducing only W1's first-half i loop to **24**, while retaining full
32 output rows, factor row stride 32, nb=32, both joins, ordinary second half.
The existing nd_kron1_w API already separates reduction count from byte strides;
never change na globally. This removes at most **65,536 FMAs/token**, half the
W3-prefix arithmetic saving, before guard cost. No equivalent log entry through
#940; #435 was input transpose staging, #664 was join removal.

Static archive check: all eight W1 A factors (records 20+27*li) are FP16 32x32
and finite, including the 256 omitted coefficients. Actual assembly uses a5 only
as the loop count and a6/a7 as independent strides: count 24, strides 128/112.
The retained learned part is NOT a Walsh matrix; wholesale FWHT substitution
does not follow from the Hadamard name or the constant-magnitude padded tail.

Do NOT assume skipping +0 products is always bit-exact. A defensible experiment:
validate omitted W1 A factors finite at open; after a 24-term tile, accept only
NORMAL finite nonzero FP32 outputs (inspect exponent bits, no fast-math). If any
of its 8 sums is zero/subnormal/nonfinite, replay that tile with the original
32-term kernel. Thus skipped finite zero-products cannot change retained normal
sums; exceptional signs/FTZ/NaN behavior uses the original path. Keep the C/asm
paths consistent and the generic fallback. Prove exact signed-zero, subnormal,
nonfinite, all-zero, random and nonzero-split cases BEFORE a board timing lane.
Price the guard instructions: if they consume the saving, retire or redesign.
This is an unmeasured local hypothesis, not a promised speedup; current lanes first.

## Research interpretation and preserved limits

[FastKron](https://arxiv.org/html/2401.10187v1) and
[KS/Monarch inference](https://proceedings.mlr.press/v267/gonon25b.html) motivate
writing results in the consumer's layout and removing intermediates. Transfer
that principle, not GPU speedup factors: Needle's scratch is internal SRAM and
#664 already measured single-dispatch Kron halves neutral with spin enabled.
The new measured W3 result reopens **avoided arithmetic**, not join or width sweeps.

Residual-difference fusion lost 6.1683->6.1650; phi staging #911 lost 0.378%.
Norm residency/tap snapshot neutral; scale fold #744, Kron widening/renaming,
LUT de-split, four-output QK, Sinkhorn four-pass exit, 36-byte records, private
codebook, expanded CQ2 offsets remain closed absent changed premise. Wide FIR
and PMU/dual-store drafts are below the ready C experiments, not lane blockers.

Preserve all dirty workers and drafts; main is older. Locks, anti-repeat guard,
240/80 MHz, goldens and host/device gates stay intact. Freeze a worker through
its chained host gate inside needle-board run N. Do not interrupt live jobs.
Next mentor: verify B1 transfer result; establish that B2 residual and B3 P1
metadata fusion really reached devices; inspect W1 replay proof only after them.
