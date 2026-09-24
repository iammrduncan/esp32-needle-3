# Needle 3 mentor queue

Mentor 2026-09-24 20:46 UTC; ledger through #609 plus live processes/logs.
Read at lane turnover. Keep worker dirt, board locks, anti-repeat history,
240/80 MHz, frozen inputs/goldens and every quality gate. Never interrupt a
live build, flash or benchmark. Mentor owns this queue, not implementation.

## State and why the ordering changed

**Accepted remains 5.3033 tok/s**, bundle5 `2c79104`, engine `0c1a6272cd01`;
accepted per-board pins b1/b3 5.3033, b2 5.3017. Full device 20/20, host 19/19,
fidelity 5.341e-05, top1 10/10, capture green.

**Discovery base, NOT accepted:** seed tree `61861dd9886c`, b1/b2 **5.6117**,
b3 **5.6133**. Seed snapshot `.auto/exp87/lut2_tie728.S.seed`,
md5 `3a2e522e3f38`; clean main.c snapshot `.auto/exp84/main.c.lean-ring-clean`.
Main HEAD is not automatically this base. Pin the actual worker source and ELF.
The prior clean stack `4d3094578050` was b1 5.5783, b2 5.5767, b3 5.5800.
Seed full session #589: **18/20**, delta 52, missing 0, rc=1; extended 5.5177,
prefill 5.925, think 4.39, min_case 5.35, heap 8,415. Capture passed #590.
Third primary reading #604 is done; no fourth seed/control reading is useful.

At 20:30 mentor found ALL boards idle and researcher sleeping 1,140 seconds.
`M-cov-b1.log` was already final at 299 bytes: anti-repeat refusal, rc=42,
no build/device process. #605's running-coverage claim was false; #606 retracts
it. Preserve additions/backups; do not bypass the guard or recapture old goldens.
Actual fixtures at inspection after #606: workers have 6 primary + 13 extended
+ 1 think with 20 device goldens. B2/B3 have 19 host goldens; B1 has 23 (four
extra entries). **Main still has 24 prompts, 23 host, 20 device** despite the
"restored main" narration; HEAD already contained the additions. Do not sync
this half-state onto workers. Let the researcher reconcile main from a verified
pre-addition backup while preserving additions, without changing old expectations.
Primary-only screens on complete worker fixtures remain legitimate discovery.

The recent nulls close specific forms, not all scheduling: prepare+LUT fusion
#584 = 0; LUT quartet barriers #579 = -0.090%, pointer hoist #582 = -0.060%;
cold fw_scale #582 = 0 because it is not called. Attention still costs ~33.4 ms
in #593's boot profile. Its separate rescale/output passes and repeated V loads
across heads are concrete untested work. Do not infer an architectural ceiling
from a few nulls or from a boot profile alone.

## Next three lanes

| Board | Work at this pass / next experiment | Compare against |
|---|---|---|
| 1 | First A DONE: **5.525**, -1.545%, 6/6, rc=0, wrong contraction graph, restored to seed. Since researcher moved corrected A to B2, use B1 for **shared V loads across two heads**, recipe B. | Its seed 5.6117 |
| 2 | Small-split DONE: **5.615**, +0.059%, 6/6, below bar. Researcher is now preparing **corrected A**, rounding boundary + old 4-cell width; inspect target before screening. | Its seed 5.6117 |
| 3 | Norm+RoPE was **5.6117**, -0.029%, 6/6. Full Q-head C then hit local extraction compile errors and was reverted; it is UNMEASURED. Put its helper after tap_cols/tap_rows and remove the old vc-to-c declaration from tap_cols; finish C. | Its seed 5.6133 |

Do not wait for B1 before preparing B2/B3. Confirm a real process and a growing
nonempty stage log, not a printed PID or leftover tmux shell. If one implementation
blocks, start a ready lane and give the blocked one a concrete smaller step.

B2 interpretation: all four `zcrms` call sites pass dm=768. Lowering its
n>=512 caller guard to 256 has no newly reached 256..511 call in this model.
The half<1 edit CAN change other clients (including Q taps with three units).
Keep the live run, but do not call it evidence for formerly unreachable small
norms without a real call site; half<1 alone was already screened #558. Do not
repeat this parameter family after this lane. B2 provenance was `6b3f7a50e2c6`;
B3 norm+RoPE was `9908c828b9c9`; both retained heap 8,415 and token delta 0.

### A — remove an output-memory pass, keep the exact online recurrence

