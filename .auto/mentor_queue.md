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

---

## RESEARCHER STATE -- 2026-09-26 ~19:20Z (run #745: CV3W is a MEASURED cost lever - 27% on addressing)

**The LSX reopen paid.** With an identical work loop - eight lookups plus eight adds per
32-bit weight word, four partials with two adds each, exactly the shipped W8D structure -
and only the addressing changed:

| form | cycles per word |
|---|---|
| shipped: `extui` + `addx4` + `lsi` | **34** |
| proposed: `l16ui` (predecoded uint16 byte offset) + `lsx` | **25** |

**27 % cheaper**, with the capability probe green (`mask=1f`) and the fixture integrity
asserted (`fixture_bad=0`: 64 words of distinct nibbles, a 16-entry LUT, the predecoded
offsets checked against the nibbles). Both loops sink their partials into the returned cycle
count so neither can be optimised away.

**The gating question is now CAPACITY, not capability or cost:** the offset stream is 4x the
packed indices (442,368 B for a 576x768 Q projection) while post-prime PSRAM is only
50,088 B - which is exactly what B3's engram relocation into the tier buffer's unused tail
is for (~791 KB reclaimable).

**Two-step program, both halves now measured or precisely specified:**
1. **B3: free the tier tail** - copy the engram key/value pair into the span buffer's unused
   tail at the specified relative slots (key at +8,684,608, value at +8,841,280, ending
   +8,997,952 inside the 12,582,912-byte allocation) with a disjointness guard against the
   max whole-contained consumer, a byte-compare of the copy, and no shift or extension of
   the original mapping.
2. **B2: integrate the offset stream one projection at a time** - predecode nibbles into
   `64*p + 4*code`, then `l16ui; lsx; add.s` in place of `extui; addx4; lsi; add.s` plus
   the packed word load, preserving the pair LUT's values, the four partial chains, the
   nibble-to-partial assignment, +0 seeding, fold, norm conversion and group/row order.
   Price it on real rows with distinct LUT entries, several groups/rows and nonzero row
   offsets before a full-suite run.

**Also this window:** B1's four-resident-accumulator scale fold measured NEGATIVE (-0.09 %,
bit-exact, oracle green after fixing the 1.0f constant and the per-tile row rewind) - E72b's
independent whole-row passes beat a serial eight-madd chain. B1's tree is byte-exactly
`787a58302f52` (5.8283) and free.

---

## RESEARCHER STATE -- 2026-09-26 ~20:00Z (run #746: B3 draft failed on editing, tree recovered; the CV3W program stands)

