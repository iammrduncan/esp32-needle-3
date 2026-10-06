# Memory, placement, and bandwidth

The durable performance model for this port is “count bytes before cycles.”
Most weights cannot live in internal RAM, and the hot CQ2 path repeatedly
streams a large packed representation through a shared external-memory path.
Once compute overhead was reduced, moving extra bytes was usually a losing
trade even when it removed instructions.

## Memory roles

| Class | Appropriate contents | Main hazard |
|---|---|---|
| Internal DRAM | stacks, synchronization state, tiny hot scratch, selected staged operands | scarce; per-core duplication and cache/IRAM choices consume it |
| IRAM | target-only hot code with demonstrated flash-cache stalls | displaces other internal resources; code growth can hurt I-cache |
| PSRAM | large mutable state, selected hot model tier, prefix/KV storage | shares cache/bus; not uniformly DMA-safe or as fast as DRAM |
| Memory-mapped flash | immutable `.cact` model and cold constants | streamed latency/bandwidth; mapping does not make it internal |
| Cache | transparent reuse for code/data/LUTs | shared, finite, workload-dependent |

## Per-token byte model

For a matrix with `R` rows, `K` columns, group size `G=128`, and CQ2 indices:

- index bytes are `R * K * 2 / 8`;
- norm bytes are `R * (K/G) * 2` for one FP16 norm per group;
- activations/LUT construction are much smaller and reusable across rows;
- output writes are normally negligible beside the weights.

Do this accounting for every projection invoked per layer and per mHC lane.
The campaign measured a weight-stream floor near 2.2 MiB/token for the relevant
CQ2 class and much larger aggregate traffic across the full token. Exact totals
depend on which rows the constrained decoder requests and which tensors are in
the PSRAM tier.

## Placement strategy that worked

### Zero-copy model mapping

The `.cact` directory and immutable tensor data are consumed directly from a
mapped model partition. This avoids a second 16 MiB copy and lets each tensor
remain a view into the archive. Bounds, alignment, dtype, and shape validation
must precede use in a hardened loader. The current binder relies on the
manifest-hashed canonical archive and is not a complete untrusted-input
validator; see [the format chapter](../general/cact-format.md).

### Selective PSRAM residency

A measured 12 MiB weight tier in PSRAM improved decode by about 1.5%. It was
not a blanket “copy everything” result. Selection must consider reuse, access
pattern, boot-copy cost, total capacity, and whether moving one tensor evicts a
hotter one. Record the selected tensor set and available PSRAM fingerprint.

### Small internal staging

Staging reused FP32 Monarch/MLP factors was the campaign's first large win,
roughly `+99.9%` over the original run. Narrow engram staging (EG2) later added
about `+0.164%`; it was kept as a sound enabler but fell below the direct speed
bar and required the compact-prefix memory saving.

The transferable condition is high reuse per staged byte. Copying a streamed
operand that is consumed once merely adds traffic.

### Compact persistent state

The narrow compact prefix reclaimed about 768 KiB without changing quality.
Its primary value was capacity for subsequent staging. Persistent-state
compression should be tested through save/restore isolation and long-context
behavior, not only heap statistics.

## Cache observations

- 64 KiB D-cache and 64-byte data lines were retained for the streaming model.
- The CQ2 pair LUT was already effectively resident. Shrinking/quantizing it
  did not remove the dominant streamed-weight traffic and risked product
  changes.
- Software-prefetch experiments had no useful target lowering/instruction and
  did not improve integrated performance.
- Profiling or added code can move cache conflicts. Rebuild the shipping image
  before quoting a profiled result.

## Byte-priced failures

| Idea | Arithmetic story | Memory reality | Disposition |
|---|---|---|---|
| FP32 norm sidecar | Avoid repeated FP16 conversion | +6.25% weight bytes; about 5.1 ms traffic to save ~1.4 ms | Closed |
| uint16/CV3W offset stream | Cheaper address formation | Around 4x packed bytes on a bus already ~69% utilized | Closed |
| Compact/quantized pair LUT | Fewer LUT bytes | LUT already cache-resident; quantization changes products | Closed |
| GDMA prefetch | Overlap model reads | Cannot directly DMA flash-mapped rodata here; bounce cost wins | Closed |
| Full phi residency | Remove delivery portion | ~18 KiB/core needed, ~12 KiB/core free | Blocked on RAM premise |
| One-core partial phi residency | Rebalance staged/unstaged halves | Estimated ~0.22% token-wide with no margin and new walker/split complexity | Priced, not built |
| Larger PSRAM tier | More fast-resident weights | Validated ceiling 12 MiB; larger reports were stale-tree artifacts | Closed absent capacity change |

The phi phase itself was about 8.1 ms/token, but only roughly 9.3% of that phase
was addressable delivery time. Optimizing a phase share is not the same as
removing the whole phase.

## Allocation and concurrency rules

- Allocate scratch per invocation or per worker unless it is immutable.
- A function-static scratch buffer is a global lock/race even if host tests are
  serialized. The folded CQ4 table looked green until two cores wrote the same
  8 KiB buffer; the safe caller-owned version was slower.
- Include both copies when budgeting “per-core” staging.
- Keep immutable lookup tables shared when they are built before workers run
  and never mutated.
- Do not infer DMA capability, internal residency, or alignment from a C type;
  inspect allocation capabilities and addresses.

## A reusable placement worksheet

For each candidate buffer, record:

| Field | Question |
|---|---|
| Size | Bytes total and bytes per core? |
| Lifetime | Boot, request, layer, operator, or row? |
| Access | Sequential, gather, reread count, alignment? |
| Source/destination | Flash, PSRAM, DRAM, IRAM/cache? |
| Added traffic | Copy/build cost per request/token? |
| Removed traffic | Which exact original reads disappear? |
| Contention | Both cores and DMA share which path? |
| Capacity effect | What state/code/stack is displaced? |
| Proof | Address/map/capability print and paired measurement? |

Only stage when the measured reuse and removed traffic exceed construction,
copy, eviction, and synchronization costs on the integrated tree.
