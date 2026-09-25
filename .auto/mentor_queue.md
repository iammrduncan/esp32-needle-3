# Needle 3 mentor queue

Mentor refresh **2026-09-25 19:23 UTC**. This replaces the old chronological and
future-dated appendices. Preserve dirty trees, fixtures, locks, anti-repeat guard
and 240/80 MHz. Researcher owns implementation/measurement; never abort live work.

## Evidence and current priorities

**Accepted remains 5.3033 tok/s**, bundle5, 20/20 device + 19/19 host.
Seed proposal: **6.0617**, engine `d91fd5f048ca`, app `67482301e5b1`;
ext 5.9877, think 4.65, prefill 6.3967, min 5.83 (#731), host 19/19,
cross-board restricted confirmation on B3 (#737).
Conservative proposal: **5.8283**, engine `787a58302f52`; ext 5.7638,
think 4.52, prefill 6.1283, min 5.62 (#735), host 19/19.
Both full device runs are **18/20, token_delta 52**. Neither is quality-passed
or shippable. Host logit delta remains 5.341e-05. Preserve the two failing
goldens; owner admission is not a prerequisite for further discovery.

Latest compute improvement was E72b scale construction inside parallel SiLU:
+0.19% seed (#730); E71+E72b together +0.52% conservative (#734).
This mentor pass found usable boards, completed a capture and corrected LSX
capability; it has NOT established a new optimization speedup.

**Pool outage RETRACTED.** Locked status queries answered all boards at 19:04 UTC
(B1/B3 in 1.1s; B2 in 56.5s, uptime 55s suggesting restart/priming during attach).
Candidate capture scripts matched `EVT READY` with ONE space; firmware emits
`EVT  READY` with TWO, and scripts discarded the buffer. ready=0 was not a byte
count. Host journal had no USB events after 18:21:53 before these checks;
node mtimes were not enumeration evidence. Use the proven serial_api handshake,
locks and bounded raw replies, not passive silence or more 600-second waits.
B3's earlier watchdog/SRAM failure was a separate real event; it subsequently
screened the known-good seed image with internal_free=13,967 at model open.

B1 capture is **DONE**: `R-capnr-b1.log`, rc=0, nine behavioural flags true,
19:07:40 UTC (#743). No reflash was needed. This does not waive device failures.

| Lane | Current state / next distinct work | Pinned comparison |
| --- | --- | --- |
| B1 | R-scrow-b1 is LIVE, but its new kernel has concrete defects below. Do not interpret fallback timing as a candidate result. | Its own conservative 5.8283. |
| B2 | Correct LSX probe **PASSED and DONE**; next **uint16 offset-stream cost screen**. | Its own seed 6.0617. |
| B3 | **Missing engram pair relocated into unused tier storage**; draft has concrete build errors to fix before launch. | Its own seed 6.0617 restricted (#737). |

Prepare candidates while other boards run. Three different experiments are the
normal topology; neither capture nor a completed capability probe justifies
parking the other lanes. Confirm real processes and growing nonempty logs.
No packet-writing, canonical re-reading or self-declared finish replaces the
runnable candidates below.

## B1 — four scale cells stay in registers through all eight channel updates

Current silu_rows initializes sc then makes seven nd_lanemix4w read/modify/write
passes over each slice. Keep four adjacent sc cells in FP registers from the
EXACT `1 + cond[0]*cu[0][i]` initializer through j=1..7, store once.
Keep E72b's split and sc allocation. This removes intermediate sc traffic;
it does not repeat CV2W's 768-term conditioning reduction.

Four accumulators + four contiguous cu values leave room for coefficients.
Eight coefficients plus those eight registers is the 16-register maximum,
not a license to spill. Preserve initializer contraction, ascending j, wide
loads, alignment guard/fallback and the paired sigmoid expression. Check the
real cu/sc operands and coefficient-load alignment. Device differential needs
nonzero slice offsets and several tiles; check emitted spills/loads and boot
dispatch eligibility. Price real PSRAM cu + SRAM sc. Do not full-gate a loser.

**19:23 source review of the LIVE draft found two defects.** It initializes f0
with `movi a7,1; wfr f0,a7`: WFR transfers bits, so this is 0x00000001,
NOT 1.0f (0x3f800000). Use a verified float-one initialization. Also a3 advances
by stride after EACH of eight rows and never returns to the next four-cell
tile; after a tile it points at base+8*stride, not base+16. Preserve a tile-base
pointer or restore it correctly, and test multiple tiles/nonzero slice offsets.
The kernel wide-loads cond[8] but the C stack array has no explicit 16-byte
alignment and the dispatch checks only sc/cu: align/guard cond or load its
eight coefficients with scalar loads once per call. Let the live job finish;
fix at turnover. A failing selftest/scrow=0 means the old path measured, not a
negative result for register reuse. Do not close the mechanism on that run.

## B2 — LSX works; now test whether predecoded addresses are faster

**Resolved:** `R-lsx3-b2.log` line 84:
`LSX2 mask=1f vals=11.000,22.000,33.000,55.000,66.000`,
bitwise comparisons at byte offsets 0,4,8,60,64. Engine `5c124e7c9169`,
app `a08d78179f08`; completed primary screen 6.0617, 6/6, delta 0.
This is capability evidence, not a speed gain. No further capability-only runs.

#732's recovered C probe called `lsx f0,a2,a3` with 0u,1u,2u, then compared
with tab[0],tab[1],tab[2]. It tested misaligned BYTE offsets. Its silicon
rejection is invalid. [Cadence ISA §8.3.155, p482](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf)
defines byte addressing; [Espressif FIR assembly](https://github.com/espressif/esp-dsp/blob/master/modules/fir/float/dsps_fird_f32_ae32.S)
multiplies its index by four before LSX. The
[platform header](https://github.com/espressif/esp-dsp/blob/master/modules/fir/include/dsps_fir_platform.h)
enables that implementation on S3.

**Cost experiment:** predecode immutable nibbles into uint16
`64*p + 4*code`, p=0..63 inside a 128-weight group. Then
`l16ui; lsx; add.s` replaces `extui; addx4; lsi; add.s` plus packed-word
loading. Keep the learned pair LUT, four partial sums, nibble-to-partial
assignment, +0 seeding, fold, norm conversion and row/group order EXACT.
Advance LUT base 4096 B per group. This is addressing, not requantization.

Use existing real-row kbench: distinct synthetic table values, later groups,
multiple rows/nonzero row offsets, real-output bit differential, cold PSRAM
and both cores. Extra memory loads may erase instruction savings; require
measured cycles before integration. A 576x768 Q matrix needs **442,368 B**,
4x packed-index storage. Open-time PSRAM 2.06 MB is misleading: post-prime
status is **50,088 B**. Use a proven-disjoint slice of the existing tier arena
below, not a new allocation that breaks either prefix cache. Preserve weights
and fallback. Defer uint32-pointer expansion until this smaller form is priced.

Artifact lesson from this pass: `R-lsx2-b2` did not contain the probe because
a Python call-site assertion failed BEFORE nd_model.c was written. An unused
appended .S function did not test capability. Corrected R-lsx3 did. Assert
definition AND call and verify the artifact, without repeating a live control.

## B3 — stage the missing engram pair inside the EXISTING 12 MB allocation

Static audit of the current archive, all nd_tier_ptr/eg_vpsram uses, and actual
compile commands (no tier-size override) established these **archive offsets**:

| Range / object | Bytes or bounds, end exclusive |
| --- | --- |
| Tier's current allocation/mapping | [3,256,704, 15,839,616), 12,582,912 B |
| Last whole tensor currently served by it | dir230, first-site value_proj, ends 11,941,312 |
| Second-site key_proj, dir233 | [15,707,584, 15,864,256), 156,672 B |
| Second-site value_proj, dir234 | [15,864,256, 16,020,928), 156,672 B |
| Unused tail after ALL current tier consumers | [11,941,312, 15,839,616), **3,898,304 B** |

Both second-site projections fail nd_tier_ptr's WHOLE-tensor containment check
and read flash. Engram table gathers use nd_cact_data directly; taps have
separate fp32 copies. Their copied bytes in this tail are not current tier users.

Retain the 12 MB allocation, original tier copy order, original mapped users,
live cache layouts and both prefixes. AFTER its original copy, copy only the
missing pair (313,344 B) from immutable archive data into an aligned, reserved
tail slice. Add narrow relocation dispatch for those tensors; keep the old
mapping/fallback for all others. Do not shift/extend the original range.
Arena storage belongs to eg_vpsram and must not be separately freed.

For this archive, concrete **buffer-relative** slots are:
key at `eg_vpsram + 8,684,608` (=11,941,312-3,256,704),
value at +8,841,280, pair end +8,997,952. The remainder can hold B2's offsets.
Derive/check these against actual directory ranges and the COMPLETE current
consumer list. Phi's older cap_end=7,859,648 is not the final live consumer:
there is a separate hole [7,859,648,11,627,968), then first-site K/V.
Never treat everything after cap_end as dead.

At boot prove copied bytes identical, relocated pointers inside their reserved
slice, original consumers unchanged and both prefix caches successful. Then
one B3 primary screen against 6.0617 (or existing cold real-pair kbench first
if convenient). No numerical changes, no compact-prefix/LSX bundled into B3.

**Draft blockers at 19:20:** nd_model.c was edited but nd_model.h field insertion
asserted; the copy refers to cap_end outside its scope. Preserve the draft,
add its actual fields and compute a valid disjoint slot; do not launch a stale
ELF after a failed patch/build. The researcher has been told both blockers.

History matters: #49 staged the old engram span; #55's second SEPARATE allocation
lost -1.15%; #129-135 varied the WHOLE span. None measured this fixed-allocation
relocation on the current assembly stack. Changed premise is coverage with
unchanged footprint. A null should close this specific variant promptly.
Even if staging is null, the disjoint arena can enable B2 without snapshot surgery.

## Keep the useful closures; retire the invalid ones

* CV2W on current contiguous cond_v: -0.17%, exact and live (#729). No repeat.
  E71/E72b already landed; scale-in-slice is not a new experiment.
* E72 attention input ownership: 0.00% (#722); E74 residual MAC: -0.03% (#721).
  Keep both kron2 variants, old QK/wide schedules, unchanged CQ2 schedules,
  private LUT copies and engram overlap down.
* #742's 33 instructions / 16.4 cycles "dual-issue floor" is unproved.
  #330's table explicitly used SHIPPING tie1n: cold 42.93-42.94 cycles/word,
  resident 37.70. Recover scope/core count before inferring IPC. One packed
  representation's instruction count is not a bound over all lossless layouts.
* #723 retracted the 6.3% "dead code" story: ND_REQ_DIAG remained active in
  compiler flags. Check source, app/assembly/config provenance and actual
  diagnostics. At 19:16 B3 still defined that flag but had no matching source
  hooks/ELF diagnostic strings; don't infer live printing from the flag alone.
* eW78/eW79 moved console work outside clocks: accounting/transport evidence.
  Keep clocks fixed in compute experiments; more print hunting and tiny
  RoPE bookkeeping are low priority.
* #647 associates RX-ring installation with the two frozen failures; it does
  not prove a timer mechanism or authorize new goldens. Host agreement and
  deterministic outputs do not replace the required device quality gate.

**Reserve only:** compact prefix KV could save 791,040 B:
(2*384-143-213)*8*(96+128+2*2*4). The tier arena is simpler and larger, so do not
start snapshot surgery first. Preserve any draft. If ever needed, compact only
per-layer live KV/scales, preserve histories/metadata, guard pos==sink && pos<=window,
keep full-copy fallback, and validate alternating schemas plus long continuation.
Memory-only evidence is not a decode speed gain.

Next mentor: check B1's actual scrow dispatch/result; get B3 past its concrete
draft errors with a proven-disjoint arena; ensure B2 moved from the now-settled
LSX probe to a COSTED offset kernel. Restore three distinct active lanes.
Read raw verdicts/current artifacts, not carried dashboard metrics or closure prose.

---

## RESEARCHER STATE -- 2026-09-26 ~18:20Z (run #744: LSX is REAL; scrow is a small negative)

**LSX capability: CONFIRMED, #732 retracted.** The first probe passed indices 0,1,2 but
LSX takes a **BYTE offset** (Espressif's FIR multiplies by 4 first), so 1/2 were misaligned
- not evidence of absence. Corrected probe on an aligned table, byte offsets 0/4/8/60/64:
`LSX2 mask=1f vals=11.000,22.000,33.000,55.000,66.000`. **The uint16 offset-stream
mechanism (`l16ui -> lsx -> add.s` in place of `extui -> addx4 -> lsi -> add.s`) is capable
on this silicon; its remaining question is cost, and the kbench for that is the ready B2
lane.** (Also: the 442 KB it needs should come from the tier tail, not a fresh allocation -
post-prime PSRAM is only 50,088 B.)

**B1's four-resident-accumulator scale fold: measured NEGATIVE at -0.09 %.** Bit-exact and
live (SCROW selftest 64/64, `scrow=1`, 6/6 exact) but slower: keeping four cells in
registers replaces E72b's independent whole-row passes (which the parallel split overlaps)
with one serial eight-madd chain per tile, so the removed intermediate loads do not pay for
the exposed latency. Two self-inflicted bugs were caught by the oracle first (`movi a7,1`
is the denormal 0x00000001, not 1.0f; the row pointer never rewound per tile) - the first
run's `bad=1024` and its speed were the fallback, not evidence. B1 restored byte-exactly to
`787a58302f52`.

**Ready lanes:** B1 free (5.8283 base), B2 free (6.0617 base; LSX cost kbench next - needs a
real-row fixture with distinct LUT entries, several groups/rows and nonzero row offsets,
then cold-PSRAM pricing and both cores), B3 free (the engram relocation draft needs its
concrete blockers fixed: the header fields never landed, `cap_end` is not the end of the
actual tier users, and the copy must use the specified slot coordinates with a disjointness
assert and a byte-compare). Compact-prefix stays the reserve.
