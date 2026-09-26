# Candidate identity and reconstruction manifest

This appendix prevents a benchmark number from being attached to the wrong
source tree. It describes the evidence edition frozen at commit
`82a3cf1551292c43007c245e016ce281c0a0f159`, through experiment run #805.
The live autoresearch branch continued after that cutoff; later commits do not
silently extend this edition.

## The short answer

| State | Recoverable identity | What can be reproduced from this repository? |
|---|---|---|
| Accepted pin | Git commit `2c791046c224fff792383daf30f31f277136285d` | Exact source tree. The ignored model archive must be restored and verified separately; rebuilds are not guaranteed to reproduce the original app image byte for byte. |
| Candidate A / B3 | Worker engine fingerprint `391b89a4c159` | Results and several component snapshots are preserved. No complete worker checkout, full engine digest, app digest, or ordered patch series is present, so the measured image cannot be reconstructed byte-identically from this checkout alone. |
| Candidate B / B2 | Worker engine fingerprint family `cbc403eec029` | Same limitation. The fingerprint is a shortened campaign identity, not a full cryptographic manifest. |
| Candidate C / B1 | Worker engine fingerprint family `1ece8792b190` | Same limitation. |
| Run #805 derivative | Tree-C-derived worker, 5.9233 tok/s restricted screen | The causal code correction and evidence are recorded, but the measured worker image is not a complete repository snapshot and had not passed the full breadth gate. |
| Edition checkout `82a3cf1` | Full Git commit plus the hashes below | Reconstructable as a mixed research checkout. It is **not** the accepted pin or any measured final candidate. |

Therefore:

- **5.3033 tok/s belongs only to the accepted source pin and its recorded
  measurement image.**
- **6.1433, 6.1250, and 5.9033 tok/s belong only to the three preserved worker
  identities.**
- A build of `82a3cf1`, or of whichever commit happens to be current on the
  live branch, inherits none of those numbers without a new measurement.

## Accepted source pin

The accepted source is recoverable directly:

```sh
git worktree add /tmp/needle-accepted 2c791046c224fff792383daf30f31f277136285d
```

That establishes source identity, not a byte-identical firmware image. The
model archive `model/needle3.cact` is intentionally ignored by Git. Its tracked
[manifest](../../../model/manifest.json) pins:

| Field | Value |
|---|---|
| Upstream repository | `Cactus-Compute/needle3` |
| Upstream revision | `9da75122d4ca11aa4a667281c9c8ba38a7eed679` |
| Source archive SHA-256 | `c9d915eca282ed42d1a09b143b592adb4cc6744ffe2d294adf5cfc5548170c38` |
| Converted archive bytes | `16,155,796` |
| Converted archive SHA-256 | `bcf34a7aced455cfc0ee5c466e264211994a6d1e8dfd4bae8dee25e210305b16` |
| Model geometry | 8 layers, context 384 |

Restore or regenerate that exact archive, then use the build and gate recipes
in [reproduction](reproduction.md). A fresh build can reproduce behavior and
performance only after its toolchain, configuration, app hash, target, and
gate results have been recorded; the accepted Git commit by itself is not a
complete binary-reproducibility package.

## Why the edition checkout is not a performance tree

At the cutoff, comparing `82a3cf1` with the accepted pin under `engine/` and
`esp32/main/main.c` produced exactly three modified paths:

- `engine/src/lut2_tie728.S` — a later NF16V-era CQ2 implementation;
- `engine/src/nd_quant.c` — the later cross-board quantization stack;
- `esp32/main/main.c` — the later spin-assisted dual-core handshake.

It retained accepted-era or other historical versions of the remaining
files, and it did not contain the complete final-tree composition. The exact
cutoff hashes are:

| Path | SHA-256 at `82a3cf1` | Last-touch lineage at the cutoff |
|---|---|---|
| `engine/src/nd_model.c` | `c072bc504c95ee29b3f8e8d0377755cfed3ec6d747c0e8cca6c244d02841dc3d` | accepted `2c79104` |
| `engine/src/nd_quant.c` | `e6f22c36a73b6280d2753d9045386664efecad1d1071a73a008bfda7be71f9d6` | `5c2abfb` cross-board stack |
| `engine/src/lut2_tie728.S` | `febabb9c4e17b5525f046b8719315b908d873f7d7e038723c3db7ed64e459533` | `bcdf070` pre-amortized loop era |
| `engine/src/gemv4_tie728.S` | `98f412abc51fa09ddc97d7d7881ecca8f3287c5db60e0e684508d5b5dd8ee24f` | `a228dba` CQ4 walker |
| `engine/include/nd_model.h` | `d8cd16c826d44a1af095d63b446bc4de9d3aeaf5b0e76005a320fb373acae451` | `731396a` |
| `engine/include/nd_quant.h` | `f77ec1e233533d5ab713bb1e259bd80752478643d9c33111e211761c51fedcad` | accepted `2c79104` |
| `esp32/main/main.c` | `fd0f001cd3964b6c63d042b0b0914abd739b7c55e7fce9cacc44bfa5df292b4a` | `2a3b3c6` spin handshake restoration |

These hashes are an audit aid for the edition checkout. They are not a recipe
for candidate A, B, or C.

## Preserved final-candidate evidence

The durable [handoff](../../../.auto/HANDOFF-2026-09-28.md) gives the measured
worker identities and compositions:

| Tree | Board | Measured composition | Rate |
|---|---:|---|---:|
| A | B3 | seed-era stack; B4W/DOT8W/SELRES composed attention; EG2; compact prefix; plain group loop; noinline QK dot; amortized group loop | 6.1433 tok/s |
| B | B2 | seed line; plain group loop; noinline QK dot; amortized group loop | 6.1250 tok/s |
| C | B1 | shippable bundle; composed attention; RX ring; plain group loop; EG2; amortized group loop | 5.9033 tok/s |

The repository preserves useful but incomplete component material:

- [exp88](../../../.auto/exp88/README.md): compact-prefix source snapshot and
  RX-ring/secondary-console patches;
- [exp89](../../../.auto/exp89/README.md): shippable-line wide-CQ4 worker
  snapshot;
- [exp90](../../../.auto/exp90/README.md): plain CQ2 hardware group loop;
- [exp91 B3](../../../.auto/exp91/nd_model.c.qkdot8) and
  [exp91 B2](../../../.auto/exp91/nd_model.c.qkdot8.b2): QK-dot source
  snapshots;
- [exp95](../../../.auto/exp95/README.md): seed-line EG2 plus compact-prefix
  snapshots;
- [exp96](../../../.auto/exp96/README.md): per-tree amortized CQ2 loop assets
  and kbench sweep wiring.

These files overlap, originate from different workers, and are not a complete,
ordered patch series against one named base. Some are whole-file snapshots,
some are diffs, and some final components exist only as descriptions and
shortened fingerprints. Blindly copying them together can silently create a
fourth tree. The experiment ledger proves the measurements; it does not turn
the fragments into a deterministic build recipe.

## How to preserve the next candidate correctly

Before freeing or resetting a worker, export one self-contained identity
packet:

1. exact Git base commit and complete worker diff, or a full source snapshot;
2. ordered patch list when composition is intentional;
3. full SHA-256 of the source/engine tree, linked ELF, app image, model archive,
   `sdkconfig`, and relevant build metadata;
4. compiler and ESP-IDF versions plus build command;
5. board/image identity, flash layout, and clock configuration;
6. raw performance, host, numeric, device-breadth, capture, and soak artifacts;
7. the explicit promotion state: accepted, candidate, diagnostic, or rejected.

If any item is unavailable, label the result as a reconstructed derivative and
re-run every required gate. Do not use a short campaign fingerprint or a
last-touch commit as a substitute for the missing identity packet.