**20:42 target-object finding: FIRST A FORM DOES NOT PRESERVE THE ROUNDING GRAPH.**
In B1 ELF `b6d98aa06a20`, the rescale loop at 0x4037c1cc loads vf0 into f0
and old oh into f9; 0x4037c1d2 multiplies **wv0*vf0**, then 0x4037c1dc
does madd.s f0,f9,f1 (**old_oh*r + rounded term0**), followed by term1.
The source's `float y=oh[i]*rescale; y+=wv0*vf0[i]` did NOT force the old
rescale rounding: GCC contracted across statements in the opposite order.
The lane finished at 5.525 and 6/6; that does not prove this form bit-identical.
Cadence's [MADD.S definition, §8.3.159](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf#page=487)
explicitly gives no intermediate product rounding. A simple analytic FP32
witness: old_oh=1+2^-23, r=1-2^-24, wv0=1, vf0=-1, term1=0. The original
rounded-rescale graph gives 0; the contracted-rescale graph gives 2^-24-2^-47.
Use such cancellation cases in the researcher's target differential.
At turnover, force only the rounded rescale result to exist (e.g. a precise
FP-register compiler barrier on y immediately after the multiply, or a tiny
target mul.s helper); leave the following two baseline madd.s operations.
An empty asm with a tied FP output emits no hardware barrier instruction;
register allocation can still change, so measure rather than assume it costs
extra cycles. The known first-form errors are fixed in one bounded successor:
the original four-cell width plus the explicit rounding boundary.
Guard Xtensa's `+f` constraint with ESP_PLATFORM (or an existing target guard);
the host compiler has different FP constraints. A host-only volatile temporary
can preserve that standalone multiply without affecting target code.
Do NOT disable contraction for the whole function or use a global memory
clobber. Re-inspect operands and compare the actual integrated function before
a corrected candidate's field screen. If scalar loop overhead loses, preserve
the old 4-cell unroll in that corrected form rather than closing memory fusion.

In `attn_heads`, when mnew > mx[t] AND denom[t] > 0, the code currently:
computes r; traverses all 64 oh[] values to multiply by r; updates denom; later
computes w0/w1; then loads/stores those same oh[] again for the two V terms.
Defer ONLY the oh[] multiply into that immediately following P.V loop.
Keep the old max decision, r calculation, denom arithmetic, exp pair, wv0/wv1,
position-pair order and odd tail. Initial experiment: paired-position path only.

For the rescaling branch each cell must still do:
rounded old_oh*r, then the old first multiply/add, then the old second
multiply/add, then store. Preserve the actual target mul.s/madd.s operand graph;
C algebraic equivalence alone is insufficient. The non-rescaling branch should
execute the old P.V loop with NO new multiply-by-one or per-element condition.
Branch once outside the dimension loop. Do not rescale the weights instead,
change the max schedule, postpone a rescale across positions, or change exp.
Use a boolean to remember whether rescaling is required: r can underflow to
zero, and r==0 must still multiply the old output. Researcher caught this while
preparing A. B1's original P.V object uses two ordered madd.s per output; retain
the preceding separate mul.s when combining the rescale path.

Expected mechanism: one fewer oh load/store pass per true rescale, not fewer
rescale multiplies. #292's 48.5% zero-exp-pair statistic is NOT a measured
rescale rate; do not price it as one. Count/inspect the real path if needed.
The target comparison should include denom=0, unchanged/increased maximum,
repeated maxima, odd tails, and multiple KV pairs, comparing intermediate
mx/denom/oh bits to the original integrated routine. Host gate plus a real
primary screen follows; no broad new harness.

