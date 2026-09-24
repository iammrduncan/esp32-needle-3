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
