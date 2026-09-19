# Ideas backlog

Ranked by expected payoff per unit of risk. Delete entries as they are tried.

## Candidate optimisations

- **Monarch MLP: hoist the fp16→fp32 conversions out of `kron_apply`.** Each of
  the three `kron_apply` calls per layer re-reads `a`/`b` once per inner
  iteration, so ~65K `nd_f16()` calls per kron (≈1.6M per token) when only 1024
  distinct values exist. Stage the converted factor into a small scratch buffer
  (or transpose it once) and the arithmetic becomes a plain 32×32 matmul. Pure
  algebra-preserving change; check bit-exactness (accumulation order changes, so
  verify against the logit-fidelity probe).
- **Sinkhorn budget.** `ND_SINKHORN = 20` iterations of exp/log on a 4×4 matrix,
  twice per row/column pass — ~10K `nd_expf` + ~1.3K `logf` per token. Either
  early-exit when the max row/col residual is below a fixed epsilon, or replace
  the exp/log pair with a scaling-only (non-log-space) Sinkhorn. Both change
  numerics; only acceptable if the fidelity probe and goldens stay exact.
- **4-bit pair-LUT for the mHC phi GEMVs.** The phi tensors go through the
  generic 4-bit `dot_group` (2 mults/weight). A 16-entry-per-position table
  (`cb[i] * xh[j]`) turns them into loads+adds. Table is in_pad×16 floats
  (~48 KB for the 3072-wide phi reduction) — probably too big for the remaining
  37 KB of internal RAM; test whether a half-width table (per group, 16 KB)
  beats the multiply path anyway.
- **Attention: int8 dot product.** The KV dot product converts every int8 to
  float per element. Accumulating in int with the LX7's 32-bit ops, or reading
  four int8 per 32-bit word (with per-head scale applied once), shortens the
  inner loop. `qk_head_dim` is 48, so 12 words per head instead of 48 bytes.
- **Prepare/LUT reuse across the block.** `attention` prepares + builds a LUT for
  q/k/v/gate, then again for out_proj; `nd_cq_prepare` runs an FWHT over the
  whole padded activation each time. Check whether out_proj's in_pad differs
  (768 vs 768) and whether one FWHT can serve both.
- **tap_projection** calls `nd_f16(weights[...])` inside the inner loop over
  taps — small, but it is per-element per-projection; hoisting is free.
- **`zcrms` / `rms_unit` / lane mixing loops** are all elementwise over 768 or
  3072 floats with a scalar loop; the ESP32-S3 GCC may vectorise with
  `-O3 -ftree-vectorize` if the pointers are restrict-qualified. Try adding
  `restrict` to the hot elementwise kernels (zero numerical change).
- **Confidence pooling** (`pool_cell`) runs per layer per token over
  n_probes × d_model. If this blob carries the head, it is pure overhead for
  decode speed; measure `ND_P_CONF` before assuming it is small.
- **Dual-core coverage.** Only GEMV rows and attention heads are split. The MLP,
  sinkhorn, tap projections, rope and engram conv run single-core. A coarse
  split (core 1 runs the MLP of the previous lane-mix stage while core 0 streams
  weights) is complicated; a simple one may be to overlap the *out_proj* GEMV
  with attention tails.
