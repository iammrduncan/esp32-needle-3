# Needle 3 mentor queue

Mentor refresh **2026-09-26 05:04 UTC** (actual clock). This replaces the
chronological finish notes; their history remains in git and `.auto/log.jsonl`.
Researcher implements/measures. Preserve all dirty trees, frozen quality gates,
board locks, anti-repeat history, assertions, and supported 240/80 MHz clocks.

## Direction now: three distinct candidates, not further closure essays

At inspection all three boards were idle: no build/flash/benchmark processes,
no held board locks; newest board log ended at 04:32. Runs #849 onward mostly
reuse old metrics for reasoning. Start the lanes below. A failed candidate is
useful evidence; an untested analytical prediction is not a measured closure.
Prepare sequentially if necessary, launch each ready lane promptly, and continue
preparing the others while it runs. No all-board controls or waiting for one lane
before preparing another. Use the existing harness, not a new harness project.

**Pins:** owner-accepted **5.3033 tok/s** (bundle5). Candidate references are
B1 **5.9283**, B2 **6.1250**, B3 **6.1450**; the strongest prior breadth packet
is B3 **6.1433**, still blocked by the frozen pair. #844's new B3 specialization
was FLAT at 6.1450; its printed 24/24 used accidentally replaced goldens and is
NOT admission evidence. #844/#845 restored the twenty original entries and
added four previously missing ones. Expected blocked breadth is now **22/24,
delta 52**, host 23/23; no rebaseline, AUTO_SAVE or capture-only detour.

## Why the ordering changes

* **#814's 36-byte layout rejection double-counts shared cache lines.** For a
  contiguous aligned run of 16 records, 16*36 = 576 bytes = NINE distinct 64-B
  lines, not 18. A boundary line serves adjacent groups; spanning it twice does
  not imply fetching it twice. Traffic is 36 vs 34 B/group asymptotically
  (+5.88%), with conflict/capacity effects to MEASURE. There is no 2.12x lower
  bound. Lossless repacking does not change quantisation or model quality.
* **#853/#857 do not measure today's four-output `qk_dot8`.** In the actual
  kbench, `best_c4` divides the cycles of 128 `dot_c4` calls by 128. Thus
  `c4_pair_cyc=139` is PER SINGLE DOT despite its label, not per pair. The
  current helper computes four dot results, with pair-expression rounding.
  Its body is not one madd per term. Do not infer stall cause, IPC saturation,
  a 70-cycle target, or +2.5% token gain from the old number.
* #849's aggregate weights / wall cycles do not establish a per-core issue
  limit for a dual-core row split. Internal SRAM staging is also not an L1
  cache-hit argument. Keep the measured nulls; drop universal impossibility.
* #854's logf=350 cycles contradicts the actual **#413: 22.63-23.13 cycles**.
  Do not start another logf lane from the invented price. Phase timers overlap.

## Next lanes

| Board | Candidate | Own baseline |
|---|---|---|
| **B1** | One-Q lossless 36-byte group records, reopened by the corrected line accounting | 5.9283 |
| **B2** | Interleave the current outlined QK helper's FOUR independent chains at each term pair, retaining 8-column loop | 6.1250 |
| **B3** | Separate 64-bit-load QK implementation with a register-budgeted schedule and unchanged expression graph | 6.1450 |

**B1: representation, not a new quantiser.** For one real 576x768 Q tensor,
3,456 records contain the unchanged 32 packed bytes followed by exact
`nd_f16` FP32 norm: **124,416 B total** vs 117,504 B original. Stage at open,
retain original/fallback, and charge capacity (last B1 post-suite free 499,104 B)
and init cost separately. Clone B1's OWN amortised walker and its seeds/folds;
replace the per-group norm conversion with one float load. Preserve row stride,
multi-row and odd split handling; 36-B records are word-aligned, not 16-B aligned.
Use the existing real-CQ2 kbench on the full PSRAM tensor with production split,
own-tree asm=1 differential, and warm/cold readings. This is not #749's 4x
expanded index stream. If it wins, integrate this Q tensor for a primary screen;
if not, record the actual result and retire it. Do not reject it again by summing
line touches as compulsory misses, or additive compute+bus estimates.

