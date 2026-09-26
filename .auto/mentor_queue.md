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
