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

---

## RESEARCHER STATE -- 2026-09-27 ~09:20Z (run #764: campaign-best tree gets kernel-level bitwise verification; B1 still needs a power cycle)

**Composed tree (B3) now stands on all three evidence classes at once:**

| class | evidence |
|---|---|
| device breadth | decode **6.1117**, ext 6.0415, think 4.68, min 5.89, prefill 6.4533, 18/20 (only the two #647 cases), internal_free 12,035 |
| host quality | 19/19, delta 0, fidelity 5.341e-05, top1 10/10 |
| **kernel-level bitwise** | `ROWRANGE` chunk 1/12/24 over rows=24 `mismatch=0`; `E49 diff rows=192 mismatch=0` (R=1/R=2 lines exact) |

Historical benches unchanged under this tree (EXP 129.03/112.04 mismatch 0; DOT c4 139, pair 404,
reord 214, bad=0; DIV bit_mismatch 0; the Gather 417/512 line is the long-recorded
gather-vs-pair-LUT reduction difference, not a regression).

**Still owed:** a purpose-built row-level differential for `qk_dot8` (C, statement-identical
arithmetic; passes screen + breadth). **State:** B3's flash holds the kbench image - re-flash from
its normal build before the next B3 lane; B2 free at 6.0883; **B1 strap-wedged pending a physical
power cycle** (operator action), after which EG2 (`.auto/exp92/`) should be re-run as a normal lane.

---

## RESEARCHER STATE -- 2026-09-27 ~10:10Z (run #765: engram[1] staged on the SEED line - built, UNMEASURED; B3 joins B1 in the silent-wedge state)

Implemented per spec on the seed line (B3, the 6.1117 composed tree): own narrow `ND_ALLOC` copy of
the pair containment leaves outside the span (313,344 B), selection **by offset** (never index), both
copies byte-compared, dispatch from `m->eg2_psram`, `EG2 ok=1 ...` proof line; the old guarded
`EGRELOC` draft was replaced rather than patched, and the engram GEMVs' own `nd_tier_ptr` fetches mean
the path is genuinely reached. Builds clean (app `98c47d4f349e`). Asset: `.auto/exp94/`.

**No measurement:** the lane emitted nothing for ~10 minutes - the same silent-wedge signature as
board 1 - so **B3 is now suspect for the same strap/host condition** too, and this candidate is
unmeasured rather than disproven.

**Operator action now covers B1 AND B3:** physical power cycle (or host-side USB port reset) for both;
afterwards each candidate is a normal lane whose `EG2` line prints during `nd_model_open`, so a
boot-only console read is the whole diagnosis. **B2 is the only currently usable board** (seed + group
`loop`, 6.0883 gated) - schedule new work there first.

---

## RESEARCHER STATE -- 2026-09-27 ~10:50Z (run #766: composed tree CONFIRMED on a second board; B2 now carries the campaign best)

| board | tree | decode | vs its own pin |
|---|---|---|---|
| B3 | composed (group `loop` + `qk_dot8`) | **6.1117** (full gate) | +0.66 % over 6.0617 |
| B2 | **same composition, ported** | **6.1083** (screen) | **+0.33 % over its 6.0883** |

Two boards agree to **0.06 %** (inside the campaign's 0.071 % three-identical-image spread), so the
`qk_dot8` half is a real lever on an independent board, not a single-board artefact. prefill 6.445,
min_case 5.88, 6/6 exact, delta 0, internal_free 12,051.

**Resilience:** with B1 and B3 both in the silent strap-wedged state, **B2 now carries the campaign's
best tree**, so the top packet has a live home during the outage. Asset refreshed:
`.auto/exp91/nd_model.c.qkdot8.b2` (md5-verified against the live worker).

**Owed:** device breadth + host gates on B2's version of the composed tree (B3's identical tree
already has breadth, host gates and the kernel-level row differential, so this is confirmation);
the purpose-built row-level differential for `qk_dot8`; and the two power cycles.
**Blocked candidates awaiting boards:** `.auto/exp92` (EG2 on the shippable tree, B1) and
`.auto/exp94` (EG2 on the seed line, B3) - both implemented, both unmeasured.

---

## RESEARCHER STATE -- 2026-09-27 ~11:40Z (run #767: EG2 crashes at open on all three boards; B2 restored; the "strap wedge" reading corrected)

**Finding (decisive):** B2 - which had *just* completed the composed-tree run (6.1083) and has never
shown a boot problem - was given ONLY the EG2 change (`.auto/exp94`'s `nd_model.c`/`nd_model.h`) and
immediately produced the identical silent-wedge signature (5 log lines, no `EG2` proof line, no
`EVT READY`, no metrics, 3 processes alive after ~10 min). Three boards, the same change, the same
silence, with B2's minutes-earlier control as the discriminator. So **EG2 crashes at `nd_model_open`**
and the earlier "board 1 is strap-wedged / needs a power cycle" conclusion is at least partly the
campaign's own recorded trap (#737): a **crash loop re-enumerates USB and looks exactly like a dead
host path**, and a looping chip can also sit printing the ROM banner I captured.

**Two candidate causes examined and BOTH REFUTED by reading:**
1. *Uninitialised engram tensors* - refuted: `nd_cact_tensor(&m->c, base + s*4 + 0/1/2, ...)` fills
   `m->engram[s]` at line **711**, while the staging block runs at line **879**, i.e. the tensors are
   loaded first.
2. *`nd_tier_ptr` dereferencing uninitialised `eg2_*` fields* - refuted: no `nd_tier_ptr` call exists
   before line ~1434; the first is inside the engram step, long after the init in the staging block.

**So the crash cause is UNKNOWN and must be measured, not guessed.** The right next step is a
**boot-only console backtrace** on B2 with the EG2 image: the ESP-IDF panic printout names the fault,
the PC and the frame, which is the whole diagnosis, and B2 is the one board whose console path is
currently usable. Do not spend another lane on EG2 before that read.

**State:** B2 restored byte-exactly to its composed tree (EG2 traces 0, `qk_dot8` 2, clean build) and
it carries the campaign's best, breadth-gated packet (6.1083 screen here / 6.1117 full gate on B3).
Assets: `.auto/exp91` (composed, B2), `.auto/exp92` (shippable EG2), `.auto/exp94` (seed EG2).

---

## RESEARCHER STATE -- 2026-09-27 ~12:20Z (run #768: EG2 does NOT crash at open - it stages correctly and faults AFTER priming; #767 corrected)

**Direct B2 boot-console read (board's own port, opened only after the flash) settles the mechanism:**

```
EG2 ok=1 bytes=313344 k=[15707584,15864256) v=[15864256,16020928)
EVT ready model=needle3 layers=8 d_model=768 vocab=8192 window=384 tools=3 psram_free=1423508 internal_free=12003
EVT priming tokens=143 / 213
Guru Meditation Error: Core 0 panic'ed (StoreProhibited)
Backtrace: 0x4283fffd:0x3fccda50      <-- unusable: 0x4283fffd > _text_end (0x42028374)
rst:0xc (RTC_SW_CPU_RST) ... loop
```

So: **the by-offset selection is exactly right** (the second site's K/V at the mentor's stated ranges,
313,344 B), the model opens, reports ready and primes both prefixes, and only **then** faults with a
StoreProhibited - and the reboot loop is why every lane went silent. **#767's "crashes at open" and
#763's "board 1 is strap-wedged" are both corrected**: a crash loop re-enumerates USB and looks like a
dead host path, the campaign's #737 trap.

**Status:** mechanism works, consumer fault not root-caused, backtrace garbled. **Next step is an
instrumented read, not another lane:** print the pointers the two engram GEMVs actually receive plus
the tensor metadata they are parsed with (the one thing never observed). Working hypothesis to check
first: the staged copy is handed over as the packed base while the **norms** pointer still comes from
the tensor struct, so a layout/bit-width mismatch would misparse exactly where it faults.

**Boards:** B2 restored byte-exactly to its composed tree (EG2 0, `qk_dot8` 2, clean build) and holds
the campaign best packet (6.1083 screen here; 6.1117 full gate on B3). B1/B3 may be **fine** after all
- their "wedge" was this same crash loop - so no power cycle is necessarily needed; re-flash a known
good image and read the console before treating either as hardware.

---

## RESEARCHER STATE -- 2026-09-27 ~12:55Z (run #769: pool state settled - B1 truly strap-wedged, B2 healthy and best, B3 untested; EG2 boots and faults after priming)

**Board 1 is GENUINELY strap-wedged - proven, not inferred.** B2's known-good composed image
(310,752 B) was written straight to B1 at `0x10000` (`write rc=0`, "Hard resetting via RTS pin...")
and the console then read 45 s: 71 bytes of ROM chatter ending in `waiting for download`. A correct
app plus a reset does not dislodge it, so **only a physical power cycle / host USB port reset** will.

**Both earlier readings reconcile - they were both partly right:**
* #763's strap conclusion was RIGHT for board 1.
* #767's "EG2 crashes at open" was WRONG: #768 proved EG2 boots (proof line, `EVT ready`, both
  prefixes primed) and faults only **after** priming.

| board | state | notes |
|---|---|---|
| **B1** | strap-wedged | power cycle owed; no measurement possible |
| **B2** | **healthy, campaign best** | composed tree, 6.1083 screen, engine `ab771aac89b2`, restored byte-exactly |
| **B3** | untested since its EG2 flash | either the EG2 crash loop or fine - **re-flash a known-good image + read the console** is the cheapest way to get a third board back |

**EG2 status:** works exactly as designed (by-offset selection, `k=[15707584,15864256)`,
`v=[15864256,16020928)`, 313,344 B) and faults with StoreProhibited after priming. Backtrace garbled
(above `_text_end`), so root cause is open and needs the **instrumented read** (print the pointers the
two engram GEMVs receive plus the metadata they are parsed with), not another lane.
