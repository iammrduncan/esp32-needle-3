# ESP32-S3 Needle 3 layer-matrix benchmark

This package repeats the frozen agent-watch tool-routing workload on three
identical 16 MB-PSRAM ESP32-S3 boards. It measures the historical 1.22 tok/s
firmware and the current eight-layer candidate against the same prompts, then
tests model slices at depths 1–8. A separate extension attempts depths 9–18 in
order and stops at the first device failure. Depths 19–20 exceed this board's
model flash partition. Depth 1 is a requested diagnostic, not an upstream
supported deployment depth; [Needle 3 documents ladder sizes 2–20](https://github.com/cactus-compute/needle#readme).

The 12 requests in [tasks.json](tasks.json) are a frozen subset of this
project's `.auto/prompts.json`, using the firmware's real `tools` and `route`
schemas. Exact-call accuracy requires the same ordered tool names and arguments;
an empty expected list means that no tool call is correct. Failed requests never
count as accurate. `EVT prefill` and `EVT done` provide firmware-measured rates;
the runner records each case's raw response, token count, timing, calls, errors,
and retry flag. Device drift means byte-exact raw output plus token-count match
against the patched eight-layer run. Host logit drift is a separate, deterministic
comparison of each depth to eight layers on ten fixed probe tokens; it does not
measure firmware numerical error. The patched eight-layer host engine is also
compared to `.auto/golden/logits.txt` to test numerical fidelity.

I examined [JevBench's public easy tasks](https://github.com/fstandhartinger/jevbench/blob/main/datasets/public/easy.jsonl)
and [Cactus's Needle 3 examples](https://github.com/cactus-compute/needle#readme).
Their intent labels and weather/smart-home tool schemas do not match this
firmware's timer, telemetry, status and model-route schemas. Relabeling them as
if they were official JevBench or Needle 3 scores would be misleading. The
frozen, schema-compatible project tasks make the baseline and every depth
comparable; the report names this workload explicitly.

## Rerun

Run inside the existing `needle-pi` container, with all three `/dev/needle-pi/*`
ports attached. The runner takes the same `/root/board-pool/locks/boardN.lock`
locks as `needle-board`; it will refuse a busy board. Flashing overwrites each
board's bootloader, app, partition table and model, but does not touch host files
outside this package's result directory. Use a new run ID for a fresh attempt.

```bash
podman exec needle-pi python3 /workspace/esp32-needle-3/benchmarks/run_matrix.py prepare
podman exec needle-pi python3 /workspace/esp32-needle-3/benchmarks/run_matrix.py run --run-id my-run
python3 benchmarks/host_drift.py --run-id my-run --max-depth 8
python3 benchmarks/run_matrix.py summarize --run-id my-run
python3 -m unittest discover -s benchmarks -p 'test_*.py'
```

To dedicate board 1 to a historical eight-layer comparison while boards 2–3
work on the optimized depth matrix, use separate run IDs and `--matrix-only`.
Run the first two commands in separate terminals, then the same-board control
and summaries after they finish:

```bash
podman exec needle-pi python3 /workspace/esp32-needle-3/benchmarks/run_matrix.py run --run-id my-comparison --compare-only
podman exec needle-pi python3 /workspace/esp32-needle-3/benchmarks/run_matrix.py run --run-id my-matrix --matrix-only
podman exec needle-pi python3 /workspace/esp32-needle-3/benchmarks/run_matrix.py run-board --board 1 --run-id my-comparison --jobs current8
python3 benchmarks/run_matrix.py summarize --run-id my-comparison
python3 benchmarks/run_matrix.py summarize --run-id my-matrix --extra-run-id my-comparison
```

The checked-in measurements use that split because the original B1w3 image
exposed a shallow-archive staging bug before the boundary fix was built. Wait
for the comparison lane to finish before its same-board `current8` control;
board locks will reject overlapping use.

The optional adapted public probes use the same pinned images and one board per
job. They are intentionally separate from the twelve-case main denominator:

```bash
podman exec needle-pi python3 /workspace/esp32-needle-3/benchmarks/run_matrix.py run-board --board 3 --run-id my-public --jobs baseline8 original8 --tasks benchmarks/public_probes.json
podman exec needle-pi python3 /workspace/esp32-needle-3/benchmarks/run_matrix.py run-board --board 2 --run-id my-public --jobs current8 --tasks benchmarks/public_probes.json
```

`prepare` downloads the pinned public 20-layer archive if missing, verifies its
SHA-256, and slices it into ignored `.runtime/benchmarks/models`. It verifies
that depth 8 is byte-identical to `model/manifest.json`. The board runner verifies
every binary and model hash before flashing and stores immutable task/image/model
identities in each result. `config.json` pins the exact prebuilt firmware images;
the historical image comes from commit `3dcd1de6eb3e610908b7d04431399fe40c574417`
(not reachable from this branch, so its [full source archive](../.auto/archives/baseline-source-3dcd1de6.tar.gz)
is checked in). The optimized images have [verified source overlays](../.auto/trees/README.md)
for B1w3, the matrix boundary fix, and the one-layer allocation diagnostic.
The `needle-matrix.bin` image is B1w3 with one boundary fix: its 12 MB PSRAM
tier-copy pad is clipped to the mapped archive length for shallow slices.
The original `needle-b1w3.bin` remains in `assets/` as the eight-layer speed
control. The boundary fix does not change the eight-layer copied span; the
package test checks byte-exact eight-layer host logits between those builds.
The one-layer slice has zero engram sites; ESP-IDF's zero-byte allocations
make the regular matrix image reject it at model open. A separate `needle-one-layer.bin`
image uses inert minimum-size buffers for that diagnostic only, leaving depths
2–8 on one fixed image. `test_package.py` also verifies that the corresponding
one-layer host build emits byte-identical logits to the matrix host build on
the fixed ten-token probe. Run it after a board is free:

```bash
podman exec needle-pi python3 /workspace/esp32-needle-3/benchmarks/run_matrix.py run-board --board 2 --run-id my-one-layer --jobs current1 --image-spec benchmarks/one_layer_image.json
```

For an additional capacity sweep after the 1–8 matrix, use a different run ID:

```bash
podman exec needle-pi python3 /workspace/esp32-needle-3/benchmarks/run_matrix.py run --run-id my-extension --extension
python3 benchmarks/host_drift.py --run-id my-extension --max-depth 18
```

`benchmarks/results/<run-id>/` contains one JSON document per job, board logs,
the run configuration, and the host-drift and aggregate summaries. A failed
depth is retained with an explicit error; it is not silently omitted from a
performance or accuracy denominator. The launcher exits nonzero if any rung
fails (including the requested one-layer diagnostic), even though its other
board lanes continue and retain their completed measurements.
