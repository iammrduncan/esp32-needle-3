# Needle 3 mentor queue

Mentor 2026-09-24 23:06 UTC. Preserve worker dirt, locks, anti-repeat history,
240/80 MHz, frozen complete goldens and every quality gate. Never interrupt a
live build/flash/benchmark. No long foreground sleeps; harvest completed logs.

## Session-3 lane status (23:08 UTC)

- B1 transplant LIVE (M-b5b4w-b1): tree = git archive 2c79104 engine+esp32/main
  (verified bundle5-only nd_model.c delta; engine `0bd9021c6136`), host 19/19,
  b1 worker harness/fixtures kept, discovery files preserved under
  /root/board-pool/preserved/b4w-transplant-b1/ (+ b1-worker-diff.patch).
- B2 DOT8W full gate LIVE (M-d8gate-b2, engine 5065619b0867, fresh signature).
- B3 QKTILE2 screen LIVE (M-t2-b3, engine 277cf888ef6b, B4W base, host 19/19;
  two-column tile, eight chains, per-sum two-product grouping and ascending
  order kept; remainder uses the same path - qk_hd 48 divides by 2).
- Correction to #627: DOT8W b3 heap was 4823, not the 5343 I carried.
- Next substitutes queued per mentor: selective A-only/B-only rescale sweeps
  on B4W (flags outside dimension loop), then odd-head-fallback outlining.

## Decision and next three lanes

**Accepted shipping: 5.3033 tok/s**, bundle5 `2c79104`, engine `0c1a6272cd01`,
device 20/20, host 19/19. B4W/DOT8W are discovery candidates. The inherited
18/20 frozen gate is a shipping blocker, not an owner-admission formality.
Do not reserve a confirmation board or declare performance discovery finished.

| Board | Next experiment | Reference and reason |
|---|---|---|
| 1 | **B4W-only attention on accepted bundle5**, host then full 20-case gate. | B1 bundle5 5.3033. Recover a potentially shippable gain without inheriting the unresolved stack. |
| 2 | **DOT8W cross-board full gate**, after finished FINPAIR is harvested. | B2 B4W 5.6967. One justified confirmation of B3's +0.526% winner, including its own extended/think/breadth. |
| 3 | **QK two-column operand tile on B4W**, after preserving/harvesting DOT8W. | B3 B4W 5.6983; also compare DOT8W 5.7283. Test the actual register spill found below. |

FINPAIR and DOT8W were already DONE at 23:03: neither is a live job to wait on.
FINPAIR = 5.6967, zero gain vs same-board B4W, heap 5087, restricted 6/6.
DOT8W = **5.7283** on B3, restricted 6/6, delta 0, heap 4823, rc=0,
engine `5065619b0867`; log `/root/board-pool/batches/M-dot8-b3.log`.
FINPAIR-on-B4W is a valid incremental experiment: never rebuild on seed solely
for a tidy measurement ladder. No noise control, third B4W breadth run, A3 board
matrix, or simultaneous duplicate discovery screens.

## 1. Accepted-base transplant: exact source, bounded interpretation

