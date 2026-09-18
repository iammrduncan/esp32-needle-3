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
- **Board power mode is a 2x decode lever.** The measured board idles in
  light-sleep at 80 MHz: 160/160/240 MHz gives 1.22/1.83/2.44 tok/s (816/546/
  406 ms per token, roughly 0.66/1.47/2.00x). The firmware's boot bench shows
  the same 816 -> 406 ms/tok. Confirm what the harness and the shipping
  firmware actually request before treating either number as the metric.

## Candidate optimisations (revised after the above)

- **Iteration speed (not the metric):** priming two schema prefixes at boot costs
  ~5 min per flash. Priming both prefixes concurrently on the two cores, or
  caching a primed prefix in flash, would roughly halve experiment latency
  without touching decode kernels. Worth it if the loop needs more samples/hour.
- **`make capture`** (7 real end-to-end scenarios + renderer verification) is the
  repo's own integrity check. Run it every ~10 kept changes to prove the board
  still behaves, not just that it is faster.

## Measured dead ends (fill in as found)

- (none yet)

## Measured dead ends (fill in as found)

- **fp32 staging of the Monarch Kronecker factors (w1a..w3b) + d2/b2/d3/d4.**
  Idea said ~1.6M `nd_f16` calls/token is the head. It is not, on device: the
  factors are 2KB each, permanently in the 64KB L1D, so the conversion is L1
  hits plus one F2F instruction. Host: 1.93s -> 1.55s (1.24x). Device: 816 ->
  406 ms/tok (2.01x) - which is impossible for ~4K ops/token and therefore is
  not this change. Revert; look at the cache configuration instead.
