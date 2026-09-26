# Numerical correctness and equivalence

Performance work on this engine is accepted only inside an explicit numerical
contract. “Looks right,” low average error, a green host build, and even an
exact kernel microtest are different claims. The campaign uses them as stacked
evidence rather than interchangeable proof.

## Define the equivalence class first

Every change belongs to one of three classes:

1. **Bit-exact transformation.** Integer decode, lookup ordering, or data
   movement must produce identical result bits for the tested shape.
2. **Numerically bounded transformation.** Floating-point reassociation,
   approximate nonlinear functions, or changed reduction order may differ
   within a named tolerance and must retain top-1 behavior.
3. **Behavioral change.** A changed oracle, prompt schema, context rule, or
   transport-visible state is a product decision, not a kernel optimization.

The class determines the gate. It must not be chosen after seeing the result.

## The gate stack

| Layer | Proves | Does not prove |
|---|---|---|
| Scalar/reference unit test | Mathematical interpretation and edge cases | Target assembly, caches, concurrency |
| Kernel differential | Exact or bounded output for enumerated shapes/alignment | Integrated call shapes or dispatch |
| Host golden, 19/19 | Whole-engine behavior on the host fixtures | Xtensa path, PSRAM, UART, dual core |
| Logit probe | Maximum delta and top-1 on frozen checkpoints | Long autoregressive stability |
| Device golden, 20 cases | Integrated target output and transport for fixtures | Semantics outside the fixture set |
| Behavioral capture, 9 flags | Product routing, pass count, tools, state, no remote calls | Broad language quality |
| Repeat soak | Determinism and liveness over repeated requests | Every reset/order/context history |

At the accepted `2c79104` pin the recorded gates were device 20/20, host
19/19, maximum logit delta `5.341e-05` against a `2e-3` limit, and top-1
10/10. The later 6.1433 tok/s candidate retained host, fidelity, top-1, its
own-tree CQ2 differential, behavioral capture, and repeat soak, but is 18/20 on
device because two frozen cases changed after a lossless RX-ring transport
change. That distinction is why it remains a candidate rather than silently
becoming the accepted pin.

## Floating-point traps

Addition is not associative. These seemingly harmless changes can alter token
selection:

- splitting a dot product between cores and combining partials differently;
- changing loop unroll or accumulator count;
- fusing scale or norm multiplication at a different point;
- staging FP16 values through FP32;
- replacing `expf`, sigmoid, reciprocal, or square root;
- traversing sparse/legal vocabulary IDs in another order.

A small logit delta is useful evidence, but the dangerous value is the margin
between the winning and runner-up legal tokens. Always record top-1 alongside a
norm/error statistic.

## Quantized-kernel proof

For CQ2, the target kernel differential should cover actual model dimensions,
all relevant row/tail shapes, alignment classes, and both worker-core row
ranges. The campaign's preserved sweep reports, for the 768-wide case,
`kernel=tie1n exact=768/768 bitexact=1`. This proves the tested kernel output;
it does not prove that the production ELF calls that kernel. Dispatch logging,
symbol/disassembly inspection, and an integrated measurement close that gap.

For CQ4 and other bounded paths, compare against the correct format-level
reference. A synthetic reference that decodes the packing incorrectly can make
the optimized implementation look wrong—or, worse, make the same mistake and
look exact.

## Stateful output and oracle hygiene

An exact-output fixture is a tuple, not just a prompt:

`(model bytes, engine image, prompt bytes, prefix/cache state, sampler,
 grammar/schema, transport, reset/order policy)`.

The two disputed held-out cases are instructive. A pure UART RX-ring change,
with engine bytes unchanged, flips exactly those cases; host and device agree
on the resulting output; the result is deterministic and order-independent
within an image. The evidence weakens an arithmetic-bug theory, but it does not
authorize rewriting the golden. The owner must decide whether the old fixture
encoded an unintended request-state behavior, the new transport changes the
product contract, or the cases remain release blockers.

## Harness failures the campaign corrected

- A missing golden once counted as exact instead of failing closed.
- Restricted device runs once reported speed without enforcing exactness.
- A generated/synthetic reference was assumed correct before independent
  validation.
- A green host result was overextended to target-only assembly and dual-core
  behavior.
- A new object in the build was mistaken for proof that its symbol survived
  link-time garbage collection.
- A compile definition typed at CMake was mistaken for proof that it reached
  the translation unit.

The durable rule is to make absent evidence fail visibly. Print the compiled
mode in the binary, inspect `compile_commands.json`, inspect the linked ELF,
and require every expected fixture to be present.

## Promotion record

For each candidate record:

- exact commit/tree and binary/model hashes;
- build flags and target memory/clock configuration;
- workload and reset/order policy;
- reference implementation and its revision;
- kernel differential dimensions;
- host and device case counts, including missing cases;
- max/mean logit error, tolerance, and top-1 count;
- deterministic repeats and end-to-end product checks;
- every failure, even when unrelated to the optimized arithmetic.

This turns “quality is unchanged” from a narrative into a reproducible claim.

