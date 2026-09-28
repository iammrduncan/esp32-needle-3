# Needle 3 on three ESP32-S3 boards: baseline and layer matrix

This report measures the same frozen agent-watch workload on the historical
1.22 tok/s firmware and the B1w3-era optimized firmware at eight layers, then
tests the latter at every requested depth from one through eight. All three
boards have an ESP32-S3, 16 MB octal PSRAM and 32 MB flash. They were assigned
different depths concurrently; the board-pool locks prevented overlapping
serial owners. The exact tasks, images, model hashes, runner and raw per-case
results are in [`benchmarks/`](../benchmarks/README.md).

## Method and provenance

- The 20-layer Needle 3 archive is pinned to
  `Cactus-Compute/needle3@9da75122d4ca11aa4a667281c9c8ba38a7eed679`
  (SHA-256 `c9d915ec…8170c38`). The endpoint-preserving slicer uses 384-token
  context. Its eight-layer output is byte-identical to the project's original
  `model/needle3.cact` (SHA-256 `bcf34a7a…305b16`). A one-layer slice takes
  source layer 0 and is diagnostic only; [upstream describes deployable
  subnetworks from two layers](https://github.com/cactus-compute/needle#readme).
- The baseline app was rebuilt from historical commit
  `3dcd1de6eb3e610908b7d04431399fe40c574417`; the comparison image is the
  preserved B1w3 6.22-tok/s candidate. The old baseline commit is not on this
  branch; its complete source tree is saved in
  [the baseline source archive](https://github.com/iammrduncan/esp32-needle-3/blob/research-archive-2026-09-27/.auto/archives/baseline-source-3dcd1de6.tar.gz).
  The exact B1w3, matrix, and one-layer source trees and provenance hashes
  are in [the final source snapshots](https://github.com/iammrduncan/esp32-needle-3/blob/research-archive-2026-09-27/.auto/trees/README.md). The matrix image differs from B1w3 by
  a single archive-bound check in the PSRAM weight-tier copy. Without it, the
  fixed 12 MB copy read past shallow model archives; host runs segfaulted and
  device boots reported cache errors. The eight-layer span is unchanged. The
  requested one-layer diagnostic needs one further zero-site allocation fix,
  so its result uses a separately identified image; depths 2–8 use the same
  matrix image.
- Twelve schema-compatible requests are frozen in
  [`tasks.json`](../benchmarks/tasks.json): timers, telemetry cadence, status,
  paired tool calls, three model routes, and an off-topic empty-call case. They
  are a subset of this repository's original [`benchmarks/prompts.json`](../benchmarks/prompts.json), not a public
  benchmark score. Exact-call accuracy requires a successful response with the
  expected ordered names and arguments, including `[]` where specified.
  Every request is counted. The separate
  [`public_probes.json`](../benchmarks/public_probes.json) contains adapted
  prompts from [JevBench's public easy tier](https://github.com/fstandhartinger/jevbench/blob/main/datasets/public/easy.jsonl)
  and [the Needle 3 weather example](https://github.com/cactus-compute/needle#readme).
  Those source schemas do not match this firmware's tools, so their results are
  explicitly not official JevBench or Needle 3 scores.
- Prefill and decode tok/s come from the firmware's own `EVT prefill` and `EVT
  done` lines, averaged arithmetically across the same twelve requests. Per-case
  timing and errors remain in the JSON; the package also computes pooled decode
  throughput from total generated tokens and total decode milliseconds. Boot,
  flashing and initial prefix priming are excluded from those rates; per-request
  `latency_ms` includes the
  serial round trip. A failed or missing case is never filled in as a pass.
- Device drift is byte-exact response text *and* generated token-count agreement
  with the patched eight-layer run on paired prompts. Host logit drift compares
  each depth to the eight-layer model over 10 fixed probe tokens and all 8,192
  logits per step; mean/max absolute difference and top-1 agreement are reported.
  This is a cross-depth model comparison, not a firmware numerical-error test.
  The separate eight-layer host-versus-frozen-golden check tests numerical
  fidelity.

## Results

The matched eight-layer comparison ran three images sequentially on board 1
with the same 12 requests and model archive. All requests completed without
retries. Rates below are arithmetic means of the per-request firmware events,
not the older campaign's best single run. The
[same-board summary](../benchmarks/results/2026-09-27-compare-v1/summary.json)
retains pooled rates; each job JSON retains every raw response.

| Eight-layer image | Decode tok/s | Prefill tok/s | Exact calls | Raw output vs optimized | Free PSRAM at boot |
| --- | ---: | ---: | ---: | ---: | ---: |
| Historical baseline | 1.2142 | 1.2467 | 11/12 | 12/12 | 13.00 MiB |
| Unmodified B1w3 image | 6.1558 | 6.5133 | 11/12 | 12/12 | 0.49 MiB |
| Optimized matrix image | 6.1492 | 6.5125 | 11/12 | 12/12 | 0.49 MiB |

The same-board decode speedup is **5.06×**, with byte-identical outputs and token
counts on all 12 cases. This does not mean 12/12 correct calls: all three images
invoke `get_status` for the off-topic `heldout_free_describe` case, whose
expected result is no call. The optimized images leave much less free PSRAM
than the baseline, part of their speed strategy. The preserved B1w3 and
archive-bound matrix images differ by just 0.0066 tok/s (0.11%) at eight layers,
confirming that the
shallow-archive fix did not materially change this rung's speed. The separate
board-3 depth-matrix repeat measured 6.1508 tok/s and reproduced all 12 raw
outputs and the same 11/12 calls.

With the optimized matrix image held fixed, the device measurements are in the
[depth-matrix summary](../benchmarks/results/2026-09-27-matrix-v3/summary.json):

| Layers | Model MiB | Decode tok/s | Prefill tok/s | Exact calls | Raw output equal to 8 layers | Free PSRAM MiB |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | 8.21 | 20.3125 | 23.4267 | 4/12 | 4/12 | 9.83 |
| 3 | 8.76 | 14.9717 | 16.6583 | 3/12 | 3/12 | 8.88 |
| 4 | 9.31 | 11.7042 | 12.9267 | 9/12 | 10/12 | 8.05 |
| 5 | 9.86 | 9.7167 | 10.5583 | 9/12 | 10/12 | 7.10 |
| 6 | 10.41 | 8.2925 | 8.9233 | 9/12 | 10/12 | 6.28 |
| 7 | 14.86 | 6.9075 | 7.3417 | 8/12 | 9/12 | 1.11 |
| 8 | 15.41 | 6.1508 | 6.5125 | 11/12 | 12/12 | 0.49 |

Every depth 2–8 returned 12/12 successful, timed responses with zero retries.
The 2-layer slice is the fastest, but its 4/12 exact-call score is not a
substitute for the eight-layer result. The drop in free PSRAM at seven layers
tracks a large step in archived model size (10.41 to 14.86 MiB), not a
measurement discontinuity hidden by interpolation.

The requested one-layer diagnostic is **not a usable deployment**. The regular
matrix image rejected its zero engram-site allocation at model open. With a
separately identified minimum-allocation image, it booted and attempted all
12 requests: 55.5767 prefill and 9.7550 decode tok/s, but **0/12 successful
responses and 0/12 exact calls**. Every generation hit `text_buffer`
truncation. Its rate describes failed generations, not useful throughput;
[the raw diagnostic result](../benchmarks/results/2026-09-27-one-layer-v1/current1.json)
and [original boot failure](../benchmarks/results/2026-09-27-matrix-v3/current1.json)
are both retained. The separately built one-layer host engine emits byte-exact
matching logits to the matrix host engine on the fixed ten-token probe, so
the minimum-allocation change itself does not explain the model's drift.

Host-side depth drift against the eight-layer slice is independent of the
device task scores. It uses ten fixed token steps and all 8,192 logits per
step; the full [host drift record](../benchmarks/results/2026-09-27-matrix-v3/host-drift.json)
includes maximum error and each top-1 token.

| Layers | Mean absolute logit difference | Top-1 agreement |
| ---: | ---: | ---: |
| 1 | 3215.782247 | 3/10 |
| 2 | 4.618430 | 4/10 |
| 3 | 3.168821 | 5/10 |
| 4 | 2.764909 | 4/10 |
| 5 | 2.222682 | 4/10 |
| 6 | 2.312140 | 5/10 |
| 7 | 1.675281 | 6/10 |
| 8 | 0 | 10/10 |

The eight-layer patched host engine also matches the project's frozen golden
logits within 0.000054 maximum absolute error (0.000005 mean), with 10/10
top-1 agreement. Cross-depth drift is not monotonic because layer selection
changes the network function, and the ten-token probe is a numerical diagnostic
only—not a task-accuracy estimate.

The extension attempted depth 9 after the complete 1–8 matrix. Its 16,734,228-
byte archive fits the model flash partition, but ESP-IDF reported
`esp_mmu_map: no such vaddr range` and firmware emitted
`ERR mmap_failed size=16734228` before model open. The runner stopped at that
first device failure; [the failed job](../benchmarks/results/2026-09-27-extension-v1/current9.json)
and [boot log](../benchmarks/results/2026-09-27-extension-v1/board3-extension.log)
are preserved. Thus **eight layers are the deepest verified on these devices**.
Depth 9 has no device rate, accuracy, or PSRAM reading; the failure is an MMU
mapping limit, not a measured PSRAM exhaustion. Depths 10–18 were sliced and
tested by the host logit probe but were not flashed after this boundary. Their
archive sizes fit the partition, whereas 19–20 exceed it.

## Adapted public-task probes

Four additional prompts were adapted from the pinned [JevBench public easy
tasks](https://github.com/fstandhartinger/jevbench/blob/d06ee95988da1350eb8ae5511daa0e06bfffe911/datasets/public/easy.jsonl)
and [Needle 3 weather example](https://github.com/cactus-compute/needle/blob/42bf1f2d0a7784b0d4d1ec94bb5ade425cf9a67c/README.md).
Only translation maps to a real route in this firmware; lights, absolute-time
reminders, and weather have no matching local tool, so the adapted expectation
is no call. These four cases are **not official JevBench or Needle 3 scores**:
their tools and labels have been changed to this project's schema. The exact
adaptations are in [`public_probes.json`](../benchmarks/public_probes.json).

The historical, unmodified B1w3, and patched eight-layer images each completed
all four requests and made the same raw outputs, but only the translation route
matched the adapted expectation (**1/4 exact**). The lights request incorrectly
produced `set_sampling_interval(60)`, the 6 pm reminder produced
`set_timer(600)`, and the Lagos weather request produced `get_status()`.
The [public-probe summary](../benchmarks/results/2026-09-27-public-v1/summary.json)
and per-case JSON for the [baseline](../benchmarks/results/2026-09-27-public-v1/baseline8.json),
[B1w3](../benchmarks/results/2026-09-27-public-v1/original8.json), and
[patched image](../benchmarks/results/2026-09-27-public-v1/current8.json)
retain the responses and timing. This small, adapted probe shows
unsupported-request overcalling, not a general benchmark accuracy estimate.

## Interpretation and limits

The matrix holds the app image, task order, context length and board class
fixed across depths 2–8; the separately labeled one-layer diagnostic needs the
zero-site allocation fix. The historical comparison changes the firmware image
at the *same* eight-layer model and workload. The first campaign's 1.2217 and
6.2200 tok/s headline numbers came from its then-current gate; this report's
matched-workload measurements are the fairer side-by-side comparison.

The task sample is deliberately small and development-derived, so its exact-call
percentage is diagnostic, not a general tool-calling accuracy estimate. The
model slices change the network function, not just compute cost; lower-depth
output drift is expected. A layer is called *device-feasible* only when it boots
and completes the task suite. The nine-layer MMU failure precludes an empirical
PSRAM-capacity claim for deeper archives. Sizes 19–20 exceed the configured
model partition and are not flashed.
