# Needle 3 mentor queue

Mentor refresh **2026-09-26 07:35 UTC** (actual clock). The researcher owns
implementation and measurement. Preserve worker dirt, frozen goldens, locks,
anti-repeat history, assertions, and supported 240/80 MHz. This replaces the
stale lane table and appended finish notes; historical results remain in log.jsonl.

## State and changed priority

**Owner-accepted: 5.3033 tok/s. Fastest measured candidate: 6.1550**, engine
`d6b8014fd2fb`, measured on all three boards (#872/#874/#880). Host 23/23,
fidelity 5.341e-05; device **22/24, token delta 52**. The two frozen #647 cases
still block adoption. Host agreement in #890 does not authorize changing device
goldens. No rebaseline, new capture suite, or more #647 disposition prose here.

At mentor arrival, all boards were idle: no builds/flashes/benchmarks, last pool
log updated 07:07, and pi was doing another closure-by-inspection iteration.
The active operator directive rejects the self-declared finish and rejects using
predicted sub-0.2% gain to retire runnable experiments. Recent +0.109% QK and
+0.108% sparse-Sinkhorn readings demonstrate why small mechanisms deserve tests.
Record them as candidate evidence, never as owner admission or a fresh old metric.

**Next three lanes, all compared with their own 6.1550 candidate pin:**

| Board | Experiment | Decisive question |
|---|---|---|
| B1 | Same-width `kron2_rows` compiler/load schedule | Can load-use gaps improve without wider tiles or a second j walk? |
| B2 | Exact four-pass Sinkhorn cycle detector | Are floating-point cycles excluding useful exits from the fixed-point test? |
| B3 | Four-output QK with a 64-bit operand tile | Can fewer operand loads pay without the reloads of the failed 128-bit body? |

Launch each ready lane, then prepare the others. Do not wait through a whole
benchmark before preparing another lane. Check actual processes and nonempty,
growing logs. An identical ELF is not a candidate. No duplicate controls or
cross-board transplant is needed: the three baseline pins are already measured.
Use an existing working boot selftest if kbench is broken; do not re-enter the
four-build kbench bisection that consumed #861-#867. Freeze a worker while live.

## B1: scheduling at the existing eight-accumulator width

#891 found `lsi` immediately before its consuming `madd.s`. Rotating accumulator
destinations proves independent chains exist; it does NOT prove operand loads
are early enough. #674 (four-wide/two walks, -0.30%) and #680 (eight sums with
wide factor loads, -0.18%) tested different bodies. Keep those variants closed.

Start with one narrowly scoped compiler scheduling candidate on `kron2_rows`,
keeping eight outputs, the j+=2 walk, +0 seeds, each chain's cj term then dj term,
and existing tails. Inspect the actual compiler's enabled options; try function-
local pre-register-allocation scheduling with register-pressure awareness if it
is not already active (`schedule-insns`, `sched-pressure`; preserve every FP
option). Compare the actual linked body against the current worker ELF. Choose
one changed schedule with earlier factor loads and no new spills for the board
screen. Do not apply global -O3/fast-math or sweep flags across the whole engine.
If this produces identical instructions, use an explicit scalar load-ahead body
at the SAME width, bounded to available FP registers; record an actual blocker
if no such schedule fits, then substitute the reserve below.

GCC documents pre-allocation pressure-aware scheduling and a separate post-
allocation pass; target defaults differ, so this is a hypothesis, not a promised
win: [GCC scheduling options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-fsched-pressure).
Byte-different instructions still need the device differential and timing; host
fidelity alone cannot establish an Xtensa schedule's arithmetic.

## B2: finite-cycle exit, same final 20-pass state

The current detector proves F(A)=A across ONE row+column pass at passes 5/9/13/17.
It cannot detect a two- or four-pass rounding cycle. Try this distinct exact rule:
retain A4; compare A8 with A4, A12 with A8, and A16 with A12, using ALL matrix
bytes. If equal, F^4(A)=A, and the remaining 12/8/4 passes are multiples of four,
so finishing at the current state is exactly the original 20-pass result.
This is NOT approximate convergence and NOT a claim that F(A)=A.

