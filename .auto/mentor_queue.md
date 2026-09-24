# Needle 3 mentor queue

Mentor 2026-09-24 23:24 UTC. Keep worker dirt, locks, anti-repeat history,
240/80 MHz, frozen goldens and every quality gate. No live-job interruption.

## Session-3 status at 23:25 UTC (durable snapshot)

Lanes LIVE: B1 late-ring transplant gate (M-ring-b1, engine 803f6fe390e3,
primary 5.365 during extended; CMake corrected to esp_driver_uart);
B2 composition FULL gate (M-cmpgate-b2, engine 10c74c756ee5, FRESH sig);
B3 counted-QKTILE2 screen (M-qkc-b3, engine 89af9fb83345; probe object shows
the hardware loop@11e1 with exactly 4 mul + 4 madd + 4 add and 12 lsi - the
mentor target graph IS inside the hardware loop; screen authorized and running).

LOGGED: #627 DOT8W 5.7283 b3; #628 QKTILE2 null; #629 DOT8W b2 gate (ext
5.6508, think 4.46, heap 4823, 18/20 delta52); #630 SELRES 5.7300 b3 +0.556%;
#631 COMPOSITION DOT8W+SELRES 5.7467 b3 (best device reading; beats both
parts; +0.856% on B4W; heap 4823; source preserved nd_model.c.cmp-d8sr);
#632 SELRES b2 full gate 5.7300 (ext 5.6485, think 4.46, 18/20 delta 52).
Outline attempt preserved as nd_model.c.outline-attempt-failed - PARKED, do
not repair this turn. B1 no-ring transplant result stands as primary-only
5.3717 incomplete gate. Main accepted remains 5.3033.

If this session dies: lanes are harvest-only (logs under
/root/board-pool/batches/, LANE_RC at EOF); sources all under
/root/board-pool/preserved/b1-recipeB/. No unharvested evidence beyond the
three live logs.

## Current lanes and next turnover

**Composition WON: DOT8W + selective-rescale = 5.7467 on B3**, +0.291% over
its best separate form (5.7300). 6/6 primary exact, delta 0, heap 4823, rc=0;
engine `10c74c756ee5`, log `M-cmp-b3.log`. Mentor checked the composition:
P.V helper equals the exact SELRES source; outside it the DOT8W difference is
comments only. Preserve this exact source before B3's next edit. No quality
acceptance yet; extended/think/breadth on this composition remain unmeasured.

| Board | Verified state at 23:24 | Next |
|---|---|---|
| 1 | Accepted+B4W+late RX ring `803f6fe390e3`: live full bench M-ring-b1, primary 5.3650; log advancing. | Finish this full gate; no interrupt, no added stack. |
| 2 | Composition cross-board FULL lane started, M-cmpgate-b2, `10c74c756ee5`, fresh signature. SELRES finished 5.7300, ext 5.6485, think 4.46, 18/20, delta 52, rc=1. | Harvest composition breadth; no repeated primary control. |
| 3 | Counted-QKTILE2 `89af9fb83345` in live host precheck, then screen in M-qkc-b3. Probe object /tmp/qkcount.o is fresh; current board ELF was still the prior composition at 23:23. | Verify the final built loop matches the probe; harvest new screen. |

No more outline repair attempts this turn: its failed source was saved as
`preserved/b1-recipeB/nd_model.c.outline-attempt-failed`. At 23:16 all boards were
idle after B1's natural timeout and repeated outline transcription failures;
mentor used Ctrl-C + direct redirect. B1/B2 benchmarks and B3's completed screen
are verified processes/logs, not launch announcements. Keep three distinct lanes
moving; confirmations above have specific new breadth/cross-board purposes.

## What is actually measured

**Accepted shipping remains 5.3033 tok/s**, bundle5 `2c79104`, engine
`0c1a6272cd01`, device 20/20, host 19/19. Main HEAD is NOT that engine: its
provenance is `b1daae10df90`. Faster discovery images still fail the frozen gate.

- **Selective rescale: B3 5.7300**, +0.556% over its B4W 5.6983; 6/6 primary,
  delta 0, heap 5343, rc=0, engine `f708f8e782f7`, log `M-sr-b3.log`.
  Only heads that actually rescale are swept; P.V stays four-wide unchanged.
