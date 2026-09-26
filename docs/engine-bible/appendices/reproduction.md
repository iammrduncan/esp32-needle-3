# Reproduction recipes

These recipes reproduce evidence classes, not one magic headline number. A
result is comparable only when the exact source/tree, model, build flags,
hardware configuration, workload, and reset/order policy match.

## 1. Record provenance first

From the repository root, save at least:

```sh
git rev-parse HEAD
git status --short
sha256sum model/needle3.cact
cat model/manifest.json
```

Also record ESP-IDF and Xtensa compiler versions, generated `esp32/sdkconfig`,
board identity, flash/console paths, app/ELF hash, boot mode signature, and free
internal/PSRAM values. For a preserved worker tree, record its own commit/diff
and engine fingerprint; main-repository `HEAD` does not identify all final
candidate trees.

Do not proceed with an unexplained dirty engine tree. Existing unrelated user
changes should be preserved and reported, not reset.

## 2. Acquire and verify the model

```sh
make setup
make model
make verify-model
```

Expected manifest identity:

- upstream `Cactus-Compute/needle3`;
- revision `9da75122d4ca11aa4a667281c9c8ba38a7eed679`;
- eight layers, context 384;
- sliced output 16,155,796 bytes;
- output SHA-256 beginning `bcf34a` (use the full value in the manifest).

The download/slicer verifies both source and generated hashes. Do not use a
moving upstream branch or regenerate quantization with different tools and
still call the result the same model.

## 3. Host build and correctness

```sh
make host
make test
bash .auto/checks.sh
```

`make test` runs Python and CTest suites. The campaign host gate additionally
expects 19/19 exact fixtures, no missing golden, the prefix-isolation check,
maximum logit delta no greater than `2e-3`, and top-1 10/10 for the frozen
probe. Inspect the actual output and exit status.

The host path validates portable arithmetic and product protocol; it cannot
execute Xtensa assembly, PSRAM/cache behavior, UART framing, or the target's
two-core schedule.

## 4. Normal firmware build and flash

Activate ESP-IDF 5.5.2, then use stable ports:

```sh
make build
make flash \
  FLASH_PORT=/dev/needle-pi/board1-flash
```

This flashes the app and writes the model partition at `0x210000`. For a known
matching model, application-only iteration is:

```sh
make flash-app \
  FLASH_PORT=/dev/needle-pi/board1-flash
```

Stop serial/API services before flashing. On cold boot, wait for both schema
prefixes to prime and a readiness line. Record boot signature and hashes before
timing.

For flag-dependent experiments use a fresh build directory and verify the
definition in `compile_commands.json`, the linked ELF, and the runtime banner.
A different command line with an identical app image is not a new binary.

## 5. Canonical device run

In the audited three-board environment the safe shape is:

```sh
needle-board run 1 -- bash .auto/measure.sh
```

The wrapper must hold that board's lock and export matching `FLASH_PORT` and
`SERIAL_PORT`. Exact wrapper installation is lab-specific. Never substitute
unverified `/dev/ttyACM*` aliases in a multi-board pool.

Expected evidence includes:

- app/model/engine signature and configuration;
- primary decode, prefill, extended, think, boot, and worst-case metrics;
- exact expected case count and `golden_missing=0`;
- device-output exact count and token delta;
- retry/reset markers;
- raw serial/flash logs and terminal return code.

A cheap screen can use the harness's primary group (historically
`AUTO_GROUPS=primary`) but is not a full acceptance run. Repeating an already
measured signature requires the explicit repeat override and a recorded reason.

## 6. CQ2 assembly differential

Perform this on the exact tree carrying the candidate kernel:

1. copy that tree to a throwaway work/build area;
2. apply only the two-line shape-sweep hoist preserved under
   [`.auto/exp96`](../../../.auto/exp96/);
3. build kbench with `NEEDLE_KBENCH=ON` and `NEEDLE_KBENCH_ASM=1`;
4. inspect linked symbols/disassembly;
5. flash the diagnostic image and capture every `KB NUM` record.

Required production-shape evidence includes the 768-wide path, other actual
row shapes, internal and PSRAM operand placement, tails, and a line like:

```text
kernel=tie1n exact=768/768 bitexact=1 maxabs=0.000e+00
```

Do not transplant another tree's complete `kbench.c`; it may reference kernels
that do not exist on this lineage. After the test, restore/flash the normal
image and prove the board responds.

## 7. Behavioral capture

Start the bridge on the measured board's own console while holding its lock:

```sh
.venv/bin/python tools/serial_api.py \
  --serial /dev/needle-pi/board3-console
```

In another shell:

```sh
make capture
```

Stop the bridge by its recorded PID and confirm no child retains the console.
The expected capture covers seven scenarios and nine flags: route match, tool
match, successful requests, no external calls, two passes for local work,
selection-only external work, telemetry progress, sampling update, and timer
expiry. Save `demo/recording.json` with app/model/schema/catalog hashes.

`make capture` uses localhost port 8081, so run only one bridge on that address.

## 8. Repeat/liveness soak

Use one already-primed image and safe attach. Send a diverse sequence followed
by identical repetitions without resetting or reconnecting. Hash the parsed
function-call payload, record every timeout/retry, and compare repeats byte for
byte. The final best-tree soak used ten requests, five distinct plus the same
five repeated; it passed 10/10 and 5/5 repeats matched.

This is a determinism/liveness test for the sampled sequence, not proof of all
request histories.

## 9. Safe serial attach/recovery

The bridge's `Device` implementation is the reference:

- create the serial object closed;
- set DTR and RTS false before opening and again afterward;
- use exclusive access;
- drain stale lines visibly before a request;
- require complete response plus `END`;
- retry the same request once after a safe reconnect;
- keep hard reset opt-in and out of a gated stateful session.

If hard reset is unavoidable, verify flash and console nodes identify the same
board, close console, keep IO0 in normal-boot state, pulse EN, then wait for both
prefixes. Never merge pre- and post-reset cases into one canonical run.

## 10. Reconstructing a final candidate

The final candidates are compositions preserved across `.auto/exp88` through
`.auto/exp96`, worker-tree fingerprints, run descriptions, and the durable
handoff—not necessarily one main-branch commit. Reconstruction should:

1. choose tree A, B, or C explicitly;
2. recover the base lineage named in
   [`.auto/HANDOFF-2026-09-28.md`](../../../.auto/HANDOFF-2026-09-28.md);
3. apply each preserved lever in recorded order;
4. verify resulting source/engine hashes where supplied;
5. build fresh and inspect the ELF;
6. run host checks, own-tree CQ2 differential, full device breadth, capture,
   and soak;
7. record that the two frozen device cases still require owner disposition.

Do not infer reconstruction from the current checkout's last-touch commit per
file; different source files can represent different points in the campaign.

## 11. Failure criteria

The reproduction has failed, rather than merely become inconvenient, if:

- a required fixture is absent;
- a case times out or is skipped;
- the flashed app/model identity is unknown;
- a reset occurs inside a purported single-session gate;
- the target kernel cannot be found in the linked ELF or dispatch;
- bit-exact/tolerance/top-1 requirements fail;
- a board/path lock cannot establish exclusive ownership;
- raw logs or terminal status are missing;
- the tree contains unpriced extra changes.

Preserve the failure with enough provenance to distinguish mechanism, harness,
board, and environment. Do not repair the golden or rerun until green without
recording the first result.

