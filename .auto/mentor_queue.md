# Needle 3 mentor queue

Mentor pass 2026-09-24 13:20 UTC; evidence through #448 and live logs.
Read at lane turnover. Preserve dirty work, locks, safety limits, quality gates
and the anti-repeat guard. Researcher implements and measures ALL three lanes;
mentor has launched/reserved NONE. These are hypotheses.

**Accepted remains 5.3033 tok/s**, bundle5 (`2c79104`), engine `0c1a6272cd01`.
Pins: b1/b3 5.3033, b2 5.3017; device 20/20, missing=0, token_delta=0;
host 19/19, fidelity 5.341e-05, top1 10/10; capture GREEN #432.
Bundle78 = asmemo + radix-4 fusion has 5.4533/5.4567 across b2/b3, ~+2.86%,
but no complete device gate. Keep this acceptance candidate without more blind
gate retries. Its internal_free is 12,103 B. Main is the accepted engine.

**Newest result:** `M-bundle86-b2.log` already contains primary **5.4350**,
prefill 5.7317, min_case 5.19, 99 tokens. Serial LUT building is **-0.336% vs
bundle78 on the same board**, although the total bundle exceeds accepted.
Do not adopt serial lutb on that total gain. B2 subsequently exited at 13:09
with TimeoutError/LANE_RC=1, no full gate. At 13:20 ALL boards are still idle;
b1's first exp79 attempt exited 8 (wrong build directory, and no flash step in
its launcher); b3's earlier attempt exited 3 on provenance. The researcher is
now preparing norm rebias. Launch b3 first, then fix b1/start b2 while it runs.
Agent context was compacted after Escape aborted the stuck turn; Pi uses
Escape to abort, Ctrl-C only clears its editor. Old M-* shells are not work.

## Next three lanes — prepare while b2 runs

| Board | Direction | Purpose |
|---|---|---|
| 1 | Correct two-core wake + Q/K/V/gate fusion screen; capture boot-only kbench output on native flash USB | A failed UART-console readiness test need not idle an output-only experiment. Secondary USB output is already enabled. |
| 2 | Now free: bounded UART input-loss discriminator, then actual fix/gate if supported | A concrete source-level mechanism may unblock bundle78's full gate. |
| 3 | Exact shorter FP16 norm conversion in shipping CQ2 assembly | Fresh hot-path arithmetic candidate, distinct from the cold C bitcast null. |

Use per-board pins, not three live controls. Confirm child processes AND growing
nonempty logs; Python buffering can hide completed primaries, so read the bounded
metric section. Launch a ready successor before writing a long interpretation.
B1 exp79 repair: correct project directory (`-C esp32`), explicitly flash the
new diagnostic after a successful build, verify its image hash, then capture.
`esptool run` only resets the old image. Convert raw cycles to us with /240.0;
the draft's /240000.0 is milliseconds. Do not label an empty capture a result.

## 1. Real synchronous projection fusion — still unmeasured

#344's 23-cycle wake and #347/#348's +0.04/+0.02% fusion used
`nd_parallel_rows`, which remained `rows_serial`: main enters kbench before
`worker_start`, and kbench never installs its own `split_rows` into that pointer.
Explicit `time_one(..., KB_SPLIT)` results elsewhere are NOT invalidated.
#444 does not replace this measurement: tap96 changed work division, chunking,
load balance and memory overlap as well as wake count. Net slowdown / 8 is not
an isolated wake cost or a lower bound; /24 is not the changed call count.
Bundle86's negative does not close fusion or notifications either. #447 correctly
retracted the parked-worker inference: worker_task blocks after EVERY job.

**Another concrete correction:** #344's raw log
`/root/board-pool/batches/20260923T0200L/b3.kb27.log` gives
`cycles_per_us_x100=23999` and `lut_build_cyc=32598`: that is **135.8 us**,
NOT 13.6 us. #443/#444's 0.127% ceiling and #447's 122-us explanation inherit
this factor-of-ten error. Also `bench_gather` allocates xh/lut with ND_ALLOC
(PSRAM), while production m->lut uses ND_ALLOC_FAST (internal). Do not simply
substitute 135.8 us as a production time either. If pricing LUT build, time the
real in_pad=768 internal operands and installed splitter. Keep data and source
conditions distinct; do not manufacture a wake estimate from the difference.

Use kbench's existing sole worker and actual `split_rows` for BOTH timed arms;
no second worker or nested dispatch. Prove core IDs and disjoint complete ranges
outside timing (n=2 serial; n>=4 both cores). Park >=1 actual tick, not
`pdMS_TO_TICKS(3)` at 100 Hz. Time caller through completion, median/range as
well as minimum; no shared volatile += completion counter.
Then reuse `bench_fused`'s 576+96+128+768 = 1,568 rows: four sequential splits vs
one concatenated job, same bytes/table/eligibility and scalar reduction order.
Compare all outputs including the cross-tensor split boundary. Use actual
tier-relative placement where possible; #353 showed scattered allocations can
reverse a layout verdict. Keep scope to this screen, then field-test a winner.
The old 64-row "interleaved" callback merely chunks concatenated row order;
do not present it as a separate round-robin scheduling mechanism.

