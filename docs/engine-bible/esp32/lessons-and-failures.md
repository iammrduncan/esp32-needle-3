# ESP32-S3 lessons, failures, and reopening conditions

This chapter records what was specific to the ESP32-S3 N32R16-class target,
ESP-IDF 5.5.2, dual LX7 cores, and the board/serial pool used by the campaign.
The companion [general chapter](../general/lessons-and-failures.md) contains the
portable inference-engine mechanisms.

The board was not a passive timing device. Octal flash/PSRAM delivery, cache
geometry, internal-RAM pressure, Xtensa ABI details, FreeRTOS scheduling, USB
serial control lines, and per-boot state all changed which ideas were correct
or even observable.

## External-memory delivery dominated the optimization shape

The deployed model is mapped from its flash partition; large weights and state
also depend on cached external memory. At the measured 80 MHz octal setup, the
dominant packed-weight paths were often delivery-bound. Instruction-count wins
therefore mattered only when they did not increase the external stream or harm
cache-line use.

### What worked

- Keep weights packed and walk them monotonically instead of expanding them.
- Keep small, repeatedly consumed activation tables in the configured 64 KiB
  data cache or stage a carefully chosen hot object in internal RAM.
- Stage small MLP factors and other high-reuse operands, not entire streamed
  matrices.
- Recover internal/PSRAM capacity by narrowing prefix state and shortening
  scratch lifetimes.
- Treat flash and PSRAM configuration, cache size/line, and CPU clock as part of
  the measured binary identity.

### What failed

| Attempt | Evidence and reason | Reopen only if |
|---|---|---|
| Expanded CQ2 offsets / integer stream | A uint16 form used four times the packed bytes against a bus already about 69% utilized. | Hardware consumes packed codes directly, the expanded stream becomes resident, or memory bandwidth changes substantially. |
| FP32 norm sidecar | Added about 6.25% stream traffic: approximately 5.1 ms cost versus 1.4 ms conversion saving. | Norms reside in internal RAM or a faster tier, or conversion becomes dominant on a different core. |
| Quad CQ2 table | About 38% slower kernel; group-outer access consumed only part of 64-byte lines and damaged forward streaming. | Layout makes every fetched line useful or a real gather facility changes the access cost. |
| Two-row CQ2 walker | Slower; the 24 KiB pair table already fit and the extra row stream/register pressure dominated. | Per-core cache grows, layout creates cross-row line reuse, and register spills remain absent. |
| 64-bit row loads | Roughly 4.6% slower where group data was 4 mod 8 aligned. | The model format guarantees 8-byte alignment and target timing confirms unsplit loads. |
| Software prefetch | Negative/null; the existing load queue covered the stream and LX7 offered no useful software data-cache prefetch path for this case. | A future core exposes a non-blocking prefetch and a measured miss-latency gap exists. |
| GDMA copy then consume | About 17.9% slower in the per-token form; copy and consumer contended for the same octal path. | DMA and compute demonstrably use independent resources, coherency is explicit, and overlap beats direct cached reads. |
| CQ2/phi residency | CQ2 residency had a measurable ceiling but could not fit safely; phi needs roughly 18 KiB/core against about 12 KiB free for only ~0.45% token-wide potential. | Internal RAM grows, assertion/debug memory is deliberately traded away, or a smaller partial-residency scheme clears the keep bar. |

The late asymmetric phi idea—stage about 9 KiB for one core and rebalance the
split—was priced at roughly +0.22% token-wide, exactly at the campaign bar and
with no margin. It remains a hypothesis, not a result. It becomes worth a build
only if the measured delivery advantage rises or more internal RAM is freed.

## Faster external-memory clocks were real but not shippable

Running octal flash/PSRAM at 120 MHz produced a real +3.51% diagnostic gain on
two boards. It was not promoted. With ESP-IDF 5.5.2 and the installed flash
model, temperature-based timing retuning returned
`ESP_ERR_NOT_SUPPORTED`. A longer soak cannot prove safe a mitigation that the
vendor stack cannot perform. PSRAM ECC also did not fit the application state.

Reopen 120 MHz only with a supported timing model for the exact flash/PSRAM
parts and SDK, then repeat cold/hot boot, temperature, integrity, cross-board,
and sustained-inference tests. A new SDK version alone is not evidence; verify
the resolved configuration and runtime retuning path.

See [the source catalog](../sources.md) for the official ESP-IDF external-RAM
and flash/PSRAM configuration documentation used to bound this conclusion.

## Xtensa/LX7 kernels require binary-level proof

The successful TIE728 CQ2 kernel preserved the C walker's packed order and
four-partial fold while scheduling one row at a time. Later hardware-loop work
removed or amortized group-loop control and reproduced its end-to-end benefit
on three different trees.

The reusable ESP32 rules are:

- derive the `call8` frame from the ABI's input/output register requirements;
  the early 64-byte frame was too small and corrupted return state;
