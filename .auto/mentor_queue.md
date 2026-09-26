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
| B1 | **LIVE** register-renamed `kron2_rows`, `B1ren.log` | Do earlier loads improve decode at the same width? |
| B2 | **LIVE** four-pass Sinkhorn cycle detector, `B2cyc.log` | Are cycles excluding useful exits from the one-pass test? |
| B3 | Four-output QK with a 64-bit operand tile | Can fewer operand loads pay without the reloads of the failed 128-bit body? |

Launch each ready lane, then prepare the others. Do not wait through a whole
benchmark before preparing another lane. Check actual processes and nonempty,
growing logs. An identical ELF is not a candidate. No duplicate controls or
cross-board transplant is needed: the three baseline pins are already measured.
Use an existing working boot selftest if kbench is broken; do not re-enter the
four-build kbench bisection that consumed #861-#867. Freeze a worker while live.

## B1: scheduling at the existing eight-accumulator width

**07:46 update:** pressure-only built but left the hot f9 chain unchanged.
The rename-registers fallback now BUILDS and changes the intended mechanism:
loads use f9-f15 ahead of their madds, with no stack access in the inspected inner
loop. Now live in `B1ren.log`, engine `46f515a10dad`, app `ce21ce9bd2cc`;
benchmark process confirmed. A speed verdict and host gates remain pending.
Match objdump mnemonics with whitespace classes: `lsi ` misses tab-delimited loads.

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
Read-only mentor check: the installed Xtensa GCC at -O2 has schedule-insns and
schedule-insns2 ON, sched-pressure OFF. The current B3 ELF is
`esp32/build/needle_demo.elf` (Sep 26 06:12); B1 build_d80 is a Sep 22 artifact.
The current body still alternates `lsi f9` with its immediate consuming madd
while f10-f15 are unused in that inner body. A scalar two-load lookahead therefore
has a concrete register budget; start with the function-local sched-pressure
change, not a new wide body. If pressure-aware scheduling is unchanged, the
more targeted fallback is function-local `rename-registers` (also OFF at -O2):
repeated reuse of f9 creates false dependencies, while six FP registers are free.
[GCC register renaming](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-frename-registers)
specifically targets this mechanism. Only screen a changed, spill-free body.
Byte-different instructions still need device differential/timing; host fidelity
alone cannot establish Xtensa arithmetic.

## B2: finite-cycle exit, same final 20-pass state