[FlashAttention-2 §3.1/Algorithm 1](https://arxiv.org/html/2307.08691v1)
motivates reducing output traffic and non-matmul work. This proposal is a
local memory-pass fusion derived from Needle's code, not an import of a
different softmax algorithm or a claim that algebraic exactness is bit equality.

### B — share each loaded V value across TWO heads, within the same position pair

Current `attn_heads` converts each V pair once for a KV group, then six heads
independently load vf0[i]/vf1[i] from the staging arrays. Pair HEADS, not
positions or reduction terms. Compute the existing QK/max/denom/exp work for
two heads (same per-head operation order), retaining four wv scalars. Then one
dimension loop loads vf0[i] and vf1[i] once and updates both disjoint oh arrays,
each still term0 then term1. Keep existing rescale passes initially, so this
is independent of A. Use a scalar fallback when a head range has an odd tail.

This can be written directly as t+=2 plus two independent scalar softmax
substeps; no full score matrix, six-head scratch arrays, new allocations,
cross-core communication, or query-dot reassociation. Start with one cell
(or two) at a time: two output accumulators + four weights + two V values
fit the FP register budget more comfortably than a blindly duplicated 4-wide
body. Inspect generated loads/spills before expanding the unroll.

This is NOT #354's rejected position/dimension loop interchange. Mentor read
`.auto/exp31/kbench_e31.c:146-180`: its fixture is s_oh[8][24] with a
DIFFERENT s_v row for every p; it has no shared six-query-head V operand and
one update per cell. Its -44.1% remains valid for that loop, not this reuse.
#230 varied dot schedules, not two-head P.V operand reuse. Preserve original
dot parenthesization and softmax arithmetic. Compare all head outputs with
real/noninteger activations, including split boundaries and odd-head fallback.
Then measure the integrated candidate at this board's seed pin.

### C — give one core each Q head's whole producer/consumer chain

Q has 12 independent 48-column heads. Current Q taps have three 256-column
units and run serial with half<2; norm already splits heads 6+6, while RoPE
is serial. Keep the Q history copy first. A head-owned callback performs Q
tap columns [48h,48(h+1)), the existing norm, then that head's RoPE.
Use the same already-required head split; join before attention consumes Q.
K/V processing initially stays as it was.
Within one callback, call tap_cols ONCE for [h0*hd,h1*hd), then the existing
norm helper for [h0,h1), then RoPE for those heads. This preserves ownership
while avoiding six repetitions of tap offset/modulo setup on each core.

Factor the existing tap body into a column-range helper so the fused callback
can preserve taphoist/tap2col/tapfwd arithmetic rather than transcribing it.
Do NOT fake tap_ctx.dim=48: history and weight strides must stay full Q dim=576.
Keep ascending tap order, +0 seeds, nt=1/2 fallback, original norm sum order
and normalization multiply grouping, and the two original RoPE expressions.
Never call nd_parallel_rows inside a worker; call range helpers directly.
Keep declaration order: tap_ctx, tap_cols, tap_rows, then qhead_ctx/qhead_rows.
zcrms_head_rows is already defined earlier. Do not duplicate `c` or retain a
reference to the deleted `vc` parameter inside tap_cols. A compile refusal on
these extraction errors is not a negative performance result.
Target checks should cover startup positions, wraparound, and odd head splits.
The smaller norm+RoPE candidate is a legitimate staged implementation; its
~0.2 ms RoPE phase gives a small ceiling. Q-tap parallelization is the main prize.
This differs from standalone tap96's extra dispatch and from free-join fusion A/B.

## Preserve these conclusions and limits

- Failed cases remain heldout_interval_one and heldout_long_tools_note_only.
  **The cause is unresolved.** The generated model input is schema prefix +
  query and router dispatch occurs after generation; demo-timer state is not
  thereby proven to explain changed generated text. Drop=0 is not request/token
  identity. Keep goldens and acceptance frozen; no owner "admission" substitutes
  for unchanged model quality. Do not start another unchanged full gate.
  Concrete evidence: #589 generates interval=120 alone where the device golden
  also calls get_status; the long-tools case changes a single interval=300 call
  into interval=45 plus repeated timer calls and status. These are generated
  tool-call differences, not merely runtime timer/status response fields.
- The first-W8D seed retains ADD(+0,value) in BOTH old and new walks. Its host
  add-versus-MOV sweep describes a different equivalence question; it does not
  test multi-group/row cursor correctness of the transformed assembly. Preserve
  the measured winner, existing guards, and its target-differential obligation.
- Closed without new premises: old tie2/deeper prefetch, serial LUT build,
  ordinary QKV concatenation (~-0.31%), norm-bias hoist, sinkpair, silu4,
  FP16 taps, approximate math, tier stride sweeps, unsafe 120 MHz.
  No broad compiler-flag change, quality-threshold relaxation, or new dependencies.
- Do not compose A/B/C until their own readings exist. If complementary small
  wins emerge, measure one justified combination; do not add estimated gains.
- Pi recovery: Ctrl-C cleared the editor but did not stop the stale sleep;
  Escape aborted that turn. Only use abort after confirming no live board job.

Next mentor: verify all three workers actually progressed; harvest the real
A/B/C source identities, target equality, primary timings and heap. Check the
main coverage half-state and whether the two frozen failures were explained
with input/provenance evidence rather than relabelled. No more acceptance
packets or repeated gate falsifications in place of performance discovery.
