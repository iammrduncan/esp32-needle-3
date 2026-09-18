# Model selection, then local self-dispatch

A Dracula terminal video made with [Charmbracelet VHS](https://github.com/charmbracelet/vhs).

- [MP4](needle3-router.mp4): 51-second, square 1080×1080 H.264 video (1.4 MB).
- [GIF](needle3-router.gif): the same presentation, looping (1.5 MB).
- [Cover](needle3-router-cover.png).
- [Full hardware capture](recording.json): model selection, both local inference
  passes, executed tools, live state, timings, expected results and source hashes.

## Story

The user's agent watch receives a task. Needle, running on an ESP32, selects a
capability route from a configured model catalog:

1. Translation → **Qwen 3.8 27B**. Show selected model. End scenario.
2. Code generation → **GPT OSS 120B**. Show selected model. End scenario.
3. Architecture design → **Claude Opus**. Show selected model. End scenario.
4. Device health → **Needle** → second inference → read actual memory and uptime.
5. Telemetry cadence → **Needle** → second inference → change the periodic sampler.
6. Countdown → **Needle** → second inference → start a real ESP32 timer.
7. Combined sampling and timer request → **Needle** → second inference → execute
   two generated tool calls. Verify telemetry advances and the timer expires.

No remote model is called. Model labels and their assigned capabilities are
configured demo policy. The host bridge orchestrates the re-entry to Needle;
both inference passes and local tool handlers run on the ESP32. Pass 1 selects
only a route. Pass 2 uses the unchanged task under a different schema to generate
actual tool arguments. No keyword matching selects a model on the host.

The video is a presentation of saved hardware results with waits condensed.
Actual latency is displayed, including both passes for local requests. The watch
is a product concept; the connected hardware is an ESP32-S3 development board.
This is a curated demonstration, not a general routing-accuracy benchmark.

## Capture again

With the current model/firmware and API running:

```sh
.venv/bin/python demo/capture.py
```

This makes seven `/agent` requests (eleven on-device inferences) and changes the
board's sampling/timer state. All routes and local tool calls must match the
stated expectations. External selections must have no second pass. Local
selections must have two. The recording also verifies periodic sampling,
changed cadence, and timer expiry. Failures are saved for review; the renderer
refuses to show a success demo from a failed capture.

The first phase exposes named capability routes with a fixed model mapping.
Those exact generated route names appear in the video so the selection mechanism
is visible. The full JSON includes the model's raw token output.

## Render MP4 and GIF

Install VHS, ttyd, FFmpeg and DejaVu Sans Mono. **VHS 0.11.0** is the tested version;
0.12.0 failed to produce media in this environment. From the repository root:

```sh
vhs validate demo/needle3-dracula.tape
python3 demo/render.py
```

For container environments where Chromium requires it:

```sh
VHS_NO_SANDBOX=1 python3 demo/render.py
```

The board is needed to capture, not to render the existing recording. The tape
creates both formats and a cover. The wrapper checks that outputs were updated
and decodes each complete file with FFmpeg to catch encoding failures.

## Suggested caption

> A tiny brain for an agent watch: Needle on ESP32 selects Qwen, GPT OSS, Opus—or
> itself. External choices stop at the model name. Local tasks call Needle again
> to generate and execute real device tools. Two passes, one microcontroller.
> Actual hardware capture; waits condensed. Experimental routing policy.
