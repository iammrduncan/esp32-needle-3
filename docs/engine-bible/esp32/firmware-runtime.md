# Firmware runtime and product path

The firmware is a thin target adapter around the portable inference engine. It
maps the `.cact` archive, installs ESP32 row parallelism, primes two schema
prefixes, accepts a line-oriented serial protocol, generates constrained tool
calls, executes local device tools, and reports timing/state. The HTTP service
runs on the host over USB/UART; the ESP32 does not serve this API over Wi-Fi.

Authoritative implementations are
[`esp32/main/main.c`](../../../esp32/main/main.c) and
[`tools/serial_api.py`](../../../tools/serial_api.py).

## Boot sequence

1. Start the second-core worker and compact both generated JSON schemas.
2. Locate the data partition named `model`, subtype `0x40`.
3. Read enough header/directory data to determine the archive extent and map it
   with `ESP_PARTITION_MMAP_DATA`.
4. Open the model, allocate state/scratch, and compile both grammars.
5. Print an early `EVT ready model=...` model/configuration line.
6. Prime and save the local-tool and route-selection prefixes independently.
7. Initialize the router, run the startup benchmark, and print the final
   interactive `EVT READY` banner.

The two fixed prefixes are approximately 143 local-tool tokens and 213 route
tokens. A cold boot has historically taken minutes because both are evaluated
before **interactive** readiness. The earlier lower-case `EVT ready model=` line
does not mean requests can be sent; attachment must wait for the final banner or
prove state through the bridge handshake. The bridge therefore allows a
ten-minute boot timeout. Do not interpret silence during priming as a hang
without checking progress lines and process/log growth.

## Prefix snapshots

Each phase shares immutable weights and scratch but owns a saved inference
state. A snapshot must include every persistent item influencing continuation:
token position, KV vectors/scales and sink bookkeeping, QKV convolution history,
engram token/value history, optional confidence pooling, and their cursors. mHC
lanes are per-token scratch and are not saved. The host prefix-isolation
test exists because a cache that restores only KV can look fast and still leak
schema/request state.

On each request the firmware restores the phase's fixed prefix and evaluates
only the original request plus assistant header. At boot it requires at least
64 slots beyond the schema prefix. Per request it rejects when
`position + suffix_tokens + 8 >= window`, so an admitted request has more than
eight slots—at least nine—remaining. Generation is then capped at 256 new
tokens and clamped to the remaining 384-token context. This is a bounded
policy, not proof that a full maximum-length generation reserve fits at
request admission.

## Generation path

The request path is:

`restore prefix -> tokenize query -> prefill query -> constrained decode ->
 validate complete JSON calls -> execute permitted local tools -> emit END`.

When the grammar reaches a complete call array, the sampler forces the
`</tool_call>` token and disengages the grammar. Generation then continues
unconstrained until EOS/IM-END, the context/token cap, or an error. Filling the
1,024-byte text buffer only sets `out_full`: model stepping continues and the
firmware reports truncation after generation. The router extracts the delimited
call array afterward. Tool arguments are validated as a set before any action
runs so a malformed second call cannot leave half of a batch applied. Device
operations in this demo are deliberately small: heap/uptime status, sampling
interval, and a countdown timer.

Inference replies emit explicit records such as token fragments, parsed JSON,
results, prefill timing, decode timing, state, errors, and a final `END`. The
`!status` and `!think` control commands emit only their state/acknowledgement
records. The bridge treats an inference response lacking the
done/calls/results trio as incomplete.

## Two-pass routing

The product-level `/agent` operation is host-orchestrated:

1. send the unchanged input under the route schema;
2. require exactly one legal route call;
3. map that capability through the checked-in model catalog;
4. if it is an external handoff, return the configured label and stop;
5. if it is local Needle, send the unchanged input under the device-tool schema,
   then validate/execute the calls on the ESP32.

No external LLM is invoked. `Claude Opus`, `Qwen 3.8 27B`, and `GPT OSS 120B`
are deployment labels. Their selection latency is Needle-on-ESP32 plus bridge
latency, not provider inference latency, and there is no locally available
Claude response or Claude quality result to report.

The HTTP bridge serializes the full two-pass transaction with a re-entrant lock
so another request cannot alter device state between route and execution.

## API boundary

The localhost bridge exposes:

| Endpoint | Operation |
|---|---|
| `GET /models` | Static catalog and route mapping |
| `GET /health` | Serial readiness plus live device state |
| `GET /state` | Device state without model inference |
| `POST /route` | Route-schema inference only |
| `POST /complete` | Local-tool-schema inference only |
| `POST /agent` | Route, then optionally local second pass |

Inputs must be single-line UTF-8, at most 255 encoded bytes, free of control
characters and firmware command prefixes, and fit the remaining context. The
bridge is bound to `127.0.0.1` without authentication; exposing it on another
interface requires a real security boundary.

## Benchmark versus request path

The boot benchmark can call `nd_model_step_hidden` directly. It omits grammar,
sampler, serial orchestration, and tool handling; in profiled builds its sample
phase can therefore be zero. It is useful for phase diagnosis but is not the
request throughput metric. Profile timers also perturb layout and should stay
out of a quoted shipping image.

## Current capture and stale prose

The top-level README preserves a 2026-09-18 capture with architecture routing
at 33.17 s. The current raw
[`demo/recording.json`](../../../demo/recording.json) was captured on
2026-09-26 and records that architecture route as `research_and_plan`, one
ESP32 pass, `remote_called=false`, routing 6.7104 s, and total 7.7111 s. Its nine
verification flags are true. Use the JSON as the current capture and label the
README table historical; neither number is Claude inference time.

## Runtime invariants

- The model mapping remains valid for the process lifetime.
- Exactly one mutable model state is active at a time.
- Prefix state is phase-specific and fully restored before a query.
- Worker outputs are complete before an operator consumes them.
- A route pass never executes a device tool.
- An external selection never calls a provider in this implementation.
- Local calls are all validated before the first side effect.
- Each inference response terminates with `END`, including errors where
  possible; control-command replies do not.
- A timeout makes the connection unavailable until synchronization is proven.

## Known limits

- The router is demonstrated on curated scenarios, not a held-out semantic
  benchmark.
- Confidence is unimplemented (`null`).
- The 384-token context and 255-byte transport input cap are product limits.
- Local commands cost two inference passes and are unsuitable for a real-time
  control loop.
- There are no real sensors, watch display, cloud providers, or actuators in the
  demo beyond heap/uptime sampling and software timers.