- **DOT8W: B3 screen and B2 full gate both 5.7283**. Gain over same-board B4W
  +0.526% / +0.555%; extended 5.6508, think 4.46, heap 4823. B2 gate
  **18/20, delta 52, rc=1**, same frozen pair. Engine `5065619b0867`, #627/#629.
- B4W `51ea5246142e`: B2 5.6967 / B3 5.6983, +1.516% over discovery seed;
  full gates B1/B2 5.6967, ext 5.6138/5.6146, think 4.44, heap 5343, same 18/20.
- **Accepted+B4W transplant** `0bd9021c6136`: primary 5.3717 (+1.290%), then
  old console stall after heldout_long_route / 17 completed cases. Reconnect
  could not recover; `M-b5b4w-b1.log` ended LANE_RC=1 at 23:14. Incomplete gate,
  NOT 17/20 exact, NOT shipping, NOT a reason to repeat that no-ring image.
- QKTILE2 #628 = 5.6983, exactly B3 B4W; 6/6, heap 5343. FINPAIR #626 = exactly
  B2 B4W 5.6967. Both closed as tested. B8W #624 = -0.03%, -256 B heap; no 16-wide P.V.
- Discovery seed `61861dd9886c`: B1/B2 5.6117, B3 5.6133; #589 full gate
  18/20, delta 52, ext 5.5177, think 4.39. Never confuse it with shipping.

All batch logs above live in `/root/board-pool/batches/`. Exact model files:
`/root/board-pool/preserved/b1-recipeB/nd_model.c.{b4w,dot8w,selres,cmp-d8sr,seed}`.
Preserve snapshots before edits; do not reverse-patch dirty workers.

## B1: narrow the blocker without inheriting the unaccepted stack

Mentor independently compared B1 to git object `2c79104`: only nd_model.c
differs; CMake/sdkconfig inputs match. Accepted/main/preserved nd_model.c.seed
are byte-identical (`0d639424f636`), so B4W's model diff is portable. B1 prompts
and device goldens also match `2c79104` exactly. Worker dirt saved under
`/root/board-pool/preserved/b4w-transplant-b1/` plus b1-worker-diff.patch;
accepted extraction `/tmp/b5tree`. Keep the current hardened harness.