- **DISPROVEN, and it nearly cost the run #2 win.** A note here claimed the
  board downclocked to 80 MHz and that 816 -> 406 ms/tok was a power-mode
  effect. It is not: `esp_clk_cpu_freq()` reports 240 MHz and the xtal 40 MHz
  for both binaries, PM is disabled, and the same 2x appears from a flash A/B
  of the two images. The 2x is the fp32 MLP-factor change (run #2).
  Lesson: measure the clock and diff the binaries before believing any
  "impossible" speedup is an artefact - and revert accidental `cp`s of a
  baseline file into the tree (that is what produced the phantom 1.22).

## Candidate optimisations (revised after the above)

- **Iteration speed (not the metric):** priming two schema prefixes at boot costs
  ~5 min per flash. Priming both prefixes concurrently on the two cores, or
  caching a primed prefix in flash, would roughly halve experiment latency
  without touching decode kernels. Worth it if the loop needs more samples/hour.
- **`make capture`** (7 real end-to-end scenarios + renderer verification) is the
  repo's own integrity check. Run it every ~10 kept changes to prove the board
  still behaves, not just that it is faster.

## Measured dead ends (fill in as found)

- **fp32 staging of the Monarch factors is NOT a dead end: it is run #2,
  +99.9% decode** (1.2217 -> 2.4417 tok/s). It sits in "what worked" now. What
  is worth recording here is the *method* mistake: a host ratio of 1.24x made
  the 2.01x device number look impossible, so it was written off as an
  artefact, and a stale baseline copy left in `engine/` then made the device
  appear to agree. Both binaries were later flashed back-to-back at a reported
  240 MHz: BASE 816 ms/tok, CAND 406 ms/tok. Host speed is not a proxy for
  device speed in either direction. The earlier tap_projection part of this
  entry remains genuinely untried (see below).

## Measured dead ends (fill in as found)

- **fp32 staging of the Monarch Kronecker factors (w1a..w3b) + d2/b2/d3/d4:
  NOT a dead end - it is run #2, +99.9% decode.** Recorded here because the
  first reading (host 1.24x, device 2.01x) looked too good for ~4K ops/token
  and was written off. The device number is real: both binaries flashed
  back-to-back, cpu_hz=240 MHz reported by both, boot bench 816 -> 406 ms/tok,
  text byte-identical. Host and device diverge because on x86 the 2KB factors
  live in L1 and the only saving is the F2F instruction; on the S3 the fp16
  loads plus per-element conversion compete with the streaming activation.
  Lesson: do not predict device gains from a host ratio, in either direction.
- **4-bit row norm hoist: neutral.** Same change that was a free refactor in the
  2-bit LUT path, applied to `gemv_rows_offset`/`gemv_rows_generic`: boot bench
  370 vs 370 ms/tok, images flashed back to back. Norm conversion is off the
  critical path in both flavours.
- **Packing the 2-bit row walker into 32-bit loads: broken, reverted.** Trying
  to read two packed bytes as one word changed results (the LUT index is
  per-pair, and the byte order is not what the shift made it), so this is not a
  safe one-liner. The row bytes are only 32 per group anyway.
- **Sinkhorn 20 -> 6: vetoed, not a dead end for a re-tuned budget.** Costs a
  real logit change (max_delta 7.8, top1 6/10) for +0.8% decode. If a future
  session re-derives a convergence-tested iteration count that keeps the probe
  bit-exact, the win is there; 6 is not it.
- **Attention: 4 KV positions per online-softmax iteration is SLOWER than 2.**
  2.56 tok/s vs 2.7933 control on board2 in the same batch (-8.4%), byte-exact.
  The quad block spills off the LX7's register file; pairing is the sweet spot.
- **2-bit LUT GEMV: two rows at a time is slightly slower.** 2.7733 vs 2.7933
  (-0.7%) on board2, byte-exact. The GEMV is flash-bandwidth bound, not issue
  bound, so a second row stream competes for the same bus instead of hiding
  latency. Row-loop unrolling in this kernel is not the lever.
- **Constrained-logits gather: norm hoist is a small real win (+0.18%).** Kept
  (13373e7); byte-exact, and it lifts the think path with the primary metric.
- **Two dead-end families, so far, on top of the fp32 staging wins:** arithmetic
  hoisting inside the GEMVs (3 nulls) and loop unrolling in the GEMVs (2 nulls).
  Everything that has moved the needle changed *what is read* or *how many times*
  a value is touched, not how the inner loop is scheduled.
- **kron_apply first-half blocking is capped at 4 rows.** 8 rows: 2.995 vs
  3.1717 control (-5.6%). Second half caps at 8 columns (+0.9% over 4). The
  LX7 register file wants 4 accumulators when the reuse is in `a` and 8 when it
  is in the loaded `c` value. Do not widen the first half again.
- **Engram-gate RMS fusion (3 passes -> 1): neutral** (3.125 vs 3.1267) and it
  quietly breaks the n1 buffer contract (n1 must still hold the attention input
  when block() reaches the attention sub-block). Not taken.
- **kron_apply is saturated.** Measured optimum: first half 4 rows x 2 j-columns
  (8 accumulators), second half 8 columns with paired b rows. Wider on either
  side regresses: 8 rows -5.6%, 4k x 4j -1.2%. The lever was always *operand
  load reuse*, not accumulator count alone - 8 rows added accumulators without
  reuse and lost. Stop tuning this kernel.
- **Blocked attention-gate sigmoid: exactly zero** (3.1817 vs 3.1817). 768
  elements once per layer is below the noise floor. Small elementwise loops are
  not worth ILP work on this board.
- **Six more nulls on the main board or in verified parallel batches:**
  FWHT 8-butterfly unroll, straight-line n=4 sinkhorn, kv_slot hoist out of the
  attention inner loop, attention-gate sigmoid blocked by 4, LUT builder 2-wide,
  restrict on rms_unit/zcrms. All byte-exact, all within +-0.05%. Instruction
  scheduling and loop-overhead removal are DONE as levers on this firmware: at
  3.76 tok/s every phase is now either bandwidth-bound or latency-bound in a way
  the scheduler cannot fix. Only structural changes remain (core-1 coverage of
  the serial stage, or fewer bytes per token).
- **Process rule learned the hard way:** a parallel batch whose board1 control
  did not match HEAD's last measured value was silently stale (cp-based resets
  instead of git). From now: `git fetch && git reset --hard FETCH_HEAD` before
  every batch, and verify the control's decode_tps equals the last logged value
  before trusting any candidate delta.
