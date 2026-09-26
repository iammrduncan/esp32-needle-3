# Needle 3 model pipeline

This page describes the model actually loaded by this repository, not a generic
Transformer and not the full upstream checkpoint.

## Sliced archive geometry

The checked model reports:

| Field | Value |
|---|---:|
| Archive size | 16,155,796 bytes |
| Tensors | 245 |
| Layers | 8 |
| Vocabulary | 8,192 |
| Model width | 768 |
| mHC lanes | 4 |
| Query heads / KV heads | 12 / 2 |
| Q/K head width | 48 |
| V head width | 64 |
| Query projection width | 576 |
| K / V projection width | 96 / 128 |
| Attention output width | 768 |
| Runtime context cap | 384 tokens |
| Archive KV-window field | 256 |
| Runtime K/V allocation | `max_seq_len` (384) slots/layer |
| Hadamard MLP padded width | 1,024 = 32 × 32 |
| Engram sites | layers 4 and 7 |
| Engram orders | 2 and 3 |
| Engram tables / slots / sub-width | 6 / 18,432 / 128 |
| Engram convolution | 4 taps, dilation 3 |
| RoPE theta | 100,000 |

The distinction between header `kv_window` and the sliced runtime
`max_seq_len` matters. This port allocates and indexes the cache with
`max_seq_len`; attention additionally protects a pinned prefix and recent ring
capacity. Do not assume a field name from upstream is the allocation used by
this C port—trace the binding.

## Token entry and mHC lanes

The tied embedding row is scaled by `sqrt(d_model)` and broadcast into four
lanes. For each layer, multi-head hyper-connections (mHC) compute:

```text
nx       = RMSUnit(flatten(lanes))
h_pre    = sigmoid(a_pre * (nx @ phi_pre)  + b_pre  + selected-lane offset)
h_post   = 2 * sigmoid(a_post * (nx @ phi_post) + b_post + selected-lane offset)
h_res    = Sinkhorn(a_res * reshape(nx @ phi_res) + b_res)
u        = weighted sum of lanes by h_pre
y        = block(u) - u
lane'    = h_res @ lane + h_post * y
```

`phi_*` are CQ4 projections. Sinkhorn is performed in a numerically stable
log-normalization sequence. The lane design increases state and small-matrix
work, but the final token representation is the mean of the four lanes followed
by the final norm.

The selected-lane offsets are architecture semantics, not optimization
constants. Reassociating the lane RMS or reduction changes floating-point order
and was correctly treated as a quality-changing experiment.

## Engram injection

Before the layer block at configured sites, the engine derives hashed n-gram
indices from recent token ids. For each order and head it:

1. hashes the token sequence with fixed seed/prime constants;
2. selects one row from the site's quantized table;
3. dequantizes only that row into one sub-vector;
4. concatenates the selected sub-vectors;
5. projects the result into an engram key and raw value;
6. applies a dilated causal tap convolution to the value history;
7. gates the value into `u` using normalized key/input similarity.

This explains two otherwise confusing facts from the performance work:

- an engram table can be several megabytes while a token gathers only a few
  rows; staging the whole table is usually the wrong bandwidth target;
- the engram key/value projection tensors are streamed in full and can benefit
  from a copied memory tier or narrow staging.

The final candidate's EG2 mechanism stages only the second engram site's K/V
pair in a narrow PSRAM copy. It was positive but below the nominal keep bar on
its own; compact prefix storage reclaimed about 768 KiB and made that staging
fit safely.

## Gated grouped-query attention

For each layer:

1. Normalize the block input with ZC-RMS.
2. Apply one FWHT preparation and build one CQ2 pair table.
3. Reuse that table for Q, K, V, and attention-gate projections.
4. Apply three-tap causal convolutions to Q/K/V histories.
5. Normalize Q and K per head and apply RoPE.
6. Quantize K/V symmetrically to int8, with one scale per KV head/vector.
7. Attend with online softmax over pinned sink positions plus recent ring
   positions.
8. Gate the concatenated attention values and apply the CQ2 output projection.
9. Normalize and add the gated attention residual.

Twelve query heads share two KV heads, so six query heads reuse each K/V row.
The retained implementation stages an int8 K/V position pair to float once per
KV group, then evaluates all of that group's query heads. Parallel units remain
query-head ranges so the two cores each receive enough work.

The online softmax keeps a running maximum and denominator; when a larger score
arrives it rescales the accumulated V output and denominator. Positions are
visited in the same order as the reference. Adjacent positions use an
interleaved `nd_expf_pair()` that preserves each scalar Horner chain's operation
order while exposing instruction-level parallelism.

### KV representation

The cache stores int8 vectors plus float scales instead of full float vectors.
The code uses the same clamp and `lrintf` rounding as the reference. A later
optimization replaces per-element divide with a Markstein residual correction:
one reciprocal, a multiply, an FMA residual, and an FMA correction reproduce the
rounded quotient for finite values; non-finite inputs retain the divide path.

Prefix sink slots are never overwritten. The remaining slots form a ring. A
prefix is clamped so at least 64 recent positions remain available.

## Monarch Hadamard MLP

The MLP pads the 768-vector to 1,024 and uses three Kronecker-structured stages
with 32×32 FP16 factors, learned diagonals, two permutations, conditioning, and
a SiLU gate. Conceptually:

1. compute eight conditioning logits from `cond_v`;
2. softmax and fold them through `cond_u` into a scale row;
3. apply first Kronecker transform and permutation;
4. diagonal scale/bias and conditioned scale;
5. apply SiLU;
6. apply second Kronecker transform and permutation;
7. diagonal scale;
8. apply third Kronecker transform;
9. scale back to width 768 and add the residual.

The 32×32 factors are small but originally converted from FP16 repeatedly
inside nested loops. Staging these multiply-read factors as FP32 at open was the
largest single improvement in the campaign: 1.2217 → 2.4417 tok/s. The general
lesson is to count **reads per distinct element**, not just tensor bytes.

The retained C loops use several independent accumulators to break serial FMA
dependency chains while preserving each output's summation order. Larger blocks
eventually spill or add strided streams and lose.

## Final head and sampling

After the eighth layer, the engine averages lanes, applies the final norm, and
returns a hidden vector. The embedding tensor is tied as the vocabulary head.
During constrained generation, only rows for legal token ids are gathered and
dotted. Full 8,192-row logits are used for unconstrained reasoning or when the
legal set exceeds the bounded subset buffer.

The archive may carry newer probe/confidence tensors, but this port's current
header explicitly describes the confidence representation as legacy/incomplete;
product APIs return `null` when the compatible head is absent. Do not claim
calibrated confidence merely because extra tensors exist.

## Persistent state and prefix caches

A complete reusable prefix contains more than K/V:

- K/V vectors and scales;
- position and pinned-sink count;
- q/k/v tap histories;
- engram token and value histories;
- optional confidence pooling state.

Two schemas therefore require two independent caches. Restoring only K/V would
produce a subtle cross-schema state leak. Compact-prefix experiments reclaimed
memory by storing only state actually needed for a prefix, but every omitted
field had to be justified and both schemas tested.

## Numerical seams to protect

- per-output accumulation order in CQ and Kronecker kernels;
- running-softmax score order and rescale placement;
- int8 K/V rounding and clamp behavior;
- Sinkhorn iteration/order;
- FP16 edge cases in assembly eligibility;
- tokenizer id order and argmax tie-breaking;
- state restored by prefix switching.

An optimization may be mathematically equivalent and still change greedy output
through floating-point rounding. “Same formula” is not the quality contract;
the defined gates are.

