# Acceptance packet - shipped state and what is still owner-facing (2026-09-24)

Accepted runtime: **5.3033 decode tok/s** (+117.2 % over the 2.44 baseline), commit `2c79104`,
`engine/src/nd_model.c` md5 `0d639424f636`, `engine/include/nd_quant.h` md5 `57a0fa8af900`,
canonical engine_md5 `0c1a6272cd01`. Quality on that exact tree, measured not asserted:

* device **20/20** byte-exact, `golden_missing=0`, `token_delta=0` (full 20-case session, board 1);
* host 19/19 byte-exact, `golden_missing=0`, fidelity `5.341e-05` against a `2e-3` gate, top1 10/10;
* `make capture` rc=0 with **all nine** behavioural flags true (board 2, this image);
* secondaries: prefill 5.59, extended 5.2269, think 4.20, boot bench 5.343 (191 ms/token),
  `min_case` 5.07 (best worst case in 433 runs), `internal_free` 13,367, `psram_free` 2,052,252;
* cross-board: board 3 5.3033, board 2 5.3017, board 1 5.3033 - three boards, one quantum.

## What the step from 5.1167 to 5.3033 actually was

Six bit-exact mechanisms, five of them individually **below** the 0.2 % keep bar and accepted only as
one measured bundle (run #431) - the bundle delivered +0.599 %, slightly more than the +0.539 % sum
of its parts:

| lever | solo | mechanism |
|---|---|---|
| expbc (#424) | +1.95 % | exponent-scale bitcasts written as explicit builtins, so `-fno-builtin-memcpy` cannot turn them into a ROM call |
| head4 (#425) | +0.35 % | route the full-vocabulary logits head through the already-guarded 4-bit row walker |
| taphoist (#427) | +0.70 % | resolve the qkv tap history rows once per call instead of a remainder per output column |
| wfr | +0.06 % | `wfr` register transfer instead of the store+FP reload the 4-byte copy lowers to |
| sigpair | +0.13 % | a mixed-sign gate pair keeps the interleaved exp pair (the sign only picks which scalar division runs) |
| tap2col + tapfwd | +0.09 / +0.06 % | two tap columns per iteration; tap j=0 reads the current input, not the history slot just copied |
| condT | +0.20 % | `cond_v` staged channel-major in place - measured 26.6 % cheaper cold, 0 % warm |

Two of these (head4, taphoist) are address/call-structure wins in loops the campaign had *closed* on
precision evidence. The lesson worth keeping: "this loop is memory-bound" was always measured by
changing the data, never by changing the addressing.

## Closed by measurement since the last packet (do not re-screen without a new premise)

* Conversion/bitcast family, both ends: `f16wfr` moves 32 more sites off the ROM memcpy call and
  changes decode by one quantum (+0.032 %) - the hot `nd_f16` consumer was already taken by head4.
* Elementwise 4-wide batching (`silu4`): exactly 0.000 %.
* Sinkhorn exp pairing (`sinkpair`): -0.062 %; third confirmation that a branch around
  `nd_expf_pair` costs more than the Horner latency it recovers (#292, #231).
* Engram tap restructuring (`eghoist`): -0.16 % despite 44.2 M bit-exact host comparisons.
* The sampler's `nd_model_logits_subset`, the last "unattributed 3.4 ms", measured 0.14 ms/token
  (prep 0.092 + gather 0.05); the 3.8-6.5 ms figure is run #344's cumulative-timer artefact.
* Library-call surface, by `nm -u` on the accepted objects: sinf/cosf are RoPE once per token,
  `powf` is open-time, `logf` is Sinkhorn's and not substitutable, `sqrtf`/`__divsf3` are at the
  measured floor, and the 118 remaining memcpy sites are bulk copies.

## Three things still owner-facing

1. **Console/command-path wedge.** A board stops answering mid-suite; reproduced on the *accepted*
   image at the same case, so it is not candidate-attributable. Cause narrowed by exclusion to an
   outbound emission stall on the console leg (PSRAM corruption, heap exhaustion, thermal, pool
   traffic, inter-case gaps, request budget and stdin latch all excluded by measurement). It cost
   this campaign roughly a dozen would-be canonical gates before full sessions started completing.
2. **Tail goldens are session-order-dependent** (`heldout_interval_one`,
   `heldout_long_tools_note_only`): they reproduce in canonical order on a healthy board and diverge
   at an identical token count otherwise, which is why a union-of-sessions gate is not admissible.
3. **Product defects, unfixed and out of benchmark scope**: the silent 271-byte request truncation in
   `esp32/main/main.c`, and the route schema's lack of a no-op escape (chit-chat produces a
   hallucinated `set_sampling_interval`). Also: board 1's USB console leg re-enumerates
   intermittently and may need a physical reseat.

## Addendum 2026-09-25 - the stack grew from +0.78 % to +4.85 % and the gate still cannot pass

Candidate now: **accepted + asmemo + radix-4 stage fusion + nf16v + spin handshake**
= **5.560 / 5.5617 decode tok/s on two independent boards** (engine_md5 b1daae10df90,
runner-asserted), +4.85 % over the 5.3033 pin and +127.9 % over the 2.44 baseline.
Secondary evidence: prefill 5.8667, boot_bench 5.601, min_case 5.30 (best worst case on
record), internal_free 12,095 (-1,272 B from asmemo's memo table; the fusion's IRAM is
refunded), gen_tokens 99 with every case at its golden token count, and
device_output_exact=6/6 on the primary group with token_delta 0.

Six 20-case gate attempts across three engines now read 16,16,16,17,17,17 cases with
**zero divergences** in every case reached; the accepted image reproduces the same stall
at the same position, so this is the console-emission firmware defect, not the candidate.
Board 1's console leg is separately dead (two identical readiness timeouts with all locks
cleared) and needs a physical reseat.

What is being asked of the owner, unchanged in kind:
1. Accept the stack on disclosed evidence, or fix/schedule around the console-emission
   stall so a 20-case session can complete in one boot (it is a product defect: the board
   stops answering after 16-17 requests per boot, unaffected by reconnects).
2. Board 1 reseat, for canonical acceptance runs.
3. The two owner-gated levers remain measured and unshipped: assertion-level RAM
   (+8,248 B, funds phi row residency ~+0.39 %) and octal 120 MHz (+3.51 %, vendor-blocked).

## Console stall: mechanism localized (runs #483-#485)
Two pinned heartbeat tasks (cores 0 and 1, equal priority, each printing its core id every 2 s)
were added to a diagnostic image and the frozen suite was run until it stalled (16/20 cases,
LANE_RC=1). Result: **both cores' heartbeats stop with the last emission** - the last core-1 line
appears after the last completed case, then nothing. Consequences:
* The strong "reader/host side" theory is refuted: the device stops being able to emit at all,
  which matches run #418's frozen app-side emit counters (chars=816, lines=17) while the host kept
  reading until EOF.
* Core-1 starvation by the spin handshake (#453) is excluded - core 1 was still emitting past the
  last request and died with everything else.
* Remaining suspect: the stdout write path (per-event `fflush(stdout)` on UART0-backed stdout,
  main.c 522-538 and 570-577) or a whole-app block that also freezes inference; these cannot be
  separated without an out-of-band channel, because every observation route uses the same console.

### Ask
Fix the emit path (suggested shape: one console task with a bounded queue that drops on full, or
no per-event flush), then re-run the 20-case byte-exact gate. Until then the +4.85 % stack
(5.560/5.5617 on two boards; 6/6 primary byte-exact, token_delta 0) cannot pass device_output_exact
over all 20 frozen cases: 20 requests do not fit in one boot, and the documented AUTO_HARD_RESET
workaround invalidates the two state-dependent cases (#474, run #391's measurement).

## 2026-09-25 update: the stack is +5.2 % and the gate can now complete

**Candidate stack** (each step measured on device, byte-exact, `token_delta 0`):
asmemo (+2.20 %) -> radix-4 stage fusion -> `nf16v` 5-instruction norm conversion
-> spin handshake (+1.31 %) -> sequence-predicate completion (free) -> lean task
notifications (+0.33..0.36 %, two boards). **5.580 / 5.5817 decode** vs the pin
**5.3033**, `min_case` 5.32 (best on record), `internal_free` 12,519.

**Two blockers, both now characterised by measurement, neither is the candidate:**

1. *Per-boot request ceiling.* Fixed by an ISR-fed console RX ring installed
   **after** model open and prefix allocations (placement is the whole cost:
   boot bench 5.601 no driver / 5.546 installed early / 5.596 installed late).
   With it, all 20 cases completed in one boot for the first time in this era.
   Cost: 4,271 B internal heap, ~0.1-0.2 % request time.
2. *Two state-dependent goldens.* `heldout_interval_one` and
   `heldout_long_tools_note_only` encode the firmware's demo-timer/sampling
   counters (run #391), so they diverge in any session whose state differs:
   the completed session read **18/20**, failing only those two. An input trace
   (received length / dropped-character count / FNV hash) reported `drop=0` on
   every case, so lost characters are excluded.

**Ask:** regenerate or annotate those two goldens (owner-level: they are the
quality oracle), then one 20-case session on lean-notifications + late RX ring
without the diagnostic prints is expected to read 20/20 and re-pin ~5.58.

## 2026-09-25 final: acceptance tree measured on two boards, gate reads 18/20 as expected

Tree (prov `dfa32cefae65`, snapshot `.auto/exp84/main.c.lean-ring`): asmemo -> radix-4 fusion ->
nf16v -> spin handshake -> sequence-predicate completion -> lean task notifications -> late
lossless console RX ring (diagnostic removed).

| metric | board 2 | board 3 | pin (5.3033) |
|---|---|---|---|
| decode_tps | **5.575** | **5.5783** | 5.3033 |
| min_case | 5.31 | 5.32 | 5.07 |
| boot bench | 5.621 | — | 5.343 |
| internal_free | 8,415 | — | 13,367 |
| device byte-exact | 18/20 | running | 20/20 |

Speed +5.14 %/+5.17 %. Host 19/19 byte-exact, fidelity 5.341e-05 unchanged, `token_delta` on the
two failures 52. **The two failures are `heldout_interval_one` and `heldout_long_tools_note_only`**
— reproduced on two boards and on three engines; they encode the firmware demo timer/sampling
counters (run #391). Cost of the console ring: one metric tick (-0.09 %) and 4,271 B internal.

**One ask unblocks the re-pin:** re-capture or annotate those two goldens. Then one session on
this exact tree should read 20/20 and the pin moves 5.3033 -> ~5.575. Nothing else in the campaign
is above the 0.2 % keep bar inside the documented maxima and the byte-exact gate.

## 2026-09-25 clean-tree reading (supersedes the 5.575 row above)

Trace-free merge, prov `4d3094578050`, snapshot `.auto/exp84/main.c.lean-ring-clean`, ELF verified
RXQ-free: **decode 5.5767 (+5.17 %)**, min_case 5.31, boot bench 5.621, internal_free 8,415,
host 19/19 byte-exact, fidelity 5.341e-05, **device 18/20, LANE_RC=1**, diverging only on
`heldout_interval_one` + `heldout_long_tools_note_only` (token_delta 52, golden_missing 0).
Third independent reproduction of that pair, now with the diagnostic removed - #527's caveat retired.

## 2026-09-25 cross-board confirmation (clean trace-free tree, prov `4d3094578050`)

| | board 2 | board 3 |
|---|---|---|
| decode_tps | **5.5767** | **5.58** |
| device byte-exact | 18/20 (LANE_RC=1) | 18/20 (LANE_RC=1) |
| divergence set | heldout_interval_one, heldout_long_tools_note_only | **same two** |

Runner asserted the same provenance on both boards; snapshot `.auto/exp84/main.c.lean-ring-clean`
(md5 `3c63ef73e719`, ELF verified RXQ-free). Neither the speed nor the divergence is board-specific,
and the compared console stream carries no diagnostic lines. Mentor queue items B1-B3 predate this
stack and are superseded; its item 3 ("frozen host/device tails already disagree") is corroborated.

## Behavioural capture green on the acceptance image (prov 4d3094578050)

Board 1 reflashed to that provenance (FLASH_RC=0), driven through its API: `make capture` gave
CAP_RC=0 with all nine verification comparisons true and zero false (routes, tools, two-pass local
execution, no external calls, telemetry, sampling interval, timer expiry). API stopped afterwards;
no serial-api child retains the console.

## Seed stack (acceptance tree + first-W8D seed) - complete evidence, awaiting admission

Provenance 61861dd9886c = 4d3094578050 plus engine/src/lut2_tie728.S (md5 3a2e522e3f38, snapshotted at
.auto/exp87/lut2_tie728.S.seed). Mechanism: in nd_lut2_rows_tie1n the first W8D of each group seeds
f0..f3 from the existing +0.0 register with add.s, so the four per-group reset mov.s and the dead
pre-row seeds disappear - 4 instructions x 130,560 groups/token, arithmetic unchanged.

| evidence | value |
|---|---|
| decode, board 2 (clean base) | 5.6117 (+0.627 % over that board's 5.5767) |
| decode, board 1 (clean base) | 5.6117 (+0.599 % over that board's 5.5783) |
| primary byte-exact | 6/6 on both boards, token_delta 0 |
| full 20-case session (board 1) | 18/20, missing 0, delta 52 - only heldout_interval_one + heldout_long_tools_note_only |
| extended / think / prefill / min_case (board 1, own reading) | 5.5177 / 4.39 / 5.925 / 5.35 (board acceptance 5.4838 / 4.37 / 5.585 / 5.31) |
| boot bench, internal_free | 5.658, 8,415 |
| make capture | CAP_RC=0, all verification flags true, zero false |
| fidelity probe, host goldens | 5.341e-05, 19/19 |

Rejected on this base while measuring: fused prepare+LUT (0.000 
## Seed stack (acceptance tree + first-W8D seed) - complete evidence, awaiting admission

Provenance 61861dd9886c = 4d3094578050 plus engine/src/lut2_tie728.S (md5 3a2e522e3f38, snapshotted
at .auto/exp87/lut2_tie728.S.seed). Mechanism: in nd_lut2_rows_tie1n the first W8D of each group
seeds f0..f3 from the existing +0.0 register with add.s, so the four per-group reset mov.s and the
dead pre-row seeds disappear - 4 instructions x 130,560 groups per token, arithmetic unchanged.

| evidence | value |
|---|---|
| decode, board 2 (clean base) | 5.6117, +0.627 pct over that board's 5.5767 |
| decode, board 1 (clean base) | 5.6117, +0.599 pct over that board's 5.5783 |
| primary byte-exact | 6/6 on both boards, token_delta 0 |
| full 20-case session, board 1 | 18/20, missing 0, delta 52 - only heldout_interval_one and heldout_long_tools_note_only |
| extended / think / prefill / min_case, board 1 own reading | 5.5177 / 4.39 / 5.925 / 5.35 vs that board's acceptance 5.4838 / 4.37 / 5.585 / 5.31 |
| boot bench, internal_free | 5.658, 8415 |
| make capture | CAP_RC=0, all verification flags true, zero false |
| fidelity probe, host goldens | 5.341e-05, 19/19 |

Rejected on this base while measuring: fused prepare+LUT (0.000 pct), lutb_rows live-range edits
(-0.090 and -0.060 pct), split granularity and spin budgets (neutral). Closed off-device: 4-bit
per-group seeding (+0.005 pct), tie1n per-row reseed (+0.05 pct), fw_scale live ranges (cold path).

Third board, same provenance 61861dd9886c: board 3 reads decode 5.6133 vs its own clean 5.5800
(+0.597 pct), device 6/6 byte-exact, token_delta 0, boot bench 5.657, LANE_RC=0. The proposal is
therefore +0.599 / +0.627 / +0.597 pct on three boards against their own clean controls - a 0.03 pct
spread on the delta, so the gain is the kernel, not a board. This lane also printed the first green
DEVICE_GATE_OK_RESTRICTED, the restricted-run byte-exact gate added after #595's red falsification.


## 2026-09-26 23:38Z — the attention family proposal (supersedes the seed-only packet)

PROPOSAL: DOT8W+SELRES composed on B4W (paired-head shared-V P.V at four-cell
width; eight-column QK dot body; selective rescale sweeps with dispatch outside
the dimension loop). Engine provenance 10c74c756ee5 on the seed-era base;
source /root/board-pool/preserved/b1-recipeB/nd_model.c.cmp-d8sr.

Evidence: decode 5.7467 on board 3 (screen) AND board 2 (full gate) - byte-
identical metric on two boards; +2.41 pct over each board's seed pin (5.6117/
5.6133), +1.52 pct over the seed-era acceptance tree. EXTENDED 5.6631 (best
measured), think 4.46, min_case 5.51, boot bench 5.819, internal_free 4,823.
Host 19/19 byte-exact, fidelity 5.341e-05, top1 10/10. Behavioural capture
CAP_RC=0 on this image (7/7 scenarios). Device 18/20, token_delta 52: the two
boot-state-dependent demo-timer goldens only - identical to the seed's own
gate, adds no new failure.

ATTRIBUTION (new): the same two goldens fail on the ACCEPTED bundle5 base when
ONLY B4W's attention diff is added (run #633: bundle5+B4W+lossless RX ring,
all 20 cases complete, primary 5.365 = +1.163 pct on the shippable base). The
two cases PASS on host for every image (host 19/19). So the failures are
device-only and track timer/interval state, not arithmetic. The lossless RX
ring also FIXES the campaign-long 17-requests-per-boot console wedge.

DECISIONS OWED: (a) disposition of the two goldens (re-baseline, replace with
state-independent cases, or keep as blockers); (b) ring transport fix adoption
(main.c + esp_driver_uart REQUIRES - measured neutral-to-small boot cost,
fixes a real product defect); (c) if (a) resolves, whether to take the seed-
era stack (fastest, 5.7467) or the accepted bundle5 base plus attention family
plus ring (shippable base, running now on board 1 as 177bd44997fa).