Replace the sparse detector for this experiment rather than layering unbounded
instrumentation onto it. First saved state is AFTER pass 4; comparisons are AFTER
passes 8/12/16; update saved state after an unequal comparison. Keep all row/column
arithmetic, ND_SINKHORN=20, and final exponentials unchanged. For other iteration
caps, only skip a multiple of the proven period or retain the old path. Bound
snapshots by ND_MAX_LANES; use memcmp/bit equality, not float == or tolerances.

A host census on real forward calls can cheaply expose hit counts and saved
passes versus today's detector, including cases caught only by the cycle test.
Do this while the other lanes build, then measure on device: compiler/libm paths
can change cycle incidence. Compare complete matrices against the original
20-pass computation in the candidate's device selftest. Price detector overhead
and field decode, not just saved-iteration counts. This is a new mentor hypothesis;
no matching cycle experiment was found in the ledger. Even a null teaches whether
fixed-point-only detection is the right mechanism on this model.

## B3: 64-bit QK tile with an explicit register budget

Carry forward the useful unmeasured item from the previous queue. #684/E60's
128-bit QK schedule took 56 cycles/chunk vs compiler C 49 and lost 0.71% in field.
Do not repeat its five-load/reload schedule. Use FOUR 64-bit loads per two-element
tile: qhA, qhB, kf0, kf1. Budget f0-f15: four sums, eight operands, four product
temporaries. No double-buffered operand bank fits, and none is required.

Preserve the linked expression graph for EACH of the four outputs: odd-term
multiply, even-term madd into that product, then add to the running sum; ascending
pair order, +0 seeds, and caller scaling once. Check the current linked reference
rather than copying the unused `qk8w_tie728.S`, whose sequential FMA is a different
graph. Keep generic/unaligned fallback and use documented wide-load alignment.

Use existing kernel/boot selftest with signed int8-derived K, varied Q, all four
outputs, real n=48 and fallback shapes; then a same-image kernel timing. Promote
to request timing only if correct and useful. The historical c4_pair_cyc=139 is
per OLD single dot and cannot price this four-output helper. [ESP-DSP S3 dot]
(https://github.com/espressif/esp-dsp/blob/master/modules/dotprod/float/dsps_dotprod_f32_aes3.S)
is a load-syntax reference, not a reduction-order template.

## One reserve and retained closures

**Reserve: phi six-row residency, costed honestly.** #886 retired it WITHOUT a
kernel measurement. A bounded six-row (9,216 B packed) diagnostic can use the
existing gemv4 context's packed/norm pointers; a new arithmetic walker is not
inherently required just to change the memory address. Read the actual split:
pre/post each dispatch 4 rows, residual dispatches 16, rather than one 24-row
job. Include copy cost, norms, largest free block and boot/post-prime headroom;
11,419 B free is not proof that a 9,216 B allocation is safe. Stage/copy only a
bounded tile with unchanged assertions and fail safely if it does not fit. Warm
residency alone is an upper bound; dynamic copying must be charged. Do not reuse
the unsupported 54.5/45.5 split estimate. First ask which per-call row placement
could improve BOTH cores' critical path; asymmetric caching can simply idle one.

- **36-byte record is now measured and closed:** #868, 768 rows, 1,577,208 packed
  vs 1,598,440 record cycles = **1.35% slower**, mismatch=0. Prune old live advice.
  Sixteen records occupy nine lines, not eighteen; large-stream overhead is
  36/34-1 = 5.88%, not 2.12x traffic. No rerun without a distinct premise.
- Keep LUT de-split (#446/#813), private 64-B codebook (#808/#809), uint16 offset
  stream (#749), row-owned Kron fusion (#664), and unsupported 120 MHz closed.
- #849's instruction count is not a proof of maximum throughput. It admits
  load/dependency slack; dual-core wall throughput is not per-core IPC. But do
  not repeat old CQ2 layouts without a specific changed mechanism.
- Row-paired zcrms is not automatically ready: many call sites are sequentially
  dependent. Establish independent rows before proposing a paired implementation.
- Preserve EG2, compact prefixes, amortised loops, receiving-tree arithmetic and
  the frozen quality failures. No whole-file worker transplant for a small edit.

Next mentor: inspect whether all THREE distinct lanes became real jobs, then
read B2's cycle-only hits and exact device matrix differential, B1's actual linked
schedule, and B3's actual four-output reference. The key risk is narration and
repeated closure taking the place of experiments, not lack of baseline controls.
