# Experiment ledger

The authoritative complete history is [`.auto/log.jsonl`](../../../.auto/log.jsonl).
This appendix is its map, not a replacement: it explains what the records mean, marks
the accepted/candidate boundary, groups the full run range into research eras, and
points back to exact queries for every omitted row.

## Snapshot and cardinality

Observed on 2026-09-25 at 21:42 America/Chicago:

- repository `HEAD`: `82a3cf1551292c43007c245e016ce281c0a0f159`;
- 807 JSONL records total;
- two `type=config` records and 805 run records;
- runs are contiguous and unique from **#1 through #805**;
- status counts: **673 keep, 115 discard, 17 crash**;
- segment counts: segment 0 = 1, segment 1 = 765, segment 2 = 39;
- no run has a null status.

This was a live campaign. The queue had already acquired newer uncommitted operational
text while this snapshot was taken. Treat the numbers above as a reproducible checkpoint,
then query the current tail before using words such as “latest” or “final.”

## What a ledger row does and does not mean

Each run normally carries `run`, `commit`, primary `metric`, secondary `metrics`,
`status`, prose `description`, timestamp, segment, and sometimes structured `asi`
analysis. Two config rows delimit/restate the metric contract.

Three interpretation rules prevent most false conclusions:

1. **`keep` means keep the evidence, not ship the code.** Verification repeats,
   harness fixes, diagnoses, analyses, handoffs, and even a closed no-change probe can be
   `keep`. Read the description and provenance.
2. **The `metric` can be carried forward.** Off-device screens and documentation-only
   entries often repeat the accepted or candidate rate while explicitly saying that no
   device speed measurement occurred. A numeric field is not automatically a new result.
3. **A maximum metric is not an accepted release.** The frozen correctness gates,
   exact-tree provenance, host/device breadth, and owner disposition are separate axes.
   Candidate worker trees are not necessarily the main checkout or a reachable clean
   commit.

`crash` likewise means the attempted evidence path failed. It may identify a harness,
allocation, board-state, or source-edit failure rather than a kernel failure. `discard`
can mean a real regression, a measured null, an analytical closure, or a proposal refused
before device time. Preserve the reason.

## State at the snapshot

| State | Rate | Evidence and meaning |
| --- | ---: | --- |
| Original eight-layer port, run #1 | 1.2217 tok/s | First frozen request-path baseline; 12/12 device and 11/11 host in the original smaller suite. |
| Campaign comparison baseline, run #2 | 2.4417 tok/s | FP16-to-FP32 staging of multiply-read MLP factors. Later summaries use **2.44** as the campaign baseline. |
| **Owner-accepted pin**, run #431 | **5.3033 tok/s** | Commit `2c79104`, bundle5. Device 20/20, host 19/19, numerical fidelity and behavioural capture green. This remains the accepted reference. |
| Candidate tree C / board 1 | 5.9033 tok/s | Shippable line plus later transport/kernel components and amortised loop. Fully gated against the candidate-era suite, but 18/20 on device. |
| Candidate tree B / board 2 | 6.1250 tok/s | Seed line plus plain and amortised group loops and `qk_dot8`; full breadth and own-tree CQ2 differential. |
| **Candidate tree A / board 3** | **6.1433 tok/s** | Recommended proposal: composed attention, EG2, compact prefix, plain + amortised group loop, and `qk_dot8`; +15.8% over 5.3033 and +151.6% over 2.44. |
| Provisional tree-C descendant, run #805 | 5.9233 tok/s | Corrected B1 `qk_dot8` outline, +0.339% over B1's own 5.9033 pin. Host caught the first double-scale transcription (`0/19`); after correction the restricted device screen was 6/6 exact with zero token delta. **No full 20-case device gate is recorded yet.** |

