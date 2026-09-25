# Needle 3 mentor queue

Mentor refresh 2026-09-25 16:54 UTC. Raw UTC log times outrank the old future-dated
researcher headings. This compact queue supersedes its chronological appendices.
Preserve all dirty trees, locks, repeat guards, frozen fixtures and 240/80 MHz.
The researcher owns implementation and measurement; never abort live board work.

## Evidence and direction

**Accepted remains 5.3033 tok/s**, bundle5: 20/20 device, 19/19 host.
Latest completed discovery #727: B2 app `b5c66b749bcc`, engine
`a9b5505a760f`, **6.0483**, ext 5.9777, think 4.65, prefill 6.3733,
host 19/19, logit delta 5.341e-05. Its full device suite is **18/20,
delta 52: NOT a quality pass**. B1 app `d0846006fc5c`, engine
`bae5184f9ca7`, is 5.7967, ext 5.7323, think 4.50, host 19/19,
same two device failures. Both latest breadth/host runs are DONE; no owed
unchanged gate remains. These are proposals, not accepted/shippable speed.
`R-echofull-b2.log` reports internal_free **13,967**, whereas #727 carries
14,991; use the actual image's raw reading for capacity planning, leave the
measured ledger intact. App/assembly/config provenance matters beyond C hashes.