- assert C/assembly structure offsets and argument layout;
- check actual alignment before selecting wide loads;
- preserve nibble identity with position-sensitive probes;
- inspect the linked ELF, not merely the `.S` source or object file;
- verify hardware-loop setup, `LBEG/LEND/LCOUNT`, required synchronization, and
  branch landing points in disassembly;
- test operands in both internal and external memory when production uses both;
- retain a C fallback because host builds and unsupported target shapes need it.

### Instruction assumptions that failed

- A new assembly object was garbage-collected when no live reference reached
  it; the build and even a changed source signature were not proof of use.
- `div.s` was not available as assumed, and the measured software divide was
  not a meaningful token-wide lever anyway.
- LSX is not implemented on this silicon.
- Wide QK and Kron variants lost to GCC's schedule or exceeded register
  capacity; the handwritten QK form measured 56 versus 49 cycles per chunk.
- A forced-inline 4-bit helper saved only about 0.10% and consumed 1 KiB of
  scarce internal memory.
- The apparent fast wide-load CQ4 forms collapsed packed nibble positions.
  Random/permutation-invariant tests missed it; distinct-nibble probes exposed
  it.
- An early assembly differential itself was corrupted by a debug output
  pointer. Test infrastructure needs canaries and a known-pass control.

Reopen handwritten assembly when the exact call mix, compiler version,
alignment, register pressure, or memory placement changes. Never reopen it on
the premise that assembly is intrinsically faster.

## Dual-core work is a protocol, not just a row split

Rows are independent enough for two-core GEMV, but the surrounding scratch and
job machinery are not automatically independent. The retained system uses a
persistent worker and explicit row ranges. Small scheduling changes became real
wins only after wake, join, and state costs were measured on the complete tree.

### The shared-table race

A folded 4-bit codebook appeared green on three boards, but both cores wrote one
static table. Favorable timing hid the race. The row-split contract now requires
all mutable temporary storage to be one of:

- built once and immutable before publication;
- partitioned by core/job;
- owned exclusively by the caller or worker;
- protected by synchronization whose cost is included in the result.

Multiple passing boards do not prove absence of a race. Add adversarial delays,
repeated calls, and concurrent differentials.

### Scheduling results were tree- and state-dependent

Spin handshakes, task notifications, split thresholds, and job sizes changed
sign as the engine grew. A wake-cost result on one tree did not transfer
automatically to another. Cross-operator overlap also collided with the inner
row splitter and shared scratch rather than hiding useful work.

Reopen scheduling ideas only when the receiving tree's job duration,
imbalance, worker state, and synchronization trace are remeasured. Ensure that:

- nested row jobs are prohibited or explicitly supported;
- a monotonic job sequence distinguishes new work from stale/spurious wakeups;
- descriptor writes are visible before the worker is released;
- completion cannot be mistaken after wraparound or reset;
- task/core affinity and priority are recorded;
- tiny jobs stay serial when dispatch overhead exceeds useful work.

### FreeRTOS time is quantized

At the configured 100 Hz tick, a nominal 5 ms `vTaskDelay` converts to zero
ticks. Reason in resolved ticks and verify the generated configuration. For
sub-tick timing or yielding, use a primitive with defined semantics for that
purpose rather than assuming millisecond source text describes runtime delay.

## Internal RAM, IRAM, and assertions are one budget

ESP32 internal memory is shared by hot data, code placement, task stacks,
runtime structures, and diagnostics. The campaign found:

- lowering the assertion level reclaimed 8,248 bytes but did not itself change
  speed;
- an IRAM placement experiment did not pay as a general lever;
- forced inlining could consume scarce code/internal memory for a sub-bar gain;
- assembly memoization cost memory but paid because it removed a frequent
  eligibility walk;
- prefix compaction reclaimed roughly 768 KiB of external state and enabled a
  selective staging candidate, while preserving prefix isolation.

Do not report “free heap” without naming capability and region. Track internal
and external free memory, largest blocks, task stacks, executable placement,
and peak lifetime on the shipping configuration. Reopen assertion/IRAM trades
only through an explicit product decision about diagnostics and safety.

## The build system can silently erase an experiment

Three distinct ESP-IDF failure classes occurred:

1. a CMake option existed in the cache but was never forwarded as a C
   definition;
2. an assembly object built but its symbol was linker-garbage-collected;
3. a worker restored from its own contaminated `HEAD`, so the “candidate” used
   an unaccepted base.

For every candidate:

- start from the named canonical commit, not the worker's current branch tip;
- record the dirty patch and combined engine hash;
- use a separate build directory or prove configuration invalidation;
- inspect `compile_commands.json` for the macro and flags;
- print the selected path/config in a diagnostic boot banner;
- use `nm`/`objdump` or the map file to prove the symbol and instruction body;
- hash the flashed application and model artifact;
- assert expected engine provenance in the runner.

A successful `idf.py build` proves only that some configured graph compiled.
It does not prove the intended candidate reached the binary.

## Board state is part of provenance

