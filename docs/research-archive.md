# Research archive

The autoresearch campaign that produced this engine (943 logged hardware runs,
1.22 → 6.22 decode tok/s) worked in a research workspace that is not on this
branch. Everything it produced is preserved at the tag
[`research-archive-2026-09-27`](https://github.com/iammrduncan/esp32-needle-3/tree/research-archive-2026-09-27)
(commit `443b6bd`, the tip of `autoresearch/decode-tps-2026-09-18`):

| Path at the tag | Contents |
| --- | --- |
| [`.auto/log.jsonl`](https://github.com/iammrduncan/esp32-needle-3/blob/research-archive-2026-09-27/.auto/log.jsonl) | The complete run ledger. Historical commit IDs predate an identity rewrite; [`.auto/identity-rewrite-map.tsv`](https://github.com/iammrduncan/esp32-needle-3/blob/research-archive-2026-09-27/.auto/identity-rewrite-map.tsv) maps them to current IDs. |
| [`.auto/trees/`](https://github.com/iammrduncan/esp32-needle-3/tree/research-archive-2026-09-27/.auto/trees) | Verified source overlays: B1w3 (6.22 tok/s, unpromoted), matrix, one-layer diagnostic, final B2/B3 workers, and per-board snapshots, with `verify.sh`. |
| `.auto/final-logs/`, `.auto/archives/` | Raw final device and host-gate logs, earlier worker sources and drafts, coordinator run records, the historical baseline source (`baseline-source-3dcd1de6.tar.gz`), early research-code checkpoints and the LED-identification prototype (compiled, never flashed). |
| [`repro/`](https://github.com/iammrduncan/esp32-needle-3/tree/research-archive-2026-09-27/repro) | The original Podman campaign recipe, host serial rules and setup notes, built on `docker.io/espressif/idf:v5.5.2` with [`Hackers-in-the-Loop/pod-pi`](https://github.com/Hackers-in-the-Loop/pod-pi) at `c95ed7c574ab9f7a2a313bb808ea30f88e1fac7e`. Build its Containerfile with `--build-arg BASE_IMAGE=docker.io/espressif/idf:v5.5.2`. Its paths and USB IDs describe the original host. |
| `tools/board-pool/` | The worker-lock and parallel-run helpers used in that container, with host-specific `/root/board-pool` paths. |

Private Pi session transcripts and local provider configuration were never
published; neither is needed to rebuild the engine or replay the benchmarks.

## Restoring a research tree

The shipping firmware on this branch is the **matrix** source
(`final-matrix-source`, provenance MD5 `1bd2ba7c68ce57848a92d3406abf9612`), so
it needs no overlay. To rebuild another candidate, work from the tag in a
separate clone:

```sh
git clone --branch research-archive-2026-09-27 \
  https://github.com/iammrduncan/esp32-needle-3.git needle3-archive
cd needle3-archive
bash .auto/trees/verify.sh
git clone --no-local . ../needle3-b1w3
cp -a .auto/trees/final-b1w3-source/. ../needle3-b1w3/
cd ../needle3-b1w3
make setup && make model && make test
# With ESP-IDF 5.5.2 activated: make build
```

Use a separate clone rather than a Git worktree when crossing a host/Podman
bind mount: a worktree's `.git` file can embed a host-only absolute path, which
stops ESP-IDF in the container from resolving `git describe`. Do not flash the
one-layer variant as a useful model; all 12 of its measured generations
truncated.

The research branch's default `engine/` was the owner-accepted 5.3033 tok/s
pin. This branch replaces it with the matrix source; see
[the matrix firmware note](matrix-firmware.md) for how the from-source build
compares with the benchmarked image.
