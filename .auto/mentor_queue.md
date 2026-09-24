# Needle 3 mentor queue

Mentor pass 2026-09-24; updated 15:41 UTC, evidence through #488 and live logs.
Read at lane turnover. Preserve dirty work, board locks, anti-repeat guard,
240/80 MHz limits and all quality gates. Researcher owns implementation and
measurement; mentor has launched/reserved no board.

**Accepted: 5.3033 tok/s**, bundle5 `2c79104`, engine `0c1a6272cd01`.
Per-board accepted pins: b1/b3 5.3033, b2 5.3017. Device 20/20, missing=0,
token_delta=0; host 19/19, fidelity 5.341e-05, top1 10/10; capture GREEN #432.
Main now carries the UNACCEPTED asmemo + radix-4 fusion + nf16v + spin stack,
engine `b1daae10df90`: b2 5.560, b3 5.5617, primary byte-exact 6/6, heap
12,095 B. Full gate still owed. Do not confuse main with the accepted image.

At 15:32 b2 exp87 ended with TimeoutError/LANE_RC=1; no board build/benchmark
remained, while pi was sleeping 840 s. B1 last real job was the successful
split-cost screen; b3 last full gate failed. Old M-* tmux shells are not work.
Stop waiting once the lane has exited. Restore THREE different discovery lanes.
**New at 15:40:** b3 `M-ntf-b3.log`, combined notification + predicate fix,
engine `210060e0ef2c`, finished rc=0: **5.5517**, 6/6 exact, missing=0,
token_delta=0, heap 12,519. That is -0.180% vs b3's 5.5617 uncorrected spin;
NOT a speed win. Next b3 = predicate-correct semaphore base, to separate the
correctness fix from notification overhead. B2 `M-rx-b2.log` is now genuinely
running the ISR RX-ring candidate on pre-notification stack, `9b7a86c32e42`;
build 20 s, flash 7 s. Preserve it. B1 remains unlaunched: start the small
existing-screen repair, not another source census or status-only iteration.

## Priority correction: the heartbeat conclusions are unsupported