All three candidate trees in the durable handoff have host 19/19, top-1 10/10,
`logit_max_delta=5.341e-05` against a `2e-3` gate, own-tree CQ2 bitwise evidence, and
device 18/20. The two failures are exactly `heldout_interval_one` and
`heldout_long_tools_note_only`, totaling `device_token_delta=52`. Run #647 showed that
a transport-only RX-ring change can flip exactly those cases with engine bytes held
constant; #718 exactly reproduced the changed `interval_one` answer on host and
showed the same repeated-tool tendency, but not byte-identical output, for the
long-tools prompt; #719 showed the device answers are deterministic and
order-independent within an image. That evidence localizes
the ambiguity but does **not** authorize silently changing goldens. The owner must
rebaseline/replace those cases, drop the ring and its >128-byte request fix, or block the
candidate.

The worktree `HEAD` is not synonymous with candidate A. The three measured trees were
parked in board-pool workers and exported under `.auto/exp*`; reproduce from their
recorded provenance and assets, not by assuming that checkout files form one candidate.

### Work active at the freeze

The same run #805 row also records a B2 experiment in flight: copy the immutable 64-byte CQ4 codebook into
aligned internal RAM and point only the 4-bit walker at it. It had built and flashed, but
the restricted screen had not returned. It is therefore **a running hypothesis, not a
result**. The B3 36-byte CQ2 group-record layout and a two-deep CQ4 codebook-load schedule
were queued substitutes, not implemented evidence. Do not attach a speed or correctness
claim to any of these until a later ledger row supplies it.

## Full chronology by era

Every run #1-#805 belongs to one of the ranges below. The anchors are selective; use the
queries later in this appendix for every individual record.

