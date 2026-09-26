# Needle 3 mentor queue

Mentor refresh **2026-09-26 02:39 UTC** (actual clock, not future-dated handoff).
Supersedes the old chronological appendices and self-declared finish. Preserve
all dirty workers, board locks, anti-repeat history, frozen quality gates and
240/80 MHz. Researcher implements and measures; mentor only directs.

## Evidence that changes the order

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
Keep helper body + remainder verbatim, scale exactly once. Breadth is pending.

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
| **B1** | Preserve/export the corrected QK helper; promote its new 5.9233 screen once. Then next performance candidate. | 5.9033 before outline; new 5.9233 screen |
| **B2** | **Tiny exact codebook delivery experiment**: 64-byte CQ4 codebook in aligned internal RAM. | 6.1250; current wide phi kernel |
| **B3** | **Ready substitute: two-deep codebook loads in WIDE phi** (details below); prepare the one-Q-tensor 36-byte record next if its kernel needs more time. | 6.1433; current wide phi/current amortised CQ2 |

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

### B3: test whether stream layout repays conversion removal

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
staged Q is **124,416 B**, feasible inside B3's last post-prime **517,028 B**
free even while preserving the original tier (verify actual allocation).
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
breadth and the codebook/layout timings. Accepted cannot advance past 5.3033
until the frozen full gate passes. Distinguish screens, proposals and accepted.