Now add ONLY the existing lossless RX ring to that accepted+B4W candidate,
installed LATE after model/prefix allocation, before input is read. Preserve
accepted quant/assembly/semaphore scheduler. Read the saved late implementation
in `.auto/exp84/main.c.lean-ring-clean`; copy just the console helper/includes
and late call, not the whole file. `.auto/exp88/apply_rxring.py` installs early
and must not be copied blindly. Include its required UART build dependency: append `esp_driver_uart` to the
existing MAIN_REQUIRES, as in B2/B3. `esp_vfs_dev` is a header in component
`vfs`, NOT a component name; do not add `driver esp_vfs_dev`.
The [IDF UART VFS documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/storage/vfs.html#standard-streams-and-freertos-tasks)
distinguishes default polling from driver-backed I/O; the existing local
transport fix is sufficient, no tracing framework or harness expansion needed.

This is a new performance candidate resolving a concrete measurement blocker.
Run its full frozen suite once. 18/20 would narrow the cause but still block
shipping; 20/20 would support a new shipping candidate. No changed pacing,
resets between cases, output oracle, or broader-stack reintroduction.

## B2/B3: validate and combine the two distinct wins

Selective-rescale's proven change is outside the dimension loop: both flags
true uses the original paired rescale; A-only and B-only sweep only that array;
neither does nothing. Use flags, never compare r with zero (underflow is valid).
Keep four-wide P.V exactly unchanged. The exact winning model file is ready
for B2, whose full gate supplies its own quality/extended/think values.

The measured B3 composition applied only that rescale hunk to preserved DOT8W. DOT8W changed
QK; selective rescale changed output sweeps. Their independent gains justify
one combination, without assuming percentages add. This is NOT the failed A3
rescale/P.V fusion. It beat both parts: preserve it and obtain its own breadth on B2 after SELRES
finishes. Do not repeat its restricted screen on B3. No third
fusion form or duplicate discovery lanes.

## Following turnover: one compiler question; outline stays parked until ready

**Counted QKTILE2 loop:** same two-column arithmetic, but a dedicated decreasing
pair count and independently advancing pointers, with no counter use outside.
Aim for hardware LOOP. Inspect the object before spending a board; if it still
uses bnez, read one loop dump or retire that form. No global flags or compiler
upgrade. Only a changed loop form warrants a new timing screen.

Why this is a real question: B4W's 22:54 ELF QK loop spills f13 through a1+0x460
(0x4037c4fc/0x4037c510). DOT8W STILL spills but uses an extended-LEND hardware
loop. QKTILE2's fresh loop 0x4037c4c5..0x4037c50b has no per-iteration spill,
eight shared loads, four MUL/MADD/ADD chains, but bnez and twice as many pointer
updates. Its null does not isolate spill cost. Target pair arithmetic is MUL
(second product), MADD(first product), then ADD to sum; keep that graph/order.
The pre/post f10 spill is outside QKTILE2's loop. These artifact checks are done.

GCC 14.2's [Xtensa hwloop_optimize](https://github.com/gcc-mirror/gcc/blob/releases/gcc-14.2.0/gcc/config/xtensa/xtensa.cc)
rejects loops with asm or a live iterator, among other conditions. Do NOT add
empty asm barriers while chasing a C hardware loop. This is a compiler trail,
not a proven diagnosis of this exact bnez.

**Odd-head fallback outlining:** useful but mechanically blocked. Its only
initial old-template mismatch was a two-line comment before fallback P.V.
Use exact block extraction once, no repeated transcription. A noinline helper
WITHOUT ND_HOT can move only the odd-HEAD fallback out of IRAM; the odd-POSITION
tail remains hot. Keep generic odd-head behavior and original expressions.
Prove placement and unchanged hot-loop code with the map, then measure speed
and heap. B4W paid 3072 B heap for added code overall; the fallback alone is NOT
known to cost all 3072 B. [GCC attributes](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Common-Function-Attributes.html)
and [IDF IRAM/DRAM tradeoff](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/performance/speed.html#targeted-optimizations)
support this experiment. Do not keep B2 idle preparing it; use the ready gate.

## Retained constraints and closures

- A3 `9e658137ba25` ~5.6467, same 18/20; separate rescale MUL + explicit FMAs
  fixes first A's wrong contraction, but adversarial host streams are not bit
  exact. A3+B v1/v2 (5.620/5.615) both lose; no retry without a changed premise.
- Full Q tap/norm/RoPE ownership C #615 +0.089%; small-split/callback joins,
  prepare+LUT fusion, LUT pointer/barrier variants and cold fw_scale stay down.
  Old tie2, deeper prefetch, serial LUT, QKV concatenation, norm-bias hoist,
  sinkpair, silu4, FP16 taps, tier sweeps and unsafe 120 MHz stay down. None
  establishes a universal scheduling ceiling.
- Frozen failures are generated TOOL changes, not status fields: interval=120
  loses get_status; long case changes interval=300 to 45 plus timer/status.
  Cause unresolved. Input is prefix+query; dispatch follows generation. drop=0
  and timer narration do not prove input/token identity. No owner waiver.
- Main's 24 prompts/23 host/20 device half-state must not overwrite complete
  worker fixtures. Pre-addition backup `/tmp/coverage-backup-279218` is 20/19/20;
  widened artifacts stay in exp88. Reconcile only as needed, no new coverage.
- Primary screens consume signatures; use only the ONE documented repeat
  allowance for genuinely new breadth, respecting rc=44/rc=42 and histories.
  Canonical source hash: cat src/*.c, src/*.S, include/*.h, main/*.c in that order.
  Host -ffp-contract=off cannot prove target contraction or assembly equivalence.

Next mentor: B1 late-RX transplant quality/source manifest; B2 SELRES result and
composition cross-board breadth; B3 counted-loop codegen/timing and real turnover. Check for drift into
mechanical editing, stale waits or self-declared acceptance/finish narratives.
