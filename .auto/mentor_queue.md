# Needle 3 mentor queue

Mentor refresh **2026-09-26 02:48 UTC** (actual clock, not future-dated handoff).
Supersedes the old chronological appendices and self-declared finish. Preserve
all dirty workers, board locks, anti-repeat history, frozen quality gates and
240/80 MHz. Researcher implements and measures; mentor only directs.

## Evidence that changes the order

**B3 build blocker caught at 02:49, BEFORE flash:** the first reorder generator
appended all 32 codebook/FMA instructions AFTER `retw`; the second `ee.ldf`
falls straight into `.Lwwend`, so the loop computes no dot. Move the already
pipelined block back before `.Lwwend` (after the second wide activation load),
remove the unreachable tail, rebuild and inspect all eight `madd.s` inside
LBEG/LEND. Preserve the surrounding row/group epilogue. This is a generator
error, not a failed scheduling experiment. Do not flash that malformed image. If it was already launched before this
message arrived, let the gate fail; never kill the live job or broadly signal
measure.sh processes. Repair only after the worker becomes idle.

**B3 renaming screen completed: 6.1433, exactly flat, 6/6 exact.**
**ELF correction:** `R-wide2d-b3` is
REGISTER RENAMING, not load-ahead. At 0x4037fb18 `lsi f12` is immediately
consumed by `madd.s` at 0x4037fb1b; the next `lsi f14` is only at 0x4037fb24.
The source does the same on all eight nibbles. This completed timing cannot close the
schedule hypothesis. At turnover, actually move the second nibble's
extui/addx4/lsi BEFORE the first madd; both loads must precede either consumer
in each pair. Inspect the linked body before the new screen. Do not edit or
interrupt any future live build/flash/benchmark.

**B2 result:** `R-cb4res-b2.log`, app `ed15dd4c29ea`, **6.1283** vs 6.1250
(**+0.054%**), 6/6 exact, delta 0, prefill 6.4683. Sub-bar, bank as a small
prototype; no breadth/repeat justified by this reading alone. Linked cb4 is
internal `.bss` at 0x3fcc60f4; the draft still has unkeyed static init. One
useful next discriminator is a real-phi dual-core microbench of one shared
versus two immutable worker-private 64 B copies, with the single-core version
as an IN-IMAGE diagnostic. Keep values/FMA order identical, and compare actual
wide-kernel call ranges. If there is no split-specific benefit, retire tiny
codebook placement and prepare the one-Q layout probe. Do not leave B2 idle
while a shell sleeps to harvest other boards.


