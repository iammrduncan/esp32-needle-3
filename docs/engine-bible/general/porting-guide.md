# Porting the engine and its optimization method

This guide is for moving the reusable parts of Needle to a different model,
quantization format, CPU, memory system, RTOS, or product runtime. It is not a
list of patches to copy. The original results depended on exact tensor shapes,
packed layouts, memory placement, numerical order, and request behavior.

The safe unit of transfer is a **mechanism plus its predicate, fallback, and
proof**, not a source fragment.

## 1. Define the port before writing a kernel

Create a port manifest with these fields:

| Area | Record explicitly |
|---|---|
| Model | architecture, layer count, widths, heads/KV heads, context/window, vocabulary, stateful modules |
| Model artifact | source revision, slicer/converter revision, file hash, tensor count and layout version |
| Quantization | bits, group size, codebook semantics, norm type, transform normalization, packed nibble/bit order |
| Numerical contract | bit-exact, token-exact, top-1 only, or bounded logit error; permitted compiler modes |
| Target | ISA and extensions, ABI, register file, core count, clock, cache line/size, memory tiers, DMA/coherency |
| Runtime | allocator, threads/tasks, stack sizes, synchronization, mapping/IO primitives, serial/network protocol |
| Product path | prompt schemas, prefix caches, grammar, sampling policy, stop rules, tool execution, retry/reset behavior |
| Evidence | reference implementation, frozen prompts, numeric probes, performance workload, keep bar and noise floor |

Do not inherit Needle's constants accidentally. The original artifact uses an
8-layer slice, width 768, 12 query heads, 2 KV heads, context length 384, an
archive `kv_window` field of 256, vocabulary 8,192, and CQ group geometry described in
[the model pipeline](model-pipeline.md). A port must derive or validate every
dimension from its own artifact. This implementation sets `m->window` from
`max_seq_len` and allocates 384 K/V slots per layer, so the header's 256 field
must not be restated as the runtime cache allocation.

## 2. Freeze a trustworthy scalar reference

Before target specialization:

1. load the complete model artifact with bounds and overflow checks;
2. expose a portable scalar implementation for every specialized operator;
3. preserve exact packing, normalization, and floating-point fold order;
4. generate small hand-computable vectors for index/stride mapping;
5. capture complete-model outputs, token counts, and logits on representative
   prompts;
6. test state reset, prefix switching, and repeated calls.

Call the real scalar function from later differentials. Do not copy its source
into a test and call the copy a reference; the campaign demonstrated that a
retyped reference can preserve the same bug.

If the new port intentionally relaxes exactness, write the error budget now.
Specify the compared logits/tokens, maximum and aggregate error, top-1 or
distribution constraint, and product behavior that may change. “Looks good” is
not a numerical contract.

## 3. Validate the model format at the boundary

For a `.cact` port, follow [the format chapter](cact-format.md). For another
format, retain the same boundary discipline:

- verify magic/version, total length, tensor count, offsets, alignments, shapes,
  dtypes, quantization metadata, and non-overlap;
- use checked arithmetic for every `offset + length` and shape product;
- hash the exact deployed artifact;
- reject unsupported architecture features rather than ignoring them;
- test truncated, reordered, misaligned, and duplicate tensor records;
- separate file-layout structs from native in-memory pointers.

Zero-copy mapping is valuable only if the mapped tier can serve the access
pattern efficiently. Treat mapping and placement as separate decisions.

## 4. Bring up one correct token end to end

The minimum useful vertical slice is:

```text
artifact open
  -> tokenizer/prompt bytes
  -> model state initialization
  -> one prefill step
  -> one decode step
  -> grammar/filtering
  -> logit computation
  -> token selection
  -> detokenization/stop handling
```

Validate each intermediate boundary against the reference where practical, but
also validate the completed token. Do not stop at the final hidden state: the
Needle boot benchmark missed a real sampler cost because it did exactly that.

Then validate two tokens, a context/window boundary, a reset, and two
independent cached prefixes. Stateful faults frequently appear only after the
first successful token.

## 5. Build a memory and byte-flow model

Inventory every persistent and transient object:

| Object class | Questions |
|---|---|
| Weights | bytes per token, traversal order, reuse count, compressed versus expanded representation |
| Activations | shape, preparation cost, consumers, lifetime, alignment |
| Lookup tables | construction frequency, size, cache residency, ownership |
| Model state | KV, recurrent/convolution state, engram state, prefix copies, reset semantics |
| Scratch | maximum live set, per-core or shared, aliasing opportunities |
| Code/stacks | fast-memory footprint, per-worker stack, exception/debug overhead |

For every proposed optimization, calculate **incremental bytes per token** before
instruction savings. Include staging copies, sidecars, alignment padding,
writebacks, DMA descriptors, and cache pollution. The rejected FP32 norm sidecar
and expanded offset stream are templates for this screen: both made arithmetic
cheaper but increased the binding stream.

