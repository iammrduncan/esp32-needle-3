# Source catalog

This catalog records what the project actually used, what each source supports,
and where a pin exists. It deliberately separates primary specifications from
comparative inspiration and project-local evidence.

## Project-local primary evidence

| Source | Role | Notes |
|---|---|---|
| [`model/manifest.json`](../../model/manifest.json) | Model provenance | Pins `Cactus-Compute/needle3` revision `9da75122d4ca11aa4a667281c9c8ba38a7eed679`, source/output hashes, eight layers, and 384-token context. |
| [`engine/include/nd_cact.h`](../../engine/include/nd_cact.h) and [`engine/src/nd_cact.c`](../../engine/src/nd_cact.c) | On-disk format implementation | Authoritative for the parser implemented here. |
| [`engine/include/nd_model.h`](../../engine/include/nd_model.h) and [`engine/src/nd_model.c`](../../engine/src/nd_model.c) | Runtime and forward pass | Current checked-out implementation; comments include many measured mechanisms. |
| [`engine/include/nd_quant.h`](../../engine/include/nd_quant.h), [`engine/src/nd_quant.c`](../../engine/src/nd_quant.c) | Quantization math and dispatch | Cactus-Quants identity, FWHT preparation, pair LUT, fallback rules, and concurrency contract. |
| [`engine/src/lut2_tie728.S`](../../engine/src/lut2_tie728.S) | CQ2 target kernel | Xtensa/TIE728 implementation and ABI. |
| [`engine/src/gemv4_tie728.S`](../../engine/src/gemv4_tie728.S) | CQ4 target kernel | mHC/logits 4-bit target implementation and ABI. |
| [`esp32/sdkconfig.defaults`](../../esp32/sdkconfig.defaults) | Target configuration | 240 MHz CPU, 80 MHz octal flash/PSRAM, cache geometry, FreeRTOS tick, optimization mode. Resolve against `esp32/sdkconfig` for an actual image. |
| [`esp32/partitions.csv`](../../esp32/partitions.csv) | Flash layout | 2 MiB app and model partition beginning at `0x210000`; requires 32 MiB flash for this model. |
| [`esp32/main/main.c`](../../esp32/main/main.c) | Firmware behavior | Model mapping, dual-core worker, prefix priming/caches, generation, serial protocol. |
| [`tools/serial_api.py`](../../tools/serial_api.py) | Host transport/API behavior | Serial attach discipline, framing, retries, HTTP routes, and two-pass orchestration. |
| [`.auto/log.jsonl`](../../.auto/log.jsonl) | Run ledger | 797 numbered records in the inspected working tree: 665 keep, 115 discard, 17 crash. Descriptions include metrics and causal findings. |
| [`.auto/ideas.md`](../../.auto/ideas.md) | Curated research narrative | Rich thematic record with corrections; chronological sections are not all current. |
| [`.auto/OWNER_ACCEPTANCE_PACKET.md`](../../.auto/OWNER_ACCEPTANCE_PACKET.md) | Acceptance/candidate summary | Best concise record of pins, gates, candidate trees, and the outstanding owner decision. |
| [`.auto/mimimodel-experiments.md`](../../.auto/mimimodel-experiments.md) | External-transfer audit | Pins the MimiModel audit commit, corrects incomparable speed claims, and records experiment dispositions. |
| [`.auto/prompt.md`](../../.auto/prompt.md) | Campaign contract | Objective, metric, frozen inputs, scope, and changing operator directives. Historical queues inside it are superseded. |
| [`.auto/golden/`](../../.auto/golden/) and [`.auto/prompts.json`](../../.auto/prompts.json) | Quality oracle | Frozen device/host outputs, logit probe, and benchmark cases. Treat edits as owner-level re-baselines. |
| [`demo/recording.json`](../../demo/recording.json) | Product capture | Saved route/tool/state/timing evidence for the seven demo scenarios. |
| Git history on `autoresearch/decode-tps-2026-09-18` | Diff-level provenance | 524 reachable commits at the snapshot; commit messages often contain the full measurement packet. |

## Model, format, and upstream implementation

### Cactus Compute / Needle

