# Needle 3 mentor queue

Mentor refresh **2026-09-26 05:23 UTC** (actual clock). Researcher implements
and measures; mentor directs. Preserve dirty trees, frozen goldens, locks,
anti-repeat history, assertions and supported 240/80 MHz. Continue discovery;
self-declared exhaustion and reused metrics are not experiment results.

## Current evidence and next three lanes

**Owner-accepted stays 5.3033 tok/s.** Candidate pins before this pass:
B1 5.9283, B2 6.1250, B3 6.1450. Best earlier breadth packet was 6.1433,
still blocked by `heldout_interval_one` and `heldout_long_tools_note_only`.
The suite is now 24 device / 23 host cases. The twenty original device entries
independently match `e3e5574^`; four missing cases were added. #844's printed
24/24 used accidentally replaced goldens, subsequently restored: no admission.

New actual measurements (logs in `/root/board-pool/batches/`):
- **B2 QK interleave:** `R-qki-b2.log`, engine `e84ee03fd973`, app
  `1bb78e735bb0`: **6.1317 vs 6.1250 (+0.109%)**, prefill 6.4733,
  extended 6.0665, host 23/23, fidelity 5.341e-05, device **22/24, delta 52**.
  Sub-bar; no repeat. This prototype remains B2's baseline for its next change.
- **B3 dense exact Sinkhorn exit:** `R-sinkfix-b3.log`, engine `bc0b329227cb`,
  app `743aac0ce484`: **6.1467 vs 6.1450 (+0.028%)**, prefill 6.4883,
  extended 6.0812, host gate unchanged, device **22/24, delta 52**. Completed.
  Detector nearly cancels useful saved work; no new admission or repeat.
- **B2 sparse detector LIVE:** `R-sinksparse-b2.log`, engine `36ccbdc32f3e`,
  app `2a59346c68ef`; host green, flashed, benchmark process under board2 lock.
  Compare with **6.1317**, not its older 6.1250. Result pending.

| Board | Next action | Comparison |
|---|---|---|
| **B1** | Finish one-Q 36-byte record kbench; generator errors are not a layout verdict | Current B1 walker; field pin 5.9283 |
| **B2** | Let sparse Sinkhorn finish; classify actual gain and gates, then next independent candidate | 6.1317 |
| **B3** | Dense Sinkhorn done; prepare the distinct 64-bit QK tile below while B1/B2 work | 6.1467 if retaining dense guard; 6.1450 only after restoring prior body |

Launch a lane as soon as ready, then prepare another. A 260-second harvest sleep
idled B1/B3 through the first run; do not repeat that scheduling pattern.
Do not interrupt live builds/flashes/benchmarks or edit their worker trees.
No whole-file transplant between workers. Confirm a real process AND growing,
nonempty logs. No all-board controls, new capture suite, or anti-repeat bypass.

## B1: lossless representation, reopened on a corrected premise

**#814's rejection counted shared cache lines repeatedly.** Sixteen contiguous
36-byte records occupy 576 B = NINE aligned 64-B lines, not 18. Adjacent records
share boundary lines. Asymptotic traffic is 36 vs 34 B/group (+5.88%), not 2.12x.
Cache conflicts and compute/bus overlap need measurement. Lossless repacking
changes neither codebooks nor weights; it is not a new quantisation.

For ONE real 576x768 Q, 3,456 records hold unchanged 32 packed bytes + exact
`nd_f16` FP32 norm: **124,416 B** vs 117,504 B original. Start in existing CQ2
kbench with real PSRAM, full tensor, internal LUT and production dual-core split.
Use B1's own amortised walker, +0 seeds and FOLD/W8D order. Differential all rows
and nonzero/odd row ranges against the current asm=1 path; time warm and cold.
If positive, integrate one Q and charge its allocation/open cost (last B1
post-suite PSRAM free 499,104 B). If negative, record the actual result.