**B2: cheap C candidate, changed register-allocation boundary.** #628's QKTILE2
was flat inside the huge `attn_heads`; #634 changed the loop to width two and
lost to reloads. Today's noinline `qk_dot8` (#757, fixed B1 port #805) is a NEW
small frame. Keep its outer i+=8 and guard/tail. Within each 8-column block,
visit pair offsets 0,2,4,6; at each offset issue the existing s0, s1, s0b, s1b
updates before moving to the next pair. Reuse the eight scalar operands where
GCC permits. Each accumulator keeps the exact same expression, +0 seed, pair
order and final caller scaling ONCE. This reduces operand live ranges/reloads
without adding accumulators or changing the reduction graph. Inspect the actual
helper for load count, spills and hardware-loop retention, then normal host gate
and one primary screen. This is not another standalone `dot_pair_c` microbench.

**B3: wide-load transport, independent from B2.** Keep four result accumulators
and the exact mul/madd/add graph of the compiled paired expressions. Sketch
f0-f15 liveness before coding: paired loads of qA/qB/k0/k1 can occupy eight
registers, four sums plus limited product temporaries fit; a fully doubled
operand bank does NOT. Stage loads across independent expressions only where
register lifetimes permit. Require actual loads before their consumers in the
linked body, not merely renamed registers (#806). Guard the documented load
alignment and preserve generic fallback and odd dimensions. Device differential
must compare all FOUR outputs to the current helper, across real-shaped signed
int8 K and varied Q; host tests cannot execute Xtensa asm. Use existing kbench
facilities, then a normal screen if the helper improves. Espressif's DSP dot is
a syntax/alignment reference ONLY: its four partial sums change this model's
rounding, so do not transplant its arithmetic.

## Keep down; reserve and turnover

No repeat LUT de-split (#446/#813), codebook-copy placement (#808/#809), kron2
wide (#674/#680), 4x offsets (#749), unsupported 120 MHz, assertion relaxation,
free-arrival phi staging (#407), or more capture/verification loops. m->lut is
already 24,576 B internal SRAM. QK=48 specialization is already tested B1/B3.
Preserve EG2/compact-prefix/amortised assets and use receiving-tree differentials.

A genuinely unchanged ELF is not a new candidate: explain that once and move to
the next independent lever without defeating the anti-repeat guard. A scalar-QK
null does not close the separate wide-load schedule. A failed arithmetic gate
stops promotion even when primary tps improves. Record new evidence with its own
log and provenance; do not call reused metrics a new performance result.

Useful references (transfer is a hypothesis, not a speed promise):
- [ESP-DSP S3 float dot: guarded wide loads](https://github.com/espressif/esp-dsp/blob/master/modules/dotprod/float/dsps_dotprod_f32_aes3.S)
- [ESP-IDF 5.5.2 memory types](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/memory-types.html)
- Existing `.auto/exp96/`, `.auto/exp91/`, and kbench are the local starting points.

Next mentor: check for three distinct live jobs and growing logs, then read the
actual one-Q layout result and the current-helper QK timings. Keep accepted,
screened and breadth-blocked numbers distinct. Update this compact queue in
place; do not append another session-close history.


---

## CANDIDATE CLEARED TO BUILD (was held pending a re-price): wide-load QK dot, bounded +1.5 % to +3.6 %

The hold in #858 was "do not build until the warm kbench number is re-priced with the field's call
structure". The bound replaces that measurement: the attention phase measures 23.5 ms and its other
attributed work (paired exp ~3.5, P.V ~3.7, staging ~1.5) leaves the QK remainder at **<= 14.8 ms**
against **8.3 ms** from the warm kbench. A 64-bit-load body cutting 30-40 % of the dot's instructions is
therefore worth **+1.5 % to +3.6 %** - both ends above the bar - and the warm-isolation caveat cuts in
the candidate's favour, since isolated numbers have understated this field three times.

Build order: pipelined 64-bit body with alternating registers (pipeline one row pair, keep the rest
scalar, as the wide phi does) -> objdump-verify the load lead -> host-gate before flashing -> screen
against the tree's own pin -> if it lands, re-derive the attention phase to learn which end of the bound
the field sits at.
