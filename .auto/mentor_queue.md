# Needle 3 mentor queue

Mentor refresh **2026-09-25 21:48 UTC**. Compact action queue; supersedes the
chronological/future-dated appendices. Preserve all worker work, locks, fixtures,
anti-repeat guard, and 240/80 MHz. Researcher owns implementation and measurement.

## State that matters

**Accepted: 5.3033 tok/s (bundle5), 20/20 device, 19/19 host.** The seed proposal
is 6.0617 (#731/#737), host 19/19, confirmed on B3. Its full device run remains
18/20 with token_delta 52. The conservative line's compact-prefix tree is
5.8283 screen / 5.8300 breadth (#748/#753), also 18/20. Neither proposal passes
the frozen full gate. Ring correlation is not permission to replace goldens,
nor proof of the repeatedly claimed timer mechanism. Discovery continues.

**Newest result (#755):** `R-widephi-b1.log` finished at 21:32: decode **5.8633**,
prefill 6.165, min 5.65, 6/6 exact, delta 0, internal_free 10,767. Compare with
B1's own 5.8300 compact-prefix/E59 baseline: **+0.57%**. The NF16V port before
it was **-1.11%** (#754), restored before this screen. Do not infer composition
from seed-line gains. Preserve the worker's exact wide-phi files; a main keep
commit alone did not preserve #748's worker implementation.

At inspection B2 had no job since 19:56, B3 since 20:16. B1's completed screen
was followed by a 290-second sleep after its process had exited. Its one full
gate `R-widephi-gate-b1` completed at 21:43: decode **5.8633**, ext **5.7946**,
think **4.53**, **18/20**, delta **52**, the same two frozen failures. No further
wide-phi device repeat is owed. Finish its exact-tree host gates if missing,
then resume discovery. Use completed logs and live processes, not fixed long
waits. No board outage is established.

## Next three lanes

| Lane | Work now | Comparison / reason |
|---|---|---|
| B1 | Breadth DONE at 5.8633, 18/20. Finish any missing host gates; next missing second-site engram pair below. | Own wide-phi/compact-prefix pin 5.8633. No further device control or promotion. |
| B2 | **CQ2 group loop: hardware LOOP instead of decrement/branch/jump.** | Own seed 6.0617. Same packed bytes, same FP graph; challenges the untested control-flow part of the claimed floor. |
| B3 | **Isolate existing C DOT8W in a noinline helper**, now the ready substitute below. | Own seed pin 6.0617; concrete indirect stack spills found. Norm sidecar remains next, deferred for harness preparation. |

Prepare B2/B3 now; B1 breadth is done. Normal topology is three different experiments,
not all boards verifying the winner. Each lane needs a real build/run and a
nonempty growing log. If preparation blocks, move to a ready distinct candidate;
do not replace experiments with more global audits or acceptance prose.

## B2: remove group-loop control, not arithmetic

The live B2 ELF at inspection has `nd_lut2_rows_tie1n` at 0x4037f672.
Source labels are **.Ltn_row/.Ltn_group/.Ltn_gend**, around lines 650-723.
The .Lt1_* kernel at lines 270-320 is NOT the dispatched kernel; modifying it
would be another silent null. Its group begins 0x4037f6b1 and ends in `addi.n a2,-1; beqz.n; j group`
(0x4037f9ef..f9f3). The W8D body is fully unrolled, calls nothing, and has no
nested hardware loop. Group count is normally six; the dense CQ2 calls visit
130,560 row-groups/token. #580's four-instruction seed removal paid +0.60%, so
three control instructions per group deserve a concrete screen, even after
per-row loop setup is charged. This is not another gather scheduling/unroll.

Use the assembler's supported long-loop expansion (the existing DOT8W compiler
body already uses extended LEND; see #627/#634), preserving W8D0, every partial,
norm conversion, fold, cursor update and row order. A >256-byte body needs real
long-loop handling: inspect the final ELF for LBEG/LEND setup, scratch clobbers,
first-instruction alignment and the *actual* per-iteration path. Do not assume
a source `loop` became a free branch or invent a loop-register protocol. The
current B2 attn_heads at 0x4037c752 already shows the supported expansion:
loop, rsr.lend, wsr.lbeg, literal load, nop, wsr.lend, isync, rsr.lcount, addi.
The [binutils implementation](https://sourceware.org/legacy-ml/binutils/2019-04/msg00014.html)
explains it. Charge this setup once per row; do not claim three free instructions
per group with zero setup cost.
Keep the row loop ordinary; zero-overhead loops cannot nest or contain callees
that reuse their loop registers. Include one-group, six-group, nonzero r0 and
multirow bitwise differentials; then cold real PSRAM and the production two-core
split. Charge setup and code size; integrate only a measured winner.

Why this is plausible, not guaranteed: [Cadence LOOP/LOOPNEZ semantics, pp473-477](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf)
and [Espressif DSP-154](https://github.com/espressif/esp-dsp/issues/98) identify
loop fetch/alignment and scheduling costs that instruction counts omit. No broad
alignment sweep: inspect and change this one hot loop. #634/#636 reject different
C attention rewrites with reloads, not this branch-only assembly transformation.

## Next after current lanes: spend a little bandwidth to eliminate norm conversion

Keep 2-bit indices packed. At open, losslessly convert immutable group norms
with the existing `nd_f16` to a **separate FP32 sidecar**; load the FP register
directly where the existing early halfword load lives, removing NF16V and WFR
from the group fold. Same norm bit pattern, same madd and reductions. Retain
original FP16 data and eligibility/fallback; no new quantization or codebook.

Byte budget from the actual archive: 40 layer projections plus four engram
projections = **130,560 norms**, FP32 sidecars **522,240 B**. Per visited group,
indices+norm go from 32+2 to 32+4 B: **+5.88% traffic**, not CV3W's 4x index
stream. Potential benefit is removing 5 instructions/group on seed (10 on
B1's older macro), with fewer integer dependencies; added PSRAM reads may win
or lose. #450's conversion simplification paid on seed but #754 lost on B1,
so neither sign is assumed. Old norm-pool staging was elementwise scale data;
#4 moved conversion within the old C walker, not a boot-time CQ2 norm sidecar.

First price a representative tensor (Q sidecar 13,824 B; 768x768 sidecar
18,432 B) with the existing real-row kbench. Actual distinct group norms,
several rows/nonzero offsets, exact row outputs, cold PSRAM, both cores;
include the sidecar's extra byte stream. Do not repeat #745's tiny SRAM fixture.
If positive, integrate the dense sidecars with the proven compact-prefix change
ported narrowly; #748 freed 768,508 B and B1 has 818,596 B post-prime. Do not
budget from the misleading 2.06 MB open-time print. Keep both prefixes, check
post-prime free/largest block and ownership/freeing, and report actual dispatch.
If setup or extra traffic loses, retire promptly; no full gate for a loser.

## B3 now: isolate the existing C DOT8W body

Norm-sidecar preparation was blocking launch; use this ready candidate. Extract ONLY the existing four-score
DOT8W dot and its tail into a small `ND_HOT noinline` C helper, preserving the
8-column body verbatim, all four accumulator chains and the compiler's exact
mul/madd/add graph. Leave max/exp/rescale/P.V in the caller. This tests a register
allocation boundary, not E52/E60's slower handwritten QK or #628's 2-column body.
Locate the actual definition `^static ND_HOT void attn_heads`, B3 nd_model.c
line 1817 at inspection; the loop starts `for (i = 0; i + 7u < qk_hd; i += 8u)`
and uses qhA/qhB, kf0/kf1, s0/s1/s0b/s1b. The first textual `attn_heads` is a
comment about another function, so `s.index('attn_heads')` selects the wrong region.

**New concrete evidence:** #752's zero-frame-access claim is FALSE. In the
current B2 QK hardware loop, 0x4037c77c loads 0x470 into a11, 0x4037c78b adds a1,
0x4037c793 stores f0 through a11, and 0x4037c7b3 reloads it. A second spill/reload
uses the same slot at 0x4037c803/0x4037c847. These are indirect **a1+0x470** frame
accesses inside every 8-column iteration. A regex counting only literal `a1`
load/store operands misses them. This also invalidates the rematerialization
explanation built on zero frame traffic; it does not itself prove a speed gain.

Compare the resulting helper's actual loop and caller save/reload cost. The
29-cycle call cost (#671) must be paid; retain the hardware loop and exact
arithmetic. #649 already rejected outlining P.V, and #652 a cold fallback:
neither measured outlining this C QK body. One ordinary primary screen against
the board's pin if codegen is promising; don't build a new profiler around it.

## B1 next: finally measure the missing engram pair, safely

Compact-prefix is already on this board. It provides enough separate PSRAM
for **only the missing second site's K/V pair: 313,344 B**. Use a narrow owned
allocation/relocation for these two immutable tensors, byte-compare at open,
keep the existing 12 MB tier and all other users unchanged. This avoids changing
or bypassing B3's conservative arena guard. Compare on B1's post-wide-phi pin.

Concrete defect in the unmeasured B3 draft: it selects `m->engram[0]` at
nd_model.c around 883. That is the FIRST pair (dir229/230), already wholly staged.
The missing pair is **engram[1], dir233/234**, archive ranges
[15,707,584,15,864,256) and [15,864,256,16,020,928), beyond whole-tensor coverage
of the tier [3,256,704,15,839,616). Prove the selected offsets/pointers and live
call-site use; otherwise another exact/null run can simply be the wrong pair.

#747 `EGRELOC ok=0` measured only the guard refusing the draft, not relocation
speed. Its scan used ALL contained archive tensors, not the set actually read
through `nd_tier_ptr`; 132,032 B is slack under that guard, not a universal
live-capacity theorem. Leave that guarded draft preserved. #55's -1.15% used a
second multi-MB span; this is a 313 KB pair on a different assembly stack with
newly reclaimed capacity. One bounded screen settles this changed premise.

## Evidence to retain; claims to retire

- Installed Pi keybindings differ from the old mentor recipe: Ctrl-C clears the
  editor; Escape aborts; double Ctrl-C quits (local pi-coding-agent README,
  lines 211-228). The 21:46 Ctrl-C did not abort generation. Queued redirects
  were consumed at tool boundaries and corrected the dead-kernel edit before
  any flash. Escape at 21:49 visibly aborted the stuck turn, with all jobs/locks
  idle; a direct action redirect followed. Only abort an idle stuck turn;
  never interrupt a live board job.

- LSX byte addressing is confirmed (#744); do not run another capability probe.
  #745's 34 -> 25 cycles/word used static SRAM arrays and a 16-float LUT shared
  by all positions, not a cold field-shaped pair table. It is instruction-cost
  evidence only. #749 crashed before printing, so it has no candidate speed.
  Full uint16-offset expansion remains LOW: 4x packed-index bytes is a poor
  streaming trade at the measured bus rate. Do not revive it as written.
- #750/#751/#752 are static audits, not new measurements of 6.0617. A count of
  useful instructions is not a speed floor. In the live tie1n ELF, packed-word
  loads are INSIDE the group body, contrary to the "all l32i in prologue" prose;
  row setup has frame accesses. Zero spills also does not prove FP registers
  alone explain every rematerialized integer address/constant.
- B1 SCROW register fold was live, exact and -0.09% (#744); CV2W -0.17% (#729).
  Keep them, kron2, repeated QK wide bodies, old CQ2 schedules, private LUTs,
  prepare/LUT join fusion and printing/RoPE micro-hunts down.
- nd_fwht2/nd_fwht3s being in flash on seed is intentional cold-path refund
  (#404/#417), not a newly found hot placement defect. Do not re-add it.
- Pool-silent story was retracted (#743). Use serial_api's proven locked
  handshake; firmware emits `EVT  READY` with two spaces. Node mtimes alone
  are not USB failure evidence. B1 behavioural capture already passed all nine
  flags. Neither fact waives the frozen output failures.
- Prefer PROVENANCE's concatenated engine + assembly + app hash; `engine_md5`
  elsewhere hashes a different file set/path form. Check actual flags: #723's
  stale ND_REQ_DIAG caused the supposed dead-code slowdown.

Next mentor: did B2/B3 become real independent jobs; what did the CQ2 loop and
C QK helper measure; is the FP32 norm sidecar still a useful next lane; did B1's 5.8633 tree get its host gates and then resume discovery; and did any relocation actually select engram[1]? Do not accept carried
metrics, a disabled dispatch, or another instruction-count closure as a result.

---

## RESEARCHER STATE -- 2026-09-27 ~04:30Z (runs #756/#757: two kept above-bar lanes on the seed line; B1 packet complete)

| lane | candidate | decode | vs pin 6.0617 | exact |
|---|---|---|---|---|
| B2 | CQ2 group hardware `loop` (live `.Ltn_*`) | **6.0883** | **+0.44 %** | 6/6, delta 0 |
| B3 | QK dot outlined to `noinline qk_dot8` | **6.0750** | **+0.22 %** | 6/6, delta 0 |
| B1 | wide phi, breadth + **host gates** | **5.8633** | +0.57 % vs its own 5.83 | 18/20 (two #647) + host 19/19, fidelity 5.341e-05 |

**B2 detail:** `loop a2, .Ltn_gend` before `.Ltn_group`, tail `addi.n/beqz/j` removed; the assembler
emitted the long-loop expansion (`loop` / `rsr.lend` / `wsr.lbeg` / `l32r` / `nop`) and it uses `a2`
only as address scratch *after* the count went into LCOUNT. Three control instructions × six groups
per row is the first above-bar result on the CQ2 walker since the seed.
**B3 detail:** the outline targets a real register-allocation boundary; the live ELF's QK loop spills
through `a1+0x470` **inside every 8-column iteration** - and that **corrects the earlier zero-spill
claim** (the regex matched only literal `a1` operands).

**Owed on both:** the bitwise row differential (multi-row, real fixtures) and breadth promotion -
the 6-case screens are output evidence, not that differential.
**Assets exported:** `.auto/exp90/` (group loop) and `.auto/exp91/` (qk_dot8), each with a README and
md5-verified against the live worker files.
**B1 next:** its packet is complete (device breadth + host gates on the wide-phi tree); only the
missing **engram[1]** pair (dir233/234, 313,344 B) remains, staged in compact-prefix capacity with the
guard kept and the pair proven by offsets before any run.

---

## RESEARCHER STATE -- 2026-09-27 ~05:40Z (runs #758/#759: two seed-line keeps PROMOTED; engram[1] prepared, unmeasured)

**Both above-bar levers are now breadth-gated on the seed line** (each reproducing its screen exactly):

| lane | change | decode | ext | think | min | device | internal_free |
|---|---|---|---|---|---|---|---|
| B2 | CQ2 group **hardware loop** (+ row differential green, chunk 1/12/24, `mismatch=0`) | **6.0883** | 6.0162 | 4.67 | 5.86 | 18/20, delta 52 | 11,795 |
| B3 | QK dot **`noinline qk_dot8`** | **6.0750** | 6.0069 | 4.66 | 5.85 | 18/20, delta 52 | 12,035 |

Neither monitor regressed (ext and think both rise), so the wins are not paid for elsewhere.
Seed line: 6.0617 pin -> +0.44 % and +0.22 % on distinct boards.
**Owed:** host gates on both promoted trees; a combined-tree composition check.

**B1 engram[1] (`EG2`) - PREPARED, UNMEASURED, tree state unresolved.** Implemented exactly per the
queue: own narrow `ND_ALLOC` copy of the pair that lies outside the span (dir233/234,
`[15,707,584,16,020,928)`, 313,344 B), served by `nd_tier_ptr` through two extra ranges, selection
**by offset** (never index), both copies byte-compared, plus an `EG2 ok=1 ...` proof line. The engram
GEMVs already fetch their pointers through `nd_tier_ptr` (`nd_model.c` ~1428-1431), so the dispatch
is genuinely reached - the earlier draft's defect (selecting the already-staged `engram[0]`) is not
reproduced. The lane ran ~17 minutes with 3 processes alive but emitted no `EG2` line and no metrics,
so the screen is **inconclusive** - and B1's tree carries this change, so its provenance hash no
longer matches the 5.8633 wide-phi pin. Assets: `.auto/exp92/`. Next window: read
`R-eng2-b1.log`, complete the screen or revert the asset before any other B1 measurement.

---

## RESEARCHER STATE -- 2026-09-27 ~06:20Z (run #760: EG2 inconclusive and reverted; B1 back on its 5.8633 pin; seed line carries two promoted levers)

**B1 engram[1] staging (EG2): NO measurement, tree restored byte-exactly.** Built and flashed
(engine `a67a3d7c55a6`, app `df4fdb6b815b`), then the lane produced nothing - no `EG2` proof line,
no `EVT READY`, no metrics - for ~20 minutes; killed inside its boot window. Because the harness
writes its boot capture only when the boot completes, an app that never reaches `EVT READY` looks
exactly like this, so the state is **inconclusive**, not negative. B1 reverted to provenance
`ac2387bfb11ea0a0fb9bdf197c9ef44f` = the wide-phi pin (#755), zero `EG2`/`eg2_` traces, clean build.
**Next step for this candidate: a boot-only console capture on B1's own `$SERIAL_PORT`** (the `EG2`
line prints during `nd_model_open`, so where the boot stops is the whole diagnosis). Asset and the
avoided defect are in `.auto/exp92/`.

**Seed line (B2/B3) now carries two promoted, above-bar levers on top of the 6.0617 pin:**

| lever | board | decode | ext | think | device | evidence |
|---|---|---|---|---|---|---|
| CQ2 group **hardware loop** | B2 | **6.0883** | 6.0162 | 4.67 | 18/20 δ52 | full gate + row differential (chunk 1/12/24, `mismatch=0`) |
| QK dot **`noinline qk_dot8`** | B3 | **6.0750** | 6.0069 | 4.66 | 18/20 δ52 | full gate |

**Owed:** host gates on both promoted trees; a combined-tree composition check (the two levers live
on different boards today). **Also open:** B3's `qk_dot8` row-level differential (its arithmetic is
statement-identical C, so the risk is lower than the asm lane's, but the check is still owed).

---

## RESEARCHER STATE -- 2026-09-27 ~07:00Z (run #761: the two seed-line levers COMPOSE - new best 6.1100, +0.80 %)

| tree | decode | prefill | min | device | internal_free |
|---|---|---|---|---|---|
| seed pin | 6.0617 | 6.3967 | 5.82 | 18/20 δ52 | — |
| + group `loop` (B2) | 6.0883 | 6.4283 | 5.86 | 18/20 δ52 | 11,795 |
| + `qk_dot8` (B3) | 6.0750 | 6.41 | 5.85 | 18/20 δ52 | 12,035 |
| **both (B3, #761)** | **6.1100** | **6.4483** | **5.88** | 6/6 (screen) | 12,035 |

**+0.80 % against the pin, versus +0.66 % for the sum of the halves** - additive to slightly
super-additive, which is worth recording because most of this campaign's compositions have been
sub-additive. Prefill and min_case are both campaign bests and no monitor regressed.
Transplant was clean (B2's inert edit to the non-dispatched `.Lt1_*` kernel was reverted first, so
the two files differ only by the live `.Ltn_*` change). Assets refreshed: `.auto/exp90` and
`.auto/exp91` (md5-verified against the live workers), composition noted in `.auto/exp93`.

**Owed:** (1) breadth promotion of the composed tree; (2) host gates on it; (3) the row-level
differential for `qk_dot8` (loop half already green at chunk 1/12/24); (4) B1's `EG2` boot capture.

---

## RESEARCHER STATE -- 2026-09-27 ~07:50Z (run #762: composed tree FULLY GATED at 6.1117; EG2 silence explained as the strap trap)

**Campaign best, complete packet (device breadth + host gates on one tree, B3):**

| metric | value | note |
|---|---|---|
| decode | **6.1117** | +0.82 % over the seed 6.0617 pin, +15.2 % over the owner's 5.3033 |
| prefill / min_case | **6.4533** / **5.89** | both campaign bests |
| ext / think / boot bench | 6.0415 / 4.68 / 6.22 | all at or above the pin's |
| device | 18/20, token_delta 52 | only the two #647 ring-related goldens |
| host | 19/19, delta 0, fidelity 5.341e-05, top1 10/10 | identical to the accepted pin |
| internal_free | 12,035 | |

Composition of `.auto/exp90` (CQ2 group hardware `loop`) + `.auto/exp91` (`noinline qk_dot8`).

**CORRECTION (#760):** the EG2 silence was almost certainly **not** the candidate. Re-flashing that
tree and reading the console showed the chip in **ROM download mode**
(`rst:0x15 USB_UART_CHIP_RESET, boot:0x0 DOWNLOAD(USB/UART0), waiting for download`) - the recorded
strap trap: a console held open during an esptool reset samples the strap wrong, so the app never runs
and the harness sees exactly the silent boot window #760 read as "may crash at open". A second
attempt resetting with the console closed returned zero bytes, so EG2 is still **unmeasured**, but its
status is now "environmental, unresolved" - retry it as a normal lane when the console behaves, and
do not carry forward the crash inference.

**Owed:** `qk_dot8` row-level differential (loop half already green); EG2 retry; and the two-line
port discipline for every future bench (`$FLASH_PORT`/`$SERIAL_PORT` from the lock, console closed
during reset).

---

## RESEARCHER STATE -- 2026-09-27 ~08:40Z (run #763: B1 strap-wedged - OPERATOR ACTION NEEDED; B2/B3 free)

**Board 1 needs a physical power cycle (or host-side USB port reset).** The chip sits in ROM
download mode and never runs the app:

```
rst:0x15 (USB_UART_CHIP_RESET), boot:0x0 (DOWNLOAD(USB/UART0))
Saved PC:0x40378d55
waiting for download
```

Evidence chain: the EG2 lane died silently at 5 log lines -> console capture showed download mode ->
a retry lane reproduced it -> a clean reset with the console **closed** returned the same state ->
a bounded `!status` on board 1's own console node returned **zero bytes**. Device nodes are present
and freshly re-enumerated (console 22:49, flash 22:55), so **the nodes are fine and the chip holds
the wrong strap** - a software reset cannot clear that. After the power cycle, re-run EG2 as a normal
lane (implementation in `.auto/exp92/`; it selects the engram[1] pair **by offset** and prints an
`EG2 ok=1 ...` proof line, and the engram GEMVs already fetch pointers through `nd_tier_ptr`, so the
dispatch is genuinely reached).

**Unaffected:** the campaign best is untouched - B3 holds the composed tree at **6.1117** with device
breadth (18/20, the two #647 cases) + host gates (19/19). B2 holds the seed + group loop (6.0883
gated). Both boards are free and usable.

**Harness lessons from this window:** a `pkill -f` pattern containing `needle-board run 1` matches the
**invoking** shell and kills the command before it writes its files - use bracketed patterns; and
expand `$FLASH_PORT` **inside** the lock's environment, never in single quotes.
