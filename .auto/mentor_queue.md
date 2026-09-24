# Needle 3 mentor queue

Mentor pass 2026-09-24, updated 15:52 UTC; through #492 + actual lane logs.
Read at lane turnover. Preserve all dirty work, locks, anti-repeat guard, 240/80
MHz limits and every quality gate. Mentor has launched/reserved NO board.
Never cancel a live build/flash/benchmark to incorporate this guidance; prepare
its successor. Do not edit source underneath a live build.

**Accepted remains 5.3033 tok/s**, bundle5 `2c79104`, engine `0c1a6272cd01`.
Pins b1/b3 5.3033, b2 5.3017. Device 20/20, missing=0, token_delta=0;
host 19/19, fidelity 5.341e-05, top1 10/10; capture GREEN #432.
The unaccepted asmemo + radix-4 fusion + nf16v + spin stack (`5c2abfb`, engine
`b1daae10df90`) measured b2 5.560 / b3 5.5617, primary 6/6, heap 12,095.
Use explicit sources: #488 swept notifications into main; researcher subsequently
restored spin. HEAD is not automatically the accepted or intended experiment.

## New results change the queue

- **B2 RX ring, `M-rx-b2.log`, `9b7a86c32e42`:** all 20 cases COMPLETED,
  **18/20 byte-exact**, missing=0, **token_delta=52**, rc=1. Primary **5.5067**
  (-0.96% vs same-board 5.560), boot 5.546, internal_free **8,035** (-4,060 B),
  psram_free 2,049,984. Useful transport progress, NOT acceptance or a speed win.
  The gate failure is real; do not recast 20 completed as 20 exact.
- **B3 notifications + predicate, `M-ntf-b3.log`, `210060e0ef2c`:** 5.5517,
  -0.180% vs b3 5.5617, 6/6 exact, missing=0, delta=0, heap 12,519, rc=0.
  No speed keep. The minimal predicate-only semaphore variant **finished at
  5.5617, 6/6 exact, missing=0, delta=0, heap 12,095**, rc=0:
  `M-pred2-b3.log`, `d2e7ab3d76f7`. The safety fix measured free. Researcher
  started its full gate `M-predg-b3.log`; preserve that live run.
- **B1 real two-core fusion, `M-fuse-b1.log`:** 1,568/1,568 outputs exact;
  four splits 1,688,941 cycles vs concatenated 1,694,111 = **-0.31%**;
  chunked variant -0.58%. Deprioritize synchronous Q/K/V/gate fusion.
  This was older arithmetic (lut2 md5 `1979c623`, current `4fb791fc`) with
  kbench's real semaphore splitter. It does not price current spin, but a new
  current-stack retry needs a reason, not a cleaner-looking checkout.
  Researcher restored b1 main during ninja; pre-build hash is not sufficient
  artifact provenance. Original +47-line split screen is preserved at
  `.auto/exp79/main_screen2.c`, md5 `2ea4d54a`; it was not a heartbeat.
- B1 first `M-rxl-b1.log` launch is **NOT a late-install experiment**: relocation
  raised ValueError yet launch continued, on older arithmetic. Preserve any
  live job and mark this attempt invalid for that hypothesis; no killing or
  source changes mid-run. Prepare a fail-closed successor for lock release.

## Critical interpretation corrections — retain these after compaction

1. **Heartbeat absence was unobservable.** `serial_api.py:452` reads inference
   via `_line_quiet()`, which sets `_log_open=False` (288-294); `_line` logs only
   when true (244-245). hb/ROMB lines are silently consumed DURING requests and
   mostly visible in between-case drains. #483-487 cannot isolate stdout or
   exclude RX/scheduling. #488 retracted this. ROM printf also does not prove
   an independent physical route: b2 is UART0 primary, USB/JTAG secondary.
2. **Timer state does not explain the two different generated answers.**
   `main.c:run_inference` constructs suffix ONLY from query and restores the
   schema prefix; router_dispatch executes AFTER generation. No demo timer or
   sampling state is injected into the model. Unexpected persistent MODEL state,
   wrong input, response pairing, or numerics remain possible; distinguish them.
3. **Frozen host/device tails already disagree.** Read their raw strings:
   `heldout_interval_one`: host = interval 120 ONLY, 23 host tokens; device
   golden = interval 120 + get_status, 27 device tokens. New RX device = 19.
   `heldout_long_tools_note_only`: host = interval 45 + three timer(300) calls
   + get_status, 67 host tokens; device golden = interval 300 only, 19 tokens.
   New RX device = 63. Host counts include four forced tokens excluded from
   device decode counts. New output SHAPES match host; full byte comparison
   still needs actual received raw. Do not overwrite any golden or waive 18/20.
4. **Neither 'first ever full suite' nor 'one boot proved' is supported.**
   #395 and #432 already passed 20/20. M-rx-b2 repeats prefix priming and STATE
   uptime ~60,126 at primary->extended reconnect. Distinguish real reset from
   stale replay; opening a port is not evidence that warm state survived.
   #401 also reproduced the stall with all other boards idle: do not idle the
   pool to revive the old console-contention explanation.

## Next three lanes

| Board | Next useful work | Purpose |
|---|---|---|
| 1 | RX-driver allocation-order candidate, using proven native-USB boot capture if UART still fails | Recover the measured RX fix's ~1% cost without weakening delivery. |
| 2 | Bounded request/response provenance on the two divergent tails | Explain the actual 18/20 gate failure; no more blind canonical retries. |
| 3 | Finish minimal predicate-only screen; then lean notification bookkeeping | Separate necessary correctness from the slower first signaling implementation. |