Mentor verified accepted/main/preserved `nd_model.c.seed` are byte-identical:
`0d639424f636`. Therefore preserved
`/root/board-pool/preserved/b1-recipeB/nd_model.c.b4w` is the portable B4W model
file. **Main HEAD itself is not bundle5** (engine `b1daae10df90`): quant, assembly
and scheduler have moved. Obtain the accepted engine/esp32 inputs from git object
`2c79104`; preserve all worker changes before assembling the new candidate.
Keep current hardened harness, frozen complete fixtures, and safe build settings.
Do not reset a dirty worker or copy the unaccepted source stack accidentally.
Canonical engine hash order: cat engine/src/*.c engine/src/*.S
engine/include/*.h esp32/main/*.c, not alphabetic ls-tree order.

This experiment asks whether the attention win survives on a quality-passing
ancestor. A clean 20/20 supports a new shipping candidate; an 18/20 or incomplete
run still fails. Do not infer that the inherited stack caused both failures from
speed alone. If the old console stall returns, record the concrete blocker once,
preserve the candidate, and keep other boards discovering. No repeated gates or
new coverage campaign. A later DOT8W transplant is justified only after B4W's
result is understood; do not silently add it to this attribution experiment.

## 2. QK register lifetime: shorten the tile before widening again

B4W shares K/V across heads, but its paired QK loop still spills. Mentor read
B1's exact 22:54 ELF: `attn_heads` size 0x186b; loop
0x4037c4dc..0x4037c575 spills/reloads f13 through a1+0x460 each four-column
iteration (`ssi` 0x4037c4fc, `lsi` 0x4037c510), then `bnez`. The earlier
no-spill observation concerned P.V, not QK.

Try two columns per tile: load qA0/qA1/qB0/qB1 and k00/k01/k10/k11 once,
update all four scalar sums, then advance. Four sums plus eight operands leave
room for temporaries within 16 FP registers. Keep each sum's two-product grouping
and ascending pair order. The existing target graph is MUL(second product),
MADD(first product), ADD(pair result to running sum). Preserve that graph and
operand order; no reassociation or contraction across the pair/sum boundary.
Handle remainders consistently with the original model path, with no new
assumption silently changing the fallback. Inspect actual K loads, spills,
hardware-loop generation and arithmetic, then screen. A shorter live range is
an experimental hypothesis, not a guarantee of faster code.

DOT8W's fresh B3 ELF still spills via a1+0x460 (`ssi` 0x4037c51e / `lsi`
0x4037c53e, another pair later in the same iteration), but uses a hardware loop
with an extended LEND. Its gain cannot be called "spill elimination". The
changed loop form and fewer pointer/loop updates are plausible contributors.
Do not close QK from P.V's B8W null, and do not blindly sweep to 16-wide.

## Ready substitutes after these lanes

**Selective rescale on B4W:** `pv_pair2` currently sweeps BOTH oh arrays whenever
either head rescales, multiplying the other head by 1. Preserve the both-rescale
paired sweep and the neither-rescale path; give A-only and B-only their own
single-array sweep, with flag dispatch outside the dimension loop. Leave the
successful four-wide P.V update completely unchanged. This removes avoidable
loads/stores on mixed masks without the failed A3 fusion or four copies of P.V.
Use real per-head flags (a rescale can underflow to zero); never infer the flag
from r. Verify finite outputs/zero and target arithmetic. This is a separate
candidate, with benefit depending on mixed-mask frequency; #292 measured
zero-exp frequency, not this frequency.

**Outline only the odd-head fallback:** B4W paid 3072 B internal heap; its hot
paired loop and original single-head fallback now coexist in IRAM. Production
rep=6 and split=6+6 make the *odd-head* fallback unused, whereas the odd-POSITION
tail still runs and must remain hot. Extract only the former into a noinline
helper in ordinary flash, preserving general odd head ranges and every original
expression. Keep the hot pair in IRAM and inspect the map to prove code actually
moved; check hot-loop codegen did not worsen. Measure speed and heap. No deleting
fallbacks, disabling assertions or allocating a new cache. GCC's
[noinline/cold attributes](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Common-Function-Attributes.html)
and Espressif's [IRAM/DRAM tradeoff](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/performance/speed.html#targeted-optimizations)
support the mechanism; the expected gain here is a local hypothesis, not a
published result. Prefer the simpler selective-rescale candidate if extraction
would stall a ready lane.

## Measured evidence and closures worth retaining

- Discovery seed `61861dd9886c`: B1/B2 5.6117, B3 5.6133. Seed full gate #589:
  18/20, token delta 52, extended 5.5177, think 4.39. It is not shipping.
- B4W `51ea5246142e`: screens B2 5.6967 / B3 5.6983 (+1.516% each vs seed);
  full gates B1/B2 5.6967, ext 5.6138/5.6146, think 4.44, heap 5343, SAME
  frozen two failures / delta 52. B4W's second breadth is complete; no more.
- B (two-wide P.V) +0.53%; B4W +1.52%. The mentor's initial narrow-register
  guess was not optimal. B8W #624 = -0.03% vs same-board B4W and -256 B heap;
  wider P.V is down. FINPAIR #626 exactly null; final normalization is down.
- A3 `9e658137ba25`: 5.6467 on B2/B3 (~+0.62%), ext 5.5538, think 4.41,
  heap 8415, same 18/20. Preserved `/root/board-pool/preserved/a3-9e658137ba25/`.
  Separate rescale MUL plus two explicit FMAs fixes first A's wrong contraction.
  Host 19/19 does not prove bit equality: adversarial 40-step streams diverge
  from step 3, max_abs 6.104e-05. Describe arithmetic evidence precisely.
- A3+B v1 = 5.620, per-cell branches; v2 = 5.615, hoisted four-way copies.
  Both slower than B4W; do not try a third fusion layout on unchanged premises.
- C full Q tap/norm/RoPE ownership #615 = +0.089% on B3. Measured and down;
  no more small-split threshold or callback joins now. Prepare+LUT fusion #584
  = zero; LUT quartet barrier #579 -0.090%, pointer hoist #582 -0.060%; fw_scale
  was cold. These do not establish a universal scheduling ceiling.
- Old tie2, deeper prefetch, serial LUT, ordinary QKV concatenation, norm-bias
  hoist, sinkpair, silu4, FP16 taps, tier sweeps and unsafe 120 MHz stay down.

## Integrity and next-pass handoff

The failures heldout_interval_one / heldout_long_tools_note_only are generated
TOOL changes, not merely reported state: interval=120 loses get_status; the long
case changes interval=300 to interval=45 plus timer/status calls. Cause remains
unresolved. Input = prefix + query; dispatch follows generation. drop=0 or timer
narration does not establish identical input/tokens. Never overwrite goldens.
Main still has 24 prompts/23 host/20 device; complete pre-addition backup is
`/tmp/coverage-backup-279218` (20/19/20). Preserve widened exp88 artifacts and
reconcile only if needed; do not sync incomplete main fixtures onto workers.

A primary screen consumes an image signature. Its ONE documented allowance can
fund a genuinely new full gate; record why. Respect exhaustion/rc=44 and coverage
rc=42. Do not modify histories, expectations or measured logs to get a green run.
Host -ffp-contract=off does not model target contraction or asm. Earlier empty
FP-barrier errors were host x87, not an Xtensa limitation; A3 needs no asm.

Next mentor: inspect the B4W accepted-base result and exact source manifest;
DOT8W's cross-board breadth; QK tile's actual loop and timing; whether all three
lanes turn over into distinct work. No control/acceptance-narration loop.
