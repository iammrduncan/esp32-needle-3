# Inference-engine anatomy

This engine is deliberately small and layered. The reusable design is not
“one giant forward function”; it is a zero-copy model reader, portable scalar
semantics, target dispatch, persistent state, constrained decoder, and a thin
platform shell. Keeping those boundaries intact made it possible to optimize
the ESP32 path without losing a host oracle.

## Module map

| Module | Responsibility | Portability boundary |
|---|---|---|
| `nd_cact` | Validate the archive header/directory and return in-place tensor views. | Pure C, little-endian format. |
| `nd_tokenizer` | Parse the embedded tokenizer, BPE encode, decode pieces, handle byte/control pieces. | Pure C; owns small host-style allocations. |
| `nd_grammar` | Compile the supported tool-schema subset and advance a byte-level JSON state machine. | Pure C; deliberately bounded schema features. |
| `nd_quant` | FP16 conversion, fast exponent, FWHT preparation, CQ GEMV, pair LUTs, row dispatch. | Portable C reference plus optional target assembly and row splitter. |
| `nd_model` | Allocate state, bind tensors, run engram/mHC/attention/MLP blocks, manage KV and prefixes. | Platform allocators and row splitter are injected. |
| `nd_sample` | Enumerate legal tokens, request subset logits, greedy argmax, advance grammar state. | Pure C except it benefits from the injected row splitter. |
| ESP32 `main` | Map the model partition, start core 1, prime schemas, run the line protocol. | ESP-IDF-specific. |
| `router` | Parse generated calls and execute the small local device tool set. | Demo/product shell, not model math. |
| `serial_api.py` | Attach safely, frame replies, expose HTTP, and orchestrate route then optional local execution. | Host bridge. |

The host tools link the same engine sources. Target-only assembly is selected by
compile definitions; otherwise the C path is the reference. This gives fast
iteration and makes it possible to compare target behavior with a portable
implementation, but it also creates a hard rule: host success cannot prove a
target assembly kernel or a two-core schedule.

## End-to-end data flow

1. The model archive is downloaded at a pinned revision, hash-verified, sliced
   without requantization, and hash-verified again.
2. The ESP32 firmware finds the `model` data partition, derives the real archive
   length from its header/directory, and memory-maps exactly that range.
3. `nd_model_open()` validates geometry, binds tensors by canonical position,
   allocates persistent state/scratch, and stages selected multiply-read FP16 or
   packed-weight regions into faster representations/memory.
4. Two compacted schema prefixes are prefilled once: device tools and routing
   capabilities. Each prefix saves KV, convolution, engram, and confidence
   state; weights and scratch remain shared.
5. For a request, firmware restores the appropriate prefix, tokenizes only the
   suffix, prefills it, and greedily generates under the compiled grammar.
6. The sampler asks the engine for logits only for grammar-legal token ids when
   that candidate set is small; it falls back to the full vocabulary otherwise.
7. Firmware emits a line-framed response. The host bridge reconstructs raw
   pieces, calls, results, confidence, and device-side timing.
8. Route requests stop after selection for externally labelled models. A local
   route sends the unchanged request through a second inference with the device
   tool schema, validates all calls, then executes them.

## Runtime ownership

The archive bytes are immutable and live in mapped flash. Tensor views contain
offsets and geometry, not copied weights. Long-lived mutable state includes:

- per-layer int8 K/V caches and float scales;
- q/k/v convolution histories;
- engram token/history state;
- optional confidence pooling state;
- two independently saved schema prefixes.

mHC lanes are per-token block scratch, not recurrent session state. They do not
belong in a saved prefix.

Scratch is shared across sequential operators: transformed activation, pair
LUT, intermediate vectors, attention buffers, MLP buffers, and one dequantized
row. That reuse is necessary on a microcontroller but makes alias/lifetime
reasoning part of correctness. The two-core contract permits only disjoint
output slices and per-call stack/context scratch during a parallel callback.

## Platform injection points

There are three intentional seams:

1. `ND_ALLOC` / `ND_FREE` place large state in external RAM on ESP32 and use
   ordinary allocation on the host.
2. `ND_ALLOC_FAST` places frequently touched, bounded scratch in internal SRAM.
3. `nd_parallel_rows` defaults to a serial implementation; ESP32 installs a
   two-core splitter.

Target assembly is narrower than these seams. It must retain a C fallback and
an eligibility predicate that checks all layout, alignment, group-size, and
numeric-domain assumptions.

## Loader/open-time work versus token-time work

Open-time transformations are valuable when an immutable value is converted or
resolved many times per token. The largest early win came from converting
multiply-read FP16 MLP factors to FP32 once. Open time also builds tokenizer
indexes, inverse RoPE frequencies, staged tap matrices, and a copied hot weight
tier.

The decision is not “precompute everything.” Every staged byte competes with KV
state, prefixes, and other tiers. A useful open-time transformation must satisfy
all three:

- repeated token-time work is removed;
- the new layout does not increase the dominant stream more than it saves;
- capacity and cache side effects are measured at the final workload.

This is why FP32 MLP factors were transformative while FP32 CQ2 norm sidecars
were closed by budget: the former remove repeated conversion/load work on small
reused arrays; the latter enlarge an already bandwidth-bound stream.

## Forward API shape

`nd_model_step_hidden()` advances all persistent model state and returns the
final normalized hidden vector. The caller can then choose:

- `nd_model_logits_all()` for an unconstrained/full-vocabulary decision;
- `nd_model_logits_subset()` for a small set of legal token rows.

This split is important. Prefill does not need logits at all, and constrained
tool generation usually permits only a small part of the 8,192-token
vocabulary. A conventional “always produce the full head” API would waste both
bandwidth and compute.

## Error handling and boundedness

The implementation rejects several header-geometry inconsistencies, a missing
model partition, oversized schemas/prompts, illegal grammar states, and
truncated generation. The low-level reader can reject an out-of-range tensor
payload when `nd_cact_data()` is checked. The current model binder, however,
trusts the manifest-pinned canonical archive and does **not** systematically
verify every tensor decode, payload bound, dtype, rank, shape, alignment,
overlap, or all fixed target caps before dereference. In particular, a valid
tag/directory is not an adversarial archive boundary.

That is a present validation gap, not a recommended design. A port or product
that accepts untrusted/replaceable archives must validate every bound and
semantic tensor contract—including head-dimension/replication caps—before
allocation or use. Silently clipping a new geometry is not acceptable.

The firmware's generation cap is a backstop, further clamped to remaining model
context. Reaching the token or output-text cap emits an explicit truncation
error. This rule was added after the old 128-token cap produced a syntactically
partial tool call that the host could otherwise mistake for ordinary failure.

## What is intentionally outside the engine

- remote model invocation and credentials;
- keyword routing or expected-answer overrides;
- HTTP and service installation;
- device-specific tool side effects;
- benchmark policy and golden promotion;
- model training and requantization.

Keeping those outside lets the same engine serve a host oracle, an ESP32
firmware, and future embedded products without baking demo policy into model
math.
