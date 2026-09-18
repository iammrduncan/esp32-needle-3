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
- **Iteration speed (not the metric):** priming two schema prefixes at boot costs
  ~5 min per flash. Priming both prefixes concurrently on the two cores, or
  caching a primed prefix in flash, would roughly halve experiment latency
  without touching decode kernels. Worth it if the loop needs more samples/hour.
- **`make capture`** (7 real end-to-end scenarios + renderer verification) is the
  repo's own integrity check. Run it every ~10 kept changes to prove the board
  still behaves, not just that it is faster.

## Measured dead ends (fill in as found)

- (none yet)