Also calculate capacity at the moment of peak lifetime, not from total free
memory at idle. A 16 KiB table that “fits” globally may not fit per core beside
task stacks and other hot scratch.

## 6. Instrument the whole request, then classify phases

Use target-side cycle or high-resolution timers around named phases. Keep the
instrumented image diagnostic; instrumentation can perturb the result. Capture
at least:

- artifact/prefix open and one-time work;
- activation preparation and lookup construction;
- each projection family;
- attention score, exponential/softmax, value accumulation, and KV load/store;
- stateful modules and normalization;
- sampler/grammar and subset/full logits;
- scheduling, synchronization, IO, and product glue.

For each hot phase, choose one primary classification:

| Class | Evidence | First design direction |
|---|---|---|
| Delivery-bound | time tracks bytes/tier; scheduling edits are null | compact streams, useful bytes per line, placement, reuse |
| Latency-chain-bound | independent chains help; wider loads do not | interleave independent outputs while preserving each order |
| Setup/control-bound | call/address/dispatch changes move time | hoist, memoize, outline/inline selectively, amortize setup |
| Capacity-bound | hit rate changes at a clear working-set threshold | shrink lifetime, tile, stage the smallest reusable object |
| Synchronization-bound | worker wake/join is a material share | enlarge jobs, persistent workers, fewer joins, rebalance split |
| Numerically constrained | faster forms change required output | preserve graph or negotiate a new quality contract first |

Do not infer the class solely from source appearance. Confirm it with one
controlled discriminator: change bytes without changing math, change control
without changing bytes, or vary working-set size around the suspected capacity.

## 7. Transfer Cactus-Quantized kernels safely

If the destination retains the same Cactus-Quants identity:

1. implement normalized activation preparation for the exact group size;
2. prove zero-padding behavior at partial groups;
3. build the CQ2 pair table or target-equivalent reusable representation;
4. retain packed index order, norm interpretation, independent partial sums,
   and final fold order;
5. specialize only exact shapes justified by the call graph;
6. retain the scalar fallback for all other shapes and alignments.

The original 24 KiB CQ2 table for width 768 fit its 64 KiB data cache. On
another target, recompute:

```text
in_pad = ceil(input_width / group) * group
pair_table_bytes = (in_pad / 2) * 16 * sizeof(float)
```

Then compare that live set with usable cache per concurrent worker. Do not copy
the one-row scheduling conclusion if the new table misses cache, and do not
copy a two-row scheme merely because the target has a larger cache; measure the
weight-stream and register consequences together.

For a different quantization format, restart from its algebra. Determine which
operand can be transformed/prepared once, what remains streamed per row, and
whether compact codes can be consumed without expansion. An integer SIMD unit
is useful only if the unpack/conversion and larger operand traffic do not
dominate it.

## 8. Add target specialization behind an exact predicate

A specialized kernel entry point should encode:

- supported shape/group count;
- pointer and row-stride alignment;
- memory-space restrictions;
- numeric-domain restrictions, such as ordinary positive FP16 norms;
- concurrency/scratch ownership;
- ABI and stack-frame assumptions.

Use compile-time assertions for shared C/assembly layouts and runtime assertions
in diagnostic builds. Keep one dispatch site with a portable fallback rather
than scattering target conditions through the model.

The kernel proof ladder is:

1. mapping vectors with distinct codes at every packed position;
2. scalar differential over randomized values;
3. production shapes, odd tails, row offsets, and group counts;
4. all relevant memory placements and alignments;
5. concurrent execution if production is concurrent;
6. linked-symbol and disassembly inspection;
7. complete host/device numeric and output gates;
8. non-profiled end-to-end timing.

If target assembly uses a different arithmetic order, it is a model change for
quality purposes even when the source expression appears equivalent.

## 9. Design parallelism as an ownership protocol

Write the worker contract before enabling a second core or accelerator:

- who owns each output row range;
- who owns mutable scratch and lookup construction;
- how a job sequence is published and completed;
- whether nested jobs are legal;
- what memory ordering makes descriptors visible;
- how reset/shutdown cancels outstanding work;
- how very small jobs avoid paying a parallel dispatch tax.

Start with coarse row splitting because quantized GEMV rows are independent.
Measure imbalance and wake/join overhead across real shapes. Persistent workers
can help, but their protocol must be differential-tested under repeated jobs,
idle periods, wraparound, and spurious wakeups.

Do not infer that cross-operator overlap is free because two source functions
look independent. Include hidden shared scratch, allocator locks, cache/bus
contention, and any inner parallel dispatch.

## 10. Layer correctness gates

Use all of these scopes; none substitutes for another:

| Gate | Catches |
|---|---|
| Kernel bitwise differential | packing, stride, ABI, tails, target math |
| Host frozen outputs | model/control regressions with fast iteration |
| Logit fidelity and top-1 | numeric drift hidden by current text outputs |
| Device frozen outputs | target kernels, concurrency, placement, device state |
| Unseen prompts | overfitting to the primary benchmark |
| Prefix/state isolation | cross-request or cross-schema leakage |
| Product capture | routing, second pass, tools, telemetry, timing behavior |
| Repeated-request soak | races, leaks, wedges, transport and state fatigue |