The three-board pool was useful only after board identity and state became
explicit. Invalid results came from stale images, kbench-only firmware,
alternate cache/config settings, worker-tree contamination, claimed console
ports, and post-reset session differences.

Discovery should use distinct hypotheses on separate pinned board controls.
Confirm only a winner across boards. The accepted 5.3033 tree reproduced at
5.3033, 5.3017, and 5.3033 tok/s on the three boards, establishing that the
normal board spread was about one reporting quantum. Cross-board agreement does
not repair a bad provenance chain; all boards can run the same wrong image.

Profiled builds are diagnostic because phase counters perturb execution. A
kbench image is not a shipping image. Restore and verify the known product image
after specialized tests.

## Serial attach can reset or strand the chip

Opening the ESP32 console with default modem-control behavior can toggle
DTR/RTS, reset the device, and leave it in ROM download mode. That failure looks
like a dead board. The working attach discipline is the one implemented by
`tools/serial_api.py`: construct the device closed, establish safe DTR/RTS
behavior around the open, drain stale input, and use the board wrapper's own
`$FLASH_PORT` and `$SERIAL_PORT`.

Additional pool lessons:

- `/dev/ttyACM*` aliases were not interchangeable board identities;
- expand wrapper-provided port variables inside the board-locked process;
- a leftover `serial_api.py` child can continue owning the console even when a
  parent-name `pkill` misses it—terminate the actual PID;
- a watchdog-looping low-memory image can resemble USB failure;
- reconnecting the host does not necessarily reset firmware state;
- flashing and request traffic require exclusive console ownership.

Before declaring hardware dead, inspect USB enumeration, port ownership, boot
mode, reset cause, and the last flashed image using a known-safe attach.

## Transport and model-state evidence must be separated

The original console request path silently truncated a 271-byte request and the
board could stop answering after roughly 16–17 requests per boot. The lossless
ISR-fed RX ring fixed the greater-than-128-byte input problem and the per-boot
request ceiling. It also flipped exactly two frozen device outputs while engine
bytes remained unchanged.

The discriminator was unusually strong:

- engine unchanged, transport changed;
- exactly the same two cases changed;
- inputs, tokenization, and restored-prefix identity were checked;
- behavior was deterministic and order-independent within each image;
- host output supported the new answer on one prompt;
- ordinary repeated tool calls remained byte-identical in a ten-request soak.

Therefore those 18/20 candidate results are not evidence of an arithmetic
kernel regression. They still require an owner decision: keep the ring and
re-baseline/replace state-sensitive goldens, drop it and lose lossless long
requests/the full suite, or retain the cases as release blockers. The rule is
to diagnose the oracle, never to weaken it silently.

The earlier stdout-stall explanation was retracted. `_line_quiet()` suppressed
diagnostic lines during requests, and the later pure transport experiment was a
better controlled mechanism test. Preserve observations separately from causal
stories.

## Firmware/product work must be measured outside the boot bench

The boot benchmark excluded grammar sampling and reported sampler time as zero;
real request decode was roughly 4% slower. It also could not validate HTTP
routing, two-pass local tool execution, telemetry progression, sampling cadence,
or timer expiry. The campaign added request-path profiles and `make capture` for
those behaviors.

On an ESP32 port, distinguish:

- boot microbenchmark;
- serial request path;
- HTTP bridge and routing pass;
- optional second model pass;
- local tool execution and telemetry;
- long-lived multi-request session.

Progress printing, per-event `fflush`, and deferred echo can be visible in token
time or liveness. Treat console output as bounded product IO, not free debug
text. If a dedicated logging task is introduced, its queue-full behavior and
loss policy become part of the product contract.

## ESP32 experiment checklist

Before spending a board run:

- [ ] Candidate begins from the named base and its patch is recorded.
- [ ] Resolved `sdkconfig`, clock, flash/PSRAM mode, cache, and FreeRTOS tick are
      captured.
- [ ] `compile_commands.json` contains the intended definitions.
- [ ] Linked ELF contains the intended target symbol/instructions.
- [ ] Kernel differential covers production shapes, odd row splits, alignment,
      and internal/external memory placement.
- [ ] Mutable scratch is private or immutable under two-core execution.
- [ ] Internal/PSRAM headroom and task stacks fit at peak lifetime.
- [ ] Board lock, serial ports, DTR/RTS behavior, and image type are known.
- [ ] Device measurement uses the target's reported token timing and frozen
      workload.
- [ ] Device exactness, host outputs, fidelity, top-1, prefix isolation, and
      missing-golden count are enforced.
- [ ] Non-profiled product firmware completes capture and a repeated-request
      soak before promotion.

The full evidence trail is in
[`HANDOFF-2026-09-28.md`](../../../.auto/HANDOFF-2026-09-28.md),
[`OWNER_ACCEPTANCE_PACKET.md`](../../../.auto/OWNER_ACCEPTANCE_PACKET.md), and
the [experiment ledger](../appendices/experiment-ledger.md). Hardware and SDK
facts are sourced in [the source catalog](../sources.md).