| Runs | Rate movement / milestone | What the era established |
| --- | --- | --- |
| **1-12** | 1.2217 → 2.6767 | Freeze the request-path baseline and goldens. Staging multiply-read FP16 MLP, conditioning, tap, norm, and engram operands as FP32 produced the largest single jump (#2, +99.9%) and then smaller real wins. The general lesson was reuse-aware conversion hoisting, paid for with PSRAM. |
| **13-29** | 2.7517 → 3.7567 | Pair attention softmax positions (#13), block Monarch/Kronecker work (#16-#22), widen int8 K/V reads (#23/#24), fuse the MLP residual scale (#25), and widen packed CQ2/CQ4 reads (#26-#28). Several `restrict`/generic-path changes were null, making live-dispatch proof mandatory. |
| **30-70** | 3.8183 → 4.1850 | Use the second core across Kronecker halves, gate, lane mix, FWHT, LUT build, attention gate, and selected projections (#30-#45). A PSRAM weight tier first paid at #49. By #70 the tree was at a stable plateau and repeated readings demonstrated near-deterministic timing. |
| **71-142** | 4.1850 → 4.7783 | Broad scheduling exploration, then the MimiModel transfer. Handwritten TIE728 CQ2 assembly delivered +13.0% (#137); one-row/four-partial scheduling added a smaller gain (#138); KV-pair staging across shared heads added +0.70% (#139). Row blocking and deeper prefetch variants lost. #142 closes the transfer queue on a verified tree. |
| **143-230** | 4.7783 → 4.9150 | The constrained sampler's first-byte legality table gained +2.16% (#147). A folded CQ4 table appeared positive (#159) but was structurally racy across two cores and was reverted (#162). Extensive tier, sampler, and request-path probes followed. Paired attention exponentials were kept at #229; dot-product rewrites were slower at #230. |
| **231-304** | 4.9150 → 5.0117 | Configuration and coverage work: 100 Hz tick (#240), compact grammar index (#288, +1.01%), 120 MHz memory as a real but unshippable diagnostic (#289, +3.51%), paired sigmoid (#290), and Sinkhorn max-exp skip (#291). IRAM, integer CQ2, GDMA, attention exp-zero skip, and several cache/memory ideas closed negative or below bar. Runs #296-#300 found and fixed golden/gate/config blind spots; correctness authority improved as much as speed. |
| **305-370** | 5.0117 → 5.0467 | Revisit memory residency and Xtensa kernels with stricter probes. CQ2 internal-vs-PSRAM delivery was measured (#330), 120 MHz was blocked by vendor timing-retune support (#331), and the plain-load CQ4 phi walker was integrated at #333 after two instructive wrong/null attempts. Division, logits-tier, and several element-loop variants closed; a FWHT rescale unroll survived at #362. |
| **371-423** | 5.0567 → 5.1167 | FWHT unroll, paired groups, and folded branch-free rescale produced the 5.1167 base (#378/#379/#395). Many adjacent transforms were null or negative. Provenance contamination and the 16-20-request console wedge repeatedly invalidated acceptance attempts; #399-#423 separate candidate effects from harness/session state and correct the base used by lanes. |
| **424-510** | accepted 5.1167 → **5.3033** | `expbc`, guarded 4-bit output head, tap hoists/forwarding, conditioning and related pieces compose into bundle5; run #431 is the accepted 5.3033 pin. `asmemo` is a large later discovery (#437), while wake/split/spin work measures real cross-core tolls (#444-#456). Heartbeat runs #483-#485 narrow the stall. Three-way worker merges are guarded rather than trusted (#509/#510). |
| **511-612** | proposal stack ≈5.58-5.61 | Build a faster discovery seed while retaining the 5.3033 owner pin. Notifications, spin/split refinements, composition, wider gates, and provenance/gate repairs move candidate trees above 5.6. Repeated inability to finish long device suites is treated separately from numerical correctness. |
| **613-650** | 5.6467 → 5.8050 proposals | Attention/load-form lanes, cross-board breadth, and staging helpers. The lossless RX ring fixes long input transport, but #647 proves the ring alone flips the same two frozen cases on an engine-byte-identical 5.3033 tree. This is the decisive accepted-vs-candidate boundary. |
| **651-725** | proposals 5.58 → 6.06 | Wide phi/P.V/Kronecker forms, composed attention, notification transport, and exact-tree gates. Load-form wins only when redundant delivery, independent accumulators, register fit, and a hot guard align. Host comparison (#718), state/order test (#719), and deferred-echo transport (#725) sharpen the frozen-pair evidence. Several attractive wide/tiled forms invert sign or diverge. |
| **726-786** | 5.83 → **6.1433** | Compact saved prefixes reclaim 768 KB (#748); byte-expanded CV3W and hot-loop peepholes close on bandwidth/instruction floors (#749/#750). Plain CQ2 group hardware loop reaches 6.0883 (#756); EG2 becomes a small positive only after compact-prefix allocation fixes (#767-#774). The amortised group loop is differentially proven (#776), kept on B2 (#777), transferred to B3 for the 6.1433 best (#781), and confirmed on B1 (#783). Breadth, host gates, per-tree differentials, and a fresh phase map close through #786. |
| **787-805** | best tree unchanged at 6.1433; B1 provisional 5.9233 | Evidence completion: owner packet, behavioural cross-check, all-tree CQ2 differentials, pool restoration, decision pages, capture on the exact best tree, deterministic soak, anti-overfit synthesis, durable handoff, and backlog pruning. #804 correctly rejected the old tail/ABI explanation for B1's divergent QK port; #805 found the actual double-scaling transcription, used the host gate to catch it before board time, and produced a restricted 6/6-exact +0.339% screen. It remains provisional pending full device breadth. |

## High-value wins and why they worked

| Mechanism | Representative runs | Why it paid |
| --- | --- | --- |
| Hoist repeated FP16 conversion into FP32 staging | #2, #3, #6, #8, #11 | The same small factors were reread many times inside hot loops. One open-time conversion trades abundant PSRAM for millions of device conversions and competing memory traffic. |
| Share loads across independent arithmetic | #13, #16-#24, #139, later load-form lanes | Pairing positions/rows/heads helps when a loaded operand feeds independent accumulators without changing each accumulator's operation order. |
| Packed CQ2/CQ4 reads and dedicated row walkers | #26-#28, #137/#138, #333, #668 | The hot C structure paid load/call/conversion overhead per tiny group. Assembly keeps partials and cursors resident and uses the LX7/TIE operations at the real shape. |
| Dual-core row splitting | #30-#45 | Rows/groups are naturally independent. It works when each half is large enough to amortize notification/wake cost and shared-bus pressure. Later threshold and toll measurements explain the exceptions. |
| Grammar indexing rather than cheaper rejection | #147, #288 | Most vocabulary pieces cannot begin with a legal first byte. Indexing visits only potentially legal IDs while preserving ascending-ID semantics and full validation of survivors. |
| Interleave independent transcendental chains | #229, #290 | Two independent Horner/sigmoid chains give the scheduler latency-hiding work while each chain retains its exact arithmetic order. |
| Hardware-loop control reduction | #756, #776/#777, #781/#783 | The dominant CQ2 walker was already near two instructions per weight; the remaining win was group-loop control. Amortising long-loop setup once per call saved work without touching math. |
| Capacity enablers | #748, #767-#781 | Compact prefix snapshots do not speed decode directly, but reclaim enough PSRAM for the narrow EG2 copy. EG2 then avoids the one engram K/V pair missed by the broad tier. |

## Failures and closed families worth carrying forward

| Family | Evidence | Closure or correction |
| --- | --- | --- |
| Folded shared CQ4 product table | #159-#162 | An apparent speed win used shared static scratch written by both cores. Host tests cannot expose that race; structural concurrency analysis overruled lucky green device timing and the change was reverted. |
| Alternative attention/QK dot schedules | #230, #669-#671, #779/#804/#805 | The compiler's ordered C dot is already strong; extra accumulators and wide loads often lose. The B1 outline's apparent tree-specific divergence was ultimately a transcription bug: helper outputs were scaled and the retained caller scaled them again. Run host goldens/fidelity after every C numeric rewrite; the corrected outline is only restricted-screen evidence until its full gate lands. |
| IRAM placement | #232 and later symbol audits | Most hot compute was already in IRAM. Moving a sampler wrapper gained below the 0.2% bar while consuming scarce internal RAM. Symbol placement must be proved in the ELF. |
| CQ2 integer rewrite | #286 | Failed the numerical stage before a speed screen. Integer-looking packed weights do not remove the exact FP scale/accumulation contract for free. |
| GDMA double buffering | #287 | Producer and consumer serialize on the shared octal bus; copying plus compute roughly adds rather than overlaps. Cache invalidation is both expensive and unsafe with flash-resident callbacks in the window. |
| 120 MHz octal memory | #289, #331 | +3.51% is real, but IDF cannot perform the required temperature timing retune for this flash part. Longer soaking does not repair unsupported calibration. |
| Assertion-level SRAM reclaim | #293/#294 | It frees useful internal RAM but changes failure semantics and is an owner tradeoff, not a free performance change. It can reopen phi residency only by explicit policy. |
| Phi/CQ2 residency | #295/#296, #330, later phase map | A reported 4.3× gap was a units error (aggregate core cycles versus two-core wall time). Real cold PSRAM tax exists, but the working set cannot fit current internal headroom; copying per token loses. |
| Software prefetch/cache configuration | #238, #296/#300 | 32-byte cache lines lose; the target lacks the hoped-for software data-cache prefetch path; relevant cache choices are constrained by S3/IDF configuration. |
| `logf(1)` and exp-zero shortcuts | #292, #299/`logfskip` | Hit rate times real phase cost is below break-even, and the attention exp-zero branch regressed. A mathematically special value is not automatically a profitable branch. |
| Split granularity, dynamic scheduling, spin budgets | #444-#456, #558/#564 | Cross-core dispatch costs roughly thousands of cycles and is state-dependent. Bracketing thresholds and spin budgets found no further robust win at the later stack. |
| Byte-expanded CQ2 streams / FP32 norm sidecars | #749, #785 | The octal bus is binding. CV3W multiplies packed index bytes; a norm sidecar adds 6.25% stream bytes for less instruction saving than delivery cost. Price bytes/token before coding. |
| Console/session stall theories | #400-#485, later soak | Candidate attribution and simple stdout-contention stories were refuted. Heartbeats/transport work localized the class; safe attach discipline and RX handling matter. The 10-request #797 soak is positive evidence, not proof that every longer session is impossible to wedge. |
| Missing-golden and unenforced device gates | #296-#300 | Several “green” paths could pass without a golden or without enforcing device exactness. The harness was repaired and deliberately falsified. Old results must be read in light of which gate actually existed then. |
| Worker/base contamination | #363, #399, #421, #748 | A bench transcription, a worker's own `HEAD`, stale object mtimes, or a keep commit made in the main workspace can all mislabel the code actually measured. Hash the engine in the worker, assert source differences and linked symbols, and export assets before reset. |

## Preserved experiment assets

At the snapshot, numbered experiment directories are:

```text
exp14 exp15 exp16
exp20 exp21 exp22
exp26 exp27 exp28 exp29 exp30 exp31 exp32 exp33 exp34 exp35 exp36 exp37 exp38 exp39 exp40 exp41
exp44 exp45 exp46 exp47 exp49 exp50 exp51 exp52 exp53 exp55
exp57 exp58 exp59 exp60 exp61 exp62 exp63 exp64 exp65 exp66 exp67 exp68
exp70 exp71 exp72 exp73 exp74 exp75 exp76 exp77 exp78 exp79 exp80 exp81 exp82 exp83 exp84 exp85 exp86 exp87
exp88 exp89 exp90 exp91 exp92 exp93 exp94 exp95 exp96
```

Absence of a number does not mean a missing ledger run; the namespaces are independent.
Some directories contain a README, others only source snapshots, patches, launchers,
compressed captures, or kbench fixtures. Some are deliberately empty/preserved markers.
Before applying an asset:

1. read its README if present and the corresponding ledger descriptions;
2. determine which worker/tree it came from;
3. verify the target path and live dispatch with source, ELF, or a proof print;
4. run the asset's differential/known-answer test on the receiving tree;
5. then run that tree's required host/device gates.

Named non-numbered research directories also preserve focused probes:
`attnzero`, `divf3`, `logfskip`, `rint`, `sigpair`, `sinkzero`, `vcond16`,
`vfuse`, `vsplit`, `vsplit2`, and `vwedge`. `golden`, `preserved`, and `runs`
hold quality/provenance evidence rather than candidate families. `tmp`, build caches, and
`__pycache__` are not durable research documentation.

## Query the raw ledger

Use `jq` against the JSONL itself; do not scrape this narrative.

Show the current tail:

```sh
tail -n 12 .auto/log.jsonl | jq -r '[.run, .status, .metric, .commit, .description] | @tsv'
```

Verify total/config/run counts, contiguity, and the current run range:

```sh
jq -s '{
  records: length,
  configs: ([.[] | select(.type == "config")] | length),
  runs: ([.[] | select(has("run"))] | length),
  min_run: ([.[] | select(has("run")) | .run] | min),
  max_run: ([.[] | select(has("run")) | .run] | max),
  distinct_runs: ([.[] | select(has("run")) | .run] | unique | length)
}' .auto/log.jsonl
```

Recompute disposition counts:

```sh
jq -s '[.[] | select(has("run"))] |
  group_by(.status) | map({status: .[0].status, count: length})' .auto/log.jsonl
```

Read an exact run or bounded era without loading the prose notebooks:

```sh
jq 'select(.run == 431)' .auto/log.jsonl
jq -r 'select(.run >= 613 and .run <= 650) |
  [.run, .status, .metric, .commit, .description] | @tsv' .auto/log.jsonl
```

Find every record that names a mechanism, including structured analysis:

```sh
rg -n -i 'amort|TIE728|RX ring|golden_missing|GDMA' .auto/log.jsonl
```

List all crashes without assuming they are engine crashes:

```sh
jq -r 'select(.status == "crash") |
  [.run, .metric, .commit, .description] | @tsv' .auto/log.jsonl
```

The ledger can advance between the count query and the tail query. For an externally
reported snapshot, record `git rev-parse HEAD`, the line count, max run, status counts,
and wall-clock time together; if they disagree on a subsequent read, label both
checkpoints instead of silently mixing them.