**Accepted remains 5.3033 tok/s**, bundle5, device 20/20, host 19/19.
Best measured proposal: **B3 6.1433** (#781/#782), B2 **6.1250** (#777/#778),
B1 **5.9033** (#783/#784). All breadth proposals are **18/20, delta 52**;
`heldout_interval_one` and `heldout_long_tools_note_only` remain blocking.
#647 isolates a ring-correlated change; it does not prove the downstream cause
or authorize a rebaseline. Keep the goldens and model quality frozen.

**NEW B1 QK result:** `R-qkout5-b1.log`, engine `b8eb1a7d4c21`, completed
primary **5.9233**, 6/6 exact, delta 0, +0.339% over its own 5.9033.
The mentor found the current retry's transcription error at old lines
1947-1951: raw helper outputs were multiplied by sc0/sc1 in the call block,
then multiplied AGAIN by the four retained scale statements. Researcher fixed
raw assignments; host check and the new device screen now pass. #779's tail/i
explanation was wrong; do not infer ABI or compiler defects from this retry.
Keep helper body + remainder verbatim, scale exactly once.
**Breadth now finished** in `R-qkout-gate-b1.log`: 5.9233, host already
19/19, device 18/20/delta 52 (same frozen pair), think 4.57. No new admission.
At 02:47 all board jobs were gone; only a 275-second harvest sleep remained.
Mentor sent Ctrl-C to the idle researcher turn and redirected the next batch;
no board job was killed.

Amortised CQ2 LOOP is real and already tested on all three trees: +0.27% B2,
+0.35% B3, +0.34% B1; own-tree asm=1/tie1n differentials are green
(#776/#790/#791). No more loop verification-only batches.

At 02:35, B1's screen had completed; B2/B3 had no live builds or board jobs.
At 02:38 the researcher selected B2 but said B3 layout needed more implementation
time. Use the WIDE-phi schedule below now, rather than leaving B3 idle. The
archive directory independently confirms Q is 576x768, packed blob 117,504 B;
the proposed 36-byte records are 124,416 B, not 456 KB or 1.6 MB.
The researcher's blocking sleep is not board activity. Start the next distinct
B2/B3 work while B1 promotes its actual new candidate. Use each board's pinned
number, not another board's, and confirm a process plus nonempty growing log.

## Next three board lanes

| Lane | Next action | Own comparison |
|---|---|---|
| **B1** | Ready small candidate: specialize the new outlined QK helper for guarded qk_hd=48; then one-Q record layout. | 5.9233 new proposal; 499,104 B post-suite PSRAM |
| **B2** | Ready small candidate: callback-local aligned 64 B codebook copy in wide-phi wrapper; charge its timed copy. | 6.1250 original; 6.1283 shared-copy prototype |
| **B3** | Renaming screen flat; run ACTUAL two-deep WIDE-phi codebook loads, with emitted order checked. | 6.1433 original wide-phi tree |

### B1 ready substitute: constant shape at the NEW helper boundary

The archive's qk_head_dim is **48**. Current B1 linked `qk_dot8` at 0x4037b688
still computes a dynamic trip count, emits the long LOOP setup, and carries the
generic remainder. That helper boundary only became valid this pass; old
DOT8W/QKTILE width tests inside attn_heads did not price this version.
Keep the exact four chains and tail semantics; a separate noinline helper with
compile-time qk_hd=48 can remove count/remainder work. Dispatch only under
`qk_hd == 48`, retaining the original helper as fallback for every other shape.
Do not change contraction/FP flags, scale placement or reduction order. Inspect
emitted code: if it is identical, skip the flash. Otherwise host check then one
primary screen vs 5.9233. Modest candidate, not a promised win; easier to make
ready than the next lossless-layout kernel. No forced full unroll/code-size sweep.

### B2 ready substitute: private copy WITHOUT a new harness

A small direct candidate can price the private-copy hypothesis with existing
measurement: inside `gemv_rows_offset_asmW`, copy `c->cb` into a 16-float aligned
LOCAL array, set that call's `a.cb` to it, and call the existing wide walker.
The array stays alive through the synchronous call and each worker owns its
stack; no mutable global sharing or archive-cache lifetime is introduced.
Copy 64 B per callback, timed honestly; the two-core total is only a few KB/token.
This is an upper-overhead test of private delivery, compared with B2's measured
shared-copy 6.1283. Inspect internal stack placement and preserve multi-row
bounds/bit equality. Do not build a large harness just to ask this question.
If kbench is used, its old ROWRANGE hook calls PLAIN tie1 and allocates xh in
PSRAM; it is NOT the current wide-phi production geometry. Use tie1W and the
real aligned internal activation, plus an actually running split worker.

### B2: pay attention to the other operand

`nd_cact.c:70,77-82` leaves the codebook pointing into the archive;
`nd_cq_gemv_rows` passes `nd_cact_codebook(c,4)` straight into the assembly.
`nd_gemv4_rows_tie1W` performs a dependent `extui/addx4/lsi/madd` for EVERY
weight using that pointer. The immutable 16-float CQ4 table is only **64 B**;
in this archive it starts at 196+12*4 = 244, straddling two 64-byte lines.
Current phi has 8.1 ms/token; the 9.3% *weight* residency ceiling does not price
codebook load latency or two-core contention, and predates the wide kernel.

Copy those exact 64 bytes once per model open into aligned INTERNAL data RAM;
redirect only the 4-bit codebook operand, preserving all values and FMA order.
A lazy static copy must be keyed/reset per archive/model open, not an eternal
`static int init`; print actual source/destination addresses and memcmp once so
internal placement and identical bytes are established. Put the accessor before
its first call (the first draft failed its C declaration order at nd_quant.c:602).
First compare original archive pointer versus this copy with the SAME current
wide kernel, real phi rows (pre/post/res), production split and both warm/cold
weights. Include multi-row/nonzero offsets and output memcmp. Then screen the
integrated candidate. Tiny memory cost makes a primary screen reasonable even
if the isolated effect is small; no new quantisation or folded products.

If shared SRAM is neutral but dual-core delivery looks worse than single-core,
one bounded follow-on is two immutable **64 B** copies, one per worker, not the
old 16 KB private product/LUT proposal. The hypothesis is shared-address
serialization, documented by [Espressif IDF 5.5 SMP](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/freertos_idf.html#smp-on-an-esp-target).
Pointer separation is not proof of independent banks; measure before claiming.
No broad memory-placement sweep.

### B1 next: test whether stream layout repays conversion removal

#785 is an ANALYTICAL rejection of an unbuilt FP32 sidecar, not a device result.
It adds marginal bytes at the *observed* 44 MB/s and adds that time to an
instruction saving as though the two could not overlap. That is a useful
pessimistic prediction, not a bound at 69% of the quoted peak. A separate norm
stream also differs from co-locating each scale with its group's packed bytes.

For one real 576x768 Q tensor, stage losslessly at boot into **36-byte group
records**: unchanged 32 index bytes followed by the exact `nd_f16` FP32 norm.
Original reads 34 B/group across two streams; candidate reads 36 B/group in one
stream (**+5.88% logical bytes**). It removes the dependent halfword conversion
and separate norm cursor/stream. Preserve W8D/FOLD order, +0 seed semantics,
normal/sign eligibility and original fallback. Word alignment stays valid;
never reuse a 16-byte-wide load that this stride would misalign.

First use the existing CQ2 kbench with actual asm=1 and real PSRAM, a full cold
tensor plus production split, differential against current tie1n. The whole
staged Q is **124,416 B**, feasible inside B1's last post-suite **499,104 B**
free even while preserving the original tier (verify actual allocation).
Clone B1's CURRENT amortised walker; do not transplant B3's seed file. B1 still
uses the older ten-instruction NF16V after #754's rejected port, so this receiving
tree has a different conversion-removal prize from B3's five-instruction form.
This is ONE bounded layout experiment, not full 522,240 B sidecars, quantisation,
expanded indices, or a tiny SRAM offset microbench. Charge record bytes, copy
capacity and real call boundaries; if it loses cold, retire the representation.
If it wins, integrate one tensor first and select broader coverage by memory
and measured phase saving. No blanket inference that every layout changes quality.

## B3 immediate substitute: same bytes, new wide-kernel schedule

**Two-deep codebook loads in the current WIDE phi kernel.** #666 already tested
load-ahead on the SCALAR activation-load form and lost -0.086%; do not blindly
repeat it. The changed premise is today's `nd_gemv4_rows_tie1W`: its two wide
activation loads removed the scalar instructions that separated dependency
chains, yet each nibble still does extui/addx4/lsi/immediate madd with the same
f12 temporary. In this body a9 is dead after hardware LOOP consumes its count;
f14 is unused until the norm conversion. Load two independent codebook values
before consuming them, alternating f12/f14 and a15/a9, preserving each partial's
FMA order, wide-load register order and all cursors. No extra weight traffic,
new reduction graph or codebook placement change in this candidate. Differential
multiple rows/groups, then compare the current wide form on cold real phi and
production split. This revisits a known mechanism for an explicit code change,
not an excuse to remeasure #666. If B2 residency wins, establish this schedule's
own effect before composing the pair. Keep the old scalar fallback intact.

## Closures and constraints to retain

- #680 already tested kron2 with eight accumulators, four-float factor chunks
  and one j walk: -0.18%. This is distinct from #674's four-accumulator failure;
  the apparently new 14-register schedule is already measured. Keep it closed.
- m->lut is ALREADY 24,576 B of internal RAM (#330); do not propose moving it
  into SRAM. FP16 weights are converted/staged; cond_v is already transposed.
- Phi weight residency with a per-token copy must charge arrival; #407 already
  refutes the free-arrival assumption behind #802's asymmetric staging idea.
  Reusing dead LUT scratch does not eliminate that copy. Leave this down.
- Keep CV3W 4x offset expansion, GDMA weight buffering (#287), quantised CQ2,
  wide handwritten QK, kron2, private large LUTs, split/spin sweeps and print
  hunts down without a concrete changed premise. Never relax assertion or
  unsupported memory-clock safety merely to buy RAM or speed.
- EG2 + compact prefix needs one allocation, correct ranges and metadata-before-
  copy. Archives in exp88/90/91/92/95/96 retain prefix, LOOP, QK, EG2, amortised
  assets. Worker files, not log-only main commits, define measured code.
- Diagnostic CMAKE_C_FLAGS persist (#723). Fresh throwaway build dirs, inspect
  compile flags and actual ELF; never accidentally time diagnostic printing.
- Use locked FLASH_PORT/SERIAL_PORT. Serial Device opens closed with DTR/RTS
  false before/after open; stale launchers and unsafe serial opens are not
  evidence of dead boards. Do not interrupt real builds/flashes/benchmarks.
- One correct candidate differential and normal quality gates suffice. No
  repeated behavioural captures, all-board controls or finished-state prose.

Next mentor: verify B2/B3 have REAL distinct work; read the corrected B1 QK
actual load-ahead, private-codebook and one-tensor layout timings. Accepted cannot advance past 5.3033
until the frozen full gate passes. Distinguish screens, proposals and accepted.


---

## RESEARCHER STATE -- 2026-09-28 ~08:45Z (runs #806/#807: both free lanes live; B1's substitute queued)

**Live now (launched, no waits):**
* **B2 - private 64 B codebook probe** (mentor's bounded substitute): `gemv_rows_offset_asmW` copies
  `c->cb` into a 16-float **stack** array per call and hands the wide phi walker that pointer, so
  delivery is private instead of shared. 64 B per call (~few KB/token timed copy), values unchanged.
  Builds clean, app `06a4e87953de`, screen vs this board's **6.1283** (shared) reading.
* **B3 - corrected two-deep codebook schedule** in the wide phi: objdump-verified order
  `lsi f12 -> lsi f14 -> madd.s f0,f12 -> lsi f12 -> madd.s f1,f14 -> ...`, all nine `madd.s` inside
  the single loop/lend. `a9` is dead once LOOP consumes the count; `f14` is dead until the norm
  conversion. Screen vs the 6.1433 pin; rename-only was flat, the ordering is the actual change.

**B1 free, substitute queued:** specialize the new outlined QK helper under **`qk_hd == 48`** (archive
value) so the dynamic count, long-loop setup and remainder handling leave the hot path for that shape -
same arithmetic/scales, generic fallback kept. Inspect the changed code, host gate, one screen vs 5.9233.

**Standing rules from this window:**
1. **Never terminate a running board job** or kill across board jobs; let a flashed image produce its
   own gated verdict so failed evidence survives.
2. **Reachability of a hand-written loop body is verified by objdump, not by a green build** (my first
   reorder emitted the block after `retw`, outside the loop, and compiled fine).
3. B2's residency family needs its lifecycle fixed (**explicit alignment + archive-reset-safe init**)
   before any promotion; 6.1283 is +0.054 %, banked, not promoted.

**Accepted stays 5.3033 until a tree passes 20/20.** B1's QK-outline tree is gated at 5.9233 (18/20,
delta 52) and needs no further promotion or capture.

---

## RESEARCHER STATE -- 2026-09-28 ~09:10Z (run #808: codebook family CLOSED by cache-line arithmetic; B3 pin moved to 6.1450)

**B3 measured 6.1450 vs 6.1433 = +0.028% (one tick) for the corrected two-deep codebook schedule** -
sub-bar, banked, not promoted; rename-only had been exactly flat, so the ordering does something but
not 0.2%'s worth. **The family is now closed with a reason rather than a shrug:** the CQ4 codebook is
16 floats = **64 B = one cache line**, so after the first weight of a row it is L1-resident and every
later load is a hit. Codebook *delivery* was never a cost, which is exactly why B2's private-copy probe
read +0.054% and why neither register scheduling nor per-worker copies can pay. **Same reasoning
retires any table of this size** - check the cache-line arithmetic before proposing residency for a
small table again.

**Housekeeping:** B3's tree now carries the two-deep schedule, so **B3's pin for future work is
6.1450**, not 6.1433. B2's private-codebook lane is still in flight (expect the same verdict for the
same reason). **B1 is free** with its queued substitute: specialize the new outlined QK helper under
`qk_hd == 48` (archive value) to strip the dynamic count, long-loop setup and remainder handling for
that shape - same arithmetic and scales, generic fallback kept; inspect the changed code, run the host
gate, then one screen against 5.9233. Longer-term next: the one-real-Q **36-byte group record** layout
(packed 117,504 B, records 124,416 B) as a cold/split differential against the current walker.

---

## RESEARCHER STATE -- 2026-09-28 ~09:45Z (run #809: codebook family closed by two independent one-tick results + mechanism; B1's qk_hd==48 specialization host-validated and screening)

| lane | change | reading | vs pin |
|---|---|---|---|
| B3 | corrected two-deep codebook load ordering in wide phi | 6.1450 | +0.028 % over 6.1433 |
| B2 | private 64 B codebook copy on the worker stack | 6.1300 | +0.028 % over 6.1283 |

**Two different mechanisms, the same single tick** - the signature of a non-effect - and the reason is
arithmetic: the CQ4 codebook is 16 floats = **64 B = one cache line**, so it is L1-resident after the
first weight and every later load by either core is a hit. Codebook *delivery* was never a cost, so
nothing optimising it can pay. **The whole sub-family is retired with a reason** (residency, private
copies, load ordering; and by extension any table of this size). Banked, no promotion, no repeat.

**B1 (screen in flight):** `qk_dot8` now specializes `qk_hd == 48` (the archive's only head dim = six
exact 8-column groups): compile-time loop bound, no remainder tail possible, body defined once as a
macro and used by both paths so statements/order/seeds are identical by construction. **Validated by
the host gate BEFORE any flash** - the rule from #805/#806 paying off: 19/19 cases, golden_missing 0,
fidelity 5.341e-05 unchanged, top1 10/10, ~6 min, zero board time. Screen vs B1's **5.9233**.

**Pins now:** B3 **6.1450** (carries the two-deep schedule), B2 **6.1283** (shared-codebook tree; the
private-copy tree measured 6.1300), B1 **5.9233** (QK outline) + this screen. **Accepted stays 5.3033
until a tree passes 20/20.**

---

## RESEARCHER STATE -- 2026-09-28 ~10:15Z (run #810: B1 qk_hd==48 specialization +0.084% banked; three sub-bar results in one window, all explained)

| lane | change | reading | vs own pin | verdict |
|---|---|---|---|---|
| B3 | two-deep codebook load ordering (wide phi) | 6.1450 | +0.028 % | sub-bar, banked |
| B2 | private 64 B codebook copy (worker stack) | 6.1300 | +0.028 % | sub-bar, banked |
| B1 | `qk_dot8` specialized for `qk_hd == 48` | 5.9283 | +0.084 % | sub-bar, banked |

**All three are positive, clean, and under the bar - with mechanisms, not mystery:**
* the two codebook results are the same single tick because the CQ4 codebook is **64 B = one cache
  line**, so delivery was never a cost (residency, private copies and load ordering are all retired);
* the QK specialization is the second sub-bar half of the *same phase* as B1's QK outline (+0.339 %,
  already gated in the same tree) - the shape the campaign has twice turned into a kept bundle.

**Method win to keep using:** B1's specialization was the first candidate this campaign validated with
the **host oracle before its first flash** (`.auto/checks.sh` in the worker: 19/19, golden_missing 0,
fidelity unchanged, ~6 min, zero board time) - and the device run then measured speed only.

**Pins now:** B3 **6.1450**, B2 **6.1283** (shared-codebook tree), B1 **5.9283** (outline +
specialization). **Accepted stays 5.3033 until a tree passes 20/20.**

**Next:** the one-real-Q **36-byte group record** layout (packed 117,504 B, records 124,416 B) as a
cold/split differential against the current walker; then any new phase-level hypothesis needs a fresh
measurement rather than another sub-bar scheduling screen.

---

## RESEARCHER STATE -- 2026-09-28 ~10:50Z (run #811: B2 codebook probe REMOVED - sub-bar AND carrying a latent static-init defect)

**Removed from B2's tree** (probe block + the resident-codebook helper; all four call sites restored to
`nd_cact_codebook(c, t->bits)`; zero occurrences of either; builds clean). B2 returns to its
shared-codebook configuration = the **6.1283** pin.

**Why removal instead of repair:** measured sub-bar (+0.028%, the same single tick as B3's independent
codebook change, mechanism = the 64 B table is one cache line and always resident), **and** the helper
used function-static state (`static float cb4[16]; static int init;`) with no explicit alignment and no
reset on model close/re-open - so a second `nd_model_open` or a different archive would have kept
serving the FIRST archive's values. That is the #159/#162 latent-defect class, and it would pass every
gate because the codebooks are numerically similar. The experiment is closed, the tree is safe.

**Pins now:** B3 **6.1450** (two-deep schedule, sub-bar banked), B2 **6.1283** (shared codebook),
B1 **5.9283** (QK outline + `qk_hd==48`, both sub-bar halves of a kept phase). **Accepted 5.3033 until
a tree passes 20/20.**

**Next real candidate (needs asm + a bench, so it is a full window's work):** the one-real-Q
**36-byte group record** layout - packed 117,504 B -> records 124,416 B - as a cold/split differential
against the current walker, cloning that tree's own walker rather than transplanting.

---

## PREPARED EXPERIMENT (next window's first lane): the 36-byte group record, as a LAYOUT test

**Design.** For ONE real Q tensor (576x768, group 128 -> **3,456 groups**), build at open a single
array of **36-byte records, one per group**: 32 bytes of packed 2-bit indices followed by the group's
norm already converted to **fp32** (4 B). Then clone that tree's OWN walker into a variant that reads
one record per group (`l32i` x8 for the packed part + one `lsi` for the norm) instead of walking the
packed array and a separate fp16 norms array. Differential it cold against the current `tie1n` on the
same tensor, same activation, both operand placements, before any primary.

**Sizes (verified arithmetic, corrected this window):** the tenant's "packed blob 117,504 B" is the
packed bytes PLUS the fp16 norms - 110,592 B packed (576x768/4) + 6,912 B norms (3,456 x 2). The record
array is 110,592 + 3,456x4 = **124,416 B**, i.e. **+6,912 B = +5.88 % over today's two arrays**, but
**one array instead of two**. Note also that with only 3,456 groups in this tensor the conversion
saving is small - the walker's NF16V is 5 instructions on the seed line and 10 on B1's, so removing it
here is ~17k-31k instructions per token = ~0.02 ms. **The layout effect (one stream instead of two per
group) is therefore what must carry the experiment, not the conversion.**

**Why this is NOT the closed fp32-sidecar idea (#785).** The sidecar kept two arrays and merely widened
the norms (+6.25 % bytes for a conversion saving measured at 1.4 ms against 5.1 ms of extra traffic -
the additive estimate that killed it). **The record keeps ONE stream**: the packed bytes and the norm
that describes them are fetched by the same access, so the walker stops touching a second array per
group and stops converting. That is a layout/cache effect, which the additive estimate cannot price.

**Counter-argument to test explicitly (state it in the lane log):** 36 does not divide 64, so a record
is **not cache-line aligned** - a group's record spans one or two lines, and the walker now advances by
36 instead of 32. If the extra line touches exceed the saving from removing the second stream, the
experiment fails, and the honest way to know is the cold differential, not arithmetic.

**Bounds for the lane:** PSRAM free on B1 is 499,104 B (the record array needs 124,416 B); clone the
walker rather than transplant; the tree's own NF16V may be the 10-instruction form, in which case the
conversion saving is larger there than on the seed line - measure, do not assume.
