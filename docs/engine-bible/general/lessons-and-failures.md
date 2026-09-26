# Reusable lessons, successful mechanisms, and dead ends

This chapter extracts the parts of the Needle campaign that transfer to other
inference engines. It deliberately separates a mechanism from the ESP32-S3
instruction or memory detail that happened to implement it. Platform-specific
traps are recorded in [the ESP32 failure chapter](../esp32/lessons-and-failures.md).

The headline lesson is simple: optimize the resource the complete request is
actually waiting for. The campaign's durable gains came from reducing streamed
bytes, repeated setup, address calculation, call boundaries, and cross-core
coordination while preserving the model's arithmetic graph. Many intuitive
failures made arithmetic cheaper in a phase that was waiting on data, or made a
microbenchmark faster by changing conditions that do not exist in inference.

## What worked, and why

### Turn algebra into reuse before tuning instructions

Cactus Quants moves the Walsh–Hadamard transform from each logical weight row to
the activation:

```text
dot((codebook[index] * norm) @ H, x)
    = dot(codebook[index] * norm, H @ x)
```

That identity makes activation preparation reusable across all output rows. In
Needle, Q, K, V, and the attention gate also share an input geometry, so one
prepared activation and one CQ2 pair table serve four projections. This is a
larger and more dependable win than shaving a few instructions from each row.

The general method is:

1. write the exact operator identity, including scales and normalization;
2. identify values invariant across rows, heads, or sibling projections;
3. compute them once at the narrowest valid lifetime;
4. make ownership explicit so reuse does not create shared mutable state;
5. confirm that the reused object fits the intended memory level.

This premise reopens whenever a model gains another projection with the same
input, or a format change makes a previously expensive common preparation
small enough to retain.

### Stage small, repeatedly consumed operands

The first large campaign gain staged repeatedly read FP32 MLP factors, moving
decode from 1.2217 to 2.4417 tok/s. Later wins reused the same pattern at finer
granularity: resolving engram tap history rows once per call, memoizing assembly
eligibility, staging conditioning data channel-major, and selectively staging a
hot engram tensor. These worked because a small object was read many times and
its staged lifetime was bounded.

A staging proposal should state:

```text
source bytes per request
+ copy bytes per request
+ destination capacity and lifetime
- avoided source reads per request
```

Staging is not inherently good. It wins only when avoided delivery or address
work exceeds the copy, competes safely for memory, and does not merely duplicate
an object that was already cache-resident.

### Optimize addressing and control even in a delivery-bound loop

Calling a loop “memory-bound” does not mean its code is untouchable. The
campaign initially closed several loops after data-layout experiments failed,
then found real gains by changing how the same bytes were addressed:

- route the full-vocabulary head through an existing guarded row walker;
- hoist invariant tap-history row resolution;
- memoize a per-call dispatch/eligibility walk;
- replace hidden memory-based bitcasts with register transfers;
- amortize loop setup across rows;
- remove or defer console/progress work outside the measured critical path.

These mechanisms leave payload size and arithmetic order unchanged. They reduce
the work wrapped around a stream. The reusable question is not only “can the
data move faster?” but also “how much scalar work is paid per cache line, row,
or call?”

### Break latency chains without reassociating each result

The successful paired exponential evaluates two independent Horner chains in
parallel while retaining the operation order inside each chain. Likewise, the
CQ2 row walker uses several independent partial accumulators but preserves the
reference fold order. This exposes instruction-level parallelism without
changing the final floating-point expression.

Use this pattern when:

- there are independent outputs or independent terms already present;
- each chain's operation order can remain identical;
- live values fit the register file;
- the final reduction can match the reference exactly.

It stops working when extra accumulators spill, two outputs compete for a
delivery stream, or the apparent independence requires a different reduction
order.

### Fuse adjacent stages only when lifetime and pressure improve

Transform stage fusion and elementwise clustering produced small but repeatable
wins on the candidate trees. The useful fusions removed a pass, a boundary, or
temporary traffic. A fusion is worth considering when it reduces at least one
of:

- full-buffer reads and writes;
- repeated scale application;
- loop/call setup;
- cross-core synchronization;
- live-buffer lifetime.

It is not enough that two loops are adjacent in source. Fused prepare-plus-LUT
variants were neutral on the final base, and wider transforms sometimes raised
register pressure. Reopen a rejected fusion when the receiving tree changes
buffer placement, call count, compiler schedule, or the number of values that
remain live.

### Treat buffer lifetime as a performance lever

Reusing `nx` storage for `xh`, removing an unnecessary dequantization row copy,
and narrowing the cached prefix representation returned internal or external
memory without changing model output. The direct speed gain can be modest, but
free capacity enables a later hot-object placement and reduces allocation
risk. Lifetime analysis should therefore track both time and the option value
of reclaimed memory.

A safe reuse proves:

