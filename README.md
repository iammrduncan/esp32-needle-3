# Needle 3 on ESP32-S3

**Plain English → local device actions.** An 8-layer Needle 3 model selects and
calls real telemetry and timer functions on an ESP32-S3. Inference, JSON grammar
constraints, argument validation, and execution all run on the microcontroller.

[Watch the MP4](demo/needle3-router.mp4) · [Download the GIF](demo/needle3-router.gif) · [Hardware recording](demo/recording.json)

![Dracula-themed ESP32 routing demo](demo/needle3-router.gif)

The 33-second Dracula demo presents five requests captured from the actual board.
Inference waits are condensed; every scene displays the measured request latency.
See [how to capture and render it](demo/README.md).

## What the router does

| Request | On-device tool | Real effect |
| --- | --- | --- |
| How much free memory does this device have? | `get_status()` | Reports heap, PSRAM, uptime, sampling and timer state |
| Sample telemetry every 5 seconds | `set_sampling_interval(seconds=5)` | Reconfigures a periodic `esp_timer` sampling heap and uptime |
| Start a 60 second timer | `set_timer(seconds=60)` | Starts an asynchronous countdown |
| Sample every 10 seconds and start a 30 second timer | Both configuration tools | Executes two validated calls from one sentence |
| Show device status | `get_status()` | Reads the changed device state |

The captured run matched all five supported requests. It collected **18 telemetry
samples** while inference ran, changed sampling to 10 seconds, and verified a
countdown expiry on the ESP32. The sampler retains the latest sample and count;
there are no external sensors or imaginary GPIO actions.

This is a small local control plane: the model routes a user's request to a fixed
set of handlers. The entire call array is checked against an allowlist and bounded
integer arguments (1–3600 seconds) before execution. Tool definitions live in
[`tools/demo-tools.json`](tools/demo-tools.json); CMake embeds this schema in the firmware.

### Current limits

- Five supported examples are a demonstration, not a general accuracy benchmark.
  The recorded unrelated question “What is the weather in Tokyo?” incorrectly
  selected `get_status`. Schema constraints ensure structure, not correct intent.
- The confidence head is not implemented; the API returns `confidence: null`.
- Requests take **19–41 seconds** in this capture. This suits occasional device
  configuration, not a real-time control loop. Device timers continue independently.
- The **HTTP server runs on the attached computer**, bridging USB UART. The model
  and handlers run on the ESP32. This build does not serve HTTP over board Wi-Fi.
- The API is a local development service bound to `127.0.0.1`, without authentication.
  Requests and state reads share one serial connection and are serialized.

## Hardware and dependencies

Tested with an **ESP32-S3, 32 MB octal flash, 16 MB octal PSRAM, 240 MHz** and
ESP-IDF **5.5.2**. The 16,155,796-byte model is mapped from a dedicated flash
partition. This partition layout does not fit a 16 MB flash board.

The tested board exposes `/dev/ttyACM0` for flashing and `/dev/ttyACM1` for its
UART console. Port names vary: use your device's actual ports, preferably stable
`/dev/serial/by-id/` names. Your user must have serial access (typically the
`dialout` group on Linux).

Host prerequisites: Git, Python 3.11+, `venv` (or `virtualenv`), GNU Make,
CMake and a C compiler for host tests. Install and activate
[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/index.html)
for firmware builds. VHS, ttyd and FFmpeg are needed only to render the demo.

## Build and run

From the repository root:

```sh
make setup
make model
make verify-model
```

`make model` downloads the pinned public archive, verifies its SHA-256, slices
the already quantized tensors to **8 layers / 256 tokens**, and verifies the
result. [`model/manifest.json`](model/manifest.json) records the revision and
hashes. Model archives and generated build files are excluded from Git.

In a shell with ESP-IDF activated (`. /path/to/esp-idf/export.sh`):

```sh
make flash FLASH_PORT=/dev/ttyACM0
```

This builds and flashes the application, then writes the model at `0x210000`.
For later firmware-only changes, use `make flash-app`. Stop the API service and
any serial monitor before flashing. The 143-token tool prefix is cached at boot;
a cold start takes roughly two minutes. The bridge waits up to five minutes.

Run the bridge in the foreground:

```sh
make serve SERIAL_PORT=/dev/ttyACM1
```