- [Needle 3 model repository](https://huggingface.co/Cactus-Compute/needle3)
  is the distribution origin. Reproduction must use the manifest-pinned revision
  and hashes, not a moving `main` view.
- [Needle model page](https://cactuscompute.com/needle) explains the upstream
  training/quantization framing. Product claims there are context, not evidence
  for this port's speed.
- [The `.cact` Format](https://www.cactuscompute.com/blog/cact-format) is the
  primary public description of the 196-byte header, nameless tensor directory,
  aligned tensor regions, and direct mapped execution. The local reader remains
  authoritative for the exact revision used here.
- [Needle 2 ESP32 engine](https://github.com/andrisgauracs/needle-2-esp32) is the
  Apache-2.0 implementation adapted by this repository. Attribution is retained
  in [`NOTICE`](../../NOTICE) and [`engine/NOTICE-LICENSE`](../../engine/NOTICE-LICENSE).
- “A Controlled Study of Attention-Only Transformers,” cited by related Needle
  work as arXiv:2607.18363, is architecture background for the Simple Attention
  Network family. It is not a specification for this exact sliced archive; the
  archive header and upstream implementation are stronger evidence for shapes.

### Comparative inference engines

- [MimiModel](https://github.com/memovai/mimimodel), pinned during the local
  audit to commit `1a19329707c5ca8b9833ef9f079c796647887eba`, supplied two
  important ideas: a handwritten ESP32-S3 TIE728 CQ2 kernel and cross-operator
  overlap. The kernel idea transferred after being rebuilt for Needle 3's
  layout; the overlap idea collided with this engine's synchronous row splitter.
- [MimiModel TIE728 audit](https://github.com/memovai/mimimodel/blob/main/docs/esp32s3-tie728-audit.md),
  [overlap audit](https://github.com/memovai/mimimodel/blob/main/docs/esp32s3-overlap-audit.md),
  and [CQ2 assembly](https://github.com/memovai/mimimodel/blob/main/needle-esp32s3/main/cq2_dot_tie728.S)
  were the concrete transfer references.
- [ESP32-AI](https://github.com/slvDev/esp32-ai), its
  [results](https://github.com/slvDev/esp32-ai/blob/main/RESULTS.md),
  [runtime](https://github.com/slvDev/esp32-ai/blob/main/runtime/llm.h), and
  [firmware](https://github.com/slvDev/esp32-ai/blob/main/firmware/esp32_tinystories/esp32_tinystories.ino)
  motivated int8 staging and activation quantization analysis. Its reported
  9.88 tok/s is not a Needle 2/3 target: most parameters are in sparse
  per-layer embeddings and its dense core is much smaller. The local audit
  rejected a direct comparison and retained only transferable runtime ideas.

## ESP32-S3 hardware and software primary sources

- [ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
  is the primary SoC source for the dual-core 32-bit Xtensa LX7 CPU, single
  precision FPU, supported 240 MHz clock, internal memory, peripherals, and
  cache options.
- [ESP32-S3-WROOM-2 datasheet](https://documentation.espressif.com/esp32-s3-wroom-2_datasheet_en.html)
  is the module source for up to 32 MiB octal flash and 16 MiB octal PSRAM—the
  N32R16-class memory capacity assumed by this project.
- [ESP32-S3 Technical Reference Manual](https://documentation.espressif.com/esp32-s3_technical_reference_manual_en.pdf)
  is the primary source for the shared I/D-cache and external-memory path. The
  project specifically relies on the configurable 64 KiB D-cache and 64-byte
  line.
- [ESP-IDF 5.5 external RAM guide](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/external-ram.html)
  documents capability allocation, cache sharing, large-working-set behavior,
  DMA restrictions, and 120 MHz PSRAM caveats.
- [ESP-IDF flash/PSRAM configuration guide](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/flash_psram_config.html)
  defines OPI/octal terminology and supported mode/frequency combinations.
- [ESP-IDF speed guide](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/api-guides/performance/speed.html)
  supports IRAM placement and warns that short microbenchmarks can vary with
  flash-cache state.
- [ESP-IDF RAM guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/performance/ram-usage.html)
  explains the IRAM/DRAM trade, task stacks, and cache-size effects on available
  internal RAM.
- [ESP-IDF FreeRTOS guide](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/system/freertos_idf.html)
  is the primary source for Espressif's dual-core SMP port. Project-specific
  row splitting is implemented above FreeRTOS and must still be measured.
- [esptool boot-mode selection](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html)
  supports the DTR/RTS attach/reset rules used by the serial harness.
- [Cadence Xtensa ISA summary](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf)
  was used for hardware-loop registers and the `ISYNC` requirement around loop
  reconfiguration.
- [GNU binutils Xtensa long-loop discussion](https://sourceware.org/legacy-ml/binutils/2019-04/msg00014.html)
  explains assembler expansion/setup costs that motivated amortizing loop setup.

The exact installed ESP-IDF source and Xtensa toolchain used to build an image
are also primary sources. Generated `compile_commands.json`, the linked ELF,
and `objdump` output are stronger than generic manuals for what that image
actually contains.

## Tooling and media

- [Charmbracelet VHS](https://github.com/charmbracelet/vhs) renders the saved
  demo. Version 0.11.0 worked in this environment; 0.12.0 did not produce media.
- CMake, Ninja, GCC/Xtensa GCC, esptool, PySerial, FFmpeg, and systemd are build
  or presentation tools. They do not establish inference correctness by
  themselves.

## How to add a source

Record the URL or local path, revision/version, access date when relevant, the
specific claim it supports, and whether it is primary, comparative, or merely
inspirational. Never cite a headline speed without model size, dense work per
token, precision, context/workload, decoder, clock/memory configuration, and
quality protocol.

