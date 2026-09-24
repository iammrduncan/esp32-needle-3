# Acceptance packet - unshipped measured wins (2026-09-24)

Accepted runtime: **5.1167 decode tok/s** (+109.6 % over the 2.44 baseline), tree 018427c
(`nd_quant.c` md5 `cc174624959b`, canonical engine_md5 `cc046f59204e`). Quality on that exact
tree: device 20/20 byte-exact with `golden_missing=0`, host 19/19 byte-exact, token_delta 0,
fidelity 5.341e-05 (gate 2e-3), top1 10/10, `make test` green, internal_free 12,855.

## Candidate A - radix-4 stage fusion + cold-path refund: +0.58 %

* `engine/src/nd_quant.c` only. Transform walks four butterfly stages fused; the field-dead
  fallbacks (`fw_scale`, `nd_fwht2`, `nd_fwht3s`) dropped `ND_HOT` and became `noinline`, which
  refunds the whole 1,024 B of internal heap the fusion initially cost (run #404: internal_free
  back to 12,855, identical to accepted).
* Bit-exact by proof, not by argument: 1,472,640 comparisons against the exported `nd_fwht`
  across n=2..1024 and five scales incl. 0 and negative; odd-split guard 6 tensors x 5 splits;
  host 19/19 byte-exact; fidelity identical.
* Speed: **5.1467 on nine agreeing readings, three boards**, prefill 5.4367, think 4.01,
  boot bench 5.194 (193 ms/token), min_case 4.91, gen_tokens 99, token_delta 0.
* Transform family is closed on every axis (width 1/2/3/4, stage fusion 2/3 pairs, radix-8
  triples, pass count 4/3, rescale placement peeled/folded/merged, stage order, call structure).

## Candidate B - Candidate A + condT (channel-major cond_v): +0.78 %

* Adds `engine/src/nd_model.c`: cond_v staged channel-major **in place** (zero memory cost -
  the alternative that builds a second table needs 196 KB against 24,192 B of steady-state
  PSRAM headroom and fails `nd_model_open`).
* Mechanism measured, not assumed (kbench, board 1): cold, strided 116,534 vs channel-major
  85,499 cycles = **26.6 %**; warm, 55,400 vs 55,423 = 0.0 %. Each tensor's cond_v is touched
  once per token, so the field is the cold case. Predicted +0.53 %, delivered +0.19 % on the
  fusion base (= +0.78 % over accepted, combining with A).
* Device differential proves the shape: fill/read/split mismatches 0/0/0, including the 4+4
  two-core split that the host's `rows_serial` structurally cannot produce.
* Reading: **5.1567 on two independent boards, identical to the digit**, prefill 5.45,
  min_case 4.92 (best worst case in 420 runs), zero divergences in every case reached.

## The only blocker: a firmware/console defect, not the candidates

A full-suite run needs 20 device cases in one attached session. Twelve attempts across three
engines and three boards reached **16,16,16,16,16,17,17,17,17,17,17,20** cases - P(a complete
session) ~ 1/11 - with **zero divergences in every case ever reached**. The accepted image
reproduces the same stall, so it is not candidate-attributable. Measured properties:

* Instrumented console reader shows the app's own line counter frozen while its `getchar()`
  EOF counter climbs, i.e. bytes stop arriving, while 16 cases had been answered and reached
  the harness: an outbound/console-leg emission stall.
* `clearerr(stdin)` does not restore service; reconnects and fresh attaches do nothing; only a
  chip reset does (observed directly in run #391).
* Excluded by measurement: PSRAM corruption (CRC stable, `bad=0` all session), heap exhaustion,
  thermal (32-35 C), concurrent pool traffic (sole-board test still stalled at 16), inter-case
  gaps of 60 s and 120 s (both measured, non-monotonic), request budget (1200 s timeouts).
* `psram_free` steady at 24,192 B from case 7 onward is the model's normal steady state, not a
  leak - and it independently explains why PSRAM ECC never fit and why a 14 MB tier span never
  booted.

## What is being asked

1. Accept A (and optionally B) on the evidence above, or move the device byte-exact gate to a
   union of sessions - which requires deciding the order-dependence of the two tail goldens
   (`heldout_interval_one`, `heldout_long_route`), because they encode live timer/session state.
2. Own the console wedge as a product defect (it also bounds real deployments: the board stops
   answering after ~16-20 requests per boot).
3. Rig reseat for board 1: its UART console leg re-enumerates mid-session (nodes re-created
   01:36/02:22, no STATE answered), so it cannot take request-driven work.
4. Two product defects already reported and still open: silent request truncation at 271 bytes
   (`ND_LINE_MAX`), and the schema's lack of a no-op escape, which turns chit-chat into a
   hallucinated `set_sampling_interval` call.

Closed and not worth reopening: 120 MHz (vendor refuses the timing retune on this flash model,
`ESP_ERR_NOT_SUPPORTED`), assertion-level RAM (its only buyer, phi residency, is closed), and
wide TIE float loads (measured -107 % and +0.000 % in the field).
