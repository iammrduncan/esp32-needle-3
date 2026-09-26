# Build, flash, benchmark, and debug

This project has three materially different builds: the portable host test
binary, normal ESP-IDF firmware, and diagnostic/kbench/profile firmware. Keep
their evidence separate. A host pass cannot validate Xtensa assembly or
dual-core state, and a diagnostic image is not a shipping throughput result.

## Reproducible setup

The checked-in entry points are in [`Makefile`](../../../Makefile):

```sh
make setup
make model
make verify-model
make host
make test
```

`make model` obtains the manifest-pinned upstream archive, verifies its source
hash, slices it to eight layers and 384 tokens, and verifies the 16,155,796-byte
output. `make verify-model` is the non-downloading integrity check. Never
substitute a same-named moving upstream file for the revision and hashes in
[`model/manifest.json`](../../../model/manifest.json).

With ESP-IDF 5.5.2 activated:

```sh
make build
make flash FLASH_PORT=/dev/needle-pi/board1-flash
make flash-app FLASH_PORT=/dev/needle-pi/board1-flash
```

`make flash` writes the application and then the model at `0x210000`.
`make flash-app` leaves the model partition unchanged and is valid only after
its hash/revision is independently known to match.

## Fresh-build rule

The campaign repeatedly found reused ESP-IDF build directories producing the
same image despite changed experimental definitions. Passing `-DNAME=value` to
an `idf.py build` invocation does not prove that CMake treated it as a compiler
definition or rebuilt the translation unit.

For any result that depends on flags:

1. use a fresh, uniquely named build directory;
2. inspect `compile_commands.json` for the exact source and definition;
3. make the binary print its mode/signature at boot;
4. hash the relevant object and final ELF/application image;
5. inspect symbol table and disassembly;
6. confirm the device booted that signature before measuring.

The anti-repeat guard exists because a new experiment label on an identical
firmware image is not a new experiment.

## Assembly proof

Adding an `.S` file or seeing its object compile does not prove production use.
Link-time section garbage collection can remove it; dispatch can select C; a
different ELF can be inspected accidentally. For target kernels capture:

- target assembler/toolchain version;
- compiled object hash;
- linked symbol address/size;
- caller/dispatch disassembly;
- instructions central to the claim, such as `loop`, `wsr.lcount`, and `isync`;
- runtime kernel/mode marker;
- reference differential from the same tree.

The kbench must be built with its assembly path explicitly enabled. The final
CQ2 recipe used a throwaway build with `NEEDLE_KBENCH=ON` and
`NEEDLE_KBENCH_ASM=1`, plus the shape-sweep hoist in each tree's **own**
`kbench.c`. Copying another tree's whole bench introduced an unrelated symbol
dependency and a false diagnosis.

## Board-pool discipline

Run device measurements inside the board wrapper so it owns the lock and
provides matching flash/console variables:

```sh
needle-board run 1 -- bash .auto/measure.sh
```

Use the exact wrapper installed by the lab; the command above is the preserved
campaign shape, not a portable system package. Never hardcode `/dev/ttyACM*` in
a multi-board run. Before/after a lane record board identity, source commit,
engine/app/model hashes, boot signature, free-memory fingerprint, and whether
the flashed image is normal, profile, or kbench.

Completed jobs were sometimes mistaken for live ones. A live job requires a
live process, a growing nonempty log, and eventually a terminal return code.
Do not infer status from a stale tmux pane or file name.

## Benchmark commands and gates

The campaign harnesses are:

| Tool | Purpose |
|---|---|
| [`.auto/measure.sh`](../../../.auto/measure.sh) | Build/flash/device workload and quality gate |
| [`.auto/bench.py`](../../../.auto/bench.py) | Serial suite, metrics, exact-output checks |
| [`.auto/checks.sh`](../../../.auto/checks.sh) | Host 19-case, fidelity/top-1, prefix checks |
| [`.auto/prompts.json`](../../../.auto/prompts.json) | Workloads/groups |
| [`.auto/golden/`](../../../.auto/golden/) | Frozen host/device/logit oracles |

The canonical full device suite is required for acceptance. A primary-only
screen can reject a bad candidate cheaply, but it must still enforce the
available exactness gate and must be labeled restricted. `AUTO_ALLOW_REPEAT=1`
requires an explicit reason when deliberately repeating the same image.

Check shell exit status *and* printed gate counts. Historical harness paths
could print an error or generator failure and continue into baseline checks.
Expected counts and `golden_missing=0` must be assertions, not informational
text.

## Profiling

Use coarse counters to localize a phase, then remove instrumentation and run a
normal image. The boot benchmark skips grammar and sampling; request-path
profiling is required for those costs. An early profile was invalid because
prompt-priming counters were not reset. Every counter needs a documented reset
and denominator.

Profile code changes section layout and cache behavior. Report its times as
diagnostic and quote throughput only after rebuilding the uninstrumented tree.

## Debugging order

When a board fails:

1. Verify lock ownership and kill only known bridge/monitor children by PID.
2. Resolve stable flash and console nodes to the intended board.
3. Attach with DTR/RTS false; distinguish app, bootloader, and silence.
4. Capture the full exception/reset cause and last boot signature.
5. Check that the model partition hash/extent is valid.
6. Confirm stack/heap/internal/PSRAM figures; a profiled 4 KiB worker stack once
   overflowed after KV staging, while 8 KiB worked.
7. Reproduce on another board with the same binary before calling it hardware.
8. Reproduce on a known-good image before blaming the candidate.
9. If assembly faults on return, inspect ABI frame and register windows.
10. If output diverges, run the smallest reference differential before changing
    the frozen golden.

Serial reset/recovery details live in [serial transport](serial-transport.md).

## Common false evidence

- A changed source tree paired with an unchanged app image.
- A new object whose hot symbol was link-garbage-collected.
- Disassembly from a different build directory than the flashed image.
- A `calls_ok` count with wrong argument count or values.
- A serial fallback labeled dual-core because too few row units reached the
  splitter.
- A boot benchmark labeled decode even though it omitted the sampler.
- A profiled/diagnostic image labeled shipping performance.
- A dirty worker tree attributed to one lever when it contained several.
- A union of cases from multiple boots labeled one canonical stateful session.
- A timeout/absent golden silently omitted from the denominator.

## Minimal handoff packet

Archive the commands, environment/toolchain versions, source diff, config,
compile command, ELF/app/model hashes, board identity, flash log, raw serial
log, metric summary, quality results, disassembly/reference proof, and final
classification. State how to restore a normal image; the final campaign had to
reflash boards left in kbench mode before the pool was genuinely resumable.