B1 transport: sysfs identifies console as **1a86:55d3 USB-UART**, flash as
**303a:1001 native USB Serial/JTAG**; not two identical interfaces. Resolved
config has `CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y`. Under
`needle-board run 1`, after exclusive flashing ends, capture boot-only kbench
on `FLASH_PORT`, controlling reset lines. This secondary console takes NO
request input. Require actual KB results/KBENCH_DONE; if it fails too, record
that exact blocker and transfer the candidate to next turnover.
[IDF secondary-console documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/stdio.html#secondary-output).

## 2. Console blocker: received input, not another fatigue narrative

New evidence, NOT a demonstrated root cause:
- `main.c:492` polls getchar(), sleeps 20 ms on EOF, and installs no UART driver.
  Local IDF basic UART VFS directly polls the FIFO; `soc_caps.h` says **128
  bytes**. At 115200 8N1, 20 ms admits ~230 bytes.
- `serial_api.py:444` sends each whole wire request in one burst. The commonly
  first failing `heldout_long_route` is 224 prompt bytes + 7 route-prefix bytes
  + newline = **232 bytes**; note-only tools is **135 wire bytes**. Earlier
  cases fit the FIFO. This predicts missing input/newline and wrong arguments
  without an engine error. It need not explain b1 readiness or every short
  command stall; keep those observations separate.
- First/reconnect STATE frames repeat uptime ~61 seconds and the priming block
  in recent logs. Do not assert warm state survived reconnect without evidence;
  stale replay is also possible.

One bounded discriminator: record received line length/hash outside timing,
UART overflow status if available, and compare exact sent bytes for these
existing long cases. Briefly contrast burst with safely paced chunks on SAME
prompt bytes. Diagnostic timing is not a speed claim. Do not shorten prompts,
skip cases, regenerate goldens, reorder acceptance or union multiple sessions.
If input loss is confirmed, implement a product fix (small interrupt-driven RX
ring + blocking VFS, or justified lossless equivalent), count heap cost, then
run the unchanged full suite in canonical order on bundle78. Driver installation
changes I/O and needs the full gate. Do not weaken 20/20 if lossless delivery
exposes a formerly truncated frozen input. Keep other lanes discovering; bound
this investigation rather than rebuilding the harness.
[IDF UART VFS behavior](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/stdio.html#uart).

## 3. New arithmetic candidate: positive-normal FP16 norm rebias

`lut2_tie728.S:652` executes NF16V for every CQ2 row/group: ten instructions
separately extract sign/exponent/mantissa, rebias, join, then wfr. There are
130,560 norms/token from the census behind asmemo. #433's f16wfr null affected
C nd_f16 call sites, NOT this macro. #138's early conversion lost: retain early
halfword load and late conversion placement.
For **positive ordinary** halfword h, exact FP32 bits are
`(uint32_t(h) << 13) + 0x38000000`. Move exponent and mantissa together.
Using existing dead scratch registers: materialize 112 then shift 23, shift h
13, add, wfr = five instructions vs ten, no persistent register or table.
Inspect emitted code. Five saved cycles with perfect two-core balance would
be ~1.36 ms/token; this is an instruction-budget hypothesis, not measured gain
or a latency bound. Screen it rather than dismissing it on #433.

Preserve general fallback semantics: either retain sign in an exact general
normal conversion, or add sign==0 to cached eligibility before selecting a
separately guarded positive kernel. Check real archive norm signs; the current
exponent-only predicate does NOT guarantee positivity. Test actual assembled
conversion vs reference over all halfwords in-domain; reject/fallback negatives,
zero/subnormals/Inf/NaN, and compare multi-row real fixtures/split boundaries.
Keep memo geometry/lifetime reset and head4/fusion intact. Host goldens do not
exercise Xtensa assembly. Start CQ2 only; a win can later transfer to GEMV4.
Mentor read-only archive census: 46 CQ2 tensors, 351,744 stored norms, zero
negative or special-exponent halfwords. This includes the two embedding tables;
the 44 full projections account for the 130,560 hot norms/token. Static data
supports the premise but does not replace predicate/fallback tests.

## Ready reserve and retained constraints

- If true scheduling cost is material, try direct task notifications instead
  of go/done binary semaphores as a separate candidate. Preserve one descriptor,
  publication/completion ordering, blocking idle, priorities and notification
  ownership (resolved array has one slot). No spin loop/tick change.
  [IDF notification APIs](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/system/freertos_idf.html#task-notifications)
  motivate the mechanism, not a Needle speed prediction.
- asmemo cached rejections, full geometry key and close/reopen reset matter:
  stale false positives can alter arithmetic. Validate actual sources, not a
  transcribed idealization; include ASM files in provenance.
- Leave tap96, sinkpair, silu4, f16wfr-alone, condT2, fp16 tap storage, wide-load
  variants, approximate math and vendor-blocked 120 MHz off the immediate queue.
  Internal-SRAM kron transpose has no PSRAM-locality premise. No generic profile
  or callee census loops; historical cumulative timers are not additive.
- No gate lottery, anti-repeat override for stale images, or claimed 20/20 from
  absent output. `bench.py:272` compares byte-exactness only AFTER all groups:
  a timed-out suite's absence of DIVERGE does not prove even its completed cases
  byte-exact. calls_ok is not raw-output equality. Use an actual device oracle
  for the ASM candidate; an existing `--groups primary` diagnostic can compare
  six cases and must be labelled only 6/6, never acceptance. No per-group harness
  rewrite needed. Keep the full 20-case shipping gate. Preserve live jobs.

Next mentor: did b1 produce a real two-core screen on its output leg, did b3
launch norm rebias, and does b2 have actual received-byte evidence? Harvest
bundle86's final status, keep bundle78 ahead of it, and require the strict full
gate before moving accepted pins.
