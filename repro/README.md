# Recreating this repository's ESP32 work

Clone the research branch, not `main`, which predates the three-board campaign:

```sh
git clone --branch autoresearch/decode-tps-2026-09-18 \
  https://github.com/iammrduncan/esp32-needle-3.git
cd esp32-needle-3
```

The **default** source is the last owner-accepted 5.3033 decode-tok/s engine.
The faster 6.2200 candidate was measured and host-gated but deliberately not
promoted because two frozen device cases still fail. Its exact source, the
matrix and one-layer firmware sources, and the final B2/B3 worker sources are
in [`.auto/trees/`](../.auto/trees/README.md). Run
`bash .auto/trees/verify.sh` before restoring any source overlay. The
historical baseline source is in
[`baseline-source-3dcd1de6.tar.gz`](../.auto/archives/baseline-source-3dcd1de6.tar.gz),
because that original commit is outside this branch.

For a new machine, install Git, Python 3.11+, `venv`, CMake, a C compiler and
ESP-IDF 5.5.2. The model weights are **not** stored in Git: `make model`
downloads the pinned public archive, verifies its SHA-256, and slices it to
the eight-layer/384-token model in [`model/manifest.json`](../model/manifest.json).
With ESP-IDF activated, these are the non-flashing checks:

```sh
make setup
make model
make test
python3 benchmarks/run_matrix.py prepare
python3 -m unittest discover -s benchmarks -p 'test_*.py'
bash .auto/trees/verify.sh
```

The checked-in [benchmark package](../benchmarks/README.md) has the exact
firmware binaries, model/image SHA-256 pins, twelve frozen requests, runner,
per-case results and host logit drift. Rerunning device measurements requires
three ESP32-S3 boards with 32 MB flash, 16 MB PSRAM, USB serial access and
the board mappings in `benchmarks/config.json`. The commands in that package
**flash the boards**; the checks above do not. Change the board-specific
serial IDs, device paths and lock locations for different hardware.

The original Podman recipe, host serial rules and setup notes are preserved in
[`podman/`](podman/). It was built on
`docker.io/espressif/idf:v5.5.2` and used the external
[`Hackers-in-the-Loop/pod-pi`](https://github.com/Hackers-in-the-Loop/pod-pi)
launcher at commit `c95ed7c574ab9f7a2a313bb808ea30f88e1fac7e`.
For the recipe itself, build with
`--build-arg BASE_IMAGE=docker.io/espressif/idf:v5.5.2`; the Containerfile's
generic default does not have `/opt/esp`. The original local setup note and
udev rules describe **this** host's paths and USB IDs, not universal defaults.
The two [`tools/board-pool/`](../tools/board-pool/) scripts are the exact
worker-lock and parallel-run helpers used in the container. They also contain
host-specific `/root/board-pool` paths and serial assignments.

The complete 943-run research ledger is [`.auto/log.jsonl`](../.auto/log.jsonl).
Final raw device and host-gate logs are in `.auto/final-logs/`;
`.auto/archives/` preserves earlier worker source/drafts, coordinator run
records, and clean run logs. Build caches, virtual environments and generated
model archives remain ignored because they are regenerated from the pinned
source and toolchain. Two old mixed build/board logs and 39 build transcripts
with credential-like configuration names were excluded from the run-log
archive; they are not inputs to the build or benchmark runner. Historical
commit IDs in the ledger refer to the pre-identity-rewrite history;
[`identity-rewrite-map.tsv`](../.auto/identity-rewrite-map.tsv) maps them to
current commit IDs where that history is on this branch.