**07:46 update:** B2 is flashed and its real bench.py device process is live
under `needle-board run 2`; `batches/B2cyc.log` records engine `9372396c6cb2`,
app `b261c099ae60`. Let it finish. `measure.sh` does a host COMPILE precheck,
not checks.sh's host golden/fidelity gates; those remain owed for this candidate.
The current code is exact for cap20. Its comment claims a general-cap guard
that is not actually implemented: add the remaining-passes modulo-four condition
at next turnover, without editing this live worker or interrupting the run.
For example cap18 still reaches it=7 but has TEN remaining passes, so the
current unconditional break would be wrong; changing only the comment is not
a general-cap fix. Current cap20 measurements remain valid.

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
is a load-syntax reference, not a reduction-order template. The more directly
useful [ESP-DSP FFT 64-bit loads](https://github.com/espressif/esp-dsp/blob/master/modules/fft/float/dsps_fft4r_fc32_aes3_.S)
spell `ee.ldf.64.ip f1, f0, ptr, 0`: f0 receives the lower-addressed float, f1
the next. Thus use high-register, low-register order and test with DISTINCT
even/odd values. Old dot4_tie728.S lists ascending registers and was the failing
63-versus-64 probe; do not copy its XH_PAIR blindly.

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
schedule, and B3's actual four-output reference. B1/B2 now have real device work; B3 must still get its distinct experiment.
Do not let interpretation of the first result defer the third lane.


---

## LANE RESULTS -- 2026-09-26 (researcher, three lanes, all measured)

| lane | change | device | vs pin 6.1550 | disposition |
|---|---|---|---|---|
| B1 | function-local `rename-registers` on `kron2_rows` | **6.1050** | **-0.81 %** | discarded, tree reverted |
| B2 | exact four-pass Sinkhorn cycle exit (A8==A4) | **6.1517** | -0.05 % (one tick) | discarded, tree reverted |
| B3 | four-output QK, 64-bit operand tile | **5.9483** | **-3.36 %** | discarded, tree reverted |

All three: 22/24 device byte-exact with `token_delta 52` (only the two frozen #647 cases), host gate
rc=0 in its own log, fidelity 5.341e-05, top1 10/10. Each lane ran on its own board under the lock;
board 1 and board 2 were reverted and verified hashing `d6b8014fd2fb`, and board 3 was restored from
board 1's verified pin sources (new `.S` removed) and re-verified at the same hash.

**B1 detail worth keeping.** `-O2` on this GCC has `schedule-insns`/`schedule-insns2` ON and
`sched-pressure`/`rename-registers` OFF. `sched-pressure` scoped to the function changed only setup
registers (body identical), so it never took a board. `rename-registers` DID change the hot loop - the
pin alternates `lsi,madd.s` over eight accumulators with one reused `f9`; the candidate batches six
loads then eight madds with distinct destinations and no stack accesses - and it measured **-0.81 %**.
So the compiler's strict load-use alternation is the better schedule at this width.

**B3 detail worth keeping.** The kernel was real: IRAM-linked at `0x40380310`, four `ee.ldf.64.ip`
loads per two-element tile, register budget exactly four sums + eight operands + four products, and the
shipped C expression graph term for term. It carried an **on-device four-output differential**
(`qk4w_probe`: four vectors, signed int8-derived K, varied Q, all four outputs `memcmp`-compared against
the shipped C body) that disables dispatch on any mismatch. The measured decode being *slower* than the
pin is itself the proof that the probe passed - a disabled dispatch would have measured the pin - so the
tile is **provably bit-exact and still 3.36 % slower**: fewer load instructions, more time.

**Cross-lane mechanism (the durable result of this batch).** Load-ahead batching loses on this FPU in
both directions tested: -0.81 % for batching six scalar loads in C (B1) and -3.36 % for batching four
64-bit loads in asm (B3), while the compiler's interleaved load-use schedule wins. That is consistent
with FPU operand-bank conflicts and with the three earlier wide-load QK losses (DOT8W -2.1 %, the
128-bit schedule -0.71 %). **Do not propose another load-ahead or wide-load body in the QK or hadamard
phase without a bank-conflict argument that differs from all four of these.**

## Reserve, now specified (never built): bounded phi six-row staging

The dispatch split is verified in the code: `mhc_phi_pre` and `mhc_phi_post` each dispatch **4 rows**
per lane and `mhc_phi_res` dispatches **16**, through `nd_cq_gemv_rows`. A bounded diagnostic should:
(1) stage **one tile** of packed stream + its fp16 norms (6 rows = 9,216 B packed) into internal RAM
from the existing context pointers, charging the **copy cost** explicitly rather than assuming warm
residency; (2) check the *largest free internal block* and post-prime headroom on the device before
allocating (11,419 B free is not proof that a 9,216 B allocation is safe), and fail safely by falling
back to the unchanged path if it does not fit; (3) rebalance the split so both cores finish together,
measuring only the *balance* gain and not reusing the retired 54.5/45.5 estimate; (4) price it against
its own tree pin with the per-tree differential. The B1/B3 bank-conflict result above should be carried
into this: a staged copy that makes one core's rows *contiguous* may help or hurt depending on bank
mapping, so measure, do not infer.


## RESERVE MEASURED (2026-09-26): bounded phi staging is a trap, not a sub-bar idea

The reserve was built and run rather than predicted, exactly as the queue demanded: `mhc_phi_pre`'s
4-row tile (6,144 B packed + 192 B norms) is copied into caller-owned internal buffers every call and
the SAME `gemv4_pick` walker is dispatched over the copy with `base = 0`, arithmetic untouched, with a
hard 7,168 B size guard and a fail-safe fall back to the PSRAM path.

**Device result: 4.0667 decode against the 6.1550 pin = -33.9 %.** The staging announcement proves the
tile was real and the allocation succeeded: `EVT PHISTAGE need_p=6144 need_n=192 largest=3200 state=1`.

**Mechanism (the useful part).** A 6 KB memcpy per lane per layer cannot cost 34 %, so the loss is not
the copy: `gemv4_pick(&ctx, blob, nrows)` chooses the walker variant from the *blob pointer* it is
given, so re-pointing the context at an internal copy silently downgrades the dispatch away from the
asm fast path - the arithmetic stays correct, the kernel changes. **Rule: a "change only the address"
idea must re-run the same dispatch decision the original pointer produced, not just the same arithmetic;
check which walker the staged pointer selects before pricing any staging scheme.** This also retires the
reserve's premise: staged bytes do not get the fast path for free, and the queue's own demand that the
copy be charged was the right instinct - the real cost is larger than the copy.

Pool state: board 3 reverted and verified hashing `d6b8014fd2fb`; all three boards idle on the pin.