Record missing references as failures, not matches. Prove that restricted or
fast screening modes still enforce the gates they report. Keep host and device
goldens separate when their execution/state model differs.

When a frozen result changes, first run discriminators:

1. same engine, different transport/runtime;
2. same runtime, scalar versus specialized kernel;
3. fresh boot versus canonical session order;
4. host fresh-state versus device restored-state;
5. repeated and permuted order within one image.

Only then decide whether the implementation is wrong, the oracle captured
incidental state, or product behavior intentionally changed.

## 11. Measure and promote with complete identity

A performance row is meaningful only with:

- source commit and dirty diff;
- combined engine-source hash and application binary hash;
- exact model artifact hash;
- resolved compiler options and generated configuration;
- target/board identity and clock/memory modes;
- workload and golden hashes;
- boot/session/reset policy;
- profile/diagnostic/shipping-image label;
- all correctness results.

Measure device-side decode time over the same generated-token accounting. Track
prefill, held-out/unconstrained prompts, worst case, memory headroom, and product
latency as secondary monitors. Establish the timing quantum/noise floor and a
keep threshold before screening.

Promote a bundle as a new tree and rerun every applicable gate. If a mechanism
is ported across branches, compare against the receiving branch's own pinned
control; never subtract numbers from different hidden bases.

## 12. Adapt the method to common destination changes

### Different model dimensions

Recalculate table capacity, group tails, head/KV ratios, row-split balance,
scratch lifetime, vocabulary filtering payoff, and prefix-state size. Rerun all
shape predicates and differentials. Wider is not merely “more of the same” when
it crosses cache or register thresholds.

### Different quantization or weight layout

Re-derive the operator identity and streamed byte count. Test packed position
identity explicitly. Treat any expansion sidecar as part of the per-token
stream unless it is demonstrably persistent in a faster tier.

### Different CPU/ISA

Inventory native load alignment, FP16 conversion, float divide/exp support,
SIMD unpack/gather capability, hardware-loop cost, ABI frames, and available
registers. Let the compiler provide a baseline schedule. Port the algorithmic
kernel first, then replace only the measured target-specific floor.

### Larger cache or unified fast memory

Reopen row blocking, table reuse, and hot-row residency only after measuring the
new working set. Retire old capacity closures explicitly; do not assume a prior
negative still applies.

### DMA or an accelerator

Count whether transfer and compute use independent resources. Include setup,
coherency, alignment, tail, and double-buffer capacity. Compare overlapped time
against direct cached access, not against a copy-only strawman.

### More cores

Measure memory-bandwidth saturation before multiplying workers. Partition
mutable scratch per worker, make the job descriptor immutable after publish,
and choose split granularity from total completion time rather than equal row
counts alone.

### Different runtime or transport

Revalidate request length, framing, newline/UTF-8 behavior, backpressure,
timeouts, retries, reset semantics, and session-state restoration. Transport can
change observed model state even with identical engine bytes; it is part of the
system under test.

## 13. Know when to stop and what to preserve

Maintain a closure table containing hypothesis, measured result, reason, and
changed-premise condition. Stop an optimization campaign when every above-bar
candidate is one of:

- measured and promoted;
- measured and rejected with a mechanism;
- closed by a conservative byte/cycle/capacity bound;
- blocked by a named quality or product decision;
- blocked by target/vendor capability.

Preserve scalar references, kernel differentials, disassembly probes, workload
and golden hashes, phase maps, result ledger, rejected source snapshots when
useful, and the exact accepted/candidate trees. A prose speed number without
those artifacts is not portable knowledge.

## Port-readiness checklist

- [ ] Port manifest is complete and model/deployment artifacts are hashed.
- [ ] Format parser rejects malformed and unsupported inputs.
- [ ] Scalar reference passes token, state, and boundary tests.
- [ ] One complete sampled token matches the reference.
- [ ] Persistent and peak scratch memory maps fit with headroom.
- [ ] Bytes per token and phase times are measured on the target.
- [ ] Every specialized kernel has predicate, fallback, and target differential.
- [ ] Packed mapping tests are position-sensitive.
- [ ] Parallel scratch and publication ownership are explicit.
- [ ] Host, numeric, device, unseen, state-isolation, and product gates can fail.
- [ ] Build, binary, model, target, and workload provenance are recorded.
- [ ] Candidate timing uses a non-profiled image and the pinned workload.
- [ ] Rejected ideas have changed-premise reopening conditions.
- [ ] Accepted, candidate, diagnostic, and rejected trees are not conflated.

Use [quantization and kernels](quantization-and-kernels.md) for the detailed CQ
mechanics, [numerical correctness](numerical-correctness.md) for equivalence
rules, [optimization method](optimization-method.md) for experiment design, and
[the evidence policy](../evidence-policy.md) for promotion standards.
