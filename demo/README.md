# A tiny local task router

A **Dracula**, 1080×1080 terminal video made with
[Charmbracelet VHS](https://github.com/charmbracelet/vhs).

- [MP4](needle3-router.mp4): 33 seconds, H.264, suitable for social uploads.
- [GIF](needle3-router.gif): same presentation, loops automatically.
- [Cover](needle3-router-cover.png).
- [Full device capture](recording.json): prompts, expected/actual calls, execution
  results, device snapshots, timings, model/schema hashes, and verification flags.

## Story

1. Ask the ESP32 for its memory usage.
2. Change its real telemetry sampling interval.
3. Start an actual countdown on the device.
4. Route one sentence to **two tools**: sampling cadence and countdown.
5. Read the changed device state.
6. Show the measured samples, timer expiry, and a known unrelated-input failure.

The model and handlers run on the ESP32. HTTP is served by the computer's USB
bridge. This is a presentation of saved hardware responses with inference waits
condensed, not a recording of real-time inference. The actual 19–41 second latency
and decode rate appear in each scene. No output is invented by the renderer.

## Capture again

With the current firmware/model and the API running:

```sh
.venv/bin/python demo/capture.py
```

This takes about three minutes and changes device sampling/timer state. The five
supported prompts must match exactly, sampling must advance, the new cadence
must apply, and the timer must expire. The sixth prompt tests an unrelated weather
question. Its misroute is retained and disclosed; it does not make the supported
workflow capture fail. Review `recording.json` before publishing a new recording.

## Render both formats

Install VHS, ttyd and FFmpeg on PATH. **VHS 0.11.0** is the tested version;
VHS 0.12.0 failed to create the media in our environment. DejaVu Sans Mono should
be installed. Run from the repository root:

```sh
vhs validate demo/needle3-dracula.tape
python3 demo/render.py
```

If Chromium cannot start in a container, use `VHS_NO_SANDBOX=1 python3 demo/render.py`.
No board connection is needed to render an existing capture. `showcase.py` reads
`recording.json`, animates the five actual routes, and displays the evidence and
known limitation. The VHS tape writes **both MP4 and GIF** and an ending screenshot.
The render wrapper rejects missing/stale outputs and decodes both files with
FFmpeg to verify them.

The two generated files are each about 0.7 MB. The repository includes them so the
demo is viewable without installing any video tools.

## Suggested caption

> Plain English → real ESP32 actions. Needle 3 routes requests to device health,
> telemetry cadence, and countdown timers—even two actions in one sentence.
> Inference runs locally on an ESP32-S3. ~1.2 tokens/sec; video waits condensed.
> Experimental: unrelated requests can still misroute.
