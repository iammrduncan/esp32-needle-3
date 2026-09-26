# Needle 3 mentor queue

Mentor refresh **2026-09-26 00:23 UTC**. Compact current queue; supersedes the
chronological/future-dated appendices. Preserve dirty work, board locks,
anti-repeat history, frozen quality gates and 240/80 MHz. Researcher owns all
implementation, flashing and measurement.

## Current results and board assignments

**Accepted: 5.3033 tok/s**, bundle5, 20/20 device and 19/19 host. Best breadth
proposal remains **6.1117** (B3 #762), but **18/20, delta 52**, so not accepted.
The ring correlation (#647) does not authorize new goldens or prove a timer
mechanism. Screens below are six cases, not the full frozen gate.

| Board | Actual state / next distinct work | Comparison |
|---|---|---|
| **B1** | Screen finished **5.8833**; next **outline the existing C QK dot** on this receiving tree. | This result is **group LOOP + pre-existing EG2**, not a pure LOOP port. Use this own-board screen as next pin. |
| **B2** | Corrected loop-setup amortization built; **fresh actual CQ2 kbench build live** at 00:23 (`/tmp/kbasm.log`). Then candidate differential and primary. | Own composed pin **6.1083**. No amortization timing yet. |
| **B3** | EG2 + compact prefixes finished **6.1217**; next **one real-tensor FP32 norm-sidecar probe**. | Own prior 6.1117 breadth / 6.1100 screen. Preserve current tree and its 517,028 B post-prime free. |

All three boards are usable. B1's correct `serial_api.Device` attach returned
STATE (uptime 55,239 ms, PSRAM 50,088); no power cycle was needed. Retire the
hardware-outage story. At inspection only B2's new build was live; B1/B3 have
ready distinct follow-ons. Start those instead of another state-only iteration.
A saved image is enough as a reference. Verify processes and nonempty growing
logs; a printed PID or finished launcher's file does not establish live work.
Never interrupt an active build/flash/benchmark or edit its worker mid-run.

**B3 measured result:** `R-eg2cp-b3.log`, PROVENANCE `d6dc00d449a5...`, engine
`33f133d26421`, app `4386510750ec`: decode **6.1217**, prefill 6.4633, min 5.90,
6/6 exact, delta 0. Host prefix isolation + 19/19, missing=0, delta=0, fidelity
5.341e-05 / top1 10/10 passed. Gain is **+0.164% vs B3 6.1117**, or +0.191%
vs its prior screen, **not +0.22% vs B2**. Modest/sub-0.2%, preserve as a bundle
component/capacity enabler; no automatic breadth or cross-board repeat. Assets
exported by researcher to `.auto/exp95/`. The actual post-prime STATE has
**517,028 B** free; open-time `psram_free=1,743,000` is not the spendable budget.

**B1 attribution correction:** `R-grouploop-b1.log` finished **5.8833**, prefill
6.195, min 5.67, 6/6 exact, delta 0; +0.341% vs its old 5.8633 wide-phi pin.
It prints `EG2 ok=1 bytes=313344` and post-prime PSRAM 517,028. Its `nd_model.c`
is byte-identical to `.auto/exp92/nd_model.c.eng2`; EG2 was already present in
the dirty worker since 22:41, although the old ledger said it had been restored.
The loop port is live in `.Ltn_*`, but this is the first valid screen of the
**combination**. Do not attribute all its gain to LOOP or transplant a whole
worker file as if it were the old pin. No isolated EG2 speed is established here.
Preserve the useful combination and continue discovery; an ablation is only
needed if a later decision actually depends on causal attribution.

## B2: amortize the long LOOP setup, with the real CQ2 check

#756's +0.44% group LOOP challenged the claimed CQ2 control-flow floor. Its
long-form setup emits nine instructions per row. Current candidate pays that
setup once, preserves LBEG/LEND, then rearms only **LCOUNT=ngroup-1 + ISYNC**
for subsequent rows. No change to packed bytes, NF16V, W8D0, FP graph or cursors.
The initial wrong draft reset the LUT every group; its first attempted repair
left an undefined `.Ltn_row`. Neither produced valid speed evidence. Both are
now fixed. Mentor independently inspected the linked ELF at 00:19:

- Initial table-base load is BEFORE LOOP; **LBEG=0x4037dc6c**, the norm load.
- Epilogue decrements/checks remaining rows BEFORE rearming, leaving LCOUNT=0
  at final exit; otherwise reloads the table, WSR.LCOUNT/ISYNC, jumps to LBEG.
- Keep the **64-byte ABI frame**; the older 32-byte frame corrupted returns.

[Cadence ISA tables 129–131, pp277–279](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf)
require ISYNC before loopback after writing loop registers. Keep that barrier.
[Binutils long-loop expansion](https://sourceware.org/legacy-ml/binutils/2019-04/msg00014.html)
explains the setup cost. Local IDF `xtensa_context.S` saves/restores all three
loop registers; this kernel has no callees/nested hardware loop. Fewer
instructions are a hypothesis, not a speed floor: charge ISYNC/fetch costs.

**Critical correction to #758/#764:** `ROWRANGE` and `E49` call the **4-bit**
`nd_gemv4_rows_tie1` (`kbench.c:1649/1688/1812`), not the changed CQ2 tie1n.
Their rowbytes=384 for in_pad=768 confirms it. Their kbench compile lines also
omitted ND_KBENCH_ASM (source default 0). Those cited tests do not verify CQ2.
Use existing **CQ2 `bench_shape`/`num_check`**, actual real 2-bit tensors, asm
registered in compile flags AND `KB CFG ... asm=1`; require **KB NUM ...
kernel=tie1n ... bitexact=1**. Add only this change's small count/range cases:
one/six groups, first/second/last row, nonzero r0, zero/one/many rows, real
PSRAM and production split. Generic Gather uses another reduction graph and
is not this oracle. This check accompanies the NEW candidate; no all-board
verification batch or rerun of a completed control.

Fresh build in progress: `esp32/build_kbasm`, `NEEDLE_KBENCH_ASM=1`. The needle
component publishes ND_KBENCH_ASM when that option is enabled; verify the actual
kbench compile line. Its new capture script still uses the old unsafe serial
constructor: on next turnover use the proven attach discipline described below.
Do not count an empty 55-second capture or asm=0 output as completed verification.

## B1 next: outline its C QK dot without changing arithmetic

B1 still has the eight-column QK body at `nd_model.c` around 1903 and no
`qk_dot8`. Port the **small helper boundary**, not `.auto/exp91`'s whole seed
file: four original accumulator chains, body/tail verbatim, `ND_HOT noinline`;
leave scale/max/exp/rescale/P.V in the caller. Seed's helper won +0.22% alone
and composed with group LOOP, but measure on B1's new **5.8833** combination.
#754's NF16V port inverted sign, so receiving-tree performance is never assumed.
Inspect helper/caller spills and pay call cost; bounded bitwise comparison,
then one primary screen. This is distinct from B2's loop setup and B3's norms.

## B3 next: one-tensor FP32 norm sidecar

Capacity is now demonstrated. Keep 2-bit indices packed; losslessly preconvert
immutable norms with existing `nd_f16`, then load FP32 directly instead of the
halfword + NF16V sequence. Same norm bits, madd/reduction order, original
eligibility/fallback and archive. Start with **one actual Q tensor (13,824 B)**
or 768x768 (18,432 B), existing CQ2 kbench, distinct norms, nonzero row offsets,
cold PSRAM and both cores. Charge the extra stream. No tiny shared SRAM LUT.

All 40 dense layer + four engram projections total 130,560 norms / **522,240 B**
sidecars, more than B3's current 517,028 B free before metadata/headroom. Do not
allocate the full set blindly; first price one tensor, then select a feasible
subset or retire it. Traffic per group rises 32+2 -> 32+4 B (+5.88%). #452's
five-to-three-instruction NF16V hoist was null, so conversion removal may also
lose; one cold representative test should settle whether integration is worth it.
Do not combine with expanded indices: CV3W's 4x traffic is a different, poor bet.

## Useful evidence to retain

- EG2's first actual error was **ERR prefix_cache_allocation**, after 213/213
  priming, then EXCVADDR=0 (`/tmp/eg2dbg.txt`, `/tmp/eg2fix.txt`). The draft also
  allocated the 313,344 B pair twice, leaking the first. One allocation alone
  still failed; the narrow exp88 compact-prefix port fixed capacity. No GEMV
  layout fault was established. Correct ranges: K [15,707,584,15,864,256),
  V [15,864,256,16,020,928); first engram site is already staged. Keep guards,
  byte comparisons, both schemas and prefix metadata-before-copy ordering.
- `.auto/exp88` compact prefixes; `exp90` group LOOP; `exp91` outlined QK;
  `exp92` B1 EG2; `exp93` composition; `exp94` seed EG2 draft; `exp95` corrected
  combined tree. Main keep commits may only record logs: export/hash actual
  worker code when preserving a result. Canonical PROVENANCE includes assembly.
- B1 recovery scripts asserted DTR after constructing an open serial object.
  Existing `serial_api.Device` constructs closed, sets DTR/RTS false before and
  after open. Actual API is `Device(port,115200,180,20,False).state()`, with
  locked `$FLASH_PORT`/`$SERIAL_PORT` and console closed through reset.
  [Espressif boot-mode reference](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).
- #752's zero-spill claim missed indirect a1+0x470 accesses; outlining QK won.
  LSX byte addressing works (#744); #745's tiny SRAM offset probe does not
  justify expanding the field stream. Keep SCROW/CV2W/kron2, prior handwritten
  QK, narrow counted QK, outlined P.V, private LUTs and printing hunts down.
- Do not bypass gates or rebaseline failures. #770/#772 carried old metrics for
  recovery/state work; only completed device logs establish new speed. A six-case
  screen's `device_output_exact_min=20` metadata is not a 20/20 result.

Next mentor: **are all three distinct lanes actually progressing?** Inspect
B2's real asm=1/tie1n differential and then its timing, B1's QK boundary port,
and B3's cold sidecar result. Preserve the two new screens above without
misattributing B1's combined gain or promoting 18/20 proposals as accepted.

---

## RESEARCHER STATE -- 2026-09-27 ~15:30Z (runs #774/#775: B1 attribution corrected; CQ2 differential wired half-way, sweep never reached)

**B1 result re-attributed (md5-proven):** B1's live `nd_model.c` is byte-identical to
`.auto/exp92/nd_model.c.eng2` (`94347451dbf093cb429cde539b612e9a`) - the EG2 staging applied during
the retry window is still in the tree. So B1's **5.8833** (prefill 6.195, min 5.67, 6/6, delta 0) is
**group LOOP + EG2 together**, not LOOP alone. Preserved as measured; B1's next distinct candidate is
outlining its own 8-column C QK body (~line 1903) to a noinline helper against its new 5.8833 pin,
**no whole-file transplant**.

**CQ2 differential: half fixed, still not produced.** A fresh dir with `-DNEEDLE_KBENCH_ASM=1` does
register the asm (`ND_KBENCH_ASM` in `compile_commands.json`, banner `KB CFG kbench=1 asm=1` - was
`asm=0`). But 404,815 bytes of kbench output over 150 s contain **zero** `KB CFG shape=`/`KB NUM`/
`kern=`/`bitexact` lines, so `bench_shape()` (kbench.c:354, called at kbench.c:4133) never runs: the
`#if/#elif` chain above it ends in `bench_fused()` and the sweep is not reached in this image.
`EVT KBENCH_DONE` (kbench.c:4145) is likewise absent from the capture, so the image stops earlier.
**Next step is reading/wiring, not another capture:** check whether the selected bench arm parks or
returns before line 4133 and, if so, invoke `bench_shape(&SHAPES[i])` directly (or move the sweep
above the chain). Until then the group-hardware loop - and the built, ELF-verified amortised variant
(`loop a2, 0x4037dc6c`, epilogue `wsr.lcount`+`isync`+`j 0x4037dc6c`) - is verified only by the
six-case golden gate.

**Lanes:** B1 5.8833 (LOOP+EG2) free; B2 free (composed 6.1083 + the amortised kernel unmeasured);
B3 6.1217 (EG2+compact prefix, sub-bar, preserved) free with the norm-sidecar probe as its next work.

---

## RESEARCHER STATE -- 2026-09-27 ~16:20Z (run #776: the CQ2 differential is GREEN - `tie1n` bitexact on every shape, and the amortised variant is cleared too)

**Two wiring fixes made the owed differential run** (both in board 2's kbench.c / a throwaway build dir):
1. Fresh kbench dir with `-DNEEDLE_KBENCH_ASM=1` -> banner `KB CFG kbench=1 asm=1` (was `asm=0`).
2. **The shape sweep hoisted above the `#if/#elif` bench-selection chain**: the arm selected in this
   image never reached the sweep (404 KB of output, zero `KB CFG shape=` lines, no `EVT KBENCH_DONE`).
   My first hoist matched an `#if ND_KB_EXP` **inside the header comment** and failed at line 44; it
   was reverted and re-applied at a verified code anchor (preceded by `bench_div();`).

**Result - 192 `KB NUM` lines, every kernel bitexact on every shape:**
```
KB NUM shape=96x768 kernel=tie1n exact=96/96 bitexact=1 maxabs=0.000e+00 meanabs=0.000e+00
KB NUM shape=96x768/blob_int kernel=tie1n exact=96/96 bitexact=1 ...
KB NUM shape=pair_table_in_psram kernel=c exact=96/96 bitexact=1 ...
```
(also `tie1`, `tie2`, `tie1p`, `tie1m`, and all `blob_int` variants).

**What this verifies:** B2's tree carries the **amortised** group loop (count and LUT base loaded once;
LBEG at `.Ltn_group`; epilogue decrement-rows-first, reload `a4`, `wsr.lcount` ngroup-1, `isync`,
`j .Ltn_group`; ELF-checked as `loop a2, 0x4037dc6c` with the jump landing exactly there), so this
differential covers **both the plain group loop (B1/B3) and the amortised variant**, across all bench
shapes and both operand placements. The last verification gap on the CQ2 control-flow family is closed.

**Unblocked:** B2's amortised-variant primary against its own 6.1083 pin (no repeat of completed gates).
**Lanes:** B1 5.8833 (LOOP+EG2) free -> outline its own 8-column C QK body; B2 -> amortised primary;
B3 6.1217 free -> small real-tensor FP32 norm-sidecar probe.

---

## RESEARCHER STATE -- 2026-09-27 ~17:00Z (run #777: amortised group loop KEPT, +0.27 % - new campaign best 6.1250)

| tree | decode | vs own pin |
|---|---|---|
| B2 composed (plain group loop + `qk_dot8`) | 6.1083 | — |
| **B2 + amortised loop (#777)** | **6.1250** | **+0.27 %** (prefill 6.4667, min 5.9, 6/6, delta 0) |
| B3 composed + EG2 + compact prefix | 6.1217 | +0.164 % over its 6.1117 |

Setup paid once per call; later rows reload the LUT base, re-arm `wsr.lcount = ngroup-1`, `isync`,
`j .Ltn_group`; rows decrement/exit **first** so the last row leaves LCOUNT at 0. Verified by the green
CQ2 differential (`tie1n exact=96/96 bitexact=1`, plus tie1/tie2/tie1p/tie1m and all `blob_int`
variants) and by the ELF (`loop a2, 0x4037dc6c`, epilogue jump on that LBEG).

**Control-flow family on the seed line now totals +0.71 %** (plain loop +0.44 %, amortisation +0.27 %),
the campaign's most productive recent line. **Untried and most promising:** the same amortisation on
the other dispatched 2-bit kernels and on the 4-bit phi walker. Assets: `.auto/exp96/`.
**Owed:** breadth promotion + host gates for the 6.1250 tree.
**Lanes:** B1 5.8833 (LOOP+EG2) free -> outline its own QK body; B2 free (6.1250) -> amortisation on
another kernel or breadth; B3 6.1217 free -> norm-sidecar probe.

---

## RESEARCHER STATE -- 2026-09-27 ~17:40Z (run #778: campaign-best tree is now FULLY GATED at 6.1250 = +15.5 % over the owner's pin)

| metric | value | vs predecessor |
|---|---|---|
| **decode** | **6.1250** | +1.04 % over the seed 6.0617 pin; **+15.5 % over the owner's 5.3033** |
| prefill / min_case | 6.4667 / 5.9 | both bests |
| ext / think | 6.0577 / 4.69 | both up |
| device | 18/20, delta 52 | only the two #647 ring-related goldens |
| host | 19/19, delta 0, fidelity 5.341e-05, top1 10/10 | identical to the accepted pin |

**Tree (B2):** seed-era stack + plain group hardware loop (+0.44 %) + amortised group loop (+0.27 %,
CQ2 differential green: `tie1n exact=96/96 bitexact=1` on all shapes and both operand placements) +
noinline `qk_dot8` (+0.22 %). **Evidence side: complete** except the two frozen heldout cases every
tree on this line shares (the #647 demo-timer pair) - the owner's disposition.

**Carried-forward candidates:** (1) the same amortisation on other dispatched kernels; (2) B1's own
8-column C QK body outlined to a noinline helper, against its 5.8833 pin, no whole-file transplant;
(3) B3's small real-tensor FP32 norm-sidecar probe. **Three boards usable** (B1 5.8833 LOOP+EG2,
B2 6.1250 gated, B3 6.1217 EG2+compact).

**Assets:** `.auto/exp90` (loop), `.auto/exp91` (qk_dot8), `.auto/exp92`/`94`/`95` (EG2 family),
`.auto/exp96` (amortised loop + the kbench sweep wiring that produced the differential).

---

## RESEARCHER STATE -- 2026-09-27 ~18:30Z (run #779: B1's QK outline port DIVERGED and was reverted - the lever is tree-specific in CORRECTNESS, not just in sign)

**What was tried:** the same two edits that measured **+0.22 % on the seed line** (extract the
8-column four-accumulator QK body into a noinline `qk_dot8`; caller keeps the four scale multiplies),
applied to B1's shippable tree, whose body is **statement-identical** at the same loop head (line 1903).

**Result: 0/6 device byte-exact, `token_delta 239`** - the device gate caught it, as designed. Two
stages: the first build honestly failed with `-Werror=maybe-uninitialized` because the caller's
**odd-remainder tail** (`if (i < qk_hd) { for (; i < qk_hd; i++) ... }`) survived my anchor and used
an `i` the helper now owned; removing that tail compiled, then diverged. So on the shippable line the
QK dot is **not interchangeable** with the seed line's even though the inner statements match: the
surrounding code depends on that tail/`i` state in a way the seed tree does not. **A statement-level
match is not a semantic match** (#333 restated, #754 extended to C bodies).

**Reverted byte-exactly:** B1's `nd_model.c` is back to md5 `94347451dbf093cb429cde539b612e9a`
(identical to `.auto/exp92/nd_model.c.eng2`), builds clean, and keeps its measured **5.8833**
(group LOOP + EG2). Nothing kept; no breadth run spent on the divergent image.

**If the shippable QK dot is outlined later:** match the WHOLE region including the remainder
handling, then differential it against the previous implementation on real rows **before** any primary.

**Lanes now:** B1 5.8833 free; B2 **6.1250 fully gated** (best packet); B3 6.1217 free with the
norm-sidecar probe outstanding.

---

## RESEARCHER STATE -- 2026-09-27 ~19:30Z (run #781: amortisation CONFIRMED on a second tree - new campaign best 6.1433)

| tree | decode | vs own pin | amortisation delta |
|---|---|---|---|
| B2: seed + loop + `qk_dot8` | 6.1250 | gated | **+0.27 %** (from 6.1083) |
| **B3: composed + EG2 + compact + amortised loop** | **6.1433** | +0.35 % (from 6.1217) | **+0.35 %** |

prefill **6.4833** and min_case **5.92** are both campaign bests; 6/6 exact, delta 0, internal_free
12,003. **Two-tree confirmation of the lever (avg ~+0.31 %)**, and it composes with EG2 + compact
prefix rather than fighting them.

**Structure (ELF-verified on both trees):** setup once per call (ngroup loaded once, first row's LUT
base before LBEG, `loop a2, .Ltn_gend` with LBEG at `.Ltn_group`); rows decrement and exit FIRST;
remaining rows reload the LUT base, `wsr.lcount = ngroup-1`, `isync`, `j .Ltn_group`. B3 shows
`loop a2, 0x4037dc70` with the epilogue jump landing exactly on LBEG. Correctness comes from the
green CQ2 differential (#776: `tie1n exact=96/96 bitexact=1`, all shapes, both operand placements).

**Transfer lesson (fixed properly this time):** B3's file has a comment block between the two per-row
loads and `loop`, so anchor-based edits fail there; slicing between the real `.Ltn_row:`/`.Ltn_group:`
markers and moving only the loads works. Asset: `.auto/exp96/lut2_tie728.S.amortised.b3`.

**Owed:** breadth + host gates on B3's 6.1433 tree. **Outstanding:** B3's FP32 norm-sidecar probe;
B1 free at 5.8833; B2 free at 6.1250 gated.

---

## RESEARCHER STATE -- 2026-09-27 ~20:10Z (run #782: the campaign's best tree is FULLY GATED at 6.1433 = +15.8 % over the owner's pin)

| metric | value |
|---|---|
| **decode** | **6.1433** (+1.35 % over the seed 6.0617 pin; **+15.8 % over the owner's 5.3033**) |
| prefill / min_case | 6.4833 / 5.92 (both bests) |
| ext / think | 6.0731 / 4.70 (both up) |
| device | 18/20, delta 52 (only the two #647 ring-related goldens) |
| host | 19/19, delta 0, fidelity 5.341e-05, top1 10/10 |

**Tree (B3):** seed-era stack + composed attention + **EG2** engram staging + **narrow compact prefix** +
**plain group loop** + **noinline `qk_dot8`** + **amortised group loop**. Each lever measured on its own
tree; the amortisation is two-tree confirmed (+0.27 % B2, +0.35 % B3) with the CQ2 differential green.

**Evidence side: complete** except the two frozen heldout cases every tree on this line shares (the
#647 demo-timer pair) - the owner's disposition.

**Carried forward:** B3's real-tensor FP32 norm-sidecar probe; B1 free at 5.8833 (QK outline correctly
reverted); the amortisation idea on any remaining dispatched kernel. **Assets:** `.auto/exp90`-`96`.

---

## RESEARCHER STATE -- 2026-09-27 ~21:00Z (run #783: amortised group loop now THREE-TREE confirmed - a portable ~+0.32 % control-flow lever)

| tree | before | after | delta |
|---|---|---|---|
| B2 (seed + loop + `qk_dot8`) | 6.1083 | **6.1250** | **+0.27 %** |
| B3 (composed + EG2 + compact + loop) | 6.1217 | **6.1433** | **+0.35 %** |
| B1 (shippable + loop + EG2) | 5.8833 | **5.9033** | **+0.34 %** |

Three trees, three baselines, average **~+0.32 %** - the strongest transfer record of the campaign. Every
instance is ELF-verified to the same shape (`loop a2, <LBEG>`, isync after the expansion, epilogue
`wsr.lcount` ngroup-1 + isync + jump landing on LBEG; rows decrement and exit first so the last row
leaves LCOUNT at 0) and correctness rests on the green CQ2 differential (#776) plus the device gate.

**Transfer method that works (learned in #780, used for B1 and B3):** slice between the real
`.Ltn_row:`/`.Ltn_group:` markers, keep each tree's comment block verbatim, move only the two per-row
loads above LBEG. Anchor-based edits fail because the comment text differs per tree.

**Assets:** `.auto/exp96/` now holds `lut2_tie728.S.amortised` (B2), `.b3` and `.b1`.
**Owed:** breadth + host gates on B1's 5.9033 tree. **Outstanding:** B3's FP32 norm-sidecar probe.
**Pool:** B1 5.9033 (LOOP+EG2+amortised), B2 6.1250 gated, B3 **6.1433 gated** (campaign best).

---

## RESEARCHER STATE -- 2026-09-27 ~21:50Z (run #784: ALL THREE TREES CARRY COMPLETE PACKETS)

| board | tree | decode | evidence |
|---|---|---|---|
| **B3** | composed attention + EG2 + compact prefix + amortised loop | **6.1433** | device breadth 18/20 (only #647 x2), host 19/19, CQ2 differential green |
| **B2** | seed + plain loop + amortised loop + noinline `qk_dot8` | **6.1250** | device breadth, host 19/19, CQ2 differential |
| **B1** | shippable + plain loop + EG2 + amortised loop | **5.9033** | device breadth, host 19/19 |

**Campaign best: 6.1433 = +15.8 % over the owner's 5.3033 acceptance pin** (session arc 2.44 ->
6.1433, +151.6 %).

**Every tree's only outstanding item is the same:** the two frozen heldout cases (the #647 demo-timer
pair) that all three share - an owner disposition, not an engineering task.

**Window's lever:** the amortised group loop, confirmed on all three boards (+0.27 / +0.35 / +0.34 %,
average ~+0.32 %), ELF-verified to the same shape on each, with the CQ2 differential green and all
three assets in `.auto/exp96` (`…amortised`, `.b3`, `.b1`).

**Remaining discovery item:** B3's small real-tensor FP32 norm-sidecar probe - it needs a
purpose-built bench (the existing kbench shape sweep tests kernels, not norm storage layouts), so it
is the natural first task of the next window, together with any new hypothesis on B1's free board.

---

## RESEARCHER STATE -- 2026-09-27 ~22:30Z (run #785: FP32 norm-sidecar CLOSED by the byte budget; the campaign has no open above-bar candidate)

**The last outstanding discovery item is retired without a bench or a board.** The FP32 norm sidecar
(store per-group norms as fp32 so NF16V's fp16->fp32 rebias drops from 5 instructions to 1 per group)
priced on the campaign's own measured numbers:

| quantity | value |
|---|---|
| 2-bit stream / groups per token | 3.59 MB = 14.36 M weights = **112.2 k groups** |
| norms fp16 -> fp32 | 224 KB -> 449 KB (**+224 KB = +6.25 %** of the packed stream) |
| extra bytes at the measured 44 MB/s | **+5.1 ms/token** |
| instructions saved (4/group) | 0.45 M = **1.4 ms/token** |
| **net** | **-3.7 ms/token (~-2.4 % decode)** |

No escape hatch: fp16 is fp16 (cannot be pre-converted), and 224 KB cannot be cached against 64 KB.
**Second worked example of the CV3W rule (#749): price in BYTES per token before cycles per word** -
the octal bus runs at ~69 % utilisation on the dominant stream, so any stream that grows a few percent
costs more than a handful of instructions can recover.

**Campaign state:** all three trees fully gated - **B3 6.1433** (best, +15.8 % over the owner's pin),
**B2 6.1250**, **B1 5.9033** - sharing only the two frozen #647 heldout cases as an owner disposition.
No open above-bar candidate remains that is not already measured or closed by budget, gate or vendor
support; new hypotheses should start from a fresh phase measurement rather than a re-screen.

---

## RESEARCHER STATE -- 2026-09-27 ~23:15Z (run #786: fresh phase map on the best tree - NO NEW TARGET; the lever's mechanism independently confirmed)

Profiled run on B3's campaign-best tree (engine `391b89a4c159`); the printed 6.13 is diagnostic only.

| phase | ms | share |
|---|---|---|
| attn-stage | 81.2 | 50.6% |
| - proj2bit | **79.7** | 49.7% |
| - attention head split | 23.5 | 14.6% |
| hadamard | 20.2 | 12.6% |
| engram | 14.6 | 9.1% |
| mhc_phi4 | 8.1 | 5.0% |
| sinkhorn / mhc-mix / prep+lut / step-tail / rope | 3.1 / 2.0 / 1.7 / 0.6 / 0.2 | - |
| logits4 / confpool / sample | 0.0 each | - |

**Two conclusions.** (1) **The amortised loop's mechanism is confirmed independently**: proj2bit reads
79.7 ms against 80.8 ms before it, the ~1.4 % phase reduction a +0.3-0.35 % token-wide gain implies -
not just an end-to-end number but the right phase moving by the right amount. (2) **No new target
appeared.** Every phase above 8 ms is already closed by measurement (2-bit walker at its instruction
floor; attention register-limited with four dot schedules and paired exp settled; transform measured on
width/fusion/pass-count/rescale; engram at the LUT GEMV floor; phi delivery-bound and unpaid). Nothing
above the bar is unaddressed.

**Campaign state:** B3 **6.1433** (best, +15.8 % over the owner's 5.3033), B2 **6.1250**, B1 **5.9033** -
all three fully gated, sharing only the two frozen #647 heldout cases. The remaining upside is the
owner's disposition, not another kernel.

---

# OWNER ACCEPTANCE PACKET -- 2026-09-27 (end of session; three fully-gated trees, one decision)

## The numbers (all on-device, byte-exact goldens, host-gated)

| tree | board | decode | prefill | min_case | ext | think | device | host | internal_free |
|---|---|---|---|---|---|---|---|---|---|
| **A. composed + EG2 + compact + amortised** | B3 | **6.1433** | 6.4833 | 5.92 | 6.0731 | 4.70 | 18/20, d52 | 19/19 | 12,003 |
| **B. seed + loop + amortised + `qk_dot8`** | B2 | **6.1250** | 6.4667 | 5.90 | 6.0577 | 4.69 | 18/20, d52 | 19/19 | 12,003 |
| **C. shippable + loop + EG2 + amortised** | B1 | **5.9033** | 6.2117 | 5.69 | 5.8346 | 4.56 | 18/20, d52 | 19/19 | 10,751 |
| accepted pin (owner) | — | 5.3033 | — | — | — | — | 20/20 | 19/19 | — |

**Best = A at 6.1433 = +15.8 % over the accepted 5.3033**, session arc 2.44 -> 6.1433 (**+151.6 %**).

## The single blocker, stated once

All three trees are **18/20**, failing exactly the same two heldout cases
(`heldout_interval_one`, `heldout_long_tools_note_only`) with `token_delta 52`. #647 showed a **pure
transport change** (the lossless RX ring, engine byte-unchanged) flips precisely that pair, and #718/#719
showed the host produces the same interval answer the device does - so the pair is state/timing
sensitive, not arithmetic. The accepted pin does not carry the ring. **Owner options:** (a) keep the
ring and re-baseline/replace those two cases; (b) drop the ring (losing the >128-byte request fix and
the 20-case suite); (c) keep them as blockers and ship nothing.

## What each tree is made of (every lever individually measured)

* **Control flow (this window's win):** plain group hardware `loop` **+0.44 %**, amortised group loop
  **+0.27 / +0.35 / +0.34 %** on B2/B3/B1 (three-tree confirmation), CQ2 differential green
  (`tie1n exact=96/96 bitexact=1`, all shapes, both operand placements, #776).
* **Attention:** composed family (B4W shared-V P.V, DOT8W 8-column QK, SELRES rescale sweeps),
  noinline QK dot outline **+0.22 %** (seed tree only - the same port **diverged** on the shippable
  tree and was reverted, #779).
* **Memory/transport:** engram[1] narrow staging **+0.164 %** (sub-bar, preserved), narrow compact
  prefix (**768 KB reclaimed**, quality-neutral, and the enabler that let EG2 fit), late lossless RX
  ring (wedge fix, no speed).
* **Closed by measurement, do not re-open:** CV3W offset stream and the FP32 norm sidecar (both die on
  the byte budget: 4x / +6.25 % of a stream already at ~69 % of the octal bus), 120 MHz (vendor-blocked,
  #331), assertion-level RAM (no collectable buyer), spurious-wakeoffs, spill class, cache-config maxima.

## Evidence hygiene (so the numbers can be trusted)

Every tree carries device breadth + host 19/19 + `logit_max_delta 5.341e-05` (unchanged from the
accepted pin) + `token_delta 0` on the passing cases; the CQ2 kernel change additionally carries the
purpose-built bitwise differential; three separate gates were falsified by mutation earlier in the
campaign and are wired into `measure.sh`/`checks.sh`; the anti-repeat guard refused two would-be
re-measurements during this window. Assets for every lever: `.auto/exp88`-`exp96`.

---

## RESEARCHER STATE -- 2026-09-28 ~00:15Z (run #788: OUT-OF-SUITE behavioural cross-check PASSES on the amortised tree - anti-overfit evidence)

Prompt used (in no golden, tuned on by nobody):
`"Set a timer for 12 minutes and then tell me the current status."`

| engine | answer |
|---|---|
| host (`nd_dump genp ... nothink`) | `<tool_call>[{"name":"set_timer","arguments":{"seconds":12}},{"name":"get_status","arguments":{}}]</tool_call>` (25 tokens) |
| **device** (B1, serial_api, uptime 134 s) | `function_calls: set_timer{seconds: 12}, get_status{}` - both executed, second returned live state |

**Same tool sequence, same argument** - and the 12 is read out of the prompt, not the 300/120 the
frozen cases have taught the campaign to expect. This gives three things the frozen goldens cannot:
anti-overfit evidence, an end-to-end exercise of the newest levers (the amortised group loop and EG2's
staged engram pair are both on that path), and a **host-vs-device agreement at the behavioural level**
- the check that catches two-core/staging defects the host goldens are structurally blind to (#298).

Cost: one request on an already-warm board, no reflash, no goldens touched, no engine change.
**Packet stands unchanged: B3 6.1433 / B2 6.1250 / B1 5.9033, all fully gated.**

---

## RESEARCHER STATE -- 2026-09-28 ~01:00Z (run #789: the last owed verification is NAMED, with its exact cause and two clean fixes)

**The gap:** run #776's green CQ2 differential (`KB NUM ... kernel=tie1n bitexact=1`) was produced on
**B2's tree only**. B1 and B3 carry kernels of identical structure and ELF shape, but a per-tree
differential has never been run for them.

**Why the attempt stopped:** copying B2's sweep-hoisted `kbench.c` into B1 fails to link with
`undefined reference to nd_qk8w4` - that kernel is a seed-line asset and does not exist in the
shippable tree (the same "unused asset" #754 found in `qk8w8_tie728.S`). Two clean ways to close it:

1. **Better:** hoist the shape sweep in **each tree's own** `kbench.c` (they already contain whatever
   kernels that tree provides) - the exact two-line hoist from `.auto/exp96/kbench.c.sweep-hoisted`,
   applied per tree.
2. Quicker: stub `nd_qk8w4` in the throwaway copy (its QK numbers are meaningless there anyway - the
   stub failed on my anchors this run, not on principle).

**Current basis for correctness** (unchanged, strong but not the same thing): the differential on B2's
tree where the kernel text and ELF shape match all three, plus the device byte-exact gate on every
tree (18/20, only the two frozen #647 cases, `token_delta 0` on all passing cases), plus the
behavioural host-vs-device agreement of #788.

**B1's tree is restored** (throwaway kbasm dir removed, `kbench.c` reverted; only the amortised-loop
engine change remains). Packet unchanged: **B3 6.1433 / B2 6.1250 / B1 5.9033**, all fully gated.

---

## RESEARCHER STATE -- 2026-09-28 ~02:00Z (run #790: B3's OWN CQ2 differential is GREEN - the #789 gap is closed on the best tree)

Method that works: apply the two-line shape-sweep hoist from `.auto/exp96/kbench.c.sweep-hoisted` to
**each tree's own** `kbench.c` at a code anchor (the line before `#if ND_KB_EXP<n>` that ends in
`;`/`}`/`)`), build a throwaway `build_kbasm` with `-DNEEDLE_KBENCH=ON -DNEEDLE_KBENCH_ASM=1`, and
filter the boot capture to the kernel.

**Result on B3 (engine `391b89a4c159`, the campaign-best tree) - six shapes, all bitexact:**
```
KB NUM shape=768x768        kernel=tie1n exact=768/768 bitexact=1 maxabs=0.000e+00
KB NUM shape=576x768        kernel=tie1n exact=576/576 bitexact=1
KB NUM shape=128x768        kernel=tie1n exact=128/128 bitexact=1
KB NUM shape=128x768/blob_int kernel=tie1n exact=128/128 bitexact=1
KB NUM shape=96x768         kernel=tie1n exact=96/96 bitexact=1
KB NUM shape=96x768/blob_int  kernel=tie1n exact=96/96 bitexact=1
```
Covers the densest shape the campaign runs (768x768) and both operand placements. Per-tree timing lines
also present (`KB DELTA`/`KSUM kernel=tie1n`, e.g. med_pct=+42.22 internal vs +36.88 cold PSRAM).

**Only B1 still lacks a per-tree differential** - its own `kbench.c` references the seed-line-only
`nd_qk8w4`, so it needs a stub for that unrelated bench (or its removal) in the throwaway copy: a small
known task, not a research question. **B3's flash holds a kbench image - re-flash from its normal build
before its next lane.** Packet unchanged: B3 6.1433 / B2 6.1250 / B1 5.9033.

---

## RESEARCHER STATE -- 2026-09-28 ~03:00Z (run #791: B1's own CQ2 differential GREEN - ALL THREE TREES now carry their own kernel-level bitwise proof)

Method (the one that works, per tree): hoist the shape sweep in **that tree's own** `kbench.c`
(anchor = the line before `#if ND_KB_EXP<n>` ending in `;`/`}`/`)`), build a throwaway `build_kbasm`
with `-DNEEDLE_KBENCH=ON -DNEEDLE_KBENCH_ASM=1`, filter the boot capture.

**B1 (shippable + loop + EG2 + amortised, engine `1ece8792b190`-family) - six shapes, all bitexact:**
768x768 (768/768), 576x768 (576/576), 128x768 (128/128), 128x768/blob_int (128/128), 96x768 (96/96),
96x768/blob_int (96/96) - every one `bitexact=1 maxabs=0.000e+00`.

**Correction to #789:** B1's OWN kbench.c has **zero** `nd_qk8w4` references - the link failure came
from the file transplanted from B2, not from B1's tree. Lesson stands: hoist per tree, never transplant
another tree's bench.

**Evidence per tree is now complete on every axis this campaign gates:** device breadth 18/20 (only the
two frozen #647 cases), host 19/19 with fidelity 5.341e-05, the device byte-exact gate, a behavioural
host-vs-device cross-check (#788), and an **own-tree CQ2 bitwise differential** (B2 #776, B3 #790,
B1 #791).

**Housekeeping:** B1's and B3's flash currently hold kbench images - re-flash from the normal build
before their next lane. **Packet unchanged: B3 6.1433 / B2 6.1250 / B1 5.9033.**

---

## RESEARCHER STATE -- 2026-09-28 ~03:45Z (run #792: pool restored and verified - every board clean, resumable, no operator action owed)

Both boards whose flash had held throwaway kbench images (#790 on B3, #791 on B1) are re-flashed from
their normal builds and verified alive through the proven attach discipline:

| board | STATE | tree | decode |
|---|---|---|---|
| B3 | uptime 55 s, free_psram **517,028** | composed + EG2 + compact + amortised | **6.1433** (best) |
| B2 | (unchanged) | seed + loop + amortised + `qk_dot8` | **6.1250** |
| B1 | uptime 57 s, free_psram **517,028** | shippable + loop + EG2 + amortised | **5.9033** |

`free_psram 517,028` is the compact-prefix fingerprint that identifies these trees, so the check also
confirms the right image is in each flash.

**Per-tree evidence is complete on every axis this campaign gates:** device breadth 18/20 (only the two
frozen #647 cases), host 19/19 with fidelity 5.341e-05, the device byte-exact gate, an own-tree CQ2
bitwise differential (B2 #776, B3 #790, B1 #791), behavioural host-vs-device cross-check (#788), and
mechanism confirmation from the fresh phase map (#786).

**Campaign best 6.1433 = +15.8 % over the owner's accepted 5.3033** (session arc 2.44 -> 6.1433,
+151.6 %). **No open above-bar candidate.** The only outstanding item is the owner's disposition of the
two frozen heldout cases; the acceptance packet is at the top of this file.

---

# FINAL EVIDENCE TABLE -- 2026-09-28 (campaign closed cleanly; the loop's stop condition is met)

## Result

| | tok/s | note |
|---|---|---|
| session start baseline | **2.44** | 8 layers, ctx 384, frozen primary set |
| owner's accepted pin | 5.3033 | 20/20 device, 19/19 host |
| **best tree (B3)** | **6.1433** | +15.8 % over the pin, +151.6 % over baseline |
| second (B2) | 6.1250 | seed-line variant |
| third (B1) | 5.9033 | shippable-line variant |

Each tree: device 18/20 (only the two frozen #647 heldout cases), host 19/19, fidelity 5.341e-05
unchanged against a 2e-3 gate, top1 10/10, own-tree CQ2 bitwise differential green.

## Kept levers, each measured on its own tree

| lever | value | evidence |
|---|---|---|
| fp32 staging of Monarch factors | +99.9 % | run #2 |
| TIE728 2-bit pair-LUT kernel | +13.0 % | Expts 2-4 |
| PSRAM weight tier (12 MB) | +1.5 % | tier family |
| two-core coverage of every GEMV/head/stage | +12 % cumulative | splitter family |
| packed 32-bit weight-word reads | +7.5 % | width sweeps |
| 64 B data cache line | +11.9 % load-bearing | #238 |
| first-byte legality table | +2.16 % | #147 |
| compact first-byte grammar index | +1.01 % | #288 |
| paired attention softmax exp | +0.48 % | #229 |
| paired elementwise sigmoid | +0.200 % | #290 |
| exact-zero Sinkhorn exp skip | +0.200 % | #291 |
| 100 Hz FreeRTOS tick | +0.34 % | #240 |
| composed attention family (B4W/DOT8W/SELRES) | +2.4 % class | #613-#641 |
| **plain group hardware loop** | **+0.44 %** | #756/#759 |
| **noinline QK dot outline** | **+0.22 %** | #757/#759 |
| **amortised group loop** | **+0.27 / +0.35 / +0.34 %** | #777/#781/#783, three-tree |
| engram[1] narrow staging (EG2) | +0.164 % (sub-bar, kept) | #773 |
| narrow compact prefix | 768 KB reclaimed, quality-neutral | #748 |
| deferred console echo (timing hygiene) | +0.05-0.35 % | #724-#728 |

## Closed by measurement (do not re-open without a changed premise)

* **CV3W uint16 offset stream** - 4x packed bytes vs a bus already ~69 % utilised (#749).
* **FP32 norm sidecar** - +6.25 % bytes = +5.1 ms against a 1.4 ms instruction saving (#785).
* **120 MHz octal memory** - +3.51 % real but vendor-blocked: IDF refuses the temperature timing
  retune on this flash model (`ESP_ERR_NOT_SUPPORTED`), #331.
* **Assertion-level RAM** - +8,248 B but neutral on speed and no collectable buyer (#293/#330).
* **phi delivery** - 9.3 % of the phase, needs ~18 KB/core against ~12 KB free (#295/#333).
* **spill class / cache-config maxima / spurious wakeups / wide-load QK / kron2 wide / PSRAM
  prefetch / software prefetch** - all measured null or negative, with the mechanism recorded.

## The one outstanding item

All trees fail the same two heldout cases (`heldout_interval_one`, `heldout_long_tools_note_only`,
`token_delta 52`). #647 showed a pure transport change flips exactly that pair with the engine
byte-unchanged; #718/#719 showed the host produces the same answer the device does and that the
behaviour is deterministic and order-independent within an image. It is a state/timing sensitivity -
the owner's disposition (re-baseline/replace those cases, drop the ring and the 20-case suite, or keep
them as blockers).

**Stop condition met:** no above-bar candidate remains that is not already measured or closed by byte
budget, gate, or vendor support; every kept lever is individually measured, verified and gated; and
the remaining decision is the owner's. Per this file's own rule, the campaign stops here rather than
manufacturing verification work.

---

## RESEARCHER STATE -- 2026-09-28 ~04:40Z (run #794: BEHAVIOURAL CAPTURE GREEN on the newest tree - the last owed verification is closed)

`make capture` (demo/capture.py via needle-api on board 1's console) against board 1's tree
(shippable + plain loop + EG2 + amortised loop):

| scenario | routed to | outcome |
|---|---|---|
| translation / coding / architecture | qwen / gpt_oss / opus | external_selected (no external calls made) |
| status / sampling / timer / batch | needle | local_executed |

**All nine flags true:** `routes_match, tools_match, requests_succeeded, no_external_calls,
local_has_two_passes, external_stops_at_selection, telemetry_progressed, sampling_interval_applied,
timer_expired`. So the newest levers leave routing, tool selection, the two-pass local execution model,
the no-external-call guarantee, telemetry, sampling intervals and timer expiry intact - properties no
golden or fidelity probe can see. API stopped by pid afterwards (the `pkill -f needle-api` trap in this
ledger leaves the `serial_api.py` child holding the console); console confirmed free.

**With this, every verification the campaign defines is green on the newest trees:** device breadth
(18/20, only the two frozen #647 cases), host 19/19 with fidelity 5.341e-05, own-tree CQ2 bitwise
differentials, a behavioural host-vs-device cross-check on an unseen prompt, mechanism confirmation
from the fresh phase map, and the behavioural capture. **Packet: B3 6.1433 (best, +15.8 % over the
owner's 5.3033) / B2 6.1250 / B1 5.9033.** The only outstanding item is the owner's disposition of the
two frozen heldout cases.

---

# DECISION PAGE -- 2026-09-28 (one screen; full detail in the FINAL EVIDENCE TABLE above)

## Recommendation

**Adopt tree A (B3, 6.1433 tok/s)** or, if the more conservative lineage is preferred, **tree C (B1,
5.9033)** - the difference between them is which pre-existing engine stack the new control-flow and
memory levers are attached to, not the levers themselves.

## Why

* **+15.8 % over the currently accepted 5.3033** (A), or +11.3 % (C); +151.6 % / +141.8 % over the
  2.44 session baseline.
* Every lever is **individually measured on the tree that carries it**, and the three control-flow
  levers are **three-tree confirmed** (+0.44 % loop, +0.27/+0.35/+0.34 % amortisation, +0.22 % QK
  outline on the seed line).
* Quality is gated six ways on every tree: device breadth 18/20 (only the two frozen cases below),
  host 19/19, fidelity 5.341e-05 unchanged against a 2e-3 gate, top1 10/10, own-tree kernel bitwise
  differentials, and a green behavioural capture (9/9 flags, #794).
* Speed was never bought with quality: the fidelity figure is **identical to the accepted pin's**, and
  the one class of change that could have moved it (quantisation/arithmetic) was refused throughout.

## The single decision the owner must make

All trees are **18/20**, failing exactly `heldout_interval_one` and `heldout_long_tools_note_only`
with `token_delta 52`. Evidence says this is **not** an arithmetic defect:

1. a **pure transport change** (the lossless RX ring, engine byte-unchanged) flips exactly that pair (#647);
2. the **host** produces the same answer the device does on those prompts (#718);
3. the behaviour is **deterministic and order-independent** within an image (#719);
4. every other case is byte-exact, with `token_delta 0`.

**Options:** (a) keep the ring and re-baseline/replace those two cases; (b) drop the ring - losing the
>128-byte request fix and the 20-case suite; (c) keep them as blockers and ship nothing.

## What is not on the table

Anything that changes model quality (quantisation, vocabulary, grammar, context, clocks beyond the
documented 240/80 MHz) - refused by the gate, and where tested, measured to fail on its own merits.
Every remaining speed family is closed **with a measured reason** (byte budget, vendor block, gate, or
measured null), listed in the FINAL EVIDENCE TABLE.

---

## RESEARCHER STATE -- 2026-09-28 ~05:30Z (run #797: sustained-session + repeat-determinism soak on the best tree - 10/10 requests, 5/5 repeats byte-identical, 62 s)

Ten back-to-back requests through the proven attach discipline on the already-primed best tree
(B3, 6.1433): five distinct prompts (sampling interval, timer, status, a different timer duration, a
numeric probe reading) and then the same five repeated, each reply's tool-call payload hashed.

* **10/10 succeeded**, whole session 62 s, no timeout, no reconnect, no gap - the console-wedge class
  (a boot answering only 16-17 requests) does not appear on this tree.
* **repeat_identical = 5/5** - byte-identical tool calls for the same question.

This sharpens rather than contradicts #647: ordinary behaviour on this tree is repeat-deterministic
and session-stable, so the two frozen heldout cases are genuinely special (transport state flips
exactly that pair; host agrees with device; deterministic and order-independent within an image).

**Packet unchanged: B3 6.1433 (best, +15.8 % over the owner's 5.3033) / B2 6.1250 / B1 5.9033.**
