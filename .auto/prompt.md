# Autoresearch: Needle 3 decode tokens/second on ESP32-S3 (N32R16)

## Active campaign -- authoritative after context compaction

This section is the source of truth for choosing work. It overrides older
"converged", "verification only", and "nothing left" notes elsewhere in the
repository. Read `.auto/mimimodel-experiments.md` completely before editing.

The hourly senior mentor maintains `.auto/mentor_queue.md` as a live source of
outside research, novel experiment ideas, and priority changes. Read it at every
lane turnover and incorporate its strongest runnable directions into the active
queue. It does not cancel an experiment already building or running, and measured
evidence may override it, but do not silently ignore it: record why a suggested
direction was selected, deferred, transformed, or retired.

## CURRENT STATE AND QUEUE -- 2026-09-23T12:35Z (runs #375-#382)

**Accepted runtime: 5.1050 decode tok/s (+109.1 %)** - run #379: two FWHT groups transformed in one
stage walk (`nd_fwht2`) on top of run #378's within-stage butterfly unroll. Both came from ONE
mechanism, which is now the campaign's sharpest tool: **put independent work in flight inside a serial
dependency chain whose operands are already resident.** Readings: #378 fw2 +0.461 % (5.0567 -> 5.0800,
three boards, three builds, zero spread, free at the heap); #379 fw2pair +0.492 % (5.0800 -> 5.1050,
5.1050/5.1000/5.1050 on two boards), internal_free 13,623 (-256 B), extended 5.031, prefill 5.39,
think 3.99, boot bench 5.151, min_case 4.87 (best worst case in 382 runs), 17/17 device + 19/19 host
byte-exact, fidelity 5.341e-05, top1 10/10. **`make capture` is owed on this image** (shipping code
changed twice since the last green capture).

