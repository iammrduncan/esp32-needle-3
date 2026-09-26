# Optimization method

The successful work followed a simple loop: establish provenance, measure a
real bottleneck, price the proposed lever in the constrained resource, test one
mechanism, run the matching correctness gate, and either promote or close it.
The important discipline was not trying more ideas; it was making each result
hard to misinterpret.

## 1. Freeze the experiment identity

Before measuring, record:

- source commit or preserved worker tree;
- model revision and file hash;
- ESP-IDF/toolchain and compile definitions;
- CPU, flash, PSRAM, cache, and partition configuration;
- board and serial endpoints;
- prompt group, generation length, reset/order policy;
- whether the image is benchmark, profile, diagnostic, or shipping class.

The campaign learned that a board can retain a valid but different image and a
worker can compare against its own moving `HEAD`. A number without tree/image
identity is not reusable evidence.

## 2. Measure the integrated baseline

Use the production request path and report both throughput and quality. The
project's headline metric is decode tokens per second over a frozen primary
prompt group, but prefill, held-out `ext`, and unconstrained `think` monitors
help classify a gain. Warm/cache state and sampler inclusion must be explicit.

Run repeats long enough to see variance. A candidate near the keep threshold
needs paired A/B measurements, not comparison with a weeks-old mean.

## 3. Build a phase map

Instrument coarse phases first. The late best-tree map was approximately:

| Phase | ms/token | Share |
|---|---:|---:|
| Attention stage | 81.2 | 50.6% |
| CQ2 projection work | 79.7 | 49.7% |
| Attention heads | 23.5 | 14.6% |
| Hadamard transforms | 20.2 | 12.6% |
| Engram | 14.6 | 9.1% |
| Phi path | 8.1 | 5.0% |
| Sinkhorn | 3.1 | 1.9% |
| mHC mix | 2.0 | 1.2% |
| Prep and LUT build | 1.7 | 1.1% |
| Tail / RoPE | 0.8 combined | <0.5% |

These labels overlap; do not add the percentages as independent components.
They locate mechanisms. Instrumentation itself can perturb instruction/cache
placement, so remove it before claiming shipping speed.

## 4. Price before coding

Compute an upper bound in the resource that is actually constrained:

- bytes/token on the external-memory bus;
- calls/token and setup cycles/call;
- internal RAM required per core;
- cache lines touched and reuse distance;
- synchronization events/token;
- instruction bytes added to hot IRAM/I-cache;
- numerical slack permitted by the gate.

Two clean closures came from byte pricing. A uint16 offset stream increased the
packed stream about fourfold on an already busy bus. An FP32 norm sidecar added
6.25% weight bytes, about 5.1 ms of traffic, to save roughly 1.4 ms of
instructions. Neither needed another micro-optimization attempt.

## 5. Use a discriminating probe

A probe needs a control that can pass and a result that distinguishes rival
stories. Examples:

- IRAM-versus-PSRAM staging at identical arithmetic isolates delivery cost;
- one-core versus two-core execution prices synchronization;
- kernel differential plus linked-ELF inspection separates arithmetic from
  dispatch/link failure;
- transport-only image changes isolate UART/request-state effects;
- identical kernel on multiple preserved trees reveals tree-specific sign.

A probe that is incapable of showing improvement proves nothing when it is
flat. A microbenchmark with unrealistic shapes or cache residency may prove a
local mechanism and still predict the integrated sign incorrectly.

## 6. Change one mechanism

Keep patches small and reversible. Preserve the known-good tree before trying
another lever. When mechanisms must compose, first measure each on the tree
that will carry it: the campaign observed that lever signs can change across
lineages because code layout, scratch pressure, split policy, and call mix
change.

## 7. Verify the implementation exists

For C/C++ flags, inspect the compile database and print a runtime banner. For
assembly, inspect the linked symbol and disassembly. For target dispatch, print
or count the chosen kernel. For memory placement, print addresses/capabilities
or inspect the map. “The source file was added” is not evidence that the hot
path executes it.

## 8. Apply the proportional gate

At minimum:

1. build and static checks;
2. reference/kernel differential;
3. host golden and logit/top-1 tests;
4. restricted device screen that still enforces exactness;
5. full device breadth;
6. repeat/paired performance;
7. behavioral capture for product-visible changes.

A crash, missing case, timeout, or nonzero token delta is a result, not a row
to omit.

## 9. Classify the outcome

- **Keep:** clears the declared bar and all required gates.
- **Banked:** sound and useful as an enabler, but below the direct speed bar.
- **Diagnostic:** establishes mechanism only; never quote as shipping speed.
- **Discard:** negative, neutral, unsafe, or quality-regressing.
- **Blocked:** plausible only if a named premise changes, such as vendor clock
  support or an owner RAM decision.

Record why and the exact reopening condition. This prevents later sessions
from repurchasing a known dead end.

## 10. Know when to stop

Stop when every phase large enough to clear the keep bar has either a measured
candidate or a quantitative closure; the best tree has the full evidence
stack; and remaining items require an explicit product/vendor premise change.
The late campaign met that condition. Generating more experiments after that
would increase activity, not information.

## Reusable review questions

- Is the comparison against the same workload and exact tree?
- Is the speed delta larger than run-to-run noise and the declared bar?
- Did bytes move between internal and external memory, and were those bytes
  included in the estimate?
- Could code layout, cache warming, or instrumentation explain the result?
- Does the microbenchmark exercise production shapes and dispatch?
- Which numerical equivalence class applies?
- Did the full product path preserve state and side effects?
- What observation would falsify the causal explanation?
- If rejected, what changed premise would make it worth reopening?

