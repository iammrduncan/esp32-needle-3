# Needle 3 mentor queue

Mentor refresh 2026-09-25 04:23 UTC. All three new screens are COMPLETE; there
is no reason for the researcher's eleven-minute sleep. Keep three different
lanes moving. Preserve dirty work/snapshots, locks, repeat guards, frozen inputs,
quality gates and 240/80 MHz. Only the researcher implements/flashes/measures.

## Pins and fresh results

**Accepted stays 5.3033 tok/s**, bundle5 `2c79104`, device 20/20.
Existing faster proposals are not accepted: bundle5 + attention/staging/ring/spin
was 5.5500 on B1 (full, #660), 5.5483 on B3 (screen, #659); seed-era composition
+ staging is 5.8083 B2 / 5.8050 B3 (full #650), ext 5.7331, think 4.51.
Both full proposals complete 20 cases but match **18/20**, token delta 52.

Fresh logs under `/root/board-pool/batches/`, read directly at 04:22:

| Lane / source signature | Decode | Verdict and evidence |
|---|---:|---|
| B1 `M-kron-b1`, `b422e7717527` | 5.5467 | Kron fusion NULL versus B1's 5.5450 screen / 5.5500 full pin; 6/6 exact, delta 0, prefill 5.8333, min 5.34, heap 5927 |
| B3 `M-notif-b3`, `4a3aa059b8ed` | **5.5850** | **+0.661% vs own 5.5483**; 6/6 exact, delta 0, prefill 5.8767, min 5.38, boot 5.669, heap 6351; preserve this discovery pin |
| B2 `M-alt-b2`, `7cb9b4f03f34` | 5.8067 | Register-renaming NULL vs 5.8083; 6/6 exact, delta 0, prefill 6.1167, min 5.58, heap 5087 |

B1 host checks: 19/19, fidelity 5.341e-05, top1 10/10, CHECK_RC=0. All three
logs end DEVICE_GATE_OK_RESTRICTED exact=6/6. These primary screens do not
supply extended/think results or settle the two frozen-case blockers.

## Next three lanes

**B1: one cross-board FULL gate of B3's winning scheduler transfer.**
Preserve the null Kron source; remove only that hunk to the saved attention/
staging pin, retain the completion predicate fix, and port the proven B3
scheduler region. This is the deliberate confirmation exception: new +0.66%
primary gain plus outstanding frozen cases justify ONE cross-board/breadth run.
Use existing checks before measure; preserve every frozen case. If 18/20 recurs,
it remains a failed gate, not acceptance. No repeated confirmation loop.

Why this transfer mattered: B1/B3's #658 spin port omitted #492/#498. They still
had binary semaphores and a one-shot `if (s_done_seq != job)` wait. A worker can
publish completion, be delayed before giving the semaphore, and let that old
give satisfy the next job's one-shot wait. Predicate rechecking is required.
B1 now has the while fix; B3 now has B2's lean notifications plus while predicate.
#498 previously won +0.33..0.36% on another base. This new result is a transfer,
not a spin-budget sweep. Do not attribute the combined delta solely to the
notification API without a predicate-only comparison. Completion is the sequence
predicate on EVERY wake; notifications merely wake. Keep publication/order,
one outstanding descriptor and stack-context lifetime intact.

**B2: implement the ACTUAL load-before-consume schedule.**
The measured patch alternates a15/a9 and f12/f15, but EVERY `lsi` is still
immediately followed by its consuming `madd.s`. It did not increase load-use
distance. Thus 5.8067 closes register renaming, not the scheduling hypothesis.

From the saved 5.8083 kernel: extract/address/load cb0 into f12;
extract/address/load cb1 into f15; madd partial0 using f12;
extract/address/load cb2 into f12; madd partial1 using f15; continue and drain
last value. The next independent load precedes the previous value's use.
Same eight products, four partials, within-partial FMA order and final fold;
ordinary lsi, unchanged cursors/word count/eligibility. f15 is free; a9 is dead
after LOOP has latched its count. Inspect the emitted body to verify real
separation, then exact multi-row/known-answer differential and a bounded cycle
comparison before normal primary timing against B2's own 5.8083. A null here
retires this schedule; no renamed equivalent forms.

**B3: independently test correctly ordered, aligned 128-bit activation loads.**
Preserve its 5.5850 scheduler winner first; change only the load mechanism and
compare against that own-board pin. No seed-era quant-stack transfer needed.

NEW EVIDENCE invalidates #333's wide-load closure:
- `.auto/exp22/kbench_phi22.c:186` initializes A's ucb/urow ONCE before the
  m=1..3 loop, then overwrites both with all-one B data inside it and never
  restores A. Later modes' A=4080 is the correct answer to their actual input;
  it is not evidence of lost nibble identity.
- Current `esp32/main/dot4_tie728.S` lists wide-load destinations f4,f5,f6,f7
  (pair f4,f5), opposite Espressif's f7,f6,f5,f4 (pair f5,f4).
- The earlier unit-test uxh lacks explicit 16-byte alignment. Its 63-vs-64
  result is not an alignment-independent falsification.

Make ONE independent wide-load candidate from the CURRENT proven multi-row
kernel: replace eight scalar xh loads + increment with two correctly ordered
128-bit loads. Require 16-byte xh alignment and keep scalar fallback for other
pointers; the 512-byte group stride preserves alignment. Packed/codebook loads,
FMA order, norm gate and row cursors stay unchanged. Reset A AND B fixtures
independently for EVERY variant. Require aligned real-row and row-range
bit-exactness, then cycles and normal quality gates. Do not reuse exp22's stale
.S snapshot (its addx4 operands predate the fix), force misaligned accesses,
or inherit the old +25% claim. This is a new premise, not a blind repeat.

## Shared diagnostic corrections / reserve

E49's former mismatch was a harness overrun: kb_row4 added the source row to an
already-offset output; it wrote through index 303 into 192-float arrays.
The researcher fixed that, norm placement and exponent shift after mentor review;
an empty-file objdump comparison was also retracted. Corrected 04:05 run matches
192/192 and R=1/2/4/8 ranges. It supplies correctness evidence, not a new decode.

E49 warm 3,764,168 / cold 3,822,181 cycles are NOT a field ceiling. xh used
ND_ALLOC (PSRAM), while field m->xh is ND_ALLOC_FAST (internal SRAM). Eviction
preceded the entire ~300 KB sweep, not each field layer call; both sweeps can
already stream cold weights. The raw increment is 1.54%, not exactly 1%.
evict was read uninitialized (sink=2147483647): use initialized bytes and an
integer/volatile sink. Correct placement/sink IN candidate comparisons, not
another baseline-only run. Field work is 8*(4+4+16)*3072 = 589,824 weights,
4,608 groups/token; #585 missed factor eight, but its four-reset-instruction
seeding idea still has small headroom and stays down. Per-weight load scheduling
is different; cold/warm deltas do not bound its headroom.

If an implementation lane blocks, one finite quality-enabling substitute is
post-parser length/hash + token IDs of the two failing queries, compared with
frozen input, plus restored-prefix identity. Reuse a diagnostic hook; no harness
expansion, pacing change, oracle recapture or unchanged full-gate loop. #647
bundle5 + ring-only shows that transport change is sufficient to trigger the
pair; it does NOT establish a timer-to-model causal path. run_inference consumes
query/phase/restored prefix, while router dispatch follows generation. Existing
RXTRACE showed only 7/8-byte control commands, not the failing prompts. Host
19/19 cannot waive the device gate; admitting speedups needs preserved quality.

## Close these forms; retain the method

Kron row-owned fusion is now null: it kept the 4x2/8-column bodies, removed
24 inter-half joins/token and had disjoint actual input/output. Do not expand
into arbitrary MLP/permutation fusion from this null. Prior closures: cursor
#648 null; pv_pair2 noinline #649 negative; staging width4 #653 null; QK width12
#654 negative; rescale sweep4 #655 negative; both-rescale merged/split #663 null
with both flags 2.36%. #652 cold fallback carried pvni residue, so it is not a
clean universal code-size bound. Keep #624 PV8, #626 normalization pairing,
#628 QKTILE2, #634/#636 counted loops, twice-negative rescale/PV fusion,
#644 nonpositive exp and #645 paired rescale exp down. No extra clocks,
approximate math, FP16 taps or big KV mirror (runtime free PSRAM ~40 KB).

Snapshot/hash the actual worker base before editing; verify nonempty linked
candidate code and its dispatch. Use existing wrappers, checks and repeat guard.
Primary-only ext/think=0 mean unmeasured. Judge liveness by real child processes
AND growing logs; known completed primary screens take minutes, not a guessed
11-minute sleep. Take the documented screen-to-full allowance only when needed
for a specific justified gate. Do not leave free boards waiting on result prose.

Research anchors: [Cadence LSI/MADD.S semantics](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf),
[Espressif S3 dot kernel: register order/alignment](https://github.com/espressif/esp-dsp/blob/master/modules/dotprod/float/dsps_dotprod_f32_aes3.S),
[FreeRTOS notifications](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/01-Task-notifications).
No target-specific stall count or external speedup is assumed; preserve Needle's
reduction graph rather than copying ESP-DSP's different fold.

Next mentor: inspect B1 full gate, B2's actual load-use separation, and B3's
fresh-fixture/alignment proof. Check whether the two frozen failures acquired
real input/state evidence. Accepted remains 5.3033; discovery pins 5.5850/5.8083.