- the last reader of the old value precedes the first writer;
- aliases are visible in the API or asserted locally;
- asynchronous workers cannot still own the buffer;
- cached prefix, KV, convolution, and engram state remain independent where the
  product requires independent sessions or schemas.

### Optimize the actual sampling path

The grammar first-byte index was a real decode win because it reduced the
candidate vocabulary before subset-logit work. The sampler improvement was
initially invisible to a boot benchmark that stopped before grammar sampling.
This is a general warning: inference ends at the selected token, not at hidden
state or logits.

Profile and gate all of:

- grammar-state transition;
- vocabulary filtering;
- subset versus full-head projection;
- logit post-processing;
- deterministic or stochastic selection;
- detokenization and stop detection.

When constrained output permits it, pre-index vocabulary metadata by cheap
grammar predicates, then calculate logits only for surviving tokens. Keep a
full-vocabulary fallback and test empty/singleton/large subsets.

### Make the test harness capable of failing

Several of the most valuable changes were to the evidence system rather than
the engine. The campaign added missing-golden accounting, enforced exactness on
restricted runs, protected the primary metric definition, tested unseen
prompts, added prefix-isolation coverage, and introduced anti-repeat/provenance
guards. Each caught a class of false success.

Before trusting a gate, inject a controlled defect and observe it fail. At
minimum, prove that the harness rejects:

- a missing reference output;
- a one-token output change;
- a numeric delta over the fidelity bound;
- a prefix-state leak;
- a target-only kernel error;
- a reused or stale binary presented as a new experiment.

### Let small wins compose, but remeasure the composition

The accepted step from 5.1167 to 5.3033 tok/s bundled several individually
sub-threshold mechanisms; together they cleared the keep bar. Later tree A
combined attention, transform, scheduling, buffer-lifetime, and kernel-control
changes to reach 6.1433 tok/s. Bundling is legitimate when every member is
mechanistically understood and the bundle is gated as a new tree.

Composition is not additive. Register pressure, cache occupancy, task timing,
and compiler decisions change with the receiving tree. A result on one lineage
is evidence for the idea, not proof for another lineage. The noinline QK dot is
the clearest example: it won on the seed line and diverged when ported to the
shippable line.

## Dead ends and their changed-premise tests

“Rejected” below means rejected under a measured premise, not forbidden
forever. Reopen only when the stated premise changes.

| Family | What happened | Why it failed | Legitimate reopening condition |
|---|---|---|---|
| Wider or expanded quantized operands | A uint16 offset stream and integer-dot variants lost their economic case before or during measurement. | They increased the dominant streamed bytes to save arithmetic in a delivery-bound loop. | The format becomes smaller, the weights move to a much faster memory tier, or a fused hardware decode consumes packed codes directly. |
| FP32 norm sidecar | Closed by budget: about 6.25% more stream bytes cost roughly 5.1 ms for about 1.4 ms of instruction saving. | It traded cheap conversion for expensive bandwidth. | Norms become resident/reused across many calls, conversion becomes dominant, or memory bandwidth rises enough to reverse the bound. |
| Quantized/compact pair table | It targeted a table that already fit in cache and changed products. | It saved the wrong bytes and violated byte-exact numerics. | The table no longer fits, a new quality contract permits the error, and end-to-end profiling shows table traffic rather than weights is limiting. |
| Two-row/cache-blocked CQ2 walkers | Slower than one-row execution. | The hot pair table already fit; another row added a stream and live accumulators. | Table reuse becomes a real miss source, the register file grows, or weights are reorganized so two rows share delivered cache lines. |
| Quad lookup table | Roughly 38% slower in the kernel. | Group-outer traversal used only part of each cache line and broke useful forward streaming. | A new layout packs all consumed entries contiguously or hardware gather changes the line-utilization equation. |
| Deeper software prefetch | Regressed. | Existing hardware queues covered the stream; prefetch consumed issue and register resources. | Latency rises, the stream becomes less predictable to hardware, or a non-blocking prefetch instruction with measured lead-time benefit exists. |
| 64-bit/wide loads by width alone | Regressed or produced wrong nibble mappings. | Misalignment split loads; other variants lost position identity despite passing permutation-insensitive probes. | Alignment/layout is changed and a position-sensitive mapping differential proves every lane. |
| Blanket fast-math/compiler flags | Rejected as a contract and mechanism. | They can change the arithmetic graph and do not target the measured delivery bottleneck. | The product explicitly adopts an error budget, complete model-quality gates pass, and disassembly shows a useful change in a compute-bound phase. |
| Special-case branches around `exp` | Multiple neutral or negative results, including attention `exp(0)` and Sinkhorn pairing. | Branch/control cost exceeded avoided polynomial work for the real input distribution. | Captured inputs show a much higher special-case rate, branch prediction changes, or a branchless select becomes cheaper. |
| More accumulators/wider second transform | Negative twice in register-heavy loops. | Register capacity, spills, and serial dependencies outweighed nominal parallelism. | More registers, shorter live ranges, a different ABI, or a new decomposition reduces simultaneous state. |
| Forced inlining | A 4-bit helper gained about 0.10%, stayed below the bar, and cost internal memory. | Saved call work was too small relative to I-cache/internal-RAM cost. | Call frequency rises, code placement is no longer constrained, or the compiler schedule demonstrably improves. |
| Cross-operator asynchronous overlap | Collided with the existing row splitter and shared scratch assumptions. | The proposed work was not independent at runtime; task/semaphore costs and nested parallelism erased overlap. | Operators have disjoint buffers and workers, measured slack exceeds synchronization cost, and nested-job exclusion is enforced. |
| Full-buffer copy for “locality” | The direct cached stream beat copy-then-consume. | The copy and consumer shared the same constrained path, adding traffic rather than hiding it. | DMA uses an independent bus, compute is long enough to overlap, coherency is explicit, and double-buffer timing beats direct access. |
| Micro-optimizing tiny elementwise loops | Four-wide batching and similar variants were zero or below the measurement quantum. | The phase had too few cells per token to matter. | Model width/depth or call frequency grows enough that the phase exceeds the keep threshold. |
| Repeated unchanged verification | Runs #302–#329 measured the same accepted image repeatedly. | A measurement process without an identity guard manufactured activity, not evidence. | Never reopen as an experiment; repeat only for an explicit noise, board, thermal, or reproducibility question. |

