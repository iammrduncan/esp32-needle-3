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