**Read the actual host reader before another diagnostic.** In BOTH main and
b2, `tools/serial_api.py:452` reads every inference response with `_line_quiet()`.
Lines 288-294 set `_log_open=False`, and `_line():244-245` prints only when that
flag is true. Unknown `EVT hb*` / `ROMB` lines are consumed without logging in
this loop. Visible heartbeats mostly come from `drain_stale()` BETWEEN cases.
Thus #483/#485/#486's absence of heartbeat lines during a timed-out request
DOES NOT show that either task stopped, exclude RX/reader trouble, or isolate
stdout. Exp87's ordinary lane log has the SAME blind spot. Retract that causal
claim in the researcher's own notes; keep the actual timeout observations.
A per-boot 16-17 request ceiling is also not proved (#480 reached 19).

`esp_rom_printf` is not by itself proof of a separate physical route: resolved
b2 config is UART0 primary, ROM serial port 0, USB-Serial/JTAG secondary. ROMB
in the UART log proves it reaches UART. Secondary config was already enabled
in earlier screens. Capture the intended leg explicitly and identify it; do
not infer routing from a function name. Two outputs stopping still need not
mean whole-app death if they share a blocked emission path.

## Next three board lanes

| Board | Work now | Why |
|---|---|---|
| 1 | Actual two-core Q/K/V/gate concatenated projection screen on the current arithmetic stack; output-only native-USB capture | #347/#348's serial-pointer null never tested this mechanism. #451 measured the toll, not fusion. B1's boot-output route works. |
| 2 | One bounded RX/observability discriminator, then the supported lossless console fix | The last hour followed an invisible heartbeat. Measure delivered bytes and request progress directly. |
| 3 | Sequence-safe direct-task-notification handshake candidate | The spin code still executes semaphore calls every job and has a possible late-completion race; its scheduling family is not closed. |

B3 turnover update: notification candidate finished; run the predicate-only
semaphore variant next. It is a changed implementation with an interpretive
purpose, not a repeated control. B3 still has `.auto/diag_repeat_ok` naming the
OLD `b1daae10df90` full gate: archive/remove that stale override before a next
launch. A new candidate does not need that exception.

Prepare the other candidates while one board runs. Use existing pins, no three
live controls. A short primary screen is only 6/6; promote only after the full
unchanged 20-case canonical device gate, host gates and capture. Do not park
b1/b3 for a claim that simultaneous console traffic changes b2's diagnosis:
#401's sole-board test already reproduced the same failure without other lanes.
Boot-only b1 and a distinct b3 candidate are useful; record concurrency.

### B2: observe the bytes the application actually receives

Add a DIAGNOSTIC bounded raw receive trace at the host's real `_line` read,
BEFORE `_log_open` suppression (timestamps, bytes/line counts and heartbeat
recognition), without changing reply parsing. First prove it observes hb lines
DURING a successful request, not just in `drain_stale`. Avoid another full
15-minute suite merely to discover the instrument is blind. One known failing
long case via the existing `--cases` diagnostic is enough to test delivery;
canonical order is still required later for acceptance/stateful output.

At the firmware request reader record received length/hash and whether newline
arrived, outside timed decode; if the newline is missing, retain progress/overflow
counters rather than waiting to print only after a complete line. Compare to
exact sent bytes. `getchar()` sleeps 20 ms on EOF; no UART driver is installed.
The local basic VFS polls a 128-byte FIFO: at 115200 8N1, 20 ms admits ~230 B.
Cases 1-16 are ALL <=89 wire bytes; case 17 (`heldout_long_route`) is the FIRST
request larger than the FIFO, at 232 B; note-only tools is 135 B. Burst-vs-paced
SAME bytes is a diagnostic, not a speed result or prompt change. If loss is
seen, use a small interrupt-driven RX ring + blocking UART VFS (or justified
lossless equivalent), price SRAM, remove diagnostics, then unchanged full gate.
Never drop TOK/JSON/RESULT/END to cure output backpressure; no new console task
or harness overhaul. A real raw trace may point elsewhere: follow that evidence.
[IDF 5.5.2 UART VFS and secondary output](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/stdio.html).

### B3: fix the predicate, then measure cheaper signaling

Source counterexample in `main.c:103-150`: worker publishes `done_seq=j`, but
can be preempted BEFORE `Give(done_j)`. Caller observes completion, starts j+1,
drains an empty done semaphore, and publishes the next descriptor. The delayed
`Give(done_j)` can then satisfy j+1's one-shot `Take(done)` while j+1 is still
running. Caller returns/reuses stack ctx prematurely. This is a possible
interleaving, NOT a measured explanation of the old console stall.

Completion must be the job sequence predicate, rechecked after EVERY wake;
semaphores/notifications are only hints. Use defined release/acquire publication
for payload and results, and inspect Xtensa codegen. The existing b3 object
ALREADY emits `memw` around volatile accesses (including each poll); do not
claim missing hardware barriers from the C spelling alone. `volatile void *s_ctx`
makes the pointee volatile, not the pointer. Preserve one descriptor, one
worker, no nested dispatch and sleeping
idle. Bound a diagnostic adversarial schedule around publication/signal plus
unequal callback durations; compare complete disjoint ranges, guard stack-ctx
reuse, then remove delay instrumentation before speed measurement.

The claim that spin removed the semaphore syscalls is false: caller still
Take(done,0)+Give(go), worker still Give(done)+Take(go,0) on the spinning path.
Try task notifications for the same single-sender/single-receiver signaling,
retaining the sequence predicate and spin budgets; check sole notification slot
ownership. A blocking fallback must actually block: `pdMS_TO_TICKS(5)` is ZERO
at 100 Hz. A predicate loop around `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`
handles late notifications; clearing alone does not. Compare with a
sequence-correct spin base, reporting corrected-base
speed as well as the incremental notification gain. No unchecked waiter-flag
scheme. Spin-budget null (#456) only closes that budget tuning; splitlow's null
(#459) cannot bound all handshake cost since it trades work balance against it.
[FreeRTOS notification motivation](https://freertos.org/Embedded-RTOS-Binary-Semaphores.html),
[IDF APIs](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/system/freertos_idf.html#task-notifications).

### B1: real fusion, same arithmetic, fewer synchronization boundaries

Reuse `bench_fused`'s 576+96+128+768 = 1,568 real rows, one shared activation/LUT,
four sequential splits versus one concatenated job. Current code already shares
activation preparation: this is NOT another LUT-reuse claim. Kbench's
`nd_parallel_rows` stays serial unless explicitly installed; use its actual
sole splitter in BOTH arms, not an additional worker. The small repair is to
install `nd_parallel_rows = split_rows` AFTER kbench's worker creation; select
`ND_KB_LANE=2`, clear old EXP30..38 flags, and confirm the fused body executes.
If carrying production
spin, first apply the sequence fix above. Prove both cores and complete/disjoint
ranges outside timing. Exact per-row reduction order, predicate fallback,
all 1,568 outputs and cross-tensor boundary coverage must hold.

Use current nf16v/asmemo, actual tier-relative blob placement and internal LUT;
scattered allocations reversed #353's layout verdict. Report caller-through-join
median/range, not only min; preserve the successful #451 FLASH_PORT boot capture
under the board lock with actual image hash and fresh nonempty log. The old
64-row 'interleaved' function only chunks concatenated order, so omit it. If
positive, field-test production synchronous fusion for q/k/v/gate and the engram
pair. Spin reduces the old toll but does not erase repeated signaling or core
imbalance. No asynchronous scratch-lifetime expansion is needed.

## Retained closures and next pass

- nf16v five-instruction rebias won +0.642%; hoisted three-instruction version
  tied, and GEMV4 already has the short form. Do not re-add either as novel.
- Serial LUT build lost -0.336% on bundle78. Keep parallel LUT build; its old
  13.6 us measurement had a factor-of-ten error and PSRAM/internal mismatch.
- Leave tap96, sinkpair, silu4, cold-C f16wfr-alone, approximate math, fp16 tap
  storage, wide-load variants and vendor-blocked 120 MHz off the immediate queue.
- No more gate lottery, coverage expansion/rebaselining, generic profile or
  canonical rereading loops. A timed-out suite does not run `bench.py`'s final
  compare, so absence of DIVERGE proves no partial byte-exact count.
- Next mentor: did raw DURING-request capture overturn the heartbeat story,
  did b3 make completion sequence-safe and actually benchmark notifications,
  and did b1 finally measure fusion with both cores? Require live child processes
  AND growing nonempty logs; verify no diagnostic config or repeat marker leaked.