## Failure modes in reasoning and evidence

### A faster isolated loop can be a slower token

Isolation changes cache warmth, caller/callee boundaries, allocation, core
contention, and setup amortization. The paired-transform investigation showed
that a tight loop can exaggerate producer/consumer locality rather than expose
a real cache-capacity win. Every microbenchmark claim needs an end-to-end phase
counter and token-rate confirmation.

### A correct host build can hide a broken target kernel

Host builds use C fallbacks and serial row execution. They cannot prove target
assembly, ABI safety, cache placement, or two-core behavior. Conversely, a
device text match alone may miss bounded logit drift. Use both, with a kernel
differential on the target path between them.

### A synthetic reference can agree with the same mistake

One transform experiment retyped a “reference” rather than calling the shipping
function. Mapping tests can also miss errors when expected sums are invariant
under permutation. Reference tests should call the production scalar path and
include hand-computable position-sensitive patterns.

### “Built” is not the same as “executed”

A cache option may never become a C definition, and an unreferenced object may
be removed by linker garbage collection. Prove the resolved macro in the
compile command, the selected path in a boot/probe line, and the symbol plus
instructions in the linked binary.

### A frozen output can encode incidental state

The two unresolved device cases changed when only the lossless input transport
changed; the engine bytes were identical. The new output was deterministic,
order-independent within the image, and consistent with host behavior. This
does not authorize silently editing a golden. It proves that the oracle's state
model must be understood before labeling an arithmetic candidate wrong.

### A causal story must remain retractable

The console failure was first attributed to stdout behavior, then localized by
the RX-ring discriminator to transport/state. The documentation preserves both
the observation and the retraction. Record timeouts, hashes, and traces as
observations; label mechanisms as hypotheses until a controlled discriminator
moves only that mechanism.

## A compact decision framework

For each candidate, answer in order:

1. **What is the exact hot phase and its token-wide share?** A 20% kernel win in
   a 1% phase cannot clear a 0.2% keep bar after overhead.
2. **What resource binds it?** Bytes, dependent latency, control/calls,
   synchronization, setup, or capacity.
3. **What changes quantitatively?** Give bytes, calls, joins, live registers,
   or reused preparations per token.
4. **What must remain invariant?** Arithmetic order, packed mapping, state,
   output protocol, and fallback behavior.
5. **What test can falsify the mechanism before scarce hardware time?** Use a
   differential, byte budget, disassembly, or adversarial mapping probe.
6. **What end-to-end evidence promotes it?** Exact tree, product path, quality
   suite, token rate, and provenance.
7. **What changed premise would reopen it after rejection?** If none can be
   named, archive it rather than rescreening it.

The campaign's final stop condition followed directly: no above-bar candidate
remained that was neither measured nor closed with a byte, quality, capacity,
or vendor-support reason. That is a better definition of “done” than a fixed
run count.

## Evidence behind this synthesis

The detailed chronology and raw measurements remain in
[the experiment ledger](../appendices/experiment-ledger.md),
[`ideas.md`](../../../.auto/ideas.md),
[`mimimodel-experiments.md`](../../../.auto/mimimodel-experiments.md), and the
final [`HANDOFF-2026-09-28.md`](../../../.auto/HANDOFF-2026-09-28.md). Interpret
all numbers using the state labels and proof rules in
[the evidence policy](../evidence-policy.md).