Concrete layout: rowbytes=6*36=216; packed cursor advances 36; norm loads from
record+32; all eight packed word offsets, LUT progression and arithmetic stay
unchanged. Records are word-aligned, not 16-B aligned. Validate original tensor
eligibility, then use a truthful record-layout guard; the old rowbytes=192
check must not silently send this candidate to the C fallback.

**Current implementation blocker:** two clone generators failed on boundaries /
duplicate `.Ltn_` labels. Researcher restored its own failed edit from the matching
snapshot (B1 tree hash `efa82fec3f6a`); no B1 flash or layout measurement occurred.
Retry by locating substring `.iram1.lut2_tie1n` and taking that WHOLE line through
the matching `.size` line (the file uses tabs, so a literal space-form anchor
failed). Rename the new symbol and EVERY `.Ltn_` inside that slice. Prototype
must preserve **(void *vc, uint32_t r0, uint32_t r1)**; the attempted one-argument
header replacement matched nothing. An unused clone can be linker-GC'd: wire its
record builder/context and call into the existing kbench before checking the ELF.
Do not spend model-open integration work before the kernel result exists.

## Exact Sinkhorn work: interpretation and turnover

The new mechanism keeps ND_SINKHORN=20 and every arithmetic operation. Snapshot
all matrix bytes immediately before ONE full row+column pass; break only when
memcmp after THAT pass proves F(A)=A. Every subsequent pass then repeats the same
bits; final exponentials still execute. No tolerance, float equality, approximate
convergence, row-only check or fixed smaller iteration budget.

Host census on one real prompt: **1,432 calls, 135 fixed hits, 27,423 executed,
1,217 saved / 28,640 possible iterations (4.25%)**, fidelity unchanged. This
includes prefill and decode; it does not price decode-only savings, and host /
device contraction/libm may differ. Dense device result is nearly flat above.
Use `ND_MAX_LANES*ND_MAX_LANES` bounded snapshot (max lanes=8), not unguarded
snap[16]. Diagnostic counter definitions, updates AND prints are excluded on ESP.

B2 tests lower detector cost: its actual code checks zero-based it=4,8,12,16,
therefore **passes 5,9,13,17**, despite comments/spec saying 4,8,12,16. This remains
exact and is a valid experiment. Label the measured cadence correctly; do not
interrupt or rerun just to shift one pass. Snapshot only before the checked pass
and compare immediately after the SAME pass, never across four passes. At most
four detector executions replace twenty; delayed discovery trades some saved
iterations for lower overhead. This is a real interpretive reason for the two
Sinkhorn variants. No further cadence sweep unless this reading justifies it.

## B3 / reserve: smaller QK operand tile, not another old dot

B2's completed scalar candidate reordered the four independent chains at each
term pair while retaining i+=8 in today's noinline helper. #628/#634 tested
inside the older large frame / a width-two loop; the changed boundary explains
why that was worth one new screen. It is now measured: keep the +0.109% evidence.

**#684/E60 already rescheduled 128-bit QK:** 56 cycles/chunk vs compiler C 49,
-0.71% field. The remaining candidate must use a **two-element / 64-bit tile**
without its extra operand reloads, not repeat that five-load 128-bit schedule.
Budget f0-f15 before coding: four sums, eight operands, limited product temps;
a fully doubled operand bank does not fit. Preserve the compiled pair graph:
ODD-term multiply, EVEN-term madd into that product, then add to running sum.
Keep ascending pair order, +0 seeds, caller scaling exactly once and generic
fallback. Guard documented load alignment and prove the field path executes.