Or install it as a Linux systemd **user service** after closing the foreground bridge:

```sh
make install-service SERIAL_PORT=/dev/ttyACM1
systemctl --user status needle3-api
journalctl --user -u needle3-api -f
```

The installer uses this checkout's absolute path and `.venv`; rerun it if you move
the checkout. It starts with your user session. Uninstall with
`systemctl --user disable --now needle3-api`, remove
`~/.config/systemd/user/needle3-api.service`, and run
`systemctl --user daemon-reload`.

## API

```sh
curl -sS http://127.0.0.1:8081/health
curl -sS http://127.0.0.1:8081/state
curl -sS --max-time 300 -H 'Content-Type: application/json' \
  -d '{"input":"Sample every 10 seconds and start a 30 second timer"}' \
  http://127.0.0.1:8081/complete
```

`GET /health` checks the device and includes `ready`. `GET /state` reads current
state directly without inference. `POST /complete` runs the model and executes
valid tool calls. Its response includes:

```json
{
  "success": true,
  "function_calls": [
    {"name": "set_sampling_interval", "arguments": {"seconds": 10}},
    {"name": "set_timer", "arguments": {"seconds": 30}}
  ],
  "confidence": null,
  "decode_tps": 1.22,
  "latency_ms": 40515.4
}
```

This excerpt omits `results` (each tool's execution status and device snapshot),
`raw` generated text, and additional prefill/decode metrics. See the complete
responses in [`demo/recording.json`](demo/recording.json).

Inputs must be single lines of at most 255 UTF-8 bytes and fit the remaining model
context. Control characters and serial command prefixes are rejected. Errors use
400 for invalid requests, 502 for firmware/tool failures, 503 for unavailable
hardware, and 504 for inference timeout. After a serial timeout, finish/reset the
board and restart the bridge to restore stream synchronization. Reasoning is off
by default; pass `--think` directly to `tools/serial_api.py` to enable it.

## Measured performance

This capture uses cached schema prefix, 8 layers, 256-token context, CPU at
240 MHz, and reasoning disabled. End-to-end times include the HTTP/USB bridge.

| Request | Generated tokens | Decode tokens/s | End to end |
| --- | ---: | ---: | ---: |
| Free memory | 10 | 1.22 | 23.37 s |
| Set sampling to 5 s | 18 | 1.23 | 29.07 s |
| Start 60 s timer | 14 | 1.23 | 23.41 s |
| Sampling + timer | 29 | 1.22 | 40.52 s |
| Read changed status | 10 | 1.22 | 19.36 s |

## Development and verification

```sh
make test        # builds the host engine, tests grammar and serial protocol
make capture     # runs six real requests; needs the board and API
make demo        # renders MP4 + GIF from the saved recording
```

CI builds the host engine and runs its grammar tests and the bridge protocol
tests. Firmware was built and exercised on the actual board. Protocol tests cover
multiple calls, preserving generated spaces, consuming responses through `END`,
firmware errors, and refusing to reuse a timed-out stream.

Optional numerical reference checks require
`python -m pip install -r requirements-dev.txt`. The
[`tools/reference_forward.py`](tools/reference_forward.py) script compares the
host C forward pass against NumPy. Profiling is off by default; configure with
`idf.py -C esp32 -DNEEDLE_PROFILE=ON build` when measuring individual kernels.

## Implementation and provenance

[`engine/`](engine/) adapts the Apache-2.0
[Needle 2 ESP32 port](https://github.com/andrisgauracs/needle-2-esp32) by
andrisgauracs. Changes add the Needle 3 archive layout, QKV causal convolution,
separate Q/K and V widths, Monarch Hadamard MLP, and mHC lane handling.
[`esp32/main/router.c`](esp32/main/router.c) owns the actual telemetry and timer
handlers; [`tools/serial_api.py`](tools/serial_api.py) provides the HTTP bridge.

Code is licensed under [Apache-2.0](LICENSE); see [NOTICE](NOTICE) for attribution.
Weights originate from [Cactus Compute](https://cactuscompute.com/needle) and are
downloaded separately from the pinned
[Needle 3 model repository](https://huggingface.co/Cactus-Compute/needle3).
Media is rendered with [Charm's VHS](https://github.com/charmbracelet/vhs).