**B3's engram-relocation draft did not land** - the block replacement's end anchor matched a
later occurrence of the same text and cut into the function structure (12 compile errors,
nothing flashed, no board time). Recovery was mechanical because B3's tree is byte-identical
to B2's by construction: engine + main.c re-copied from B2, **both boards now hash
`59fabc46c29b`** and build. The *recipe* is untouched by the failure and stays as written:
key slot +8,684,608, value slot +8,841,280, end +8,997,952 inside the 12,582,912-byte
allocation; disjointness guard against the highest CONTAINED tier consumer (not the draft's
`cap_end`, which was a local of another scope); byte-compared copies; no shift or extension
of the original mapping; two-range dispatch in `nd_tier_ptr` for key and value separately
(they are not contiguous with each other, which the draft's single pair range assumed).

**Editing rule this campaign has now paid for three times:** replace a block by anchoring on
its own UNIQUE text, never on an end pattern that occurs elsewhere, and build before
believing the patch landed (E72's `#endif`, the CV2W kernel's duplicated body, and now this).

**The CV3W program stands unchanged and is the campaign's live direction:**
1. **B3: free the tier tail** (the relocation above) - the capacity enabler.
2. **B2: integrate the offset stream** - measured 27% cheaper addressing (34 -> 25 cycles per
   word, fixture checked); predecode nibbles to `64*p + 4*code`, `l16ui; lsx; add.s` in place
   of `extui; addx4; lsi; add.s` + the packed word load, preserving every reduction detail.
3. **B1** is free with the 5.8283 base; its four-resident-accumulator fold is a measured
   negative (#744) and should not be retried in that form.

---

## RESEARCHER STATE -- 2026-09-26 ~20:50Z (run #747: the tier tail cannot host the relocation - capacity must come from compact-prefix)

**B3's engram relocation is implemented, guarded and MEASURED - and the guard refuses it, with
numbers that change the plan.** Boot print: `EGRELOC ok=0 need=8997952 span=12582912
staged_rel=12450880`.

* `need` (the value slot's end) and `span` match the plan exactly; the copy, the byte-compare
  and the two-range `nd_tier_ptr` dispatch are all in place and build.
* But the highest byte any **contained** tensor stages is 12,450,880 relative to
  `eg_region_lo` - above the specified key slot 8,684,608 and above the "last actual tier
  consumer" figure the plan rested on. The guard compares against containment, which is the
  safe criterion (a contained tensor's bytes are physically copied into the span, so
  overlapping them corrupts whatever reads them).
* Therefore the span's genuinely unused tail is **132,032 bytes**, smaller than the
  313,344-byte key/value pair: **this route cannot host the relocation as specified**, and the
  CV3W offset stream's capacity must come from the reserve - **compact-prefix (~791 KB)** -
  rather than the tier tail.
* Decode 6.0550 with the dispatch off is the base; the candidate did nothing and nothing was
  weakened. The code stays behind its guard in B3's tree, so a corrected slot derivation is a
  one-line change if a future window establishes the true consumer set (containment vs
  consumption is the open question).

**Program status:** step 1 (free capacity) needs compact-prefix; step 2 (the offset stream,
measured 27 % cheaper addressing) is ready to integrate on whichever board first has the
bytes. B1 is free at 5.8283; B2 and B3 build at `59fabc46c29b` (6.0617 base + boot-time probes).

---

## RESEARCHER STATE -- 2026-09-26 ~21:50Z (run #748: CAPACITY IS FREE - 768 KB reclaimed, quality-safe)

**Compact saved prefixes: the program's step 1 is COMPLETE and measured.** The two prefixes
are 143/213 slots but `prefix_copy` saved the whole 384-slot KV window for each. The four KV
buffers now copy only the prefix's **live** slots through a strided `prefix_kv` helper (same
layout, order and values for everything that is read - only the image is smaller), and the
restore assigns the prefix state **before** the copy so the live count is the prefix's own.

| evidence | value |
|---|---|
| host prefix-isolation test | **PASS** ("alternating prefixes reproduce identical logits", rc=0) |
| device decode | 5.8283 = base unchanged (the restore sits outside the decode clock) |
| device exactness | 6/6 |
| **post-prime PSRAM free** | **818,596 B vs the 50,088 B baseline = +768,508 B reclaimed** |

That is more than the 442,368 B a 576x768 offset stream needs, so the measured **27 %-cheaper
addressing** (#745, 34 -> 25 cycles per word, fixture-verified) now has room to be
integrated. (Evidence-line bug noted: the `PREFIXKV` print derived both figures through the
compacted path, hence `saved=0`; the status-API number is the authoritative one and the print
should compute the full size independently.)

**The program now reads:**
1. ~~Free capacity~~ **DONE (#748): +768 KB, quality-safe.**
2. **Integrate the offset stream (B2, or B1 now that the bytes exist):** predecode nibbles to
   `64*p + 4*code`, then `l16ui; lsx; add.s` in place of `extui; addx4; lsi; add.s` + the
   packed word load, preserving the pair LUT's values, the four partial chains, the
   nibble-to-partial assignment, +0 seeding, fold, norm conversion and group/row order;
   price it on real rows with distinct LUT entries, several groups/rows and nonzero row
   offsets before any full-suite run.
3. B3's tier-tail relocation is closed by measurement (#747): the span's unused tail is only
   132,032 B, too small for the 313,344-byte pair.

---

## RESEARCHER STATE -- 2026-09-26 ~23:10Z (run #749: CV3W CLOSED ON THE BYTE BUDGET, 2.8x against it)

**The predecoded offset stream cannot work, and the campaign's own measured numbers say so:**

| quantity | value | source |
|---|---|---|
| proj2bit weight stream | 3.59 MB/token in 80.8 ms = **44 MB/s** | phase map |
| measured octal bus peak | **64 MB/s** | #287 GDMA, #330 |
| utilisation today | **69 %** | the two above |
| offset stream (1 B/weight = 4x) | **14.36 MB/token -> 224 ms at 100 % bus** | arithmetic |
| the addressing win it buys | 8 `addx4` of 33 instr/word = 24 % of issue slots = **<= 19.6 ms** | #745 |

So the lever is **2.8x slower even at an unattainable 100 % bus utilisation**, and the 19.6 ms it
could win cannot pay a 143 ms byte increase. **#745's 34 -> 25 cycles/word was measured on a
64-word fixture that lives in the 64 KB data cache** - its streaming cost never appeared. That is
the third instance of the warm-fixture trap (#295 phi, #364 transform, now #745), and it is worth
stating as a rule for this campaign: *an addressing/algebra change must be priced in BYTES per
token before it is priced in cycles per word.*

**What stands from the CV3W window, on its own merits (not as enablers):**
* **compact-prefix (#748): +768,508 B of post-prime PSRAM** (818,596 vs 50,088), quality-safe
  (host prefix-isolation passes), decode unchanged because the restore is outside the decode clock.
  Free memory is now ample; the byte budget, not capacity, is what blocks weight-stream ideas.
* B3's tier-tail relocation stays **closed by measurement** (#747: unused tail 132,032 B).

**What this leaves as the campaign's honest targets** (all cheap, none a weight-stream change):
1. **Profile-guided pairing of the remaining unpaired transcendental loops** - not blind pairing:
   instrument the host engine, find the loops > ~5k calls/token with independent arguments, pair
   them with NO added guard (the class that won #229/#290/#291, and the class #292 showed loses when
   a guard is added).
2. **`nd_sample`'s per-token legality path** - the sampler is ~0.7 ms of a ~150 ms token and its
   enumeration was already indexed (#288); re-check for a per-token rebuild that could be cached
   across steps.
3. **The idle-peer split review** (the mentor's #46 item): one audit of every `nd_parallel_rows`
   call site against the measured ~4,000-cycle handshake (#451) and the interleaved-tick
   consistency argument, prompted by the RX-ring ISR.
4. Anything that changes *what is stored* in the weight stream needs a byte budget first - the
   offset stream is the proof of why.

---

## RESEARCHER STATE -- 2026-09-26 ~23:55Z (run #750: the hottest kernel is AT ITS FLOOR, by inspection)

Dumped the live `nd_lut2_rows_tie1n` from board 2's 6.0617 ELF and read it as a sequence
(316 instructions total):

| op | count | role |
|---|---|---|
| `add.s` | 67 | 64 accumulate + 3 fold |
| `extui` | 64 | nibble extraction, 8 per 32-bit word |
| `addx4` | 64 | the nibble's x4 byte scaling (required: `lsi` is base+immediate only) |
| `lsi` | 64 | pair-LUT lookups |
| `l32i.n` | 16 | **all in the PROLOGUE** (arg struct, 2 slice loads, group norm) - per call, not per group |
| rest | ~44 | group bookkeeping (slli/add.n/addmi/addi.n/mull/wfr/j/mov.s/entry/bgeu/sub/beqz) |

The body is exactly the documented W8D floor - `extui x4 / addx4 x4 / lsi x4`, then `add.s x8`,
repeated for the eight words of a 128-weight group - with **no redundant instruction, no spill,
no reload**. So the 49%-of-token phase is instruction-exact, and the campaign's analytic floor
(2 instructions/weight at IPC ~1.33) now has instruction-level evidence.

**Audit technique worth reusing (zero board cost):** objdump the *live* ELF, group the
instruction mix, then read the sequence to separate per-call setup from per-group body. Next
candidates for the same treatment: the attention P.V kernel (the wide form that shipped at
+0.63 %), the QK dot that GCC owns, and `gemv4_tie728` (phi, 8.1 ms).

**Standing closures this window (do not re-screen):** CV3W offset stream (byte budget, #749);
tier-tail relocation (#747); tier-tail tail = 132,032 B; warm-fixture pricing is now a named
trap (3 instances: #295, #364, #745). **Standing keeps:** compact-prefix +768 KB (#748).

---

## RESEARCHER STATE -- 2026-09-27 ~00:30Z (run #751: the whole kernel set is clean; spill class verified absent on the newest tree)

Second peephole pass, on every other shipped kernel of the 6.0617 image, against the two failure
modes the load-form program identified (register pressure and redundant loads):

| kernel | instrs | notable | stack refs |
|---|---|---|---|
| `nd_gemv4_rows_tie1W` (wide phi) | 76 | 2 wide loads, 8 lsi, 9 madd.s | **0** |
| `nd_gemv4_rows_tie1` (plain phi) | 85 | 16 lsi, 8 addx4 | **0** |
| `nd_kron1_w` (kept wide kron) | 33 | 8 wfr / 8 madd.s / 8 ssi | **0** |
| `fwht_rows` | 414 | 46 l32i.n, 39 ssi, 38 lsi | **0** |
| `nd_fwht3s` (fused stage-pair) | 230 | 36 lsi / 36 ssi / 20 addx4 (float-stride address) | **0** |
| `lanepre_rows` | 67 | - | **0** |

**So the spill class is absent on the newest tree by direct inspection**, not by inheritance from
the older whole-ELF scan, and no kernel carries a redundant memory operation in its body. With
`nd_lut2_rows_tie1n` (audited last run: exactly the W8D floor) this covers the hot path.

**Hot-path inventory for whoever runs next:** `attn_heads` (6,486 B of C, kernels inlined),
`nd_model_step_hidden` (7,241 B), `nd_lut2_rows_tie1n` (918 B), `nd_gemv4_rows_tie1W`,
`nd_kron1_w`, `fwht_rows`, `nd_fwht3s`. **The most promising remaining host-only action** is the
same dump-and-read treatment applied to `attn_heads`' inlined body (the 24.1 ms phase), because it
is the only hot code that has never been read instruction by instruction - its 6.5 KB of C carries
the QK dot, the paired exp, the P.V wide kernel and the staging, and the load-form rule (wide wins
where the compiler re-reads, loses where a serial chain is exposed) is exactly the kind of thing
that is visible there.

---

## RESEARCHER STATE -- 2026-09-27 ~01:10Z (run #752: attention's instruction budget is REGISTER-limited, not redundant - and that explains three nulls)

Read `attn_heads` (24.1 ms, never audited) instruction by instruction from the live 6.0617 ELF:

| measurement | value |
|---|---|
| instructions | 2,414 |
| **frame accesses (spills)** | **0** |
| backward branches (nested loops) | 33 |
| `movi` + `l32r` (constant materialization) | **412 = 17 %** |
| same share inside the hot loops | 406/2,255 = 18 %, 288/1,696 = 17 %, 201/1,202 = 17 %, 132/818 = 16 % |

With **zero** frame traffic, those constants are not spill refills - GCC is **rematerializing**
addresses and float constants rather than keeping them live, the trade it makes when the 16-entry
float register file is the binding resource (the same pressure that decided kron2 vs kron1).

**This closes the last open explanation in the attention phase:** the live-range/hoist family
measured THREE nulls (#579 quartet barriers -0.090 %, #582 pointer+codebook hoist -0.060 %,
E-live-range edits -0.06 %) even though the constants are visibly there - forcing them live costs
registers the loop does not have, so the instruction count does not move. The other direction
(reducing the constant *count* by restructuring the exp polynomial or the clamp bounds) is a
rounding change, refused by the byte-exact and fidelity gates.

**Per-kernel audit status (all read from the live ELF, zero board cost):** walker at the W8D
floor (#750); wide phi, plain phi, wide kron, `fwht_rows`, `nd_fwht3s`, `lanepre_rows` all clean
with zero frame refs (#751); `attn_heads` register-limited by construction (#752). **The kernel
set is now audited; further speed work needs a change in what is stored or computed, both of
which are frozen by the gate.**

---

## RESEARCHER STATE -- 2026-09-27 ~02:00Z (run #753: compact-prefix promoted to breadth; the packet is complete again)

Full 20-case gate on the compact-prefix tree (B1, engine `c69648d2784a`):

| metric | value |
|---|---|
| decode | **5.83** (= the restricted screen 5.8283, reproduced on breadth) |
| ext / think / min | 5.7623 / 4.52 / 5.62 |
| prefill / boot bench | 6.1283 / 5.925 |
| device exact | 18/20, token_delta 52 = **only** the two #647 ring-related demo-timer goldens |
| host | 19/19, fidelity 5.341e-05, top1 10/10 |
| internal_free | 11,023 (restricted run printed 14,171 - measurement-point difference: 14 more requests, sampler structures allocated) |

So the prefix compaction adds **no** divergence of its own, and the shippable packet stands with
screen + breadth evidence. The runner required the one documented `AUTO_ALLOW_REPEAT` allowance
for a restricted-to-full promotion (a restricted screen appends the shipping signature) - **budget
one allowance per tree**, as the ledger already warns.

**Also closed off-device this window:** the conv-tap history is a **ring** (`(pos - j) % taps`,
no per-token shift) with an existing `nt == 3` fast path that reads the freshly written projection
instead of a copied history row; the general path's per-element modulo compiles to inline
`quou`/`remu` (no `__umodsi3` call in the image), and the whole tap phase is measured at 2.3 ms of
a ~165 ms token, so the modulo is sub-bar by the phase's own size. No candidate.

**The complete state:** seed line 6.0617 (+14.29 % over the owner's 5.3033 pin, host 19/19,
cross-board confirmed), shippable line 5.8283 (+9.90 %, host 19/19, breadth + compact-prefix).
Both blocked only on the owner's disposition of the two #647 goldens. Every hot kernel audited at
the instruction level; every phase closed by measurement; the remaining above-bar routes are owner
decisions (re-baseline/replace the two goldens; admit the seed-era stack; 120 MHz is vendor-blocked;
assertion-level RAM has no collectable buyer).

---

## RESEARCHER STATE -- 2026-09-27 ~02:55Z (run #754: a lever's SIGN is tree-specific - NF16V port INVERTED)

Porting the seed line's NF16V (five-instruction fp16->fp32 rebias + its load-bearing
sign/exponent predicate) to the shippable line:

| evidence | result |
|---|---|
| host oracle over all 65,536 encodings | `in_domain=30720 rejected=34816 mismatch=0 guard_miss=0`, `0x3c00 -> 3F800000` |
| macro after the edit | byte-identical to the seed line's |
| device exactness | 6/6, token_delta 0, boot bench unchanged (values right) |
| **decode** | **5.765 vs this board's 5.83 = -1.11 %** (seed line: **+0.642 %**) |

**New lesson, stronger than sub-additivity: a lever's SIGN depends on the tree it lands in.**
The +0.642 % was never a property of the transformation. Reverted byte-exactly (worker
provenance hash back to `c69648d2784a6422139c201c1627a0ea`, verified with the harness's own
**concatenation** form - my first check used the old path-dependent form and mismatched, the
campaign's recorded path-string trap for the third time).

**Two harness facts recorded:**
1. `measure.sh`'s reported `engine_md5=` metric line (line ~197) hashes only `engine/src/*.c` +
   `engine/include/*.h` **by path**, while `PROVENANCE`'s `PROV_ENGINE` **concatenates**
   `engine/src/*.c *.S engine/include/*.h esp32/main/*.c`. They are different numbers for the
   same tree - compare like with like, and prefer PROV_ENGINE.
2. **B1's baseline is not a pure shippable+compact-prefix tree**: it carries an E59 fused
   transform (`nd_fwht4r`, prints `FWHT4R live=1` at boot). Any future per-board claim must be
   read against that board's own tree, not against a line description.

**Where the +3.8 % between the lines really lives (diffed, not guessed):** `nd_quant.c` 224 lines,
`nd_model.c` 334, `gemv4_tie728.S` 440, `lut2_tie728.S` 81, plus `qk8w_tie728.S` which is an
**unused asset** (referenced only by kbench; garbage-collected from the shipping ELF - the wide QK
loss was correctly not shipped). `nd_gemv4_rows_tie1W` (wide phi) is called in the seed tree and
**not** in the shippable tree - that is the next portable candidate, and #754 says to measure it
against the receiving board's own tree rather than trusting its +0.36 %-class predecessor.