**Live update 16:54 UTC:** B1 CV2W completed `R-cv2-b1.log`, engine
`7f897ace5772`, app `10b81d693587`: **5.7867 vs 5.7967 (-0.17%)**,
6/6 restricted exact, 64 selftest trials bad=0, cv2=1 and n1_align=0;
prefill 6.085, min 5.58, internal_free 10,823. No repeat/full gate for this loss.
The final kernel alternates the two ordered chains. Preserve its work/evidence;
its next useful lane is the CQ2 offset-stream probe below.
B2 `R-slice-b2.log` is LIVE: engine `d91fd5f048ca`, app `67482301e5b1`,
real locked build/flash. It retains the seven E71 wide updates and sc allocation,
and constructs each slice before that slice's unchanged SiLU. Leave it untouched.
Its comparator is eW79 app `ff82c60d9e1e`, **6.0500** restricted screen (#728),
not the earlier 6.0483 image. No additional eW79 gate is needed before discovery.
B3 remains blocked: read-only host kernel logs at 16:43-16:44 UTC show flash USB
`90:E5:B1:D1:C1:C8` disconnect/re-enumeration about every 12 seconds on
`2-1.2.1.1` / ttyACM4 while its CH343 console persists. Node presence is not
recovery. Preserve donor `b255280ba9a9`; no further serial-probe/recovery loop.

**Priority shift: reopen concrete conditioning loops, not entire phase labels.**
E71 paid +0.39% on seed by widening seven cond_u updates, yet its scale construction
is STILL SERIAL before an already parallel SiLU. Historical cond2/cond4 losses
#380/#381 used STRIDE-8 cond_v (`.auto/exp41/make_variants.py` proves it);
condT later changed that matrix to contiguous channel rows. Those old results do
not measure a wide-load ordered reduction on the current layout. A measured
variant's floor is not a proof that every implementation is optimal.

## Next three lanes

| Board | Next distinct experiment | Comparison |
| --- | --- | --- |
| B1 | CV2W lost; **CQ2 offset-stream / LSX probe next**, resident-cu if not ready. | Preserve the CV2W variant; use a recorded local base, no live control. |
| B2 | **Scale construction inside parallel SiLU is LIVE.** Harvest, then reserve at turnover. | Current eW79 screen 6.0500; keep E71 arithmetic/kernel. |
| B3 | **CQ2 predecoded offset stream / LSX** mechanism screen when hardware is genuinely usable. | Its own preserved base/pin; focused kernel first. |

If B3 stays blocked, move its ready candidate to the first healthy turnover;
do not park a healthy board for hardware recovery, packet prose or another
quality dump. Prepare the next candidate while another lane measures. Verify a
launch with actual process + nonempty growing log, not PID or tmux name alone.

### B2 first: scale construction shares the SiLU split

Current `hadamard_mlp_unscaled` builds all 1024 scale cells on core 0: one
initializer then seven `nd_lanemix4w` passes. It THEN calls
`nd_parallel_rows(silu_rows, ..., n/128)`. Put the SAME initialization and
seven updates at the start of that callback, restricted to its `[lo,hi)` slice.
Pass `cond`, `cu`, n and sc in the context; each row starts at `cu+j*n+lo`.
Each callback builds its own sc slice then consumes it in the unchanged SiLU loop.
No additional splitter invocation, nested parallelism, allocator or new assembly.
The synchronous return keeps stack cond alive; each slice has exclusive writes.
Do NOT scalarize into per-element eight-row gathers or remove sc on this first
test: that would undo E71 and confound scheduling with a new memory walk.
Keep j ascending, exact initializer contraction, wide guard/fallback and the
paired sigmoid expression unchanged. Preserve sc allocation on this first test.

This moves 65,536 independent per-token FMAs and their transport onto the two
existing lanes. It does not promise a 2x model speedup: shared PSRAM bandwidth and
slice overhead may erase the benefit. #43's separate blend splits were null;
this experiment specifically avoids a new barrier and uses E71's later kernel.
Check both split slices against the existing serial sc/SiLU output before screen.
Do not bundle unrelated product shapes (E73 failed 0/6 doing that).

### CV2W archive note: new layout tested, current version lost (-0.17%)

Current `cond_rows` gives each core four channels but reduces them ONE at a time
through scalar x/cv loads and a single 768-term FMA chain. Prototype TWO channels
together on the CURRENT channel-major matrix. Fetch four x values and four
values from each contiguous channel row with aligned wide loads; alternate the
two accumulators while each consumes indices i,i+1,i+2,i+3 IN THAT ORDER, then
advance. Two accumulators + x4 + weight4 + weight4 = 14 FP registers. Keep each
accumulator initialized to +0 and its exact scalar FMA chain. Do not use four
partials of ONE dot, a tree fold, or the old stride-8 generator unchanged.

Guard real x and cv row pointers (n1 is not automatically FAST16 just because the
weight pool is); make dispatch observable once at boot, not per token. Keep the
C tail/fallback for odd channel ranges and unsupported alignment. A compact
oracle must include nonzero channel offsets, more than one four-element tile,
real staged cv, and scalar vs split results. Reuse exp41's test logic rather than
inventing a new harness. Inspect the emitted loop for spills and lost loads.
Price real PSRAM cv + SRAM x, not two resident synthetic SRAM arrays. If this
cannot be readied promptly, take the resident-cu reserve so the lane stays useful.

The new premise is both **layout after condT AND wide delivery**. The old cond2
and cond4 losses (-0.394/-0.427%) remain valid for their old implementation.
Espressif's [matrix assembly](https://github.com/espressif/esp-dsp/blob/master/modules/matrix/mul/float/dspm_mult_ex_f32_aes3.S)
shows wide operand loads feeding independent scalar FMAs. Its
[dot-product assembly](https://github.com/espressif/esp-dsp/blob/master/modules/dotprod/float/dsps_dotprod_f32_aes3.S)
uses four partial sums and a final fold: that numerical graph is NOT a drop-in
replacement for this single-accumulator reduction. These sources inform the
transport mechanism; they do not establish a Needle speedup.

### B3 / first healthy turnover: CQ2 addresses instead of packed nibbles

New architecture lead (not a measured win): predecode ONE projection's immutable
nibbles at model open into uint16 byte offsets `64*p + 4*code`, p=0..63 within
a group. Keep the existing 16-entry pair LUT, learned codebook, prepared x,
four partial sums, nibble-to-partial assignment, first-pair seeding, fold, group
norms and row order exactly as that lane's base does them.
With group LUT base unchanged, `l16ui offset; lsx value,base,offset; add.s`
can replace `extui; addx4; lsi; add.s` plus packed-word loading. Eight pairs
would have roughly 24 body instructions versus 32 + one packed load today.
This trades extra sequential offset loads/PSRAM bytes for integer address work;
a second memory-load bottleneck may erase it. That is what the screen must decide.

Offsets are <=4092 and multiples of four; advance LUT base 4096 B per group,
reset at each row. A 576x768 Q matrix needs **442,368 B** of expanded offsets
(4x its packed index bytes), not internal RAM. The last raw PSRAM headroom was
2,062,492 B: one projection fits arithmetically, but check largest allocatable
block and failure fallback. Leave the original archive/tier and norm storage
intact for the first differential. No whole-model expansion or permanent LUT copy.

Use the existing kbench on real rows: first establish this target assembles and
executes LSX, then exact synthetic mapping (distinct table entries/norms and
multiple groups/rows), real-output differential and cold/dual-core cycles.
A kernel win earns narrow integration; an instruction count alone does not.
[Cadence ISA, LSX section 8.3.155](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf)
defines base-plus-register float loading. Local S3 core config enables FP;
actual target support/throughput still belongs to the researcher's probe.
History search found no measured predecoded-offset/LSX candidate; old unchanged
CQ2 scheduling nulls do not measure this representation change.

### Ready smaller reserve: keep four scale accumulators in registers

E71 still reloads and stores sc in all seven channel passes. Keep four output
cells resident from their exact `1 + cond[0]*cu[i]` initializer through j=1..7;
store once. Retain one FMA per cell/channel and the same order. Eight cond values,
four accumulators and four row values fit 16 FP registers; if the actual ABI or
loop spills, reduce live coefficients rather than changing arithmetic. No weight
repacking or extra permanent allocation. Walk adjacent tiles so the eight cu
row streams reuse their cache lines; compare against the board's current seven
wide passes. This is register reuse, distinct from B2's scheduling experiment.
Run it separately before considering composition with B2's winner.


## Retain the actual closures and strict gates

* E72 attention-input ownership is a measured NULL (#722); E74 MLP residual
  wide MAC is -0.03%, also NULL (#721). Do not repeat either unchanged.
* The supposed "dead code costs 6.3%" explanation is RETRACTED (#723):
  CMAKE_C_FLAGS retained `-DND_REQ_DIAG=1`, so per-token prints were live.
  Both current compile databases have zero enabled ND_REQ_DIAG entries. Check
  actual compile commands after diagnostic builds; stripping source or hashing
  only C is not equivalent to checking the image/configuration.
* E73's other ~6k-cell products stay low priority. Keep unchanged CQ2 schedules,
  both kron2 variants, old QK reschedules, private LUTs and engram projection
  overlap down. A 768-input pair table is 24,576 B, beyond current free SRAM.
* eW78 moved TOK emission after the decode timestamp; its metric gains are
  +0.35% B1 / +0.05% B2. The sources establish changed accounting/reporting,
  not a measured end-to-end latency gain. Stop hunting work to move outside
  clocks. Prefill progress prints cannot directly accelerate the decode loop.
  Preserve clock boundaries in these compute experiments and use same-app pins.
* The frozen pair remains an unresolved acceptance blocker. #647 associates
  the change with RX-ring installation; host agreement and repeatability do not
  establish a causal timer mechanism or authorize new goldens. #711/#712's
  initial think=1/OOB diagnostic interpretations were invalid. A later limited
  order probe is not proof that all state/race mechanisms are excluded.
  Do not call 18/20 a pass or make owner approval a precondition for discovery.

Next mentor: check whether B1/B2 actually ran the distinct conditioning candidates,
whether the parallel scale path paid beyond E71, and whether B1 immediately
replaced the losing CV2W screen with new work. Next
try the CQ2 offset-stream probe if not already measured; do not reopen old
packed-word schedules under its name. Read the
raw quality verdict and app/config provenance; check B3 for genuine externally
changed hardware state without repeating probes. Replace measured nulls promptly.

---

## RESEARCHER STATE -- 2026-09-26 ~07:30Z (runs #729-#730: one negative, one small keep, one repair)

**CV2W (two-channel ordered conditioning reduction) DISCARDED at -0.17 % (#729).** Kernel
correct and proven live (CV2W selftest bad=0, `cv2=1`, `n1_align=0` - the E66 lesson
satisfied), arithmetic order-preserving per channel, and still slightly negative: the
conditioning reduction is limited by its 768 dependent madds per channel, so halving the
load count does not pay. Measured on the CONTIGUOUS row layout, so the family is closed on
its own terms rather than by inheritance from the old stride-8 cond2 result.

**E72b KEPT (+0.19 %, #730): the conditioned-scale construction now happens inside the
parallel SiLU slices** instead of a separate whole-row pass, riding the split the callback
already had, with E71's kernel retained and the per-element fold order unchanged. Seed
line reads **6.0617** (screen, sub-bar but positive; full gate owed if quoted).

**Repair note (B1):** reverting CV2W with a line-range deletion overshot and removed the
PV/kron declaration block; the tree was rebuilt from the compiler's own undefined-symbol
list and now builds as `f02762ca5dbb`, which differs from the measured `bae5184f9ca7` by
declarations only. **B1's next lane must re-anchor with a screen before any candidate is
priced on it** - a declaration-only diff still invalidates the tree as a baseline.

**Next lanes:** (1) B1: the CQ2 uint16-offset/LSX kbench probe (predecode one projection
into uint16 group-LUT byte offsets, then l16ui + LSX + the same add chain; ~24 body
instructions per eight pairs against 32 plus a packed-word load; 442,368 B PSRAM for a
576x768 matrix) - target support and the extra-load tradeoff need the existing kbench with
exact mapping/row/group checks, and resident-cu is the ready fallback if the probe cannot
be prepared promptly; (2) B2: full gate on the 6.0617 tree when a number is wanted;
(3) B3: hardware blocker confirmed (host kernel shows the flash USB cycling ~every 12 s,
so node presence is not recovery) - prepare the resident-cu reserve for a healthy turnover.

---

## RESEARCHER STATE -- 2026-09-26 ~08:20Z (runs #731 + B1 re-anchor)

**Seed line gated at 6.0617 (#731):** decode 6.0617, ext 5.9877, think 4.65, min 5.83,
18/20 (the two #647 goldens) = **+14.29 %** over the 5.3033 pin - the campaign's best
fully-gated reading, carrying the lane-block sweep, the E68/E69 lifetime work, E71/E72b
conditioning work and the deferred console echo. Host gates owed on this exact image if
the number is to be submitted.

**B1 re-anchored after the repair: 5.7983** (its pre-repair same-mode value was 5.7967),
so the declaration-only reconstruction of `f02762ca5dbb` is performance-neutral within the
quantum and the tree is usable again - **its own same-mode baseline is 5.7983**, which is
what any B1 candidate must be priced against.

**Next lanes:** (1) B1: the CQ2 uint16-offset/LSX kbench probe (predecode one projection
into uint16 group-LUT byte offsets, `l16ui` + LSX + the same add chain; ~24 body
instructions per eight pairs against 32 plus a packed-word load; 442,368 B PSRAM for a
576x768 matrix) with exact mapping/row/group checks against its 5.7983 anchor, resident-cu
as the ready fallback; (2) B2 host gates if the 6.0617 number is to be submitted;
(3) B3 remains hardware-blocked (flash USB cycling ~12 s), resident-cu reserve prepared for
a healthy turnover.

---

## RESEARCHER STATE -- 2026-09-26 ~09:20Z (run #732: LSX is not there; best line packet complete)

**CV3W (uint16-offset / LSX predecode) CLOSED ON CAPABILITY, not on cost (#732).** The
probe ran `lsx f0, a2, a3` three times on the device against a known float table:
`LSXPROBE ok0=1 idx1_matches=0 idx2_matches=0 v1=0.0000 v2=0.0000`. Index 0 returns the
right value (so the opcode is decoded and does not trap), but indices 1 and 2 return ZERO -
neither the plain C index nor any shifted variant. The address arithmetic the mechanism
needs is therefore not available on the ESP32-S3 as documented, and no amount of body
scheduling can rescue it. The assembler accepts the opcode, so this could only be settled
on the board - which is exactly what the mentor's addendum asked for. Probe code removed;
B1 restored byte-exactly to `f02762ca5dbb` (anchor 5.7983).

**The campaign's best line is now a complete submission candidate:** seed `d91fd5f048ca`
= **6.0617 gated** (ext 5.9877, think 4.65, min 5.83, 18/20 with only the two #647 goldens)
**plus host gates 19/19 with logit_max_delta 5.341e-05 unchanged and top1 10/10** on the
same image - device breadth and host quality on one tree, +14.29 % over the 5.3033 pin.
The shippable line stands at 5.7967/5.7983 (+9.31 %) with the same gate structure.

**What remains:** (1) the owner's decision on the two ring-related goldens - the only item
with real upside, with evidence in #647/#711/#712/#718/#719; (2) B3's hardware blocker
(flash USB cycling ~12 s; node presence is not recovery) with the resident-cu reserve
prepared for a healthy turnover; (3) optionally B2's capture (`make capture`) if the owner
wants the submission packet tightened. Every lever family this campaign built is now
either kept-and-gated or closed by measurement or capability.

---

## RESEARCHER STATE -- 2026-09-26 ~09:55Z (run #733: capture blocked by board state; two of three boards need a physical check)

**The owed behavioural capture of the seed-line candidate did not complete, and the cause
is board state, not code.** The capture runner flashed the 6.0617 tree successfully
(flash_rc=0, hash line present, engine `d91fd5f048ca`) and then waited for `EVT READY`;
board 2's console produced **zero bytes** - no boot banner at all - through the scripted
600 s wait and a further 280 s manual read with the documented DTR/RTS discipline. Nothing
was reflashed, no loop was attempted, no process holds the lock, and the tree is unchanged.

**Hardware picture at this point: board 3 has the visibly cycling USB node (refreshed again
at 17:40) and board 2's console is silent after a successful flash, so TWO of three boards
need a physical/host-side check before more device work.** Board 1 last read 5.7983 and is
otherwise the healthiest.

**Nothing measured is in doubt:** seed `d91fd5f048ca` 6.0617 gated + host 19/19, shippable
`f02762ca5dbb`/`bae5184f9ca7` 5.7983/5.7967 with host 19/19. The only outstanding capture
debt is the behavioural suite on a candidate image.

**Harness note:** the existing capture lane only handles the ACCEPTED image on board 1 (it
resets the worker from the branch and copies sources from the main checkout). Capturing a
candidate in place is a new path and must start with a board-health check - a successful
flash does not imply a live console.

**Next window should begin with hardware triage, not another device experiment**, unless the
owner's decision on the two ring-related goldens arrives first.

---

## RESEARCHER STATE -- 2026-09-26 ~11:00Z (run #734: the conditioning levers compound on the shippable line)

**E71 + E72b ported to the shippable line and they are ADDITIVE: 5.8283 vs its own 5.7983
anchor = +0.52 %** (prefill 6.1283 vs 5.9517, min 5.62, boot 5.924, 6/6 exact). On the seed
line E71 was +0.39 % and E72b +0.19 %; together +0.52 % here, which matches their different
shapes - E71 shortens each of the seven conditioning passes, E72b removes the separate pass
altogether - so the two do not overlap. Shippable line: **5.8283 = +9.90 %** over the
5.3033 pin on a restricted screen; full gate + host gates owed on this tree.

**Hardware:** board 2's console is still silent (one further 20 s read, zero bytes, no loop
attempted) and board 3's flash node remains the cycling one - two boards need a physical
check before more device work there. Board 1 is healthy and is now the only lane that ran
this window.

**Lane economy note:** with B2 and B3 down, the productive move is to keep B1 discovering
and re-anchor its own numbers after each change (its tree changed, so the 5.7983 anchor
belongs to the pre-port image and the new anchor is 5.8283).

**Next:** (1) full gate + host gates on the 5.8283 shippable tree if that number is to be
quoted; (2) port any remaining seed-only measured lever - with E71/E72b done, the seed
line's extra content versus shippable is the quant/asm stack itself, which is a larger
question than a lane; (3) hardware triage remains the gate to using B2/B3 at all.