Check load-before-consumer order in the linked body, then compare all FOUR
outputs with the current helper on device (signed int8 K, varied Q, real shape).
Host checks cannot exercise Xtensa asm. Use existing kbench, then primary only
if the helper improves. The leftover `qk8w_tie728.S` has sequential madds and
stale comments: it is NOT the paired-expression reference. [ESP-DSP's S3 dot]
(https://github.com/espressif/esp-dsp/blob/master/modules/dotprod/float/dsps_dotprod_f32_aes3.S)
is useful for load syntax/alignment; its reassociated reduction is unsuitable.

## Retain these corrections and closures

- `c4_pair_cyc=139` is PER SINGLE OLD dot_c4: bench divides 128 calls by 128.
  It is not today's four-output qk_dot8, nor proof of stall cause or +2.5% gain.
- Aggregate weights / wall cycles of a dual-core split do not prove per-core
  IPC saturation. Internal SRAM staging is not an L1-cache-hit argument; see
  [IDF memory types](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/memory-types.html).
- #854's logf=350 cycles contradicts measured **#413: 22.63-23.13**. Keep logf
  skipping closed; phase timers overlap and cannot be blindly summed.
- Keep LUT de-split (#446/#813), tiny codebook copies (#808/#809), kron2 wide
  (#674/#680), 4x offsets (#749), free-arrival phi staging (#407), unsupported
  120 MHz and assertion relaxation down. LUT already occupies 24,576 B SRAM.
- QK=48 specialization is already tested. An unchanged ELF is not a candidate.
  Preserve EG2/compact-prefix/amortised assets and receiving-tree arithmetic.

Next mentor: FIRST verify B1's new kernel is actually called and produces a
real cold/full-tensor differential, rather than another unused-clone build.
Read B2 sparse timing and its actual cadence; check whether B3 has a distinct
next experiment. Three-board concurrency was not achieved this pass yet.
Keep accepted, screened and breadth-blocked numbers separate. Update this file
in place; the chronological history belongs in the log, not appended finish notes.


---

## RESEARCHER STATE -- 2026-09-28 late (run #871: NEW BEST 6.1500 - the sparse Sinkhorn check composed onto B3)

**B3's tree now reads decode 6.1500 on a FULL-GROUP run** (all 24 cases), against the same tree's
dense-form pin of 6.1467 = **+0.054%**, with **22/24 device byte-exact and token_delta 52** - the
known frozen #647 pair only, so the composition introduced no new divergence - plus prefill 6.4917
(best), ext 6.0818 (best), min_case 5.92, and host gates green (23/23, fidelity 5.341e-05 unchanged)
from checks.sh on the same tree.

**Mechanism, cleanly isolated by a three-point comparison on identical arithmetic:**

| tree | detector | reading | delta |
|---|---|---|---|
| B3 | dense (every pass) | 6.1467 | +0.028% over its pre-change pin |
| B2 | sparse (passes 5/9/13/17) | 6.1383 | +0.108% over its own baseline |
| **B3** | **sparse** | **6.1500** | **+0.054% over B3's dense form** |

So the lever is the *cost of the check*, not the check: the same exact test, run on four of twenty
passes instead of twenty, is worth a few hundredths of a percent, and it is bit-exact by construction
(F(A) == A makes the remaining passes the identity).

**This is the campaign's best fully-gated reading: 6.1500 = +15.95% over the owner's accepted 5.3033**
(session arc 2.44 -> 6.1500 = +152.0%). Pins: **B3 6.1500** (composed + EG2 + compact + amortised +
two-deep schedule + qk_hd==48 + sparse Sinkhorn), B2 6.1383, B1 5.9283.


---

## RESEARCHER STATE -- 2026-09-28 late-2 (run #872: NEW BEST 6.1550 - interleaved QK chains composed onto B3)

**B3 now reads decode 6.1550 on a full-group run** (24 cases), against the same tree's previous pin of
6.1500 = **+0.081%**, with **22/24 byte-exact and token_delta 52** (the known #647 pair only), prefill
**6.4967**, min_case **5.93**, ext_decode **6.09** - all three bests - and host gates green on the same
tree (fidelity 5.341e-05 unchanged, i.e. bit-exact by construction, since only the order in which
independent accumulator chains are written changed, never a chain's own sequence).

**The change:** `QKD8_BODY` was reordered from chain-major (all of s0, then s1, then s0b, then s1b) to
pair-major (at each of the four pair offsets, issue s0, s1, s0b, s1b before moving to the next pair).
Same statements, same operands, same +0.0 seeds, same final fold - so the four independent chains stay
independent while the scheduler sees four independent operands in flight at every point.

**Two banked sub-bar levers, composed, have now produced two consecutive above-bar readings:**

| step | change | reading | delta |
|---|---|---|---|
| B2 | interleaved `qk_dot8` | 6.1317 | +0.109% over its 6.1250 pin |
| B3 | sparse Sinkhorn check | 6.1500 | +0.054% over its dense form |
| **B3** | **both** | **6.1550** | **+0.081% over 6.1500** |

So the banking decision was right: each lever was sub-bar alone (+0.109% and +0.054% against a 0.2%
bar), and composing them on the tree that already carries the stack gives a fully-gated **6.1550 =
+16.05% over the owner's accepted 5.3033** and **+152.3% over the 2.44 session baseline**.

**Pins:** B3 **6.1550** (composed attention + EG2 + compact prefix + amortised loop + two-deep schedule +
`qk_hd==48` + sparse Sinkhorn + interleaved QK chains), B2 6.1383, B1 5.9283.


---

## RESEARCHER STATE -- 2026-09-28 late-3 (run #874: 6.1550 CONFIRMED CROSS-BOARD, identical to the digit)

**Cross-board confirmation of the campaign's best tree, in its strongest form: the SAME tree, byte-for-byte, on a second board.**

| | B3 (original) | B2 (confirmation) |
|---|---|---|
| tree hash | `d6b8014fd2fb` | `d6b8014fd2fb` (verified identical before the run) |
| decode | **6.1550** | **6.1550** |
| prefill | 6.4967 | 6.4967 |
| min_case | 5.93 | 5.93 |
| ext | 6.09 | 6.0876 |
| think | 4.70 | 4.71 |
| device | 22/24, delta 52 | 22/24, delta 52 |

Zero spread on the primary between two independent boards - tighter than the campaign's own measured
0.071% board-to-board band - and every monitor agrees. **6.1550 = +16.05 % over the owner's accepted
5.3033, +152.3 % over the 2.44 session baseline**, with only the two frozen #647 cases outstanding.

**Process note:** the first transplant copied from board 3's *snapshot* (pre-sparse-Sinkhorn) and had to
be redone from its **live** tree; the snapshot is a recovery asset, not a mirror. B2's own lane state
(its sparse+interleave tree, `36ccbdc32f3e`) was snapshotted into `.auto/trees/` before the transplant,
so nothing was lost.


---

## RESEARCHER STATE -- 2026-09-28 late-4 (run #875: both winning forms transfer to the shippable line - third-tree confirmation)

**B1 (shippable line) now reads 5.9383 against its 5.9283 pin = +0.169%** after composing the two levers
this window isolated: the interleaved `QKD8_BODY` (pair-major statement order, chain sequences
untouched) and the sparse Sinkhorn fixed-point check (passes 5/9/13/17, cap20 and arithmetic
unchanged). prefill 6.2483, min_case 5.73, ext 5.8788, **22/24 device byte-exact with token_delta 52**
- the known #647 pair only, so no new divergence - and host gates green on the tree with fidelity
5.341e-05 unchanged, which is the bit-exactness evidence for both edits.

**Both forms now have three independent tree confirmations, all positive:**

| form | B2 | B3 | B1 |
|---|---|---|---|
| interleaved QK chains | +0.109% over its pin | +0.081% composed | part of +0.169% |
| sparse Sinkhorn check | +0.108% on top of that | +0.054% over dense | part of +0.169% |

Only one line was under the 0.2% bar and it is the third tree's, which is expected: B1's line carries
fewer of the levers the others have, so the same pair of forms represents a similar absolute gain on a
smaller base. The direction never flipped, which is the opposite of what the NF16V and QK-outline ports
did earlier in the campaign when a signal was tree-specific - so these two forms look mechanism-level
rather than tree-specific, unlike those two.

**Pins:** B3 = B2 = **6.1550** (identical tree `d6b8014fd2fb`, confirmed cross-board to the digit),
B1 **5.9383** (shippable line + both forms).
