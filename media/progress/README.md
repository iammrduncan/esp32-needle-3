# Needle 3 progress film

This directory contains the code and rendered assets for the final autoresearch
progress story.

- `needle-progress.mp4` is the 18-second, 1280×720 master.
- `needle-progress.gif` is the README preview.
- `needle-progress-poster.png` is the static field-report poster.
- `render_progress.py` draws every frame and all metric typography in code.
- `needle-progress-background.png` is the text-free photographic layer used
  only by the static poster.

The story uses `.auto/log.jsonl` as its evidence source: run 1 measured 1.2217
decode tok/s, run 2 reached 2.4417, run 941 first recorded the 6.2200 best, and
run 943 is the final shutdown report. That is 5.09× the measured baseline. The
final result is described as verified but not promoted because the known
run-647 transport-sensitive device pair remains unresolved.

To re-render, install Pillow and either FFmpeg or `imageio-ffmpeg`, then run:

```sh
python media/progress/render_progress.py
```

The film is pure code-generated motion. FFmpeg is used as a codec only. The
static poster's text-free photo was generated with OpenAI's built-in image tool,
then composited with exact labels and measurements by `render_progress.py`.
