# Needle 3 mentor queue

Mentor refresh 2026-09-25 04:18 UTC. At entry B2 alone was doing E49 kernel
diagnostics; B1/B3 had no live work. Resume THREE DIFFERENT discovery lanes.
Preserve worker dirt/snapshots, board locks, repeat guards, frozen fixtures and
240/80 MHz. Only the researcher implements, builds, flashes and measures.

## Evidence and comparison pins

**Accepted remains 5.3033 tok/s**, bundle5 `2c79104`, device 20/20.
Neither faster proposal is accepted or quality-green:

| Board / preserved runtime | Own decode pin | Evidence |
|---|---:|---|
| B1 bundle5 + attention composition + staging + ring + spin, `e6d55f29e286` | 5.5500 full (#660); 5.5450 screen | ext 5.4808, think 4.34, heap 6183, capture green |
| B3 same bundle5-derived proposal | 5.5483 screen (#659) | six primary exact; caller-spin 1800 sub-bar (#661) |
| B2 seed-era composition + staging, `91e79eeeb304` | 5.8083 | cross-board full 5.8050 (#650), ext 5.7331, think 4.51, heap 5087 |

Both full proposals complete 20 cases but match **18/20**, token delta 52.
#647 bundle5 + ONLY ring reproduces the same pair: that transport change is
sufficient to trigger it. This does NOT establish a timer-to-model causal path;
run_inference consumes query/phase/restored prefix, and router dispatch follows
generation. Frozen oracles stay unchanged. Host 19/19 cannot waive device gates.

Live at 04:18: B1 `M-kron-b1` / `b422e7717527` and B3 `M-notif-b3` /
`4a3aa059b8ed` are real locked measure.sh builds, with nonempty provenance logs
in `/root/board-pool/batches/`. B1 host 19/19, fidelity 5.341e-05, top1 10/10,
CHECK_RC=0. B3 scheduler compiled. B2 E49 is finished; start its next candidate
while these two lanes run. No new decode reading yet. Leave live jobs untouched.

## Next three lanes (start idle lanes while B2 finishes)

**B1 — fuse the two Kronecker halves under one row owner.**
New dependency-based fusion, not the closed transpose-staging idea (#435).
Each `kron1_blocks` block writes four complete hada_c rows; `kron2_rows` consumes
only its own hada_c rows. In one split callback, run the existing first-half
body on [b0,b1), then the existing second-half body on [4*b0,4*b1). Dispatch
once over na/4. All three actual call sites use distinct hada_a input / hada_b
output; preserve fallback for unsupported/aliasing shapes. No changed reduction
order, new scratch, nested nd_parallel_rows or reads of another core's unfinished
rows. Keep the tuned 4x2 and 8-column inner bodies. This removes 24 inter-half
joins/token and lets each core consume its own intermediate. Benefit is uncertain
on the spin stack: one small primary screen settles it. Compare to B1's own pin;
do not expand into arbitrary MLP fusion/permutation crossing on a null.

**B2 — 4-bit codebook-load scheduling; E49 mismatch is resolved.**
E49's output offset double-count wrote through index 303 into 192-float arrays;
the researcher repaired it, norm placement and exponent shift after mentor
review. An empty-file objdump comparison was also retracted. Repaired 04:05 run
matches 192/192 and all tested
R=1/2/4/8 ranges. Warm 3,764,168 / cold 3,822,181 cycles are ISOLATED readings,
not a field ceiling: xh uses ND_ALLOC (PSRAM) whereas m->xh uses ND_ALLOC_FAST
(internal SRAM), and eviction happens once before the whole ~300 KB sweep,
not between the field's layer calls. Both sweeps can already stream cold weights.
The raw increment is 1.54%, not exactly 1%. evict is also read uninitialized
(sink=2147483647): use initialized bytes and an integer/volatile sink. Fix these
placement/sink details IN the candidate comparison, not another standalone
control. No speculative kernel/LOOP bisection from the now-resolved mismatch.

Next candidate: in gemv4_tie728.S, replace repeated immediate
`lsi f12; madd.s ... f12` consumers with alternating codebook registers f12/f15
(f14 is also dead inside the word body). Load the NEXT nibble's codebook entry
before consuming the CURRENT one. Same eight products, four partials, within-
partial FMA order and final fold; ordinary lsi, unchanged word count/cursors.
This hides potential load-use latency without deeper weight prefetch or row
blocking. It is NOT #333's rejected ee.ldf forms, nor CQ2's swept gather variants.
Require exact multi-row differential and an independently initialized, aligned
nibble known-answer before a normal primary screen against B2's 5.8083. If kernel
cycles are null, stop after this schedule.

Count the field correctly: 8 layers * (4+4+16) rows * 3072 = **589,824 weights**
and 4,608 groups/token, not #585's 576 groups/token. That reset-seeding closure
missed a factor eight but still predicts a small gain; do NOT revive the four-
instructions/group idea. The load-consumer experiment acts per weight. E49 is
single-core isolation, not decode; a cold/warm delta does not upper-bound
instruction-scheduling headroom.

**Next B2 turnover: reopen ALIGNED wide loads on new evidence, not old labels.**
At 04:11 mentor found the decisive #333 fill probe itself was confounded:
`.auto/exp22/kbench_phi22.c:186` initializes A's ucb/urow ONCE before m=1..3,
then overwrites both with all-ones B inside the loop and never restores A.
Thus later modes' A=4080 is the correct answer to their actual all-one input;
it does not show lost nibble identity. Separately, current
`esp32/main/dot4_tie728.S` spells XH_WIDE as f4,f5,f6,f7 (and pair as f4,f5),
opposite Espressif's demonstrated order f7,f6,f5,f4 (pair f5,f4).
The earlier unit uxh has no explicit 16-byte alignment; 63 vs 64 is not an
alignment-independent falsification. These are concrete changed premises.

After the scalar schedule (do not interrupt a live lane), make ONE independent
128-bit-load candidate from the CURRENT proven multi-row kernel, changing only
eight scalar xh loads + increment to two correctly ordered wide loads. Require
16-byte xh alignment with a scalar fallback for other pointers; group stride
512 preserves alignment. Keep packed/codebook loads scalar, all arithmetic and
row cursors unchanged. Reset A AND B fixtures independently for EVERY variant;
then aligned real-row differential, row-range differential, cycles and normal
quality gate. Do not reuse exp22's stale .S snapshot (its addx4 operands also
predate the fix), force unsupported alignment, or copy old +25% as a speedup.
This is a stronger ready reserve than another width/clock/closure exercise.

**B3 — complete the missing scheduler transfer onto the bundle5-derived line.**
Source audit: B1/B3 main.c still uses binary semaphores and the OLD one-shot
`if (s_done_seq != job) xSemaphoreTake(...)`; B2 already has lean notifications
and a predicate-rechecking while loop. #492 fixed that delayed-give race, and
#498 measured lean notifications +0.33..0.36% versus its predicate-correct base.
#658 ported only the older spin hunks, omitting both later changes. Port the
proven current B2 scheduler region only, retaining B3's engine, RX ring location,
clock/config, task stack and spin budgets. Do not copy the whole main.c or old
regeneration recipes. Completion is the sequence predicate on EVERY wake;
preserve payload publication/order, one outstanding descriptor and context
lifetime. Notifications are wake hints, not proof of completion.

This is a transfer to a base missing the implementation, not another spin-budget
sweep. One primary screen vs B3's 5.5483, then breadth/capture only if warranted.
Record that the port also restores the correctness predicate; do not attribute
the combined delta solely to notifications without a predicate-only comparison.
Apply the known predicate-only safety fix before any new B1 semaphore screen
and disclose it in candidate provenance; no stale one-shot completion.

## Turnover, reserve and measured closures

Use existing wrapper and `checks.sh && measure.sh`. Snapshot/hash the ACTUAL
worker base before patching; verify the linked candidate symbol/body is nonempty
and reached. Judge work by live child processes and growing logs, not a tmux
name or printed PID. Primary-only ext/think=0 are unmeasured sentinels. New device
failures block promotion. Use the documented screen-to-full repeat allowance
only when a full gate is justified.

If mechanically blocked, take this finite quality-enabling reserve: on one
diagnostic image, establish the failing queries' post-parser byte length/hash
and token IDs versus frozen input plus restored-prefix identity, before inferring
timer/FPU state. Existing RXTRACE captured 7/8-byte control commands, not the two
prompts. Reuse a hook; no suite expansion, pacing change, oracle recapture or
ongoing unchanged full-gate loop. Admitting speedups requires preserved quality,
not an owner waiver based on an unproven explanation. Other boards discover.

Retire prior queue work now measured: sink-aware cursor #648 null; pv_pair2
noinline #649 negative; cold fallback #652 confounded by pvni residue (not a clean
universal code-size bound); staging width4 #653 null; QK width12 #654 negative;
rescale sweep width4 #655 negative. #663 merged-vs-split both-rescale sweep is
null, both flags occur 2.36%: close that branch family. Keep #624 PV width8,
#626 normalization pairing, #628 QKTILE2, #634/#636 counted loops, twice-negative
rescale/PV fusion, #644 nonpositive exp and #645 paired rescale exp down. Staging's
two-base +1.05..1.07% is real but not a general noinline policy. Local negatives
do not establish that all scheduling/compute is exhausted. No extra clocks,
approximate math, FP16 taps or big KV mirror (runtime free PSRAM after both
prefixes is only about 40 KB).

Research anchors: [Cadence ISA, LSI/MADD.S](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf)
specifies exact scalar/FMA operations to preserve; no target-specific stall
count is assumed. [Espressif S3 dot kernel](https://github.com/espressif/esp-dsp/blob/master/modules/dotprod/float/dsps_dotprod_f32_aes3.S)
is a scheduling reference, not permission to copy its changed reduction graph.
[FreeRTOS task notifications](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/01-Task-notifications)
and local #492/#498 support the transfer. Kron row ownership is a new inference
from this code's dependencies, not a promised external speedup.

Next mentor: inspect repaired E49 bounds/result, actual B1/B3 launches, and
restoration of the omitted completion predicate/lean notifications. Then harvest
three distinct screens against their OWN pins. Preserve accepted 5.3033 versus
proposals 5.5500/5.8083 until strict gates pass.