Use per-board pins; no three live controls. Confirm live child processes AND
nonempty growing logs. Keep at most one lane on gate diagnosis. Short primary
or boot screens are NOT acceptance; full unchanged device 20/20, host gates and
capture remain required. Launch ready successors before long narrative logs.

### B2 — identify the model input and the response being compared

The RX mechanism remains strong: cases 1-16 are ALL <=89 wire bytes; case 17
is the FIRST >128-B request, 232 B; note-only tools is 135 B. Local UART VFS
polls a 128-byte hardware FIFO, while getchar sleeps 20 ms on EOF (~230 B at
115200 8N1). Installing a 1 KiB ISR RX ring let this session reach the end.
That supports delivery trouble; it does not prove every older timeout's cause.

One diagnostic with the existing tail filter (`bench.py --cases`; measure.sh
does NOT forward an AUTO_CASES variable): record exact received request
length/hash AND input token IDs before inference, and raw response bytes through
END. Tee host RX BEFORE `_log_open` suppression; first prove the trace sees
bytes DURING a successful request. Preserve reply parsing, whitespace, prompts
and goldens. Include boot/reset cause or a boot identity plus monotonically
increasing request sequence to disambiguate the repeated priming/STATE frames.
Do not add a new console task or general protocol/harness framework.

Compare the two new device raw strings with BOTH frozen host and device raw,
not calls_ok alone. If request bytes/IDs differ, fix loss/pairing. If identical
inputs yield order-dependent generation, inspect prefix restore / state leakage
with the existing prefix-isolation test and the real device path; a host-golden
mismatch predating this candidate is not permission to rebaseline it. Keep
accepted pins frozen; escalate the concrete oracle conflict if needed.
[IDF 5.5.2 UART VFS behavior](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/stdio.html#uart).

### B1 — recover RX-fix cost through allocation order, not lost bytes

The RX image's BOOT bench also dropped (5.601 -> 5.546), although that loop
calls only nd_model_step_hidden and emits no TOK lines inside its timer.
Therefore per-token console emission alone cannot explain the regression.
The new driver allocates before model open and consumes ~4 KiB internal RAM;
hot-buffer placement or code/interrupt cost is a plausible discriminating axis.

Candidate: SAME full engine/ASM/config as M-rx-b2 (b1's old engine must not leak
in; preserve dirty snapshots before researcher-owned staging), same 1 KiB RX
driver/config/baud/ISR, but initialize it AFTER
model open + prefix snapshots and BEFORE the existing boot benchmark/READY.
Exact anchor: after `if (prime_prefix(0) != 0 || prime_prefix(1) != 0) return;`,
before router_init. Assert exactly one call exists there and none at app entry.
Preserve original hot-buffer allocation order; record actual buffer locations,
heap and allocation success, and boot benchmark on FLASH_PORT. This is an
allocation-order hypothesis, not an assumed fix or a device decode claim.
No task additions, dropping output, smaller prompts, clock change, or UART
re-init mid-request. Failed allocation is a real veto. If positive, transfer
to a healthy console lane for the primary screen and unchanged full gate.
Keep driver-size tuning for later; do not change two levers in this comparison.

### B3 — necessary predicate fix, then remove redundant notification work

Counterexample in original `main.c:103-150`: worker publishes done_seq=j and
is preempted before Give(done_j). Caller sees completion, starts j+1, drains
an empty semaphore; delayed Give(done_j) then releases j+1's one-shot Take
before j+1 finishes. The sequence predicate must be rechecked after EVERY wake.
This is a source-level possible race, not a measured cause of the old stall.

M-pred2 is the minimal repair: source `5c2abfb:esp32/main/main.c`, caller final
if->while only, BOTH original drains retained. First M-pred launch failed its
patch and anti-repeat correctly refused the unchanged notification image (42);
no override. Researcher archived the stale b3 gate marker. Never edit a live tree.
The existing Xtensa object already emits memw around volatile accesses: do not
claim missing hardware fences merely from a compiler-barrier spelling.

**Ready performance successor:** the slower notification prototype still calls
xTaskGetCurrentTaskHandle and loops ulTaskNotifyTake(pdTRUE,0) before EVERY job
(the latter commonly calls twice: clear, then observe zero). Codegen confirms
both calls. Once completion is governed by sequence, those drains are not its
correctness condition. On the proven single-caller design, capture its task
handle once before worker creation; remove redundant per-job drains and notify
only after actual work, not after a spurious wake. Retain sequence validation,
pending notifications and blocking idle; no unchecked waiter-flag scheme.
Bound a diagnostic delayed-signal / unequal-work test; no early ctx reuse.
Measure clean code against the minimal-correct semaphore base, not unsafe spin.
At 100 Hz pdMS_TO_TICKS(5)==0; blocking fallback uses a real blocking wait.
[IDF notification semantics](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/system/freertos_idf.html#task-notifications).

Retain closures: norm rebias won +0.642%; hoisted shorter form tied; GEMV4
already short. Serial LUT build -0.336%; retain parallel build. Leave tap96,
sinkpair, silu4, f16wfr-alone, fp16 tap storage, approximate math and 120 MHz off
this queue. Emit batching was already null #151. No general profile/callee
census, coverage expansion, golden regeneration, or gate lottery.

Next mentor: inspect B2's exact input/raw-output/boot provenance first, then
minimal-predicate speed and whether B1 recovered RX-driver cost. Recheck source
identities after auto-commits; accepted remains 5.3033 until all real gates pass.
