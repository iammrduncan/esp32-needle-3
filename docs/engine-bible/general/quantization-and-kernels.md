# Quantization and kernel design

The engine's dominant operation is not a conventional dense FP matrix multiply.
Its weights are Cactus-Quantized and combined with a Walsh–Hadamard transform so
the runtime can operate on packed indices without materializing full weights.

## Cactus-Quants identity

For one quantization group, logical weights have the form:

```text
w = (codebook[index] * norm) @ H
```

where `H` is the orthonormal Walsh–Hadamard matrix. Because `H` is symmetric and
orthonormal:

```text
dot(w, x) = dot(codebook[index] * norm, H @ x)
```

The engine therefore transforms each activation group once, then walks compact
indices for every output row. It does not expand an entire weight matrix.

This changes optimization priorities:

- activation preparation is amortized across rows and sometimes projections;
- packed-index and norm traffic dominate large GEMVs;
- a faster integer MAC is irrelevant if unpack/expanded-byte traffic grows;
- cache-line utilization and traversal order can outweigh instruction count.

## Preparation path

`nd_cq_prepare()` zero-pads to the group boundary and applies the normalized
FWHT per group. Groups are independent and split across cores on ESP32. Q, K,
V, and attention gate share the same input geometry, so the forward pass builds
one prepared activation and one pair table for all four projections.

Retained FWHT work includes staged FP32 scales/factors, fused radix-4 forms on
candidate lineages, paired independent chains, and bounded unrolling. Several
apparently wider transforms failed because the reference was wrong, register
pressure grew, or a microbenchmark isolated a loop that was not the end-to-end
bottleneck.

## CQ2 pair LUT

At two bits, adjacent positions have 4 × 4 = 16 possible codebook-value sums.
For a fixed transformed activation, the engine builds 16 float partials per
position pair. The row walker then performs one indexed float load and add for
each pair, followed by group norm scaling and a fixed four-partial reduction.

For input width 768, the pair table is:

```text
768 / 2 pairs × 16 floats × 4 bytes = 24 KiB
```

It fits in the configured 64 KiB D-cache. This empirical fact explains why
two-row/cache-blocked walkers lost: they increased live accumulators or changed
stream geometry without rescuing a table that was already resident.

### C reference versus TIE728

The handwritten CQ2 kernel retained the same:

- nibble order;
- group/row order;
- four independent partial sums;
- FP16 norm interpretation;
- final fold order.

The first successful form processed one row with four partials and an eight-pair
gather batch. It saved roughly 30–33% of kernel cycles and improved full decode
about 13% on the then-current tree. Loading the next norm halfword early helped;
converting it early, deeper software prefetch, or processing two rows together
lost.

Later control-flow work converted the group branch to an Xtensa hardware loop
and then amortized long loop-register setup across rows. The amortized form was
confirmed on three distinct trees at +0.27%, +0.35%, and +0.34% end-to-end and
was bit-exact on real shapes and memory placements.

## CQ4 path

Four-bit weights are used for the tied embedding/head, mHC phi, and some gather
paths. Packed 32-bit reads cover eight weights. The retained target kernel's
main advantage was keeping the group loop, norm, and partial accumulators in one
out-of-line assembly schedule; forcing the C helper inline gave only a small,
sub-bar gain and consumed internal memory.

Wide TIE loads were not automatically correct. Synthetic probes with distinct
nibbles showed variants that appeared faster had collapsed nibble identity:
permutation-invariant probes passed, while position-sensitive expected sums did
not. This is why kernel validation needs hand-computable mapping patterns as
well as random numeric error.

## FP16 conversion

The portable fast path reconstructs an ordinary FP16 value's sign/exponent/
mantissa bits and falls back for exponent 0 or 31. Target code uses a register
transfer rather than a stack store plus float reload.

The CQ2 assembly later narrowed its eligibility further to positive ordinary
norms so the conversion could be reduced to a shift/add/reinterpret sequence.
Every halfword encoding was exhaustively checked against the real predicate.
Reducing ten instructions to five helped; reducing five to three did not move
end-to-end speed, demonstrating that instruction count alone is not a floor.

## Fast exponentials

`nd_expf()` range-reduces to base 2, evaluates a degree-5 polynomial, and
constructs the power-of-two scale by exponent bits. Attention evaluates two
independent exponents per adjacent KV pair. `nd_expf_pair()` interleaves the two
Horner chains but preserves operation order inside each chain. On captured real
arguments it reduced cycles per pair and delivered a measurable full-token win.

Branching around apparently special exponent inputs often lost. `exp(0)` skips
and Sinkhorn pairing saved arithmetic but added tests/control flow in loops where
the serial polynomial was already scheduled well enough. Measure the real input
distribution and branch cost.

## Kernel dispatch checklist

A target kernel needs all of:

1. exact geometry and alignment predicate;
2. numeric-domain predicate for any shortened conversion;
3. ABI layout assertions at compile time;
4. C fallback for every other case;
5. real linked-symbol and disassembly check;
6. synthetic mapping tests that expose stride/index errors;
7. bitwise differential over production shapes, row offsets, group counts, and
   relevant memory placements;
8. host and device model gates;
9. end-to-end timing on a non-profiled image.

Assembly bugs found during this campaign included an undersized `call8` frame
that corrupted return registers, double row-cursor advancement, wrong row-pair
stride, lost nibble identity, and a debug output pointer that corrupted the
reference. None is exotic; all are reasons to make the differential part of the
kernel, not an afterthought.

## Why several intuitive paths failed

| Idea | Outcome | Why |
|---|---|---|
| Quad table: one byte → four weights | ~38% slower kernel | Group-outer access consumed only half of each 64-byte cache line and damaged forward streaming. |
| Two CQ2 rows per table pass | Slower | 24 KiB table already fit; extra row stream/register pressure added cost. |
| 64-bit packed row loads | ~4.6% slower | Group address was 4 mod 8, splitting the access on LX7. |
| Deeper software prefetch | Slower | Hardware load queue already covered the index stream; prefetch spent issue slots/registers. |
| Expanded CQ2 indices / integer dot | Rejected | 4× or larger field traffic overwhelms saved extraction/MAC work on the measured bus. |
| FP32 norm sidecar | Closed by budget | +224 KiB/token stream cost exceeded the ~0.45M removed instructions. |
| Dense int16 PIE-style path | Inapplicable/poor transfer | No native 2-bit unpack; widening weights makes delivery dominate. |
| `-ffast-math` as a blanket fix | Rejected by contract | It can change the arithmetic graph and does not address the measured bandwidth-bound phases. |
| Cache blocking without measured reuse gap | Neutral/regression | Hot activation/table already fit; more accumulators or poorer row order dominated. |

## General transfer rule

Classify a kernel before optimizing it:

- **delivery-bound**: minimize bytes, maximize useful bytes/cache line, preserve
  monotonic streaming, stage only truly repeated streams;
- **latency-chain-bound**: introduce independent accumulators/chains without
  reassociating each output;
- **call/control-bound**: outline or inline only after inspecting the linked
  schedule, and consider hardware-loop setup amortization;
- **setup-amortized**: share preparation across projections/rows/tokens when
  lifetime and capacity allow;
- **synchronization-bound**: enlarge jobs or reduce joins, not merely move math.

The winning changes in this project came from correctly classifying the phase;
the losing changes usually optimized a cost the hardware was already hiding.

