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
