# Worker tree snapshots (2026-09-28)

Each tarball is the complete `engine/src`, `engine/include` and `esp32/main` of one pool worker at
handover, named `board<N>-<PROV_ENGINE prefix>.tar.gz`. They exist because the handoff's claim that
every tree is reproducible from `.auto/exp88`-`.auto/exp96` was **not quite true**: those assets carry
individual levers, but a tree's *base* state (the seed-era stack, the shippable line's bundle, the
composed attention family) lives only in the worker checkouts, uncommitted. If a worker were lost, the
tree could not be rebuilt from assets alone.

To restore a tree: unpack over a checkout of the matching lineage (or over any worker of that
lineage), then verify with the recorded hash:

```
tar -xzf board3-<hash>.tar.gz -C <worker>
cd <worker> && cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c | md5sum
```

| board | tree | PROV_ENGINE |
|---|---|---|
| 1 | shippable + plain loop + EG2 + amortised + QK outline + `qk_hd==48` (pin 5.9283) | `efa82fec3f6a9a0407328bb98e42b6d4` |
| 2 | seed + plain loop + amortised, split LUT build, shared codebook (pin 6.1250) | `a01263caeaf7d6f9a9c05bbe767434e0` |
| 3 | composed + EG2 + compact prefix + amortised + two-deep schedule + `line_too_long` fix (pin 6.1450) | `2b7d119bdc44...` (full value in the handoff's provenance section) |

## Final campaign and benchmark sources

The branch's default `engine/` remains the **accepted 5.3033 tok/s pin**. It is
not the unpromoted 6.22 tok/s candidate used by the later layer benchmark.
The following source overlays preserve the worker-only engines and firmware
sources. Each contains complete `engine/src`, `engine/include`, and `esp32/main`,
plus any other file that differed from this branch's build or measurement path.
`verify.sh` checks the same concatenated MD5 that `measure.sh` printed in its
hardware logs. These snapshots are source, not an assertion that a fresh build
will reproduce an identical timing on different hardware.

| Overlay | Provenance MD5 | Evidence / role |
|---|---|---|
| `final-b1w3-source` | `1ada94b5b3d0c038f5f47e80006958df` | 6.2200 tok/s, 23/23 host gate; restored from the preserved B1noFB2 source plus the measured W3 wrapper and matched to `final-logs/B1w3.log` |
| `final-matrix-source` | `1bd2ba7c68ce57848a92d3406abf9612` | B1w3 plus the archive-bounded tier-copy fix; source for `benchmarks/assets/needle-matrix.bin` |
| `final-one-layer-source` | `b8b3b60013fd92c702bb5ee50f6a07f2` | Matrix source plus inert minimum engram allocations; one-layer diagnostic only |
| `final-b2resid2-source` | `d2b3c85ae2e45eee69a6de7393ea0c7d` | Final B2 worker source, matching `final-logs/B2resid2.log` |
| `final-b3p1fix-source` | `9e4ca21af947fbde703efd702f867614` | Final B3 worker source, matching `final-logs/B3p1fix.log`; its device run did not produce a complete quality-gate summary |

From a fresh clone, verify and overlay a candidate into a **separate worktree**:

```sh
bash .auto/trees/verify.sh
git worktree add --detach ../needle3-b1w3 HEAD
cp -a .auto/trees/final-b1w3-source/. ../needle3-b1w3/
cd ../needle3-b1w3
make model
make test
# With ESP-IDF 5.5.2 activated: make build
```

Use `final-matrix-source` or `final-one-layer-source` instead to rebuild the
corresponding benchmark image. Do not flash the one-layer variant as a useful
model: the measured 12/12 generations truncated. The raw final device and
host-gate logs are under `.auto/final-logs/`; earlier preserved source/drafts
and run logs are under `.auto/archives/`. The historical baseline source is
also archived there because its original `3dcd1de6...` commit is not reachable
from this branch. Historical commit IDs in the run ledger predate the identity
rewrite; `.auto/identity-rewrite-map.tsv` maps reachable old IDs to new IDs.