**The mechanism is bounded, and measured on both sides.** Interleaving *reductions* costs: cond2
(two 768-term conditioning sums) -0.394 % and cond4 -0.427 %, monotonically worse with more chains
(#380/#381), because that loop is bound by its strided `cond_v` loads and the interleave only added
register pressure. And fw2single - pairing with NO within-stage unroll - ties the accepted image
(+0.033 %, one tick, #382), so the two transform levers are one mechanism, not two. Rule before
spending a board on any "more ILP" candidate: ask what the loop is bound by. Resident operand +
pure arithmetic pays; strided loads do not.

**Three lanes in flight (batches 20260923T1630L/1700L).** `fw3pair` - three groups per walk, which is
*exactly* the per-core group count, so the paired path covers everything and the single-group tail
never runs (5.1100 primary in hand = +0.098 % over accepted, sub-bar); `fw2single` - logged as #382;
`fwres2` - one rescale pass over both adjacent paired groups instead of two (one-line diff, bit-exact,
host 19/19 + odd-split guard green).

NEXT THREE: (4) log `fw3pair` and `fwres2` with their gates. (5) If both are independently positive
but individually sub-bar, build and measure **`fw3res`** (three groups + one rescale over 3g) as ONE
change - the same precedent as run #371's bundle, where two measured sub-bar halves of the same phase
crossed the bar together; if instead either is null, the transform family closes at ng=2/inner=2 and
the remaining above-bar items are owner decisions only (phi row residency +0.39 % gated on the #293
assertion-level RAM, 120 MHz +3.51 % vendor-blocked, and the bar itself). (6) Run the owed
`make capture` on the accepted image, on board 1, while other boards screen.

**Harness and method facts paid this window.** (a) A paired-transform loop guarded by `gi + ng < g1`
never executes when ng exceeds the per-core group count (ngroup=6 split 3+3 gives each core exactly
THREE groups), so it silently re-measures the accepted path - the guard is `gi + ng-1 < g1`, and ng=4
is not a curve point on this geometry while ng=3 is. (b) A candidate generator must pin and md5-assert
its BASE file: once fw2pair was accepted, the tree no longer contained the text the pair generator
matched, and a stale base would have produced a "candidate" identical to the accepted code. (c) A
`&&`-chained `cp` of a candidate that failed to generate silently tested the accepted tree and printed
19/19 - the md5 of the tested tree is part of the evidence. (d) The same generator emitted a literal
channel offset inside a loop over channels; the host golden caught it and
`.auto/exp41/test_cond_equiv.c` now guards it. (e) An edited `CMakeLists.txt` in a worker carried
`-O3` into three later lanes (run #377) - `board_config_check.sh` now hashes build files across the
pool. The reusable guards are `.auto/exp41/test_cond_equiv.c` and `.auto/exp41/test_odd_split.c`, the
latter covering the 3+3 split the host's `rows_serial` can never produce.

## Superseded queue -- 2026-09-23T11:20Z (runs #367-#374)

**Accepted runtime: 5.0567 decode tok/s (+107.2 %)** - run #371's bundle: `sigmoidf_pair` moved into
IRAM with `ND_HOT` (+0.18 % mean, #336/#342) plus `fwht_rows` loop-invariant load hoisting
(+0.065 %, #365), applied as ONE change. **5.0567 on all three boards from three independent fresh
builds, per-variant board spread exactly zero**, extended 4.985, prefill 5.3367, think 3.96, boot
bench 5.103, min_case 4.83 (best worst case in 374 runs), 17/17 device + 19/19 host byte-exact,
fidelity 5.341e-05, top1 10/10, `internal_free` 13,879 (-768 B), `make capture` rc=0 with all nine
flags on this image. Disclosed marginal call: +0.198 % is inside the metric's own 0.034 % quantum of
the 0.2 % bar, kept on zero board spread plus three agreeing secondaries - see the run's own log line.

**The combination lever is now spent, and that is a measured statement.** Combining was legitimate
because two levers worth >= 0.06 % each had been individually bar-blocked; what remains unshipped is
`zchead` (+0.032 %), `hadascale` (0.000 %), `kvstore` (0.000 %), i.e. ~0.03 % in total, so no further
combination clears the bar and none should be built.

**Experiment 38 refuted a load-bearing diagnosis (run #374).** #364's closure blamed cache warmth for
a +36.75 %-isolated / +0.000 %-field transform. Measured: on the operand the field actually uses
(`fwht_rows` transforms `c->xh`, `ND_ALLOC_FAST` internal SRAM, which this part does not route through
the data cache) a 160 KiB PSRAM eviction changes the cost by *exactly nothing* - 47,470 cycles warm and
cold; a PSRAM operand pays only +8.98 % cold, and warm PSRAM equals internal exactly. So warmth
explains at most ~9 points of 36.75, and zero of the field case. Two replacement rules replace the too
coarse "a warm kbench number prices nothing": (1) for an internal-SRAM operand, cold-isolation is
inapplicable, so a warm screen is not cache-inflated; (2) what a tight back-to-back bench loop inflates
is *instruction mix*, so screen a per-call kernel with the field's call structure (one call per group,
immediately followed by its consumer), not with a cache sweep.

**Experiment 39 closed the last build-config axis (runs #372/#373).** `-O3` on the needle component
(every engine file has always been `-O2`, verified in `compile_commands.json`) is **+0.032 % in
isolation and -0.166 % stacked on the bundle** - both bases converge on 5.0483. Compiler policy and
the campaign's hand placement are alternative policies for the same decisions, not additive levers (the
same shape as #335's `always_inline` losing to moving the callee). Engine stays at `-O2`; the
build-configuration family - historically the most productive cheap one - is now exhausted.

**POOL CONTAMINATION FOUND AND FIXED (run #377), and it is a class, not an incident.** Experiment
39 applied `-O3` by appending to the *worker's* `esp32/components/needle/CMakeLists.txt` and no lane
removed it, so every lane that subsequently ran on boards 2 or 3 measured candidate + `-O3`. Affected
verdicts: run #376 (`fw4`) and run #375 (`fwinline`) are both confounded - the 6,144 B heap figure
belongs to `-O3`, not to the unroll, and `fwinline`'s -0.166 % is `fwinline+(-O3)` (which equals
run #373's `o3bundle` reading exactly, i.e. it added nothing under -O3; clean at -O2 it is untested and
re-queued). The accepted bundle's triple reading (batch 20260923T1145L) predates the -O3 edit and is
unaffected. Fixed by restoring both workers and extending `.auto/board_config_check.sh` to hash
`esp32/components/needle/CMakeLists.txt`, `esp32/main/CMakeLists.txt` and `.auto/bench.py` against main
for all three boards (`BUILD_FILE_DRIFT`), because `sdkconfig` has had a drift guard since #300 while an
edited CMakeLists had none. Standing rule: when two runs of *different* code agree on a number that is
supposed to be incidental, suspect shared state before believing the agreement.

**RUNNING: batch 20260923T1430L** - board 1 `silu4` (SiLU 4 elements per iteration, host 19/19
byte-exact; the next point on the pairing curve that shipped +0.200 % at #290, on the phase with the
most remaining non-GEMV mass), boards 2 and 3 clean `fw2` re-confirmations at -O2 after the withdrawal., each
host-proven 19/19 byte-exact before any flash (the host compiles this same C, which is how the #363
transcription class is caught without a board), one lever each, all bit-exact by construction because
butterflies in one stage are disjoint pairs:

| board | variant | lever |
|---|---|---|
| 1 | `fw2` | butterfly inner loop unrolled by 2 - #360's +17.95 %, which #364's refuted diagnosis killed |
| 2 | `fw4` | the curve point; unroll-8 lost -28 % in the rescale, so the register-file ceiling is a known risk |
| 3 | `fwinline` | `nd_fwht`'s body inlined into `fwht_rows` through ONE shared static-inline definition, removing a call per group (~96/token) - run #333's third category |

NEXT THREE after E40: (4) log whichever of the three wins or fails and cross-board only a winner.
(5) The transform is ~80 % of `prep+lut` (3.5 ms of a 158 ms bench token), so if `fw2` pays, price
`fw2`+`fwinline` together - those two mechanisms are independent (grouping vs call removal) and this is
the one remaining place where the campaign has two measured sub-bar halves of the same phase, which is
the combination that just shipped the bundle. (6) If all three are null, `nd_fwht` is closed from every
side and the queue is empty: what remains above the bar is owner decisions only - phi row residency
(+0.39 % ceiling, ~18 KB/core against 13,879 B, gated on the #293 assertion-level RAM), 120 MHz
(+3.51 %, vendor-blocked #331), and the bar itself.

Harness facts paid this window: `bench_e38` is `static` with one call site so `-O2` **inlines** it and
an `nm` symbol probe refuses a perfectly good image - for a macro-gated candidate the proof is a string
only that body prints, plus the macro on that file's own `compile_commands.json` line; `idf.py` must run
from the project directory and has no `--before/--after`; a lane that depends on files in another
tree must sync and hash them itself (three refusals were my per-board manual sync, not the guard
misfiring); `tmux new-session` inherits the *server's* environment, so lane env goes inside the pane
via `env VAR=... cmd`; killing one stale pane session is required before a board's lock frees - never
`kill-server`; and `measure.sh`'s exit-42 anti-repeat guard correctly refused a fourth measurement of
an image that already had full-suite verdicts on two boards, which was manufactured work on my part.

## Superseded queue -- 2026-09-23T10:05Z (runs #360-#366)

**Accepted runtime: 5.0467 decode tok/s (+107.2 %)** - run #362's `fwht_rows` rescale unroll, confirmed
on two boards. Active batch `20260923T1000L`: three distinct `nd_model.c` elementwise unrolls (board 1
`kvstore`, board 2 `zcsplit`, board 3 `zchead`), each host-proven byte-exact before flashing.

Three closures and one schedule debt belong in whoever's hands this file is next:

1. **CLOSED OFF-DEVICE, no board spent - the per-element `lrintf` in `kv_store_int8`.** The accepted
   ELF proves `kv_store_int8` (IRAM, 0x4037b228) calls **flash-mapped `lrintf`** (0x42013f48) once per
   element, ~1,792 elements per decode token - the callee-placement class worth +0.18 % in #336. It is
   not reachable: `frint.nf`, `frint.z.f`, `frint.xf`, `frint.mf`, `frint.pf`, `itrunc.s` and `quou.s`
   are **all rejected by the shipped assembler** - the ESP32-S3 TIE FP engine implements no rounding
   instruction at all. So dropping the call needs software float<->integer transfers, which this
   campaign has measured twice as expensive (#230, #292) and once as a 3.9x loss against a ROM helper
   that was already at the FMA-chain floor (#338). Premise retired with the ISA fact, not with a guess.
   Revisit only if a tie-correct sequence is ever written *and* proven over dense exact-.5 ties - the
   naive `+0.5f` and truncate is forbidden: it is round-half-away, and run #2e was rejected for less.
2. **The scheduler debt I owe myself:** `tmux kill-server` destroyed two in-flight lanes at 08:59Z
   (logs stopped cleanly at `flash_s=6`, no `MEASURE rc`). It also killed the session the harness runs
   in. **Kill lanes by session name (`tmux kill-session -t e36n`) - never the server.** No bad data
   resulted: the images were built and flashed correctly, so both lanes were re-measured without paying
   a redundant flash.
3. **A generator's assertions are what make generated code trustworthy.** The KV variant asserted on
   the shipped loop body; the first version failed because the store is `(int8_t)lrintf(q)` and not
   `(int8_t)q` - which is exactly the failure mode (a hand-typed Markstein fixup differing in a range
   the goldens never reach, #230) the assertion exists to catch. A confusing `accum_loops_untouched=3
   vs 4` turned out to be my own probe counting a comment, verified with a real diff.
4. **`make capture` is still owed** (shipping code changed at #362). The first attempt aborted
   correctly: `idf.py` run from the worker root fails ("CMakeLists.txt not found", build_rc=2), and the
   lane now refuses to capture a stale image instead of carrying on. Use the repo's own target:
   `make -C <worker> flash-app FLASH_PORT=$FLASH_PORT`.

NEXT THREE after the active batch: (4) run the owed `make capture` on the first free board. (5) If
`zcsplit`/`zchead` pay, batch the remaining independent emit loops (cond fold, lane mix, `pool_cell`,
dequant rows) as one three-board batch; if they are null, the elementwise-unroll family is closed at
one win and the remaining candidates are the sub-bar banked pair (`sigmoidf_pair` IRAM +0.18 %,
`fwathoist` +0.065 %) which need an owner bar decision, not more measurements. (6) Cold-isolate kbench
(160 KB PSRAM eviction before each timed pass, the form #350/#352 used) - run #364 showed a warm screen
can be +36.75 % in the lab and +0.000 % in the field, so every warm number in this ledger is an upper
bound at best.

## OPERATOR REDIRECT -- 2026-09-22 -- overrides every later `NEXT`, `closed`, and stop note

## THREE INDEPENDENT BOARD LANES -- OPERATOR DIRECTIVE 2026-09-22

The owner supplied two additional boards to triple experimental throughput. The
normal discovery topology is therefore **three different experiments at once**:

- board 1 = candidate/experiment A;
- board 2 = candidate/experiment B;
- board 3 = candidate/experiment C.

Do **not** routinely spend board 1 on an unchanged live control, and do not put
the same candidate on boards 2 and 3 during discovery. Each board already has a
pinned accepted-image baseline and the campaign has established negligible
board drift. Compare a discovery result with that board's pinned baseline first.
Only after a candidate clears the keep bar should a later batch cross over or
repeat the winner on another board. While that confirmation runs, the remaining
board(s) must continue screening new candidates whenever independent work is
ready.

A contemporaneous control or duplicate candidate is an exception reserved for
a result whose interpretation genuinely depends on temperature, board identity,
or observed noise; record that reason before taking the board. It is not the
default batch shape. Log every distinct candidate and A/B result separately,
even when several belong to one broad research theme. Verify launched jobs by
checking live processes and non-empty logs within 10 seconds; a printed PID is
not evidence that a board is working.

Immediate three-lane assignment for the current 4-bit work: integrated plain-
load GEMV4 assembly, forced-inline C `dot_group`, and the wide-load register-
fill-order/correctness probe or corrected wide-load kernel. Do not allocate an
unchanged-control lane for this discovery batch.

## PERFORMANCE WORK CONTINUES -- OPERATOR DIRECTIVE 2026-09-23

The owner explicitly rejects the later self-declared "stop condition" and wants
continued performance experimentation. Run #345/#346 closes the response-pairing,
generation-limit, think-ack, and expanded-golden work. Preserve that completed
fix once, then stop expanding prompts, goldens, verification machinery, canonical
readings, or measurement-authority prose unless a concrete performance candidate
cannot be measured without a fix. Correctness work is not the active research
objective now.

Keep all three boards occupied by **distinct performance hypotheses**. If an
end-to-end candidate is not ready, run a focused device microbenchmark or ABI/
instruction probe for that lane; an idle board is not acceptable while a
measurable hypothesis exists. Start the next three lanes immediately. High-value
directions include independent scheduling/fusion experiments around the dominant
CQ2 work: (A) fuse or concurrently schedule the engram key/value CQ2 projections,
(B) batch/interleave independent Q/K/V/gate CQ2 projections across cores instead
of paying sequential barriers, and (C) test a multi-row/two-stream CQ2 assembly
schedule that can hide PSRAM latency or amortize row overhead. These are starting
directions, not analytical closures: screen them on real archive bytes, enforce
bit equality, and replace a disproven lane with a new performance hypothesis.

Do not declare the campaign complete merely because the current phase table is
attributed or a predicted gain is below the old 0.2% bar. Measurement, not prose,
retires a runnable idea. Within ten minutes of reading this directive, either
three board jobs must be live with non-empty logs, or the log must name a concrete
per-lane build/hardware blocker and immediately substitute another experiment.

Pipeline discipline is part of the experiment system: maintain at least six
ranked, runnable hypotheses (three active plus the next three). While a board
batch measures, prepare the patches/scripts for the next batch. When a lane
finishes, launch its replacement before writing the long interpretation. Check
live PIDs and log growth after launch and at each tool boundary. More than two
minutes of avoidable board idleness is a scheduler failure to correct immediately.
Never occupy only one board with harness/correctness work; if such work is truly
blocking, the other two boards continue independent performance screens.

The accepted shipping image is **5.0117 decode tok/s** on the frozen workload, with 17/17 device
and 16/16 host byte-exact output, token delta 0, fidelity 5.341e-05, top1 10/10, extended 4.9390,
think 3.93, prefill 5.2867, min case 4.79, and 15,215 bytes internal free. The speed crossing came
from Experiment 16 (+1.01 %), paired elementwise sigmoid (+0.20 %), and the exact Sinkhorn exp(0)
skip (+0.20 %). Experiment 19 also measured 5.1667 at 120 MHz (+3.51 %) but remains a diagnostic
until its documented temperature/stability risk is addressed.

Runs #302-#329 were an invalid verifier loop: the same accepted image was measured 28 consecutive
times only to satisfy the autoresearch extension's per-iteration mandate. **Do not run or log the
accepted image alone again.** `.auto/measure.sh` now enforces this before taking a board: it hashes
the actual shipping inputs plus resolved sdkconfig and exits 42 for any previously measured image.
A genuinely new candidate gets one explicit confirmation only with `AUTO_ALLOW_REPEAT=1` and a
non-empty `AUTO_REPEAT_REASON`; the accepted image's repeat budget is already exhausted. Do not
delete or bypass the signature history. Controls run only inside concurrent candidate batches.

**Experiment 20 is MEASURED and CLOSED with no candidate (runs #330-#331).** Step 1 retired the
premise: run #204 is the `-DNEEDLE_LUT2_ASM=OFF` ablation, no `+3.8 %` / 10.4 KiB residency variant
exists anywhere in git history, and the 24,576 B pair table has always been internal RAM
(`nd_model.c:642` + `ND_ALLOC_FAST`). The kbench residency construction that does exist
(`blob_int`) was rebuilt, made buildable again (the bench's GDMA half is now behind
`ND_KBENCH_GDMA`; the broken `-lesp_hw_support` line is gone, so a shipping configure never sees it),
and verified: internal- and PSRAM-backed operands are **bit-exact** row-for-row on both boards, and
the real-capture fixture still replays 256/256 bit-exact on the host. Cold-state kbench, two boards,
min of 25 rounds, shipping `tie1n`: every cold PSRAM pass costs **42.93-42.94 cycles/word** whatever
its shape, internal-RAM operands cost **37.70** whatever its shape - i.e. a flat **13.90 % delivery
tax on every 2-bit pass**, worth **+6.0 % decode**, not +3.8 %; and taking the pair table *out* of
internal RAM costs +14.3 % to +31.9 %, which prices the residency the build already has. None of it
is collectable: 3.59 MB/token of 2-bit weights against 6,200 B (level 1) or 8,248 B (level 0), and
the only per-token form - copy into a window - is #287's measured -17.9 %, because the copy shares
the octal bus with its consumer. Full ledger: `.auto/ideas.md`, "Experiment 20". Accepted runtime is
unchanged at **5.0117 decode tok/s**; no board ran the shipping image this experiment.

**NEXT -- Experiment 21: make the measured 120 MHz result reliability-testable** (the queue below is
unchanged; #289 already proved the +3.51 % speed twice, do not re-measure it).

**Experiment 21 is MEASURED and CLOSED, blocked by vendor support (runs #331, board 1+2).** The
+3.51 % at octal 120 MHz cannot be made reliability-testable with IDF 5.5.2 on this board: with
`SPIRAM_SPEED_120M` + `ESPTOOLPY_FLASHFREQ_120M` + `IDF_EXPERIMENTAL_FEATURES` plus IDF's own
mitigation `SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR`, PSRAM comes up at 120 MHz and then
the app aborts forever - "The flash model has not been verified support this feature", init function
failed 0x106 (ESP_ERR_NOT_SUPPORTED), named by addr2line as
`__esp_system_init_fn_psram_adjust_timing_point_via_temperature`
(`mspi_timing_by_mspi_delay.c:882`). Reproduced on two boards, so there is no bootable safe-120 MHz
image to soak. ECC does not fit either (~1.09 MB of parity; `ERR prefix_cache_allocation`), and the
workload's own die swing is ~5 C against IDF's ~20 C failure axis, so a self-heating soak would have
measured the wrong axis. 80 MHz stays the production default on measured grounds. Kept, default OFF
and proven free (two 5.0117 canonical runs with it off): `esp32/main/thermal_diag.c` behind
`NEEDLE_THERMAL_DIAG` - die temperature plus a CRC32 over a live 256 KiB PSRAM buffer every 5 s.

**ACCEPTED RUNTIME IS NOW 5.0300 decode tok/s (+106.1 %)** - Experiment 22's integrated
plain-load CQ 4-bit row walker (run #333, +0.365 %, 17/17 device + 16/16 host byte-exact,
fidelity 5.341e-05 unchanged, internal_free 14,903). The earlier 5.0117 figure in this file is
the *pinned discovery baseline* for the lanes, not the shipping value.

**NEXT (in this order).** (1) `make capture` is due - shipping code changed at run #333.
(2) Cross-board confirmation of 5.0300 on board 2 (discovery -> confirmed); board 3 keeps
screening. (3) The banked multi-row differential for `nd_gemv4_rows_tie1` (row-range kernel
vs N per-row calls of the proven single-row kernel, real phi bytes): run #333's first build
diverged (5/17 byte-exact, `DEVICE_OUTPUT_DIVERGED`, and *faster*) because the row epilogue
double-stepped the packed and norm cursors, and no primitive test could see a multi-row
cursor - build that test before touching the kernel again. (4) Then re-derive the phase map
(`AUTO_PROFILE=1`): phi dropped from ~9.2 ms of GEMVs toward ~8.1 ms, so the ranking of what
is left (2-bit GEMV at its floor, attention, MLP, engram) should be re-checked against a
197 ms boot bench instead of 198.

Closed by Experiment 22's lanes, do not re-open: forced-inline C `dot_group` (+0.10 %, below
bar, -1,024 B heap); `ee.ldf.64/128.ip` wide loads (probe A returns exactly probe B's answer,
so nibble identity is lost before the float loads - no register reorder fixes it, see
`.auto/ideas.md` 22C). Harness facts that cost real time this window are recorded in
`.auto/ideas.md` (the `measure.sh` `set -euo pipefail` + missing-`sdkconfig` silent exit that
killed four lanes; tmux windows must run `needle-board run N --` *inside* the window; an
unreferenced new `.S` is garbage-collected and the run silently re-measures the accepted
runtime - `nm` the ELF for the candidate symbol before believing a number).

**Superseded NEXT (Experiment 24 as originally written): integrate the measured bit-exact 4-bit row kernel and price it end to
end.** The isolated screen (run #332) says one handwritten row walker per row - no TIE instructions,
plain `lsi` - is **bit-exact on real archive bytes and +11.6 %** (5.647 vs 6.338 cycles/weight),
worth ~1.07 ms/token = **+0.53 % decode**, because the shipping build calls `dot_group` once per
group instead of inlining it. Steps: gate it like `nd_lut2_asm_ok()` (g == 128, ordinary-fp16 norms,
4-byte alignment), differential-test on the real 4-bit fixture in `.auto/exp22/`, then a three-board
batch; watch `internal_free` (~200 B of IRAM). Full detail in `.auto/ideas.md`, "Experiment 22".
Also open, cheaper and separate: determine the register fill order of `ee.ldf.64/128.ip` - the two
wide-load variants measured +21 %/+25 % but fail the known-answer probe (63.0 against 64.0), so their
speed is a dropped term, not a win; if the order is fixable there is another ~10 % above the plain
form. Experiment 23 (race-free private 4-bit folded tables, 16 KiB) still waits on the owner's
assertion-level decision (#293) for the RAM.

For every iteration, implementation, fixture work, differential testing, disassembly, profiling,
and off-device screening all count as research progress. The generic instruction to call
`run_experiment` does not permit a control-only run. If a candidate is not ready, keep working on it;
the executable guard is intentionally the final authority.

**CURRENT STATE (run #331, authoritative).** Experiment 20 closed with the measured 13.90 % CQ2
delivery tax and no candidate; the kbench harness builds again. Earlier: Two candidates have been measured and rejected
since #291, and both closures are load-bearing for what to try next. #292: skipping the
exactly-zero exponential inside the ATTENTION softmax pairs was proven bit-exact on the whole
real 49,152-pair capture (357,792 elements, 0 mismatches) and still measured **-0.43 %** with a
1,024 B internal-RAM cost - a 48.5 %-hit branch is nearly maximally unpredictable and it destroys
Experiment 12's interleaving, so `nd_expf_pair()` must stay the only statement in that block.
#293: dropping IDF's default assertion level 2 to 0 measured **neutral** speed (5.0100 vs 5.0117)
but returned **+8,248 B of internal heap** (15,215 -> 23,463, reproduced on two boards); it is not
shipped because the metric is unchanged and removing runtime failure diagnostics is the owner's
call - see the re-priced RAM-blocked ideas in `.auto/ideas.md` if that decision ever changes.
`make capture` is green on the accepted image (rc=0, all nine behavioural flags true), so the
capture clock is reset as of this commit.
Earlier: **run #291.** Accepted runtime at commit `4aeb436`:
**5.0117 decode tok/s (+105.4 % over the 2.44 baseline)**, 4.9286 extended, 3.93 think,
5.2867 prefill, 4.79 min_case, boot bench 5.057 / 198 ms/token, 14/14 device + 13/13 host
byte-exact, token_delta 0, fidelity 5.341e-05 / top1 10/10, internal_free 15,215,
psram_free 2,052,252. `make capture` green on this exact image (rc=0, all nine behavioural
flags true), so the capture cadence clock resets to the next shipping code change.
Experiments 12-19 are ALL measured (12/16/18 kept; 13/14/15/17 rejected; 19 measured as an
isolated +3.51 % diagnostic that the documented-maxima rule forbids to ship) and the named
queue is COMPLETE. Since then the productive lens has been **bit-exact removal of redundant
transcendental work**, which produced run #290 (paired elementwise sigmoid, +0.200 %) and
run #291 (skip the exactly-zero exponential in Sinkhorn's log-sum-exp, +0.200 %). The next
candidate is already priced on real captured data and banked at the top of the open work in
`.auto/ideas.md`: the same exact-zero skip in the **attention** softmax pairs, 48.5 % of
which have one argument exactly zero (+17 % of that phase's exp work, ~+0.6 % predicted,
with the measured -2.10 % scheduling risk from runs #230-231 stated alongside it). Anything
**Harness fact for the next window (measured this cycle):** `git push` from this container fails -
`could not read Username for 'https://github.com'`, no credentials - so `origin` stays behind the
accepted runtime, and a worker that runs `git fetch origin` will be REVERTED to a stale baseline
(that is how a control can read low and invalidate a batch). Sync workers by local path instead,
no network needed, verified this cycle to land all three boards on the accepted commit:
`git -C /root/board-pool/boardN fetch /workspace/esp32-needle-3 autoresearch/decode-tps-2026-09-18 && git -C /root/board-pool/boardN reset --hard FETCH_HEAD`.

above the documented 240 MHz / 80 MHz clock remains ineligible, and every candidate must
still clear 14/14 + 13/13 byte-exact output, token_delta 0, the fidelity gate and top1 10/10.

**Superseded state (run #227).** 4.8917 decode, 4.81 extended, 3.87 think, 5.2167 prefill,
boot bench 201 ms/token, internal_free 15759.
Repeated three-board batches agree to the last digit, so there is no board drift and the metric is
deterministic to ~0.04 % (one quantisation tick), not noisy. Runs #222-#227 were unchanged
verification repeats and added no information; do not continue that pattern.

The campaign's productive lens at the end was **request-path work the boot bench cannot see that
runs while core 1 is idle**; it yielded #147 (first-byte legality table, +2.16 %) and #176
(two-core legality filter, +0.20 %). Everything else in the token is now two-core or at a named
floor, and the closures with evidence are in `.auto/ideas.md`. Cadence rules that apply now:
run `make capture` **when the shipping code changes** (green at #188 on this code; a periodic
re-run with no code change adds cost and no information), couple three-board controls to novel
candidates instead of running periodic baseline-only batches, and treat any candidate below the
0.2 % keep bar as not worth a build.

**Experiment 16 is measured and KEPT (run #288, +1.01 %: 4.9417 -> 4.9917 decode, 14/14 +
13/13 byte-exact, capture green).** Accepted runtime is now **4.9917 decode tok/s**.
**NEXT: Experiment 19, the 120 MHz octal-memory diagnostic - isolated, non-shipping by
default** (ESP-IDF labels 120 MHz DDR experimental; it is a thermal/stability campaign, not
a free clock, and its result must be measured against two 80 MHz controls with board
assignments swapped).
Experiment 14 is measured and CLOSED as rejected (run #286): the real layer-0 `q_proj` fixture
replayed bit-exact 1024/1024, int8 CQ2 on ONE tensor moved `logit_max_delta` to 0.3282 (164x the
2e-3 gate) and the best case of the whole integer family - int16 activation with an exact codebook
- still hit 0.01818 (9.1x), while the 13/13 byte-exact host goldens stayed green. Numbers, the
kept fixture and the harness trap are in `.auto/ideas.md`. Proceed 15, then 16, then the
isolated/non-shipping 19 diagnostic. The accepted control is **4.9417 decode tok/s**. Reaching
5.00 requires saving about 2.36 ms from the ~202.36 ms request token. Earlier analytical closures
are hypotheses, not substitutes for these concrete screens.

### Research loop rules

1. **No unchanged verification loops, with no per-iteration exception.** Do not run or log the
   accepted image by itself. A control belongs in the same concurrent batch as a novel candidate.
   Context pressure, a generic instruction to produce a run each iteration, or wanting a noise datum
   is not permission to run the control. Persist work and continue the candidate instead.
2. Work on the first unmeasured experiment below. Each experiment must end in a concise measured
   disposition in `.auto/ideas.md` and `.auto/log.jsonl`: hypothesis, implementation/variant,
   image hashes and board assignments, isolated timing where requested, full-model delta, quality,
   memory, and accept/reject reason. Then immediately advance to the next experiment.
3. Use all three boards concurrently whenever images are ready: boards 1, 2, and 3 run three
   distinct candidate experiments during discovery. Compare against the pinned per-board accepted
   baselines; do not consume a routine lane on an unchanged control or duplicate candidate. For a
   plausible winner, confirm later with an assignment swap/crossover while unused lanes continue
   new experiments. Fresh-configure every compile-time variant and verify `compile_commands.json`
   plus image hashes before flashing.
4. Screen cheaply with real captured inputs and a device microbenchmark. Run the full device suite
   only for a correct, plausibly faster screen. A shipping candidate must retain 14/14 device and
   13/13 host byte-exact output, token delta 0, the existing fidelity threshold, top1 10/10, and all
   ordinary tests. Run `make capture` after a shipping code/config change, not periodically when the
   code is unchanged.
5. Change one lever per candidate. Revert rejected implementation code before starting the next
   experiment. Never share mutable static scratch between `nd_parallel_rows` workers. Record internal
   free RAM for every full candidate; only 15,759 bytes remain on the accepted control.
6. An analytical objection is a prediction to test, not a disposition. Stop an experiment without
   device measurement only for a concrete build/ABI/hardware blocker, and record that exact blocker.

### Ordered open experiments

**Experiment 12 -- paired/interleaved attention exponential.** Disassemble the current `nd_expf`
calls in `attn_heads`. Capture the real input pairs reaching the adjacent `w0`/`w1` calls. Build a
device microbenchmark for `nd_expf_pair(a,b)` that interleaves the two degree-5 polynomial dependency
chains while preserving each scalar chain's operation order, clamp behavior, and output bits. Try
carefully structured C and, if C does not schedule it, handwritten Xtensa/TIE728. Report cycles per
pair over captured inputs and bit mismatches versus two scalar calls. Integrate only a bit-exact,
faster kernel, then measure attention phase and end-to-end decode.

**Experiment 13 -- ESP-DSP S3 dot-product audit.** Benchmark Espressif's optimized
`dsps_dotprod_f32_aes3` against the current 64-wide attention Q.K dot and representative small
Kronecker dot shapes, using the real alignments. Also inspect/borrow its instruction schedule in a
specialized inline-free local kernel so component-call overhead does not decide the result. Report
isolated cycles and numeric deltas. Full-model-test only the shapes that win; reject any reduction
order that fails the existing output/fidelity gates.

**Experiment 14 -- perform the original CQ2 integer feasibility screen.** The old Experiment 8 was
not performed as specified and is reopened. Use real CQ2 shapes, codebooks, row norms, alignments,
and captured prepared activations. Measure both (a) packed 2-bit indices with int8 activation and
codebook arithmetic and (b) one representative tensor expanded to int8 rows in PSRAM. Test tensor
and per-group scales. Report quantization cost, bytes read, staging bytes, cycles, maximum/mean error,
and fidelity/top1. If a scalar representation passes the numeric screen, benchmark
`dsps_dp_s8_aes3` or a handwritten S3 integer dot before deciding whether the path is bandwidth-bound.
Do not expand to full integration unless the screen is both acceptable and faster.

**Experiment 15 -- operator-internal GDMA double buffering.** This is distinct from the rejected
cross-operator worker overlap. Use ESP32-S3 AHB GDMA async memcpy to prefetch the next sequential
PSRAM weight block into one of two small DMA-capable internal buffers while TIE728 consumes the
current buffer. Start with 2 KiB and 4 KiB buffers and representative dominant CQ2 shapes. Measure
copy-only bandwidth, compute-only time, overlapped time, wait time, and heap impact. Verify cache/DMA
coherency explicitly. Integrate only if overlap beats direct cached PSRAM reads and fits safely.

**Experiment 16 -- compact first-byte grammar index.** Build once a PSRAM-resident vocabulary index
of ascending `uint16_t` token IDs grouped by first byte plus 257 offsets. At each grammar state,
enumerate only allowed-byte buckets; preserve the exact legal candidate set and restore globally
ascending token-ID order before logits and tie-breaking. Do not repeat the rejected 8 KiB internal
first-byte cache. Microbenchmark table build, per-token filtering on captured real grammar states,
memory, and complete candidate-list equality before end-to-end measurement.

**Experiment 17 -- selective hot-code IRAM audit.** Use the map file and disassembly to determine
whether `attn_heads`, the hot grammar traversal, subset logits, or paired-exp helper execute from
flash. Move one measured hot function at a time to IRAM; record IRAM/DRAM movement and end-to-end
timing. Do not blanket-annotate functions and do not retain a placement that endangers the current
internal-memory margin.

**Experiment 18 -- exact KV reciprocal screen.** Microbenchmark Xtensa `recip0.s` plus Newton
refinement for the divisions in KV int8 storage and any similarly shaped measured division hotspot.
First compare produced int8 KV-cache bytes over captured real inputs; a KV candidate is eligible only
if every byte matches the control. Report cycles and full `kv_store_int8` phase time. Reject quickly
if the 1.1 ms phase cannot yield a measurable full-token improvement.

**Experiment 19 -- 120 MHz octal-memory diagnostic, isolated and non-shipping by default.** Build one
120 MHz octal flash/PSRAM candidate against two 80 MHz controls to test whether external-memory clock
is the remaining CQ2 limit. Record boot/config evidence, temperatures, phase timings, and correctness.
If it wins, repeat with board assignments swapped and perform a long hot/cold thermal soak with memory
integrity checks. ESP-IDF labels 120 MHz DDR experimental; do not call it shipping-safe or make it the
default unless the stability campaign passes and temperature tuning/recovery is addressed.

After Experiment 19, derive the next candidate from the measured winners and remaining phase map.
Do not fall back to baseline-only runs. If no experiment wins, write a final evidence table and stop
the campaign cleanly rather than manufacturing verification work.

Historical closed families remain closed unless an experiment above explicitly distinguishes itself:
PSRAM tier span/stride/copy-limit sweeps; C row-block/order variants; packed load-width sweeps; and
the old worker-slot cross-operator schedules. Experiments 15 and 19 are explicitly new memory-system
tests, not permission to repeat those old variants.

## Objective

Make the shipping inference path on the attached board decode faster, without
buying speed with model quality. The workload is one greedy, grammar-constrained
decode of the Needle 3 tool-calling model (8 layers, d_model 768, 12 heads / 2 KV
heads, 4 mHC lanes, Monarch Hadamard MLP width 1024, engram sites at layers 4 and
7, context 384) over a flash-mapped `.cact` archive. Weights stream through the
cache straight out of flash; there is no copy into RAM.

Baseline at session start: **~1.23 tok/s decode, ~1.26 tok/s prefill**, 8 layers,
16,155,796-byte archive, ~37 KB internal heap free, ~13.6 MB PSRAM free. Host
(x86, single core) runs the same engine at ~86 tok/s, so the device is not
compute-limited in the same way — do not trust host speed as a proxy.

## Metrics

- **Primary**: `decode_tps` — mean of the device-reported `EVT done ... tps=` over
  the six frozen `primary` prompts (higher is better).
- **Secondary monitors**: `ext_decode_tps` (5 held-out/extra prompts),
  `think_tps` (unconstrained full-vocabulary path — a different code path),
  `prefill_tps`, `boot_bench_tps` (firmware's own 6-token boot bench),
  `min_case_tps`, `gen_tokens`, `internal_free`, `psram_free`,
  `device_output_exact`/`device_token_delta`, `host_output_exact`,
  `logit_max_delta`, `logit_top1_match`.
- Secondaries are tradeoff monitors. They never justify keeping a primary
  regression, and the quality ones can veto a primary win (see Constraints).

## How to Run

`./.auto/measure.sh` (~10-13 min: host pre-check, `idf.py build`, flash, boot +
prefix priming, then 14 real requests on the board (6 primary + 7 extended + 1 think)). It prints `METRIC name=value`
lines plus `CASE`, `GROUP`, `BOOT`, `PROF` diagnostics.

`./.auto/checks.sh` runs the correctness/quality gates (host build, repo tests,
frozen-input guard, byte-exact generation, forward-pass numeric fidelity).

Useful env knobs on `measure.sh` (none of them change what the metric means):
`AUTO_PROFILE=1` for the per-phase breakdown, `AUTO_NOFLASH=1` to re-measure the
already-flashed app, `AUTO_GROUPS=primary` for a short run, `AUTO_SAVE=1` to
rewrite the device golden (**only** for a documented re-baseline).

## Benchmark integrity — read this before touching anything

- The prompt set in `.auto/prompts.json` is frozen: `primary` defines the metric
  and must not be edited, reordered or trimmed. Adding coverage to `extended` is
  welcome; shrinking `primary` is cheating.
- Speed authority is the **device**. The host build is the *quality* authority.
- A candidate is keepable only if the generated text is **byte-identical** to the
  frozen goldens, on host (all 11 prompts) and on device (`device_output_exact`
  equals the case count). If text diverges, the change is a quality change:
  discard it, whatever it does to tok/s.
- `logit_max_delta` (fixed 10-step forward probe, full 8192-row logits) must stay
  under 2e-3; it separates "1 ULP from a reassociation" from "the model is now
  doing something different". It is a veto, not a licence to diverge.
- Forbidden as a means of going faster: changing layer count, the archive, its
  quantisation, the KV codebook/`kv_bits`, context length, vocabulary, the tool
  or route schemas, greedy sampling, the grammar, or the prompt set. Forbidden
  outright: overclocking beyond the documented 240 MHz / 80 MHz octal
  flash+PSRAM, benchmark-only fast paths, skipping warmup, or reporting a metric
  other than the device's own decode timing.
- The model quality that matters is real routing/tool behaviour, and the board can
  only be measured on a handful of prompts. That is exactly why the goldens are
  byte-exact and the held-out cases exist. Do not "improve" a case by changing
  what it expects.
- Beware the metric's own blind spots: tok/s is per-request and ignores prefill,
  and a change that only helps the 4-bit path can regress the dominant 2-bit path
  (and vice versa). `think_tps` and `ext_decode_tps` are there to catch that.

## Files in scope

- `engine/src/nd_quant.c` — CQ GEMV kernels: 2-bit pair-LUT path, 4-bit generic
  path, FWHT activation prep, LUT build, gather. Most decode time lives here.
- `engine/src/nd_model.c` — forward pass: mHC lanes, phi GEMVs, QKV convolution
  taps, per-head RMSNorms, RoPE, int8 KV cache, online-softmax attention,
  Monarch Hadamard MLP, engram gather, Sinkhorn, confidence pooling.
- `engine/src/nd_sample.c` — constrained greedy sampling, grammar-candidate
  enumeration, subset logits.
- `engine/src/nd_grammar.c`, `nd_tokenizer.c`, `nd_cact.c` — schema-constrained
  decoding, SentencePiece lookup, archive reader.
- `engine/include/*.h` — kernels/structs; `ND_PROFILE` phase enum lives in
  `nd_model.h`.
- `esp32/main/main.c` — dual-core row splitter worker, prefix priming, request
  loop, boot bench.
- `esp32/sdkconfig.defaults` — cache size/line, CPU 240 MHz, octal flash/PSRAM
  80 MHz. (`esp32/sdkconfig` is gitignored: always edit the defaults file, then
  delete `esp32/sdkconfig` to regenerate.)
- `esp32/main/router.c`, `esp32/partitions.csv`, `tools/`, `host/` — read freely;
  touch only to fix a real bug, and `partitions.csv`/schemas are frozen.

## Off limits

`model/manifest.json`, `model/needle3.cact`, `tools/demo-tools.json`,
`tools/model-routes.json`, `tools/model-catalog.json`, `esp32/partitions.csv`,
`.auto/prompts.json` (`primary`), `.auto/golden/*`.

## Constraints

- `make test` equivalents must pass (10 Python + grammar + prefix-isolation tests)
  — the prefix-isolation test is the one that proves schema switching does not
  leak KV/conv state, keep it green.
- No new dependencies. No new threads beyond the existing two-core split.
- Internal SRAM headroom is ~37 KB. Anything sized in internal RAM must fit.
- Keep the `ponytail:`-free, heavily commented style of the engine: comments in
  this repo explain *why* a kernel is shaped the way it is, and several record a
  measured dead end. Preserve them; add new ones for new constraints.
- Device changes are expensive to test (~12 min/run). Think and use the host
  build + `AUTO_PROFILE=1` before spending a device run.

## What's Been Tried

(Update as experiments accumulate.)
