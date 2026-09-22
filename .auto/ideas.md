# Experiment 13 - dot-product schedule audit: CLOSED, rejected, no integration (run #230)

**Blocker, recorded exactly:** Espressif's `esp-dsp` component is not in this tree, not a managed
component, and not in ESP-IDF 5.5.2 (`find / -name 'dsps_dotprod*'` -> nothing), and the campaign
forbids new dependencies. So the library-vs-library comparison cannot be run as written; the
prompt's alternative - reproduce its schedule in a local kernel and let the shapes decide - is what
was measured. `dot_reord` is a C model of the aes3 schedule (multiple accumulators, fold at the
end), not the assembly itself.

**Measured on device (kbench, board 2, accepted 4.9150 base, CCOUNT 24000/us, min of 25 rounds,
rows rotated over 8 distinct buffers):**

| shape | variant | cycles | vs control |
|---|---|---|---|
| attention Q.K, n=48 | `dot_c4`: one accumulator, two-term groups (what ships) | 138 | control |
| | `dot_pair_c`: the two dots of a position pair interleaved | 403 per pair vs 276 | **-46.0 %** |
| | `dot_reord`: four accumulators, fold once | 213 | **-54.4 %** |
| Kronecker, n=32 | `dot_c4` | 97 | control |
| | pair / reorder | 288 / 157 | -48.5 % / -61.9 % |

`bad=0`: the interleaved pair really is bit-identical to two scalar dots, so it was rejected on
speed alone, not on numerics. The Q.K dot is ~138 cycles and ~14,400 dots/token, i.e. ~8.3 ms =
4 % of the token, so the prize was worth having.

**What this kills, and what it does not.** The exp() pair win (Experiment 12) came from removing a
*latency* stall; these dots are not latency-bound - splitting the dependency chain into four
independent chains made things *worse*, so the loop is bound by instruction issue and operand
traffic, and extra accumulators only add live registers plus the final fold. That closes
"interleave the two dots" and "more accumulators" as C-level levers.

**And the 128-bit-load assembly lost too** (measured, same board/image): `dot_tie` with
`ee.ldf.128.ip` post-increment loads and the shipped two-term group into one accumulator -
bit-exact, `bad=0` - took **286 cycles against the C loop's 138 (-107 %)**. Two reasons, both
general: a 128-bit float load on the S3's ai engine is not one LSU operation, and an
`asm volatile` block is a scheduling fence, so GCC's own loop - which interleaves the two dots of
a position pair and software-pipelines across iterations - loses none of that to a hand-written
body. The `loop` instruction itself adds per-iteration overhead the C loop does not pay. `qh` head rows are 192 bytes apart, so a 16-byte-aligned model base
makes every head row 16-aligned; the staged `kf0/kf1` rows are plain stack arrays and would need
`__attribute__((aligned(16)))`. An order-preserving assembly dot (128-bit loads, still the shipped
two-term group into one accumulator) stays bit-exact by construction, so it is the only candidate
that could pass the byte-exact gate. **Disposition: the shipped `dot_c4` is the measured best of four
schedules** (pair -46 %, reorder -54 %, 128-bit asm -107 %; Kronecker n=32: pair -50 %, reorder
-61 % against 97 cycles). No integration, main tree unchanged, `esp-dsp` absence recorded as the
concrete blocker for the library-comparison half. The dot's 8.3 ms/token (138 cycles x ~14,400
dots) is therefore *not* reducible by schedule, load width, accumulator count or pairing - only by
changing what is computed, which the byte-exact gate refuses.

**Fixture caveat, on the record:** `reord_maxabs = 0.000e+00` is an artefact - the fixture values
are near-integers in int8 range, so every partial sum is exactly representable and the probe
cannot see a reduction-order error. It did not matter here (reorder lost on speed anyway, so no
gate decision was needed), but a future numeric-fixture dot test must use irrational-ish values or
it will report false exactness.

**Microbench discipline rule (cost two wasted builds, applies to every kbench addition):** with a
single fixed operand pair, GCC hoists a *pure noinline* call out of the timing loop (a 48-element
dot measured 8 cycles), and a variant that writes only to locals measures 0. Rotate over several
distinct operand buffers and consume every result through an FP register barrier
(`asm volatile("":"+f"(v))`) - a memory sink adds traffic and biases the comparison.

# Experiment 12 - paired attention exp: KEPT, +0.48% decode (run #229, 4.8917 -> 4.9150)

**Isolated kernel (device, real inputs).** `nd_expf_pair(a,b)` interleaves the two independent
degree-5 Horner chains of the attention online softmax. Fixture = 1024 argument pairs **captured
from the six real primary generations** (`host/nd_dump.c -DND_EXP_CAPTURE` writes the actual
`(s0-m, s1-m)` arguments; 49,152 pairs collected, strided to 1024 so all six prompts appear;
range [-10.63, 0], 27.5% exact zeros because the running max makes the larger of a pair exactly
zero). kbench on board 2, CCOUNT calibrated 24000 cycles/us:

| variant | cycles/pair | saving | mismatch |
|---|---|---|---|
| two scalar `nd_expf` | 247.42 | - | control |
| `nd_expf_pair` (C) | **198.04** | **+19.96 %** | **0 / 1024** |

**Why there was room (disassembly, not guesswork).** `attn_heads` lives in IRAM at 0x4037b138 and
GCC expanded the two inlined `nd_expf` calls **one after the other**, spilling live floats to
`a1+0x4a0..0x4f8` to do it. Interleaving gives the scheduler two independent operands per slot
and both chains stay in registers. Bit-exactness is by construction: each chain performs
`nd_expf`'s operations in `nd_expf`'s order with the same constants, and the two clamps stay in
the scalar path (the pair falls back to two scalar calls out of range).

**End to end.** Batch `20260921T210606.624475Z`: board 1 pristine control read 4.8917 exactly (so
the batch is trustworthy), boards 2/3 both **4.9150**, extended 4.8371, prefill 5.2433, min_case
4.70, byte-exact 14/14. Swapped confirmation with the candidate on the canonical board: 4.9150 /
4.8357 / 3.88 think / 5.2433 prefill / 4.70 min, 14/14 device + 13/13 host, fidelity 5.341e-05,
boot bench 201 -> **199 ms/token**, internal_free 15759 -> 15503, flash +256 B.

**Prediction vs measurement, kept on the record**: arithmetic said +0.86 % (49 cycles x ~8500
pairs/token); it delivered +0.48 %. The estimate assumed none of the exp latency was already
hidden by neighbouring work - some was. Half of an analytically predicted win is still a win.

**Banked headroom (new candidate, not a follow-up to a failure).** 198 cycles/pair is ~99 cycles
per exponential against ~11 FP ops + a handful of int ops of actual work. Experiment 12's own
condition for reaching for assembly ("if C does not schedule it") was not met, because C did
schedule it and paid. But a handwritten TIE728 pair with no spill/reload at 60-80 cycles/pair
would be a further ~+1.5 %, and the same kbench + captured-pair fixture path is already built, so
it is cheap to screen. Do it as its own experiment with the same bit-exactness gate.

**Reusable method, worth more than the 0.48 %**: capture the real predicate arguments on the host
(it runs the same engine) and feed them to the device microbenchmark. This kills the
#149-calibration failure mode (a synthetic/fresh-state microbench overstated a sampler saving 3x)
at its root: the bench now runs on the distribution the shipping model produces, and reports
per-element bit mismatch rather than a tolerance.

## PSRAM tier: LOW-END coverage now measured too (run #206) - and a knob beat a model

`ND_TIER_TRACE=1` (already in the tree, prints at model open) answered a question the campaign
had been assuming: `emb=11136..3255168 in_span=0` - the logits embedding and candidate-gather
rows, read every token, were entirely **below** the staged span, while 8 MB of the 12 MB
allocation was dead padding. Because `hi_p = lo_p + ND_TIER_SPAN_BYTES`, pulling `lo_p` down to
the embedding's offset cost **zero extra copy bytes** (the padding above the blob shrinks) and
`nd_cact_tier()` translates relative to `lo_p`, so coverage followed for free: three lines.

Measured result: **4.8917, exactly the accepted value**, extended 4.81, think 3.87 (decisive -
think reads all 3.14 MB of the embedding every token), byte-exact 14/14. Re-ran with the trace
to prove it was live (`in_span=1`, `content=7848512` vs 4602944) before calling it a null, since
a silent fallback looks identical. **Tier family closed**: span, stride, copy order, allocation
ceiling, and now low-end coverage.

Why it is worth nothing: the tier is a *copy into PSRAM*, not a cache, and its measured value
belongs to converting big sequential flash reads (the ~4.4 MB/token of per-layer projections)
into PSRAM reads. Scattered logits rows (~1500 of 8192, spread over 3.14 MB) are limited by the
access pattern, not the address space.

**Method lesson (the keeper):** a two-line diagnostic knob already in the tree settled this in
one build, where earlier cycles had built models of the same question. When something is
cheaply observable, observe it: `ND_TIER_TRACE` both generated the candidate and then falsified
its own null against the silent-fallback failure mode.

## The sampler was a real lever, and the bench could not see it (run #147: KEPT, +2.16 %)

`nd_sample_hidden` decided legality by walking every vocabulary piece's bytes
through the grammar, once per decode step. On device that was 14.6 ms of a 207 ms
request token (7 %) - and the boot-bench phase map reported `sample = 0.0 ms`
because the bench calls `nd_model_step_hidden` and never samples. Fixed by
resolving byte 0 once per byte value for the current state (256 grammar steps)
and vetoing candidates by table lookup before any grammar call; survivors still
take the full byte walk, so the candidate set, the argmax and its tie-breaking are
identical. **+2.16 % decode, +2.13 % extended, +2.4 % on the worst case, with the
boot bench and prefill unchanged to the digit** - that invariance is the cleanest
internal control of this campaign, because neither path samples.

Dead end on the way, worth remembering as a method lesson (#146): memoizing the
whole candidate list on the grammar state measured **exactly zero**, because the
state changes on essentially every accepted token. The lever was not *skipping*
the walk, it was *repeating the cheap part of it per byte value instead of per
candidate*. When a hot loop is too expensive, look for the work inside it that
only depends on a small domain - a byte, a type tag, a shift - and hoist that.

Open follow-ups in the same phase: the survivors' full byte walk and one
`nd_tok_piece` call per candidate per step remain, so if another look is wanted,
split `ND_P_SAMPLE` into table-build / survivor-walk / piece-lookup in a profiled
build (harvest recipe below works and takes ~10 min on a warm board).

## Full-token accounting CLOSED (runs #147-#153)

The constrained sampler's real-request cost is now fully attributed, and the last
"5x anomaly" in it is not an anomaly. Of the ~10 ms it still costs per request
token, **~7.2 ms is `nd_model_logits_subset`** - the projection over the grammar's
candidate rows. At the rate the 2-bit path sustains everywhere else (~6 ns per
weight) that implies ~1 200-1 500 candidate rows x 768, which is what a JSON-value
grammar legitimately allows; the rows are visited in vocabulary id order, which in
this archive *is* ascending row order, so the gather is already sequential and
there is no sort to exploit. Cutting the candidate set would change what the model
may say - forbidden, not optimised. The remaining ~2 ms is the piece lookup and
survivor walks, and caching piece[0] there measured **-0.17 %** (#149).

Consequence: after the #147 first-byte table there is no phase of the decode token
above 0.2 % that is not (a) a 2-bit GEMV at the TIE728 instruction floor, (b) the
same GEMV's subset variant over a legitimate row count, (c) exp-bound Sinkhorn or
attention whose only bit-exact headroom was already taken, or (d) the 4-bit phi
GEMV whose residual is PSRAM line-fill on a non-resident working set (#141). The
campaign's remaining honest work is verification and the periodic behavioural
capture.

## Benchmark-coverage extension attempt, and two things the firmware does that matter (run #155)

Tried to widen `extended` (the rules invite this; `primary` is frozen and was
verified byte-identical to git before and after). Added 4 held-out cases covering
real gaps: `research_and_plan` had **zero** routing coverage, `get_status` only
appeared with a memory question, and nothing tested the schema's numeric bound.
Host side was clean - 15/15 byte-exact, and the model chose `research_and_plan`
and emitted the 4-digit `3600` correctly. The **device** run could not complete
the `think` group: reproducibly, after ~15 requests in one attached session, the
`!think 1` toggle goes unacknowledged and the next request times out - the same
"console reads fine, writes block" signature as the API attach failure in #152.
Reverted; the 12-case canonical run is green again, so the campaign's own numbers
are unaffected. Two theories were falsified rather than assumed: it is not the
long-running timer my case started (removed it, still failed), and it is not
request-line length (my longest line was 237 bytes of the 272-byte reader).

Two durable findings came out of it, both worth more than the coverage would have
been:

- **`esp32/main/main.c` silently truncates a request line at `ND_LINE_MAX-1` = 271
  bytes.** The console read loop drops overflow characters and keeps waiting for
  `\n`, so a longer request is answered as its first 271 characters with no error
  of any kind. Fine for this benchmark (longest request today is 75 bytes); a real
  product defect for any host that sends arbitrary user text. Flagged for the
  owner; the fix is an explicit `ERR line_too_long`, not a bigger buffer.
- **CORRECTION (run #157): there was no console write-block defect.** Three cycles
  were spent diagnosing "console reads fine but writes block" across #145, #152 and
  #154. The real cause is mine and mundane: inside `needle-board run N`,
  `/dev/ttyACM0` is the **flash** port (`needle-pi/flash`) and `/dev/ttyACM1` is the
  **console** (`needle-pi/console`) - the AGENTS.md alias convention. I pointed
  `needle-api --serial /dev/ttyACM0` at the flash port, so every write timed out
  while the boot log still streamed (the USB-Serial-JTAG flash port shows boot
  output, which is what made it look like a working-but-gated console). One flag
  change and the API came up first try and `make capture` passed all 7 scenarios
  with all 9 verification flags true on the 4.8817 runtime. Lesson: when a
  home-rolled reader behaves differently from the repo harness that uses the *same*
  device class, suspect the arguments, not the driver - bench.py was succeeding
  with `serial_api.Device` the whole time, which was the fact I should have
  followed.
- Still open and real: `bench.py`'s per-request timeout, not the console, is what
  killed the long-route-request extended cases in #154 (writes were fine there;
  bench.py had the right port). If longer held-out prompts are ever wanted, the
  timeout is the knob to look at, not the port.

## Request-path profile harvest: how to do it in one shot (and why I stopped at run #158)

Two ways I failed to get a per-request `EVT prof` table, both worth not repeating:

- `idf.py -DNEEDLE_PROFILE=ON` in a **fresh** build dir does not define
  `ND_PROFILE` even though `esp32/components/needle/CMakeLists.txt` has
  `option(NEEDLE_PROFILE ... OFF)` + `target_compile_definitions(... PUBLIC
  ND_PROFILE)`. Verify with `-D CMAKE_C_FLAGS="-DND_PROFILE=1"` on a fresh dir,
  and grep the **exact token** `-DND_PROFILE` in `compile_commands.json` - not
  `-DND_PROFILE=1`, which is not how a bare define appears (that wrong grep made
  a working build look broken and a broken one look verified).
- To read the table for a real request: attach with
  `sys.path.insert(0, '/workspace/esp32-needle-3/tools'); import serial_api;
  serial_api.Device('/dev/ttyACM1', ...)` **inside `needle-board run N`** (that
  port is the console; 0 is flash), then write the prompt with
  `dev.serial.write(b"...\n")` and drain with `dev._line()` until `END`.
  `Device.complete()` swallows the raw lines, and `prof_dump` prints **after**
  `EVT done`, so breaking on `EVT done` hides the table. `prof_dump` does not
  zero `nd_prof`, so a request dump also contains the boot bench's totals.

Stopped because the answer cannot change a decision: the only number in dispute
was whether `nd_model_logits_subset` costs ~7 ms (my arithmetic) or more, and its
only fixes are a smaller candidate set (changes what the model may emit -
forbidden) or a faster 2-bit GEMV (measured at its floor a dozen times). A
measurement that cannot alter the next action is not worth 14 minutes of board
time, which is what a flash costs here including the five-minute priming.

## 4-bit folded codebook: CLOSED UNSAFE (runs #159-#162), and a shared-state audit of everything shipped

`gemv_rows_folded` (kept at #159 for +0.17 %) wrote **one file-scope 8 kB table from
both cores**. `rows_dual_core` (esp32/main/main.c) releases `s_go` and then runs its
own half with **no barrier between the halves**, so the two invocations drift; a
tick landing mid-build mixes two groups' values into one operand. It was byte-exact
on three boards and the host by timing luck - equal-sized halves starting together
stay in phase often - and the host can never catch it because `nd_parallel_rows` is
`rows_serial` there. Reverted at #162; the race-free caller-side variant (build the
table outside the split, one `nd_parallel_rows` per group) measured worse on every
monitor (4.87 / 4.7857 / 3.95 vs 4.8817 / 4.7986 / 3.87) because it trades one
handshake per projection for one per group plus a read-modify-write of `y` per
group, and per-core tables need 16 kB against 8.3 kB of free internal RAM. The
deeper reason it was always going to be small: the fold replaces one **internal
SRAM** operand (`xh`) with another; the phase is dominated by the 4-bit *weight*
stream in PSRAM, which the fold does not touch. Micro-optimising a non-dominant
operand cannot exceed that operand's share.

**Audit rule this created:** before keeping any kernel that `nd_parallel_rows` runs,
check for shared mutable state, not just for matching numbers. Every kept split
kernel was re-read with that question and all are clean:

| kernel | why it is safe |
|---|---|
| `lut2_rows_tie1n` (TIE728) | per-row registers only; the pair table is read-only |
| `attn_heads` (#139) | staging arrays are function locals (`kf0/kf1/vf0/vf1/mx/denom/ohp`), head ranges are disjoint, outputs are per-head |
| `gather_rows`, `gemv_rows_generic`, `gemv_rows_offset` | pure per-row, no file-scope writes |
| first-byte legality table (#147) | 256-byte array on the stack, sampler runs on one core |
| fp32 weight pools, 12 MB PSRAM tier | built once, read-only afterwards |

## Sampler family CLOSED - measured primitive costs and a calibration lesson (runs #149)

Microbenchmarked on device (ND_PROFILE boot block): **`nd_tok_piece` = 45 cycles**,
**`nd_gstate_byte` = 93 cycles** per call. After the first-byte table, the
constrained sampler costs ~2 ms of a ~205 ms token: ~0.1 ms to build the 256-entry
table, ~1.6 ms for the piece lookup + filter over ~8176 ids, ~0.06 ms for the
survivors' byte walks. The subset-logits projection is timed separately (7.2 ms,
TIE728 kernel, at its floor).

Caching piece[0] per id in 8 KB of internal RAM (built once, falls back to the
call if the allocation fails) measured **-0.17 %** - byte-exact, correct, and
slower. Replacing one 45-cycle call with a dependent-load chain (global pointer,
branch, byte array, 256-byte stack table) gave the scheduler less to work with
than the call did.

**Calibration lesson (this is the keeper):** run #147's win was predicted at
~12.8 ms from the 93-cycle microbench and delivered ~4.5 ms, because that
microbench probed a *freshly opened* grammar state - the cheapest possible
predicate - and copied the state struct per call. Mid-string states are several
times more expensive. A microbench of a predicate has to run on the states the
real loop visits, or it will overstate savings ~3x. Both this #149 (and probably
#146's zero) trace back to that same over-estimate.

**Bonus: this closes the last anomaly in the campaign.** The boot bench measured
~4 % faster than real requests purely because requests paid a 12.8 ms grammar
walk the bench structurally never runs. After #147 the bench-to-request gap is a
~2 % ordinary residue (console emit, bookkeeping).

## Measurement noise floor of the primary metric (run #144)

Three byte-identical-source images, freshly configured, measured on all three
boards in one batch: 4.7800 / 4.7783 / 4.7817 decode tok/s - **spread 0.071 %
peak-to-peak, sigma ~0.03 %**, all 12 cases byte-exact. The 0.2 % keep bar is
therefore about 6 sigma, and a candidate claiming <= 0.1 % is not resolvable on
one run: it needs a repeat batch, not a verdict. Identical sources legitimately
produce *different* md5s per board (ESP-IDF stamps the build time); identical md5
across *different* flags is the failure signal, not this.

# Ideas backlog

Ranked by expected payoff per unit of risk. Delete entries as they are tried.

## Candidate optimisations

- **Monarch MLP: hoist the fp16→fp32 conversions out of `kron_apply`.** Each of
  the three `kron_apply` calls per layer re-reads `a`/`b` once per inner
  iteration, so ~65K `nd_f16()` calls per kron (≈1.6M per token) when only 1024
  distinct values exist. Stage the converted factor into a small scratch buffer
  (or transpose it once) and the arithmetic becomes a plain 32×32 matmul. Pure
  algebra-preserving change; check bit-exactness (accumulation order changes, so
  verify against the logit-fidelity probe).
- **Sinkhorn budget.** `ND_SINKHORN = 20` iterations of exp/log on a 4×4 matrix,
  twice per row/column pass — ~10K `nd_expf` + ~1.3K `logf` per token. Either
  early-exit when the max row/col residual is below a fixed epsilon, or replace
  the exp/log pair with a scaling-only (non-log-space) Sinkhorn. Both change
  numerics; only acceptable if the fidelity probe and goldens stay exact.
- **DONE, run #159: 4-bit folded codebook for the mHC phi GEMV (+0.17 % decode,
  +2.6 % think, +0.21 % extended, bit-exact).** `nd_cact_codebook()` keys only on
  archive + bit width, so `cb[k] * xh[j]` is row-independent - the same fact the
  2-bit pair table exploits. Sized right it is *8 kB*, not the 48 kB I had guessed,
  because one group is 128 positions x 16 levels and the group is reused by all 24
  rows; `gemv_rows_folded` goes group-outer / row-inner and keeps dot_group's four
  partials, so every `y[r]` is bit-identical. Two lessons: (1) the backlog entry
  said "probably too big" without doing the per-group arithmetic; (2) the recorded
  "-12 % ceiling, not worth it" analysis assumed the kernel was issue-bound, while
  its own numbers (7x above both floors) said latency-bound - and the standard cure
  for latency-bound code is fewer dependent loads, which is exactly how the 2-bit
  kernel won +33 %. Re-derive an old disposition's *premise*, not just its number,
  before trusting "closed".
  Follow-ups: halve the table to 4 kB by processing each group in two position
  halves while carrying s0..s3 across the halves (bit-exact, no speed claim); and a
  real TIE728 kernel over this single resident table is now a different question
  than the one the -12 % estimate answered, because the operand shape changed.
- **Attention: int8 dot product.** The KV dot product converts every int8 to
  float per element. Accumulating in int with the LX7's 32-bit ops, or reading
  four int8 per 32-bit word (with per-head scale applied once), shortens the
  inner loop. `qk_head_dim` is 48, so 12 words per head instead of 48 bytes.
- **Prepare/LUT reuse across the block.** `attention` prepares + builds a LUT for
  q/k/v/gate, then again for out_proj; `nd_cq_prepare` runs an FWHT over the
  whole padded activation each time. Check whether out_proj's in_pad differs
  (768 vs 768) and whether one FWHT can serve both.
- **tap_projection** calls `nd_f16(weights[...])` inside the inner loop over
  taps — small, but it is per-element per-projection; hoisting is free.
- **`zcrms` / `rms_unit` / lane mixing loops** are all elementwise over 768 or
  3072 floats with a scalar loop; the ESP32-S3 GCC may vectorise with
  `-O3 -ftree-vectorize` if the pointers are restrict-qualified. Try adding
  `restrict` to the hot elementwise kernels (zero numerical change).
- **Confidence pooling** (`pool_cell`) runs per layer per token over
  n_probes × d_model. If this blob carries the head, it is pure overhead for
  decode speed; measure `ND_P_CONF` before assuming it is small.
- **Dual-core coverage.** Only GEMV rows and attention heads are split. The MLP,
  sinkhorn, tap projections, rope and engram conv run single-core. A coarse
  split (core 1 runs the MLP of the previous lane-mix stage while core 0 streams
  weights) is complicated; a simple one may be to overlap the *out_proj* GEMV
  with attention tails.
- **DISPROVEN, and it nearly cost the run #2 win.** A note here claimed the
  board downclocked to 80 MHz and that 816 -> 406 ms/tok was a power-mode
  effect. It is not: `esp_clk_cpu_freq()` reports 240 MHz and the xtal 40 MHz
  for both binaries, PM is disabled, and the same 2x appears from a flash A/B
  of the two images. The 2x is the fp32 MLP-factor change (run #2).
  Lesson: measure the clock and diff the binaries before believing any
  "impossible" speedup is an artefact - and revert accidental `cp`s of a
  baseline file into the tree (that is what produced the phantom 1.22).

## Candidate optimisations (revised after the above)

- **Iteration speed (not the metric):** priming two schema prefixes at boot costs
  ~5 min per flash. Priming both prefixes concurrently on the two cores, or
  caching a primed prefix in flash, would roughly halve experiment latency
  without touching decode kernels. Worth it if the loop needs more samples/hour.
- **`make capture`** (7 real end-to-end scenarios + renderer verification) is the
  repo's own integrity check. Run it every ~10 kept changes to prove the board
  still behaves, not just that it is faster.

## Measured dead ends (fill in as found)

- **fp32 staging of the Monarch factors is NOT a dead end: it is run #2,
  +99.9% decode** (1.2217 -> 2.4417 tok/s). It sits in "what worked" now. What
  is worth recording here is the *method* mistake: a host ratio of 1.24x made
  the 2.01x device number look impossible, so it was written off as an
  artefact, and a stale baseline copy left in `engine/` then made the device
  appear to agree. Both binaries were later flashed back-to-back at a reported
  240 MHz: BASE 816 ms/tok, CAND 406 ms/tok. Host speed is not a proxy for
  device speed in either direction. The earlier tap_projection part of this
  entry remains genuinely untried (see below).

- **MimiModel queue lever (overlap the gate projection with the Q/K path on core
  1): UNSAFE, not taken.** Two independent reasons, both measured on device.
  (1) The schedule itself is a data race: the gate job writes m->gate[half:out]
  while core 1's attention-head split reads and writes around m->gate, so the
  rows core 1 consumes are not stable. That is exactly what the
  `EVT ERR async-overlap` tripwire exists to catch.
  (2) It also exposed a REAL latent hazard in the splitter that is now fixed in
  main.c: the worker gave s_done on EVERY wake and a second splitter shared the
  same s_go/s_done pair, so a wake could be taken by the wrong waiter, which
  then read rows the worker had not written. One serialized job slot (fn
  published before the wake, cleared after it runs, s_done given only for a job
  that ran) removes the whole class. Verified: pristine tree 4.185 / 12-12
  byte-exact on the same board that had been showing 0/12, so no stock-code
  regression ever existed - every divergence was harness-local.
  Lesson for this repo: any new cross-core job must go through the single job
  slot, and no job may run concurrently with an nd_parallel_rows split.

## Measured dead ends (fill in as found)

- **fp32 staging of the Monarch Kronecker factors (w1a..w3b) + d2/b2/d3/d4:
  NOT a dead end - it is run #2, +99.9% decode.** Recorded here because the
  first reading (host 1.24x, device 2.01x) looked too good for ~4K ops/token
  and was written off. The device number is real: both binaries flashed
  back-to-back, cpu_hz=240 MHz reported by both, boot bench 816 -> 406 ms/tok,
  text byte-identical. Host and device diverge because on x86 the 2KB factors
  live in L1 and the only saving is the F2F instruction; on the S3 the fp16
  loads plus per-element conversion compete with the streaming activation.
  Lesson: do not predict device gains from a host ratio, in either direction.
- **4-bit row norm hoist: neutral.** Same change that was a free refactor in the
  2-bit LUT path, applied to `gemv_rows_offset`/`gemv_rows_generic`: boot bench
  370 vs 370 ms/tok, images flashed back to back. Norm conversion is off the
  critical path in both flavours.
- **Packing the 2-bit row walker into 32-bit loads: broken, reverted.** Trying
  to read two packed bytes as one word changed results (the LUT index is
  per-pair, and the byte order is not what the shift made it), so this is not a
  safe one-liner. The row bytes are only 32 per group anyway.
- **Sinkhorn 20 -> 6: vetoed, not a dead end for a re-tuned budget.** Costs a
  real logit change (max_delta 7.8, top1 6/10) for +0.8% decode. If a future
  session re-derives a convergence-tested iteration count that keeps the probe
  bit-exact, the win is there; 6 is not it.
- **Attention: 4 KV positions per online-softmax iteration is SLOWER than 2.**
  2.56 tok/s vs 2.7933 control on board2 in the same batch (-8.4%), byte-exact.
  The quad block spills off the LX7's register file; pairing is the sweet spot.
- **2-bit LUT GEMV: two rows at a time is slightly slower.** 2.7733 vs 2.7933
  (-0.7%) on board2, byte-exact. The GEMV is flash-bandwidth bound, not issue
  bound, so a second row stream competes for the same bus instead of hiding
  latency. Row-loop unrolling in this kernel is not the lever.
- **Constrained-logits gather: norm hoist is a small real win (+0.18%).** Kept
  (13373e7); byte-exact, and it lifts the think path with the primary metric.
- **Two dead-end families, so far, on top of the fp32 staging wins:** arithmetic
  hoisting inside the GEMVs (3 nulls) and loop unrolling in the GEMVs (2 nulls).
  Everything that has moved the needle changed *what is read* or *how many times*
  a value is touched, not how the inner loop is scheduled.
- **kron_apply first-half blocking is capped at 4 rows.** 8 rows: 2.995 vs
  3.1717 control (-5.6%). Second half caps at 8 columns (+0.9% over 4). The
  LX7 register file wants 4 accumulators when the reuse is in `a` and 8 when it
  is in the loaded `c` value. Do not widen the first half again.
- **Engram-gate RMS fusion (3 passes -> 1): neutral** (3.125 vs 3.1267) and it
  quietly breaks the n1 buffer contract (n1 must still hold the attention input
  when block() reaches the attention sub-block). Not taken.
- **kron_apply is saturated.** Measured optimum: first half 4 rows x 2 j-columns
  (8 accumulators), second half 8 columns with paired b rows. Wider on either
  side regresses: 8 rows -5.6%, 4k x 4j -1.2%. The lever was always *operand
  load reuse*, not accumulator count alone - 8 rows added accumulators without
  reuse and lost. Stop tuning this kernel.
- **Blocked attention-gate sigmoid: exactly zero** (3.1817 vs 3.1817). 768
  elements once per layer is below the noise floor. Small elementwise loops are
  not worth ILP work on this board.
- **Six more nulls on the main board or in verified parallel batches:**
  FWHT 8-butterfly unroll, straight-line n=4 sinkhorn, kv_slot hoist out of the
  attention inner loop, attention-gate sigmoid blocked by 4, LUT builder 2-wide,
  restrict on rms_unit/zcrms. All byte-exact, all within +-0.05%. Instruction
  scheduling and loop-overhead removal are DONE as levers on this firmware: at
  3.76 tok/s every phase is now either bandwidth-bound or latency-bound in a way
  the scheduler cannot fix. Only structural changes remain (core-1 coverage of
  the serial stage, or fewer bytes per token).
- **Process rule learned the hard way:** a parallel batch whose board1 control
  did not match HEAD's last measured value was silently stale (cp-based resets
  instead of git). From now: `git fetch && git reset --hard FETCH_HEAD` before
  every batch, and verify the control's decode_tps equals the last logged value
  before trusting any candidate delta.
- **Splitting the leftover per-layer stage now buys ~0.05%, not 1%.** The MLP
  (both kron halves, SiLU, lane mix) and the GEMVs were the splitable mass.
  Measured against a 3.9517 control in one batch: rms scale-pass split +0.04%,
  engram tap-matmul split +0.04%, p1/p2 permutation split +0.13%, combined
  fold+SiLU gate worker (one handshake instead of two) -0.4%. The lever is
  spent; do not split anything smaller than a kron half.
- At 3.95 tok/s the profile has no phase above ~15% that is not already
  two-core or flash-bandwidth bound. Remaining ideas would change *what is
  computed* (quality risk) or how bytes are streamed from flash (cache-blocking
  the LUT GEMV - tried once and the naive 4-row block was wrong; a correct
  cache-blocked version remains the only big-ticket idea left).
- **Dynamic self-scheduling (both cores pull 4-unit grants from one atomic
  counter) is 6% WORSE** than the fixed half-split: 3.70 vs 3.9517 control,
  byte-exact. The fetch_add per 4 units is not free on the LX7 and the two
  halves are already balanced. Do not replace the splitter with a work queue.
- **RoPE split over heads: neutral** (3.955 / 3.9517 vs 3.9517 control, and
  identical on a second board). 12+2 heads x 24 pairs is below the handshake.
- **zcrms emit-pass split: +0.13%** (3.9567 vs 3.9517) - under the 0.2% keep bar
  for a second handshake per layer. rms scale-pass split: +0.04%.
- Everything measured since the lane-mix split is inside +-0.15%: the two-core
  lever is closed. Reverted the rope split to keep the tree minimal.

## Experiment 2/3/4 CLOSED (run #137-#138): the TIE728 assembly kernel is in

**+13.0 % decode (4.190 -> 4.735 tok/s, +93.9 % over baseline), byte-exact.**
`engine/src/lut2_tie728.S` `nd_lut2_rows_tie1n` replaces the C row walker for
every 2-bit projection whose geometry/alignment/norms pass `nd_lut2_asm_ok()`
(all of them in needle3.cact), selected inside `nd_cq_gemv_lut2` before
`nd_parallel_rows`, C path retained as fallback, guarded by CMake option
`NEEDLE_LUT2_ASM` (ON). Microkernel saving vs C on real tables: +33.4 % cycles
saved in split mode, `exact=out/out bitexact=1` on 768x768 / 576x768 / 128x768 /
96x96. Cost 1 KB IRAM. Full notes in `.auto/mimimodel-experiments.md`.

Kernel structure that measured best (do not re-derive): one output row per call;
four independent partials `f0..f3`; eight `lsi` gather loads per index word
batched at the top; nibble -> address with `extui`+`addx4` (2 instructions per
slot, that is the floor for this table layout); `add.s` into four partials, fold
`(s0+s1)+(s2+s3)` then one `madd.s` with the group norm; the group's norm
halfword `l16ui`'d at the *top* of the group. Losing variants, all bit-exact,
all reproducible on three boards: deeper index prefetch (-1.9 pp), two-row
blocking (-6.7 pp), early norm *conversion* (-1.1 pp vs hoisting the load only),
and `lsi` cannot reach a whole group with immediates (field caps at 1020 bytes,
so a per-group table base advance of 0x1000 is illegal - three `addi` per group
is the floor).

Remaining open work in this family: Experiments 7-11 (compiler floating-point
candidate, CQ2 integer path, compact quantised pair-LUT, one quantised activation
reused across Q/K/V/gate, Xtensa SIMD integer dot). Experiment 8's premise
changed: the C kernel is no longer the baseline, so an integer path must now beat
`tie1n`, not C.

## Full per-token phase map (accepted 4.78 tok/s tree, boot-bench token = 201 ms)

Measured with `AUTO_PROFILE=1` on board 3; timers added to `ND_P_*` are
diagnostics only (ND_PROFILE is off in every measured image).

| phase | ms/token | share | state |
|---|---|---|---|
| 2-bit GEMV total (`proj2bit`) | 86.5 | 43 % | TIE728 kernel in use. **At its instruction floor: `W8D` is 32 instructions (8 `extui` + 8 `addx4` + 8 `lsi` + 8 `add.s`) per 32-bit index word, one word = 8 nibbles = 8 weight *pairs* = 16 weights, so 2 instructions/weight.** 14.3 M weights/token x 2 = 28.6 M instructions ~ 86.5 ms means sustained IPC ~1.33, i.e. throughput-bound. See the "Instruction floor" section below. |
|  - q/k/v/gate projections (inside `attention()`) | ~59 | 29 % | derived: `attn-stage` 101.8 - heads 36.7 - stage 5.1 |
|  - out_proj + logits | ~27 | 13 % | same kernel |
| attention head split | 36.7 | 18.3 % | products+exp bound; KV row staging already shared 6 ways |
| Hadamard MLP | 24.6 | 12.2 % | of which `kron_apply` 11.1 - **GCC already emits `loop`+`lsi`+`madd.s`, no asm headroom** |
| engram | 16.4 | 8.2 % | gathers (flash latency) + its own 2-bit GEMVs, which already use the asm kernel |
| mHC phi (**4-bit** generic path) | 13.5 | 6.7 % | **~5 instructions/weight; the one clear arithmetic target left** |
| attn stage: qkv taps 3.3, head norms 0.5, rope 0.2, kv store 1.1 | 5.1 | 2.5 % | all small; do not split anything here |
| mHC mix 4.7, sinkhorn 4.3, prep+LUT 3.5, step tail 0.5, sampler 0.0, conf pool 0.0 | 13.0 | 6.5 % | sampler and conf pool are *free*; sinkhorn is exp-bound |
| unattributed | ~14 | 7 % | per-layer glue, lane init, tier pointer math, timer overhead |

Two things this map kills: the missing decode time is **not** an unnamed
attention stage (taps+norms+rope+KV store is 5.1 ms total), and it is not the
per-layer glue.

**CORRECTION (run #145): `sample = 0.0 ms` was a provenance artefact and the
sampler is NOT free.** The `EVT prof` block this map came from is printed after
the *boot bench*, and the bench drives `nd_model_step_hidden` directly - it never
runs the grammar or the sampler. Two independent measurements say the real
request path costs ~4% more than the bench (bench 4.984 tok/s vs primary 4.7783
at 201 ms/token), i.e. ~8-9 ms/token of work the bench cannot see, and the
per-case data below localises part of it to the grammar/sampler. `prof_dump()` in
`esp32/main/main.c` (ND_PROFILE-only) now prints the same table for a real
request's decode window, so this is measurable; the first harvest is still
pending (see "Route-phase gap", below).

## Route-phase gap: CLOSED as context size, not overhead (run #145 harvest)

Harvested with `prof_dump` on a profiled board-3 image, driven through
`tools/serial_api.py`'s `Device` (open with **DTR/RTS pinned False**: toggling
them resets the chip into the ROM loader, which is why a hand-rolled reader sees
nothing). Tools vs route request, same board, same image:

| phase | tools ms/tok | route ms/tok |
|---|---|---|
| proj2bit | 111.2 | 111.2 |
| attention (head split) | 36.3 | **51.6** |
| hadamard / engram / phi / logits / prep / sinkhorn / mix / taps / norms / rope / kv | identical | identical |
| sample | 14.6 | 15.2 |
| whole block | 195.8 | 211.1 |

Every phase matches to 0.1 ms except attention, and the prefill lines explain it:
`sink=143` vs `sink=213`. The route case simply attends over ~50 % more positions.
Not a defect, and the fixes (shorter span, fewer sinks) are frozen by the archive.

**Real-request phase map, and how to read it.** The boot bench understates every
phase by ~28 % and cannot see `sample` at all, so use the request table above for
sizing work.

MEASURED on the current tree (run #166, `AUTO_PROFILE=1` + direct `serial_api`
drive, tools case `sampling5`, think mode off - `Device()` defaults to think ON,
which measures the 8192-row vocabulary path instead and showed `logits4` 63 ms):

| phase | ms/token | share |
|---|---|---|
| whole block | 187.2 | 92.4 % |
| proj2bit | 105.7 | 52.2 % |
| attention heads | 35.5 | 17.5 % |
| hadamard | 24.5 | 12.1 % |
| engram | 20.1 | 9.9 % |
| mHC phi (4-bit) | 13.5 | 6.7 % |
| **sample (all of it)** | **8.9** | **4.4 %** |
|  - of which `logits4` (subset projection) | 5.7 | 2.8 % |
| sinkhorn 4.3 / mhc-mix 4.7 / prep+lut 4.2 / taps 3.3 / norms 0.5 / rope 0.2 / kv 1.1 | 18.3 | 9 % |

Reproducible to two decimals across two identical requests. Two consequences: the
subset projection is 5.7 ms, not the ~7.2 ms I inferred, so #162's folded gather
could not have paid and the sampler family is closed on measurement; and the
sampler's own arithmetic is ~3.2 ms, matching #149's microbench. Note `proj2bit`
reads 86.5 on the bench and 105.7 here while `whole block` is ~188 in both: that
is timer attribution (the engram's GEMVs land in both counters), not a speed
effect - 105.7 ms is already ~66 % of the octal-PSRAM byte peak, so do not read
19 ms of extra GEMV as addressable work. Note the timers overlap: `whole block` (94 %) contains the GEMVs that
`proj2bit`, `engram` and `mhc_phi4` also count, so do not sum them; the block plus
`sample` is the token.

Per-case decode rates on one image (identical on all three boards, spread 0.07 %):

| case | phase | decode tps | prefill tps |
|---|---|---|---|
| sampling5 / timer60 / batch / heldout_timer45 / heldout_sampling15 / heldout_batch | tools | 4.77-4.85 | 5.25-5.27 |
| **route_translate** | **route** | **4.560** | **4.97** |
| route_code / heldout_route_timer | route | 4.500 / 4.520 | 4.95-4.97 |

`primary` = five tools cases + `route_translate`, and its mean reproduces exactly:
(4.840+4.830+4.770+4.850+4.830+4.560)/6 = 4.780. So the route case alone costs
**~6 %**, and bringing it to the tools rate is **+0.94 % on the metric** - well
above the 0.2 % keep bar, and it is *not* an arithmetic change (the model runs
identically; this is per-request/per-token overhead in the selection path).

What it is **not**: a prefix re-priming cost. `run_inference` restores a cached
prefix per phase (`s_prefixes[phase]`), both prefixes exist, and restore is
O(1). What it also is not: the attention context (route runs one pass, tools two,
so tools has the *longer* context and is still faster).

What is left, in order of evidence: (a) the route grammar's per-token candidate
enumeration / subset-logit construction in `nd_sample.c` - never measured on a
real request, and the phase that the boot bench structurally cannot see; (b) the
per-token console emit (both phases pay it, so it cannot explain the *gap*, but
it is part of the ~8-9 ms/token bench-to-request difference). Measure with
`prof_dump` on a tools request and a `!route` request before writing anything.

Harness caveat for that measurement: a bare `pyserial` reader on the board
console (DTR set, RTS reset pulse) returned **zero bytes** here, even for the
non-mutating `!status\n` - it hung in `open()`. The repo harness and
`tools/serial_api.py` both read that console fine, so drive the harvest through
one of them rather than inventing a third reader.

## The phi anomaly (open, run #140 follow-up)

`mhc_phi4` is 13.5 ms/token = prepare 4.3 + generic-path GEMVs 9.2, for roughly
74 K weights. That is **~12x the per-weight cost of the 2-bit pair-LUT path** and
it survives every analytic explanation tried so far: the tensors are inside the
PSRAM weight tier, `xh` is internal SRAM, the packed rows are sequential, and the
arithmetic floor is ~5 instructions per weight. So either the 12 KB padded
activation is costing far more than its size suggests (re-read once per row, 24
rows per token), or something in the memory path is pathological.

Attempted microbench, blocked by a harness fact worth knowing: `NEEDLE_KBENCH=ON`
adds `bench_phi()`/`kbench.c` to the *same* `app_main`, so a kbench image still
runs the two-prefix priming first, and the firmware's console stream **stalls at
exactly 4096 bytes right after `EVT priming tokens=14`** in a raw
`pyserial`/DTR-deasserted capture - i.e. a plain reader sees the ROM log and the
first EVT lines and then nothing, so you cannot harvest `KB ...` output without
whatever the repo harness does (its 5-minute window and its own console
handling). Do not conclude "the app hung" from an empty capture.

First fix attempt instead of the microbench: `ND_GEMV_BLOCK` (`nd_quant.c`,
opt-in via `NEEDLE_GEMV_BLOCK`, default 0) blocks the generic path's rows so N
rows share one activation sweep. Bit-exact by construction (per-row accumulation
order unchanged). This deliberately revisits "row blocking is a loss", which was
measured on the **2-bit** path where the activation (3 KB) is the *small* operand
against 192 B rows - phi is the inverse ratio (12 KB activation, 1.5 KB rows), so
the assumption changed, which is the only legitimate reason to retry.

## Instruction floor: why the 2-bit GEMV (43 % of the token) cannot go faster

Established from the disassembly and the archive directory, not from a guess
(runs #144-#145). Two floors that agree on the same number:

- **Instructions.** `W8D` - the inner body - is 8 `extui` + 8 `addx4` + 8 `lsi` +
  8 `add.s` = 32 instructions, and consumes a whole 32-bit index word: 8 nibbles,
  each a *pair* of 2-bit weights, so **16 weights per 32 instructions = 2
  instructions per weight**. 14.3 M weights per token x 2 = 28.6 M instructions,
  which at the measured 86.5 ms is a sustained IPC of ~1.33 - the kernel is
  throughput-bound, so instruction count is the lever, and 4 ops per nibble
  (extract, 4x-scaled index, table load, add) is the floor for any table that
  stays cache-resident, because `lsi` can only take base+immediate. Halving it
  needs 4 weights per lookup (an 8-bit index, 1 KB per slot), which was measured
  38 % slower: the table stops fitting.
- **Bytes.** The 2-bit path reads q(576)+k(96)+v(128)+gate(768)+out_proj(768) =
  2336 rows x 768 weights = 1.79 M weights per layer, 14.3 M per token = 3.59 MB
  packed plus ~0.22 MB of group norms. At the ~64 MB/s octal-DTR PSRAM at 80 MHz
  actually sustains through the cache, that is ~59 ms: the measured phase is at
  69 % of bus peak. Fewer bytes means changing quantisation, which is frozen.

Things that follow, and that were each confirmed by measurement rather than
argument: row blocking the LUT path (2-row -5.1 %, 4-row -5 %), the quad table
(-38 %), deeper index prefetch (-1.9 pp), packed-word widths (32-bit is optimal),
and 20+ scheduling nulls all sit on this floor. And the same ratio reasoning says
`kron_apply`, at 1.75 instructions per product with `loop`+`lsi`+`madd.s`, has no
asm headroom either. Do not re-derive any of it; if it changes, it changes with
the archive.

## mHC phi / generic-path family CLOSED (run #141)

`ND_GEMV_BLOCK` row blocking (4 and 8 rows per activation sweep, opt-in knob,
reverted) measured **null**: control 4.7783 (exactly HEAD), B=4 4.7700, B=8
4.7717, all byte-exact. Combined with the geometry and disassembly work in the
same cycle, phi's 9.2 ms is now fully accounted and *not* addressable:

- geometry (archive directory): tensors 223/224/225, `in=3072`, `group=128`,
  `bits=4`, `rowbytes=1536`, out 32/32/128 rows; packed+norms byte accounting is
  exact, and decode touches 24 rows = 36,864 B and 73,728 weights per token.
- codegen: GCC's 4-bit loop is a hardware `loop` of 1 `l32i` + 8 `extui` +
  8 `addx4` + 8 `lsi(cb)` + 8 `lsi(xh)` + 8 `madd.s` = **4.1 instructions per
  weight**. A handwritten kernel's whole ceiling is `lsc`-pairing the `xh` loads
  (-12%) = ~1.1 ms = **+0.5 %**, not worth the ABI risk on top of a 1 KB IRAM
  budget already spent.
- memory: phi is *inside* the PSRAM tier, so it is not a flash stream; 37 KB per
  token cannot stay resident in 32 KB of L2, and 17 KB of free internal RAM
  cannot hold it either. The residual 12x per-weight gap vs the 2-bit path is
  PSRAM line-fill latency on a non-resident working set. Row blocking leaving it
  untouched is exactly what that diagnosis predicts.

Do not re-open: phi asm, phi row blocking, phi in internal SRAM, "phi is
4-bit-slow because of unpacking". If a future archive changes phi's `in_pad` or
`group`, re-measure `mhc_phi4` once and revisit.

## Harness facts worth keeping (learned by losing device time)

- New compile-time knobs: declare them `set(FOO "0" CACHE STRING ...)` in
  `esp32/components/needle/CMakeLists.txt`. `idf.py -DNEEDLE_KBENCH=ON` did
  **not** enable an `option()` in a fresh build dir (binary byte-identical to
  HEAD), while the cache-string form worked the same day. Always verify through
  `compile_commands.json`.
- The asm define is `ND_LUT2_ASM=1` but the option is `NEEDLE_LUT2_ASM`; a guard
  that greps the option name false-fails.
- Fresh-configure **every** board in a batch: a stale board1 build dir dropped
  the asm kernel and read 4.2217 = the C-kernel runtime (-11.7 %). A control that
  reads low is a build-integrity failure, never a result.
- Raw `pyserial` console capture stalls at exactly 4096 bytes right after
  `EVT priming tokens=14` when DTR is deasserted - the USB CDC bridge needs DTR.
  Set `dtr=True` before concluding the app hung.
- `AUTO_PROFILE=1` device profiling is the cheapest way to localise a phase: two
  runs of it located a 65 ms double-count and then excluded it, at ~8 min each.

## External cross-checks
- **Cross-check vs the independent MimiModel engine (memovai/mimimodel, Needle 2
  on ESP32-S3).** Its published optimization log agrees with everything measured
  here and adds two levers this repo had not tried:
  (a) a *request-sized PSRAM weight tier, ordered by profiled projection cost*
      (+2.3% warm latency there; this repo has 14.6 MB free PSRAM and streams
      ~9 MB/token from mmap'd flash at ~30 MB/s);
  (b) *cross-operator scheduling*: running mHC/Sinkhorn/gate work on the second
      core while core 0 does independent work (-5.6% latency there - the same
      family as this repo's kron/silu/lane splits, and the same conclusion that
      only whole stages are big enough).
  Its "what did not work" list independently confirms three of this repo's dead
  ends: int16 PIE assembly (slower - unpack dominates over 2-bit decode),
  linear-space Sinkhorn (underflows), and a two-token blocked CQ2 kernel
  (only 1.11x for a lot of state). Its TIE728 note is about aligned float loads
  + a handwritten 2-row/8-accumulator CQ2 kernel. This repo's C row blocking
  (-0.7%) and packed-word row reads (+6.7%, kept) are not an equivalent test;
  the exact assembly microkernel remains open as Experiment 2.

## Historical ledger of closed lever families (as of run #70, plateau 4.185 tok/s)

This ledger's old "converged/verification only" conclusion predates the active
MimiModel follow-up queue. It closes only the families named below; it does not
close Experiments 2-4 or 7-11. The authoritative next action is at the top of
`.auto/prompt.md`.

Every family below is measured on device with byte-exact goldens. Nothing in
this list should be retried unless its stated blocking assumption changes.

- fp32 staging of every per-token fp16 weight (+100%). DONE - all staged
  (MLP factors, d/b vectors, cond_u/cond_v, d1, qkv taps, engram taps, norm
  scales, conf probes).
- Two-core coverage of every per-layer stage (+12% cumulative): FWHT, pair
  table build, GEMV rows, attention heads, gate, qkv/engram taps, both kron
  halves, SiLU, cond fold, lane mix/pre-combine, zcrms/rms emits, logits
  gather, per-head norms. Nothing smaller than a kron half pays for a
  handshake; dynamic self-scheduling was -6%.
- Packed-word reads (+7.5%): 32-bit is the measured optimum. 64-bit (-4.6%,
  group start is 4 mod 8), 128-bit (illegal, slices not 16-aligned).
- PSRAM weight tier (+1.5%): the 4.49 MB projections+phi span is the ceiling.
  Widening it (10 MB) fails the allocation; a second span costs -1.15%.
- GEMV row blocking: 2-row LUT block -5.1%, 4-row -5%, generic-path 2-row
  neutral. The 24 KB pair table is already cache resident.
- Row order: forward wins; reverse -1.1%, interleaved -0.8%.
- Sinkhorn budget retune: vetoed by the fidelity probe (max_delta 7.8).
- Engram slot gather split: races on m->row / xh aliasing; 0/12 byte-exact.
- Attention: paired softmax won (+2.8%), quads lost (-8.4%); int8 K/V word
  reads banked (+6%).
- Instruction scheduling / loop-overhead removal: ~20 nulls, all within
  +-0.05%. Closed.
- Remaining levers change WHAT is computed (quantisation, vocab, grammar,
  layers, clocks) and are forbidden by the rules.

At run #70, treat 4.185 tok/s (+71.4% over the 2.44 baseline) as converged for
the lever families listed in this historical section. Do not use that conclusion
to skip the later MimiModel experiments tracked in `.auto/mimimodel-experiments.md`.
- **BUILD-INTEGRITY RULE (learned by losing ~10 device runs): `-DCMAKE_C_FLAGS=...`
  on `idf.py build` does NOT rebuild anything.** The flag is already in
  CMakeCache from an earlier configure, so ninja sees no change and relinks the
  SAME binary; several "identical md5 for every variant" results were this, and
  they were then flashed and measured as if they were candidates. Always either
  `idf.py -B <fresh build dir>` per variant (verified: `grep -o
  '-DND_TIER_SPAN_BYTES=[0-9]*u' <dir>/compile_commands.json` shows the flag AND
  the md5 differs), or add the knob as a CMake `option()`/`target_compile_definitions`
  in the component. Cross-check: two builds with different flags that produce the
  same md5 means one of them is not what you think it is.
- **PSRAM tier geometry, fully measured (span from lo_p; content = 4.39 MB):**
  4.5 MB tight copy 4.185-4.1867, **12 MB 4.190-4.195 (accepted)**, 16 MB 4.107.
  Copy order does not matter (ascending == lowest-block-last), and stride
  512/2k/8k/16k/32k are all within noise of the same-span baseline. The one real
  effect: with a span bigger than the content, the LAST bytes memcpy touched are
  the ones the S3's copy engine leaves resident, so a padded span keeps the
  tier's head hot. Do not re-derive this by changing strides again.
- **Tier copy STRIDE at span 12 MB: fully mapped, no lever.** 128/192/256/384/
  512/768/1024/4k/8k/16k/64k and ascending order all give 4.190-4.195 with the
  byte-exact gate green; the accepted plain single memcpy at the same span is
  indistinguishable. Keep one memcpy, keep the 12 MB span. The 14 MB span does
  NOT boot: a 14 MB allocation succeeds but the tier copy then exceeds the bench
  harness's window (no EVT ready at all, twice). Family CLOSED.
- **Tier span ceiling is the ALLOCATION, not cache: 12 MB is the maximum that
  boots.** 13 MB and 14 MB never reach EVT ready (clean rebuilt images, two
  boards, two spans); 12 MB boots everywhere and reads 2,052,252 B free. The
  earlier "16 MB boots at 4.107" reading came from a stale pre-reset tree.
  Conclusion: the span is at its ceiling and stride/order/limit variants cannot
  add value. The 12 MB span + single memcpy is final.
- **Experiment 5 (async cross-operator overlap) - CANNOT WORK with this job
  slot, and the tripwire proves it quantitatively.** Overlapping the pair-table
  build (the only per-layer job whose inputs are ready and whose output is not
  consumed by a split) left it in flight until attention, and the very next
  `nd_parallel_rows` fired the guard 3543 times in one run: the table is 16 KB,
  its build is ~2 us, and the q/k/v GEMVs that follow each take ~30 us - the job
  is over before it could ever hide anything, so there is no schedule that both
  overlaps and avoids the slot collision. The gate-projection variant of the
  same idea was already shown to race the attention-head split on m->gate.
  Two independent schedules, two structural failures: with row-level splitting
  already covering every GEMV, there is no independent per-layer work left for a
  second core. Closed.

## Experiment 14 - CQ2 integer path: MEASURED, CLOSED as rejected (run #242, no device run)

Hypothesis tested: a CQ2 dot in integers (int8/int16 activation x the 4-value codebook,
int32 accumulate, fp16 row norm at dequant) could replace the float pair-table kernel.
Executed against the real captured fixture, in the recipe's order, and killed at the
numeric stage - so the recipe's step 3 (device speed screen) never opened and was not run.

**Fixture (real, not synthetic).** `-DND_EXP_CAPTURE` hook extended with `nd_cq2_capture`
(`host/nd_dump.c` + one `#ifdef` call in `attention()`), capturing layer-0 `q_proj`: prepared
activation, the pair table the kernel used, the kernel's full output vector, and 32 spread
rows' packed bytes + fp16 group norms. 4 files x 8 records from the prefill and decode
phases of two frozen primary prompts (`sampling5`, `timer60`), 1024 scored rows. Real
geometry is **576x768** (q_proj out = n_heads*qk_head_dim), group 128, 32 packed bytes and
6 norms per row - the recipe's "768x768" was wrong; directory record 2 confirms 576x768,
117,504 bytes. Harness validated: replaying the shipping row walker on the captured table
is **bit-exact on 1024/1024 rows**, so every error below belongs to the integer path.

**Numeric screen (`.auto/exp14_screen.c`, float replay self-checked in the same run).**
mean|y| over the scored rows is 1.771, so these are relative-scale numbers:

| variant | max_abs | rms |
|---|---|---|
| A  int8 activation (tensor scale) x int8 codebook -> int32 | 4.379e-02 | 1.235e-02 |
| Apg same, per-FWHT-group activation scale | 4.315e-02 | 9.739e-03 |
| Axc int8 activation, codebook kept exact | 4.310e-02 | 1.192e-02 |
| A16 int16 activation x int8 codebook | 1.222e-02 | 2.788e-03 |
| B  boot-time int8 expansion of the rows | = A exactly (asserted) | = A |

Per-group scales buy 21% of the rms and nothing else. Axc ~= A proves the error is the
**activation**, not the codebook, so no codebook treatment rescues it. B is algebraically
A (stored byte = round(cb*norm/(|norm|*max|cb|/127)) = the int8 codebook level, dequant
scale |norm|*max|cb|/127), checked equal in the screen: it differs only in bytes.

**End-to-end consequence (host forward-probe gate, the campaign's own measure).**
Control on this tree reproduces the accepted 5.341e-05 / top1 10/10. Substituting
**one** tensor - layer-0 `q_proj`, 117,504 B = 0.73% of the model's CQ2 bytes:

* int8 path: `logit_max_delta = 0.3282` = **164x the 2e-3 gate**, FIDELITY FAILED, top1 still 10/10.
* int16 activation + exact codebook (best case of the whole family): `0.01818` = **9.1x the gate**, FAILED.
* host goldens for the int8 substitution: **13/13 byte-exact, token_delta 0** - the golden
  suite is blind to a representation change that moves logits by 0.33. Rule #1 of #230,
  now demonstrated on a whole path, not a primitive. The remaining 45 CQ2 tensors are
  11.84 MB of the 11.96 MB CQ2 stream (74.1% of the 16,143,248 B model), so a full
  integer model is far outside the gate, not marginally.

**Bytes / staging / capacity (variant b).** CQ2 = 46 tensors, 11,959,296 packed bytes,
45,023,232 weight elements. Int8 rows = 45,023,232 B = **3.76x** the stream: it does not fit
16 MB PSRAM, nor the 12 MB tier, and per decode token it would raise the weight stream from
11.96 MB (59 MB/s at 4.94 tok/s) to 45.0 MB (223 MB/s). Capacity-dead before it is speed-dead.
**Quantisation cost/token**: 768 activations per reduction axis (q/k/v/gate share one prepared
activation) + 4 codebook values (6 scales/axis if per-group) - negligible, and irrelevant.

**Byte-exactness, stated as the recipe demanded:** no integer dequant reproduces the float
multiply-add chain bit-for-bit, so 14/14 device + 13/13 host byte-exact goldens can never be
met by an integer path at any width. The campaign refuses a model-quality change, therefore
the path is rejected independently of speed; the fidelity numbers above show it would also
have failed on its own merits. Not run: kbench `bench_int()` vs `nd_lut2_rows_tie1n`
(recipe step 3, gated on the numeric screen). `dsps_dp_s8_aes3`: unavailable - esp-dsp is in
neither the tree nor IDF and adding it is a forbidden new dependency (#230 precedent);
recorded, not silently skipped. No image was built, so no hashes and no board assignment.

**Reproduce.** Fixture (4 x 8 records, the real activations and the kernel's own outputs) is
kept gzipped in `.auto/exp14/`, with the capture hook as `.auto/exp14/capture-hook.patch`
(`git apply` it, then `cmake -S host -B /tmp/hostcap14 -DCMAKE_C_FLAGS=-DND_EXP_CAPTURE`,
`ND_CQ2CAP=... ND_CQ2SKIP=<prefill length> nd_dump model/needle3.cact genp
tools/demo-tools.json "<primary prompt>" 128 nothink`), and the screen is
`.auto/exp14_screen.c` (`cc -O2 -ffp-contract=off -Iengine/include .auto/exp14_screen.c
host/build/libneedle_engine.a -lm`; exit 0 only if the float replay is bit-exact). The
integer engine probe itself is NOT kept - it is a dead path, and its only durable content is
the two fidelity numbers above.

**Harness trap bought by this run (keep).** The first probe reported the control's value
exactly (5.341e-05) because the call-site edit inserted the integer call *before* the
shipping `nd_cq_gemv_lut2` line, which then overwrote the result. `nm` showed
`nd_cq_gemv_i8` present and the build log was clean, so symbol presence proved
compilation, not execution. Verify an engine probe by differential output, or by deleting
the original line behind `#ifdef/#else` - never by `nm` or a green build.

## Experiment 14 recipe (executed above; kept as the record of what was specified)

Goal: decide the CQ2 **integer** path on measurement, in this order, and stop as soon as a stage
fails. Screen in **C**, not asm - Experiment 13 measured `asm volatile` costing 2x on this core.

1. **Fixture (host, ~10 min).** Extend the existing `-DND_EXP_CAPTURE` hook (`host/nd_dump.c` + the
   `#ifdef` in `engine/src/nd_model.c`, pattern already proven) to dump, for layer 0's `q_proj`
   only: the prepared activation `xh` (768 floats, 3 KB), the 16-entry pair LUT, the packed row
   bytes and the group norms for a few rows, plus the float kernel's output. Run one real primary
   prompt through `/tmp/hostcap/nd_dump ... genp`. This is the "captured prepared activations" the
   experiment demands; do not substitute synthetic ones (#149's 3x over-estimate came from that).
2. **Numeric screen (host, cheap).** Implement both prototypes against that fixture: (a) int8
   activation x int8 4-value codebook into int32, then tensor-wide *and* per-group scales then the
   real row norm; (b) boot-time expansion of one tensor's codebook values to int8 rows in PSRAM
   (model the bytes, not the run). Report max/mean abs and relative error versus the float kernel's
   real output, staging bytes and quantization cost per token. Kill here if the error cannot plausibly
   satisfy `logit_max_delta <= 2e-3` *and* byte-exact goldens - note byte-exactness is the binding
   constraint, not the 2e-3 probe: an integer path is not bit-exact, so it needs the campaign to
   accept a quality change, which it currently refuses. State that consequence explicitly in the
   disposition either way rather than discovering it after a device run.
3. **Speed screen (device, kbench, only if 2 survives).** Add `bench_int()` with rotated real buffers
   (the microbench rule from #230: distinct operand buffers, consume every result, `asm volatile` is
   a fence). Control is `nd_lut2_rows_tie1n` on the real 768x768/576x768 shapes and the split mode,
   not the C row walker - it is 13.45% faster than C (measured #204). Report cycles and bytes read.
   `dsps_dp_s8_aes3` is unavailable for the same reason as `dsps_dotprod_f32_aes3` (#230): esp-dsp is
   not in the tree or IDF and is a forbidden new dependency - record it, do not silently skip it.
4. **Decision rule.** Integrate only if it is both faster than `tie1n` *and* passes 14/14 device +
   13/13 host byte-exact, token delta 0, fidelity, top1 10/10. Otherwise write the numbers and move
   to Experiment 15 (GDMA double buffering) - which is a memory-system test, not a schedule test.

## Two rules bought by real failures (runs #230-#231)

**1. Differential-test numeric primitives against the PREVIOUS implementation, verbatim.** The
exponent-field-insertion change to `nd_expf` passed new-pair-vs-new-scalar on 4,000,000 pairs and
also would have passed every golden case - and it was *wrong* for `x` near -87.68, disagreeing with
the shipping kernel by ~1.7x, because `p` in [0.61,1) has unbiased exponent -1, so exactness needs
`k >= -125`, not `-126`. Copying the old function into the test and diffing bits over a dense sweep
(80,184,321 comparisons, clamp edges included) found it in one run. **The 14/14 + 13/13 byte-exact
gate cannot catch a numeric bug in an input range the frozen prompts never reach**, so "goldens are
exact" is not evidence about a math primitive. Keep `/tmp/expref.c` as the pattern (or re-derive it
from `git show HEAD:engine/include/nd_quant.h` if it is gone).

**2. Trading an FPU op for integer bit math is not free on this core, and a guard is not free
either.** Replacing `bits -> memcpy -> FP multiply` with `memcpy -> int add -> memcpy` measured
**-2.10 % decode** (4.8117 vs 4.9150, three boards, byte-exact), with prefill, think, extended and
the boot bench all moving the wrong way: float<->int transfers still go through memory, so no move
disappeared, while the correctness guard added compares and a branch to straight-line code - and in
`nd_expf_pair` the branch also destroyed the interleaving that Experiment 12's +20 % came from. The
exp scale construction is now measured and closed; do not try the integer-tail idea again in either
function.

**Harness:** `pkill -f needle-api` does **not** stop the API - it spawns
`tools/serial_api.py --serial ...` as a child, which keeps `/dev/needle-pi/console` and makes the
next `needle-board run 1` fail with "Board 1 is busy" (it cost a control slot in
`20260921T222845.825872Z`). Kill `serial_api.py` by pid, and confirm no `serial_api` process
remains before releasing a board after `make capture`.

## Experiment 17 (IRAM placement) - measured, closed as a lever (run #232)

Symbol-table audit of the accepted image (addresses, not assumptions): `attn_heads`,
`kron1_blocks`, `kron2_rows`, `nd_lut2_rows_c`, `nd_lut2_rows_tie1n` are all `0x4037xxxx` = IRAM
already. The flash-mapped (`0x42xxxxxx`) set is `nd_gstate_byte` (1971 B, aggregate 0.06 ms per
run #149 - worthless), `pool_cell` (503 B, ~0 ms), `nd_sample_hidden` (312 B), `nd_cq_prepare`
(107 B), `nd_tok_piece` (37 B), `kron_apply` (62 B wrapper over IRAM loops),
`nd_model_logits_subset` (58 B wrapper).

Moving `nd_sample_hidden` to IRAM (`ND_HOT`, verified `0x420114d8 -> 0x4037c3c0` by `nm`, so the
sampler driver now sits beside its already-resident `lex_words` split kernel) measured **+0.067 %
decode** (4.9183 vs 4.9150), +0.12 % extended, every other monitor identical to the tick,
byte-exact 14/14 + 13/13, **-256 B internal RAM**. One metric tick is inside the 0.071 % spread
that three byte-identical images showed, so it is not confirmable and it is under the 0.2 % bar:
rejected, reverted.

Conclusion: **placement is not a lever here** - the loops that own time are resident, and what is
left in flash is either a wrapper or so small/cold that the L1 instruction cache already covers it.
Any remaining single-function candidate is <= 0.1 % by this measurement, so do not burn a build per
function. Note also that `internal_free` (15503) is now the binding constraint on placement ideas:
IRAM has to be paid for out of the same pool as data scratch.

## Experiment 15 - operator-internal GDMA double buffering: MEASURED, CLOSED as rejected (run #287, three boards)

Hypothesis tested: the AHB GDMA could prefetch the next sequential PSRAM weight block
into one of two small internal DMA buffers while TIE728 consumes the current one, so
the dominant CQ2 GEMV stops paying PSRAM latency at the cache. This is the memory-system
test, not the rejected schedule test. Measured in `kbench` (`bench_gdma`, kept in
`.auto/exp15/kbench.c.with_gdma_bench`, raw device output in `.auto/exp15/board{1,2,3}.gdma.txt`),
real `needle3.cact` bytes: the 768x768 CQ2 tensor staged into PSRAM exactly as the tier
stages it (147,456 B), blocks of 10 rows (1,920 B, the 2 KiB budget) and 21 rows
(4,032 B, the 4 KiB budget), the shipping `tie1n` kernel, 20 blocks swept per round,
5 rounds, min-of-rounds reported, CCOUNT calibrated 24,000 cycles/us.

**All three boards agree to under 0.1 %** (image hashes 008116dd…, e335c7b5…, 27c16c8c…;
each worker rebuilt fresh, `kbench_compile_lines=3 asm_sources=4`, `nd_lut2_rows_tie1n`
resident in IRAM). Numbers below are board 1 medians, cycles per block.

| mode | 2 KiB block (10 rows) | 4 KiB block (21 rows) | what it bounds |
|---|---|---|---|
| `copy_raw` submit-then-wait, one at a time | 19,381 (23.8 MB/s) | 25,766 (37.6 MB/s) | copy cost, no overlap |
| `copy_pipe` two in flight, **no compute** | 12,845 (35.9 MB/s) | 15,693 (61.7 MB/s) | copy ceiling |
| `comp_psram` row walker, cached PSRAM | 18,307 | 43,165 (22.4 MB/s) | **the shipping path** |
| `comp_int` same rows, operands in internal RAM | 18,287 | 38,332 (25.2 MB/s) | the ceiling: +0.11 %, **+11.2 %** |
| `overlap` prefetch b+1 while consuming b | 30,733 | 50,914 | **the proposal: -67.8 %, -17.9 %** |
| `wait` blocked on the done semaphore | 281 (0.9 % of overlap) | 554 (1.1 %) | the copy is *not* the stall |

Every overlapped row was **bit-exact** against the PSRAM path (`int_rows_mismatch=0`,
`dma_rows_mismatch=0`), so this is a speed verdict, not a correctness one.

**Why it fails, in the order the evidence rules things out.**
1. *It is not the wait.* The consumer blocks 281 of 30,733 cycles per block, so the copy
   really is hidden. The overlapped loop is slower than the direct loop because of what
   the two extra in-flight PSRAM reads do to the shared octal bus, not because the DMA is
   slow to finish. Per block, compute alone is 18.3k cycles and the copy alone is 12.8-15.7k;
   together they cost 30.7k, i.e. they **serialise on the memory system** instead of overlapping.
2. *The ceiling is not worth reaching.* `comp_int` is the "weights already in internal RAM"
   bound: +0.11 % on the 2 KiB block. Only at 4 KiB, where 20 blocks (80 KB) overflow the
   32 KB data cache, does residency buy anything (+11.2 %) - and that gain belongs to the
   *cache*, which already has it: the 12 MB PSRAM tier plus the L1 D-cache is exactly this
   mechanism, already measured at its own ceiling (tier family closed: span, stride, order,
   allocation limit).
3. *The bandwidth is not there to steal.* GDMA's best sustained rate - 61.7 MB/s at 4 KiB
   with two transactions in flight and nothing else running - is the same order as the
   ~64 MB/s the cached path already sustains for the whole 3.59 MB/token weight stream. A
   prefetcher that must share the bus with the consumer cannot deliver a free second stream.

**Cache/DMA coherency, checked explicitly, both directions.** Destination first dirtied
by the CPU (`memset` 0xA5) and read back so its lines are in the data cache, then
overwritten by GDMA, then read again: `stale_without_invalidate=0` and
`stale_with_invalidate=0` at both sizes on all three boards - on this access pattern no
stale bytes were observed either way, so the invalidate bought nothing measurable here.
Two hazards found, both hard:
* **Doing the invalidate correctly is impossible inside the loop.** `esp_cache_msync()`
  briefly disables the cache, and the async-memcpy completion chain (IDF's
  `esp_async_memcpy.c` plus the done callback) is mapped from flash, so a GDMA done ISR
  landing in that window fetches code from a disabled cache: `Guru Meditation Error:
  Core 0 panic'ed (InstrFetchProhibited)`, **identical PC 0x4020c4df and identical
  backtrace on two boards**, at the first submit after the coherency test. A shipping
  design would need the whole callback chain in IRAM, which the campaign's 1 KB IRAM
  budget (spent on `lut2_tie728.S`) cannot pay.
* **Even without a transaction in flight, the invalidate costs 1.416 M cycles**
  (`msync min 1,416,510 / max 1,416,880`, and a single 64-byte call costs the same
  1,416,51x) - a fixed ~5.9 ms, size-independent, reproducible across all three boards to
  the last digits. Cause not chased (nothing in `engine/` or `esp32/main/main.c` calls
  cache maintenance - verified by grep - so the accepted runtime is untouched by this),
  but it alone ends the experiment: 76 blocks/token x 5.9 ms is not a price any 2 % win pays.

**Heap impact (measured, would-be shipping cost).** A pair of DMA buffers costs 4,600 B of
internal heap at 2 KiB and **8,728 B at 4 KiB** (allocation overhead included), plus the
80 KB internal window needed for the ceiling control. The accepted runtime has
**15,247 B internal free**, so the 4 KiB pair alone consumes 57 % of the remaining internal
RAM for a measured -17.9 %.

**Disposition: not integrated; `esp32/main/kbench.c` and the kbench-only CMake
requirement reverted; main tree back at the accepted 4.9417 runtime.** No canonical
decode measurement was spent on it - the proposal lost in the microbenchmark on three
boards before it could touch the request path, so a 13-minute flash would have measured
the control. `decode_tps` therefore stays at the accepted 4.9417 (unchanged, not
re-measured this run). Memory-system family now closed from both sides: cross-operator
worker overlap (#133, #213), tier geometry, and operator-internal DMA prefetch.

**Harness facts bought by this experiment.**
* `nd_lut2_rows_*` index `packed`, `norms` **and** `y` by the row number they are handed.
  Sweeping row blocks with a block-local payload buffer must offset `packed` and `norms`
  independently and call the kernel with `(0, rows_blk)`; the first version passed the
  absolute row base and wrote `y[750]` into a 10-float buffer - heap corruption, later a
  crash. Symptom to remember: a *bit-exactness* failure (10 rows/round) that appeared only
  on the internal-RAM path.
* Never `git stash -u` inside a board worker: the untracked `.venv` symlink and
  `.board.json` are infrastructure, and taking them away costs that board's slot in the
  batch (`timeout: failed to run command '.venv/bin/python'`). Restore with
  `git show 'stash@{0}^3:.board.json' > .board.json` and re-create the venv symlink.
* `esp_async_memcpy` needs `esp_hw_support` + `esp_mm` in the component's REQUIRES
  (`esp_cache.h` is provided by `esp_mm`); adding them to the kbench-scoped block keeps
  the shipping component's dependency list untouched.

## Experiment 16 - compact first-byte grammar index: MEASURED, KEPT, +1.01 % decode (run #288, 4.9417 -> 4.9917)

**Hypothesis.** The constrained sampler decides which vocabulary pieces the current
grammar state can spell by examining all 8,176 pieces every decode token (a
`nd_tok_piece` call plus a first-byte table test each). Index the vocabulary by first
byte once - ascending ids per byte, counting sort, plus the list of the byte values
that actually start a piece - and per token walk only the buckets whose byte the state
accepts. Legality itself is untouched: every visited id still takes the same
`token_ok` byte walk, results still land in the same one-bit-per-id bitmap, and the
caller still reads it in ascending id order, so the candidate list, the argmax and its
tie-breaking are the shipped ones.

**Off-device screen first (`.auto/exp16/`, host build behind `-DND_SAMPLE_STATS`).**
All six primary prompts, 93 real decode steps: the grammar admits a **median of one
first byte** per step (max 11), only **94 of 256** byte values ever start a piece, and
the bucket walk touches a **median of 51 ids against 8,176** - **1.62 %** of the
shipped walk in aggregate, with **zero** steps above half of it. The bucket
enumeration reproduced the shipped candidate list **element for element and in
ascending order on all 93 steps**. That is what justified board time, and it is also
why this is not the rejected run #149 (which kept all 8,176 iterations and only
shortened each one, measuring -0.17 %): here 98 % of the iterations disappear, and the
two-core split the walk needed at that depth (`nd_parallel_rows`, run #176's +0.20 %)
is dropped because ~50 ids do not pay for a handshake.

**Three-board batch (fresh builds, board 1 pristine control), batch `20260922T052753.408423Z`.**
All three workers ran at 1000 Hz because their `esp32/sdkconfig` is gitignored and so
survives `git reset --hard` - the control read 4.9250, exactly the pre-tick accepted
value, which both proves the drift and confirms the control is otherwise healthy.
At that identical configuration: control **4.9250**, candidate **4.9733** and
**4.9767** (+0.98 % and +1.05 %), both `device_output_exact=14/14`,
`device_token_delta=0`. Images 84276dd4… / 7564bb66… vs control 2a54c156….

**Canonical confirmation on the original board at the shipping configuration** (tick
100 Hz, 64 B data cache line, `sdkconfig` deleted and regenerated, image 280320 B,
md5 dcce6a62…, `lex_index_build` in the map at 0x42011234):
**decode 4.9917** = +1.01 % over 4.9417 and +104.6 % over the 2.44 baseline,
extended **4.9129** (+1.06 %), think 3.92, prefill 5.265 (unchanged), **min_case 4.77**
(previous best 4.72, so the worst case improved with the mean rather than trading
against it), boot bench 5.039 / 198 ms/token **unchanged** - the same request-path-only
signature and internal control that run #147 produced, because the bench calls
`nd_model_step_hidden` and never samples. `14/14` device and `13/13` host byte-exact,
`token_delta 0`, fidelity `5.341e-05` (gate 2e-3), top1 `10/10`, `make test` green
(grammar + prefix isolation), and `make capture` rc=0 with all nine behavioural flags
true (routes, tools, two-pass local execution, no external calls, telemetry, sampling
interval, timer expiry). Memory: `internal_free` 15,247 -> **15,215** (32 B for the
index handle); the ~17 KB index itself is `ND_ALLOC`ed in PSRAM (2,052,252 B free at
boot; allocated on the first constrained sample, so it is not in that boot-time number),
and allocation failure falls back to the original full walk, so the index is an
optimisation rather than a dependency.

**Why it paid more than the screen implied.** The screen priced the arithmetic
(~1.5 ms of piece lookups); the delivered +2.05 ms of saved token time also includes
the per-token handshake that the split used to pay and the work both cores did on the
256-entry first-byte probe (two builds of a 256-step grammar walk per token, one per
core, replaced by one walk over the 94 bytes that can actually start a piece). Lesson:
a phase built as `nd_parallel_rows` carries a fixed handshake plus duplicated per-core
setup, and removing the *need* for the split can be worth more than removing the work.

**Boundary for future ideas in this phase.** The sampler's remaining cost is now
dominated by `nd_model_logits_subset` (5.7 ms, a legitimate row count at the 2-bit
GEMV floor) and by `token_ok`'s byte walk over the ~50 visited ids - the enumeration
itself is no longer a lever. The index is keyed on (tokenizer, vocab) and rebuilt if
either changes; it covers vocab <= 8,192 (one pass of the bitmap), above which the
chunked walk remains.

## Experiment 19 - 120 MHz octal memory: MEASURED as a diagnostic, +3.51 % real, NOT shippable (run #289, two boards + two controls)

Question asked: is the remaining CQ2 token limited by the external-memory clock? Answer,
measured: partly - a 1.5x MSPI clock buys **+3.51 %** end to end, so roughly 3.5 % of the
token is clock-limited and the rest is at the instruction floor the campaign already
mapped. This is a *diagnostic*, per the campaign's own rule that anything above the
documented 240 MHz / 80 MHz octal flash+PSRAM is forbidden as a means of going faster;
nothing was integrated and the shipping tree is unchanged at 4.9917.

Two configurations were tried, both with `IDF_EXPERIMENTAL_FEATURES=y`:
* **PSRAM 120 MHz with flash left at 80 MHz: cannot be built.** The concrete blocker is a
  compile-time one, not a judgement: `static assertion failed: "FLASH and PSRAM Mode
  configuration are not supported"` from `esp_common/esp_assert.h` in the esp_psram
  configuration. On ESP32-S3 flash and PSRAM share the MSPI clock tree, so only the
  combined step exists. Recorded as the exact blocker rather than as an opinion.
* **Flash and PSRAM both 120 MHz (octal DDR):** built and measured on two boards, with two
  80 MHz controls in the same concurrent batch (batch `20260922T060142.676310Z`, candidate
  on the canonical board 1, controls on boards 2 and 3):

| metric | 80 MHz control (both boards, identical) | 120 MHz board 1 | 120 MHz board 3 (earlier batch) |
|---|---|---|---|
| decode_tps | **4.9917** | **5.1667** (+3.51 %) | 5.1667 (+3.51 %) |
| prefill_tps | 5.265 | 5.4517 | - |
| ext_decode_tps | 4.9129 | 5.0814 | - |
| think_tps | 3.92 | 4.05 | - |
| min_case_tps | 4.77 | 4.93 | - |
| boot bench | 5.039 (198 ms/tok) | 5.213 (192 ms/tok) | - |
| internal_free | 15,215 | **14,031** (-1,184 B) | - |
| device byte-exact | 14/14, delta 0 | 14/14, delta 0 | 14/14, delta 0 |

Two independent boards at 120 MHz agree to the last digit (5.1667) and both controls agree
to the last digit with the accepted runtime, so the delta is the clock, not the board.
Memory integrity held under the full 14-case suite on both 120 MHz boards (byte-exact,
token delta 0) - which is the *only* integrity evidence this configuration has, and it is
not enough: it is one thermal condition.

**Why it is not shippable, stated precisely rather than as a reflex.** IDF's own Kconfig
help for `SPIRAM_SPEED_120M` in octal mode: "Octal PSRAM 120 MHz is an experimental
feature, it works when the temperature is stable. Risks: if your chip powers on at a
certain temperature, then after the temperature increases or decreases by approximately 20
Celsius degrees (depending on the chip), the accesses to/from PSRAM will crash randomly."
That failure mode is precisely the axis a 30-minute soak cannot cover, so a soak would buy
false confidence rather than safety: cold-boot-then-warm and warm-then-cold are different
cases from warm-then-warm. The campaign rule (documented maxima only) therefore stands.
What a real stability campaign would need, if the owner ever decides the 3.5 % is worth the
risk: `SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR` (IDF's real-time timing retune,
which depends on octal 120 MHz + experimental) plus thermal-cycling integrity tests across
the whole specified range, not a single-room soak - and the 1,184 B of internal RAM the
tuning path costs would have to come out of 15,215 B.

**What the number is for.** It bounds the memory-clock share of the token at ~3.5 %, which
is one more reason the 2-bit GEMV family is closed: even a 50 % wider MSPI clock moves the
whole token by 3.5 %, while that phase is 43 % of it - i.e. the phase is not waiting on
bytes, it is waiting on the 2 instructions per weight floor. Any future claim that the
kernel is bandwidth-bound has to beat that ratio.

## Paired elementwise sigmoid: KEPT, +0.200 % decode (run #290, 4.9917 -> 5.0017)

The first candidate after the named queue closed, and it came out of the phase map rather than the
list. `sigmoidf_` is implemented on `nd_expf` (degree-5 Horner, ~99 cycles, latency far longer than
its work), and two loops run it elementwise over thousands of *independent* values per token:
`silu_rows` (the Monarch MLP's gated stage, 1024 x 8 layers) and `agate_rows` (the attention output
gate, 768 x 8). Experiment 12 had only ever paired the *attention* softmax, so this mass was
unpaired. Interleaving each adjacent pair through the already-shipped `nd_expf_pair` recovers part
of that +19.96 %.

**Bit-exact by construction, and tested as a primitive, not only by goldens** (rule #1 after the
`nd_expf` exponent-field bug): the pair is used only when both elements take the SAME branch of
`sigmoidf_`, so each element's exponential argument is exactly the one the scalar path would have
passed (`-x` for x >= 0, `x` otherwise), the two divisions keep their scalar form, mixed signs fall
back to two scalar calls, and `nd_expf_pair` itself falls back to scalar outside its exact range.
Differential test against `sigmoidf_` copied verbatim: **1,600,202 comparisons, 0 bit mismatches,
max_abs 0.000e+00**, over a +-400 dense sweep plus the branch boundary, +-0, subnormals, clamp edges
and +-1e30 (`-ffp-contract=off`, test kept in `.auto/sigpair/test.c`). Chunks are 128 wide so a pair
never straddles the two cores; in `silu_rows` both operands are loaded before either output is
written because `a` is read-modify-written.

**Measured.** Three-board batch: control (board 1) exactly 4.9917, both candidate boards exactly
5.0017; canonical confirmation on the original board at the shipping configuration also 5.0017,
byte-exact 14/14 + 13/13, token_delta 0, fidelity 5.341e-05 identical to the control, top1 10/10,
`make capture` rc=0 with all nine flags true. Cost +1,200 B flash, **zero internal RAM**.
Monitors: extended +0.12 %, prefill +0.16 %, min_case 4.78 (best worst-case seen), boot bench
5.046 - and the bench moving *is* the mechanism check, because unlike the sampler work this phase is
inside the bench. think_tps unchanged, so the 4-bit/vocabulary path is not harmed.

**Prediction vs measurement, again.** Arithmetic said ~+0.8 % (7,200 pairs x 49 saved cycles);
delivered +0.200 %, i.e. a quarter of it. Third time this campaign has measured the same thing
(#12: half, #288: more than priced): pairing removes *exposable* latency, and how much of a
kernel's latency is still exposed depends on what the scheduler had already hidden. The price is
one build, so a priced +0.2 % candidate that is bit-exact by construction is still worth spending -
but do not price a pair at its full cycle count a fourth time.

**Harness fact (cost a confusing "undefined reference").** A test source placed in `/tmp` picked up
a stale `/tmp/nd_quant.h` because a quoted `#include` searches the *including file's* directory
before `-I` paths; the test then failed to link in a way that looked like a problem with the engine.
Keep test sources inside the repo tree (`.auto/sigpair/`) and delete scratch headers.

**Open follow-up in the same family, priced but not spent.** The remaining unpaired transcendental
mass is the 4x4 Sinkhorn: ~1,280 `nd_expf` plus ~1,280 `logf` per token across the 8 layers. The
`logf` half is the expensive half and cannot be paired with `nd_expf_pair`; pairing only the exp
half inside a row/column sum is bit-exact *if* the partials are added back one at a time in the
shipped order, which the loop already does. Expected well under 0.2 %, so screen it off-device
before any board time.

## FINAL EVIDENCE TABLE - campaign closed at 5.0117 decode tok/s (+105.3 % over the 2.44 baseline)

Updated after runs #291-#301. Two independent bounds (clock, instruction floor) plus three measured
closures added since: the Sinkhorn `logf` skip (13.58 % hit rate, 787-cycle break-even), the software
prefetch family (no `pref`/`dpref` opcode, `__builtin_prefetch` emits nothing), and the cache-config
family (closed by the S3 Kconfig itself - 32 KB I-cache and 32 B instruction line are the maxima).
Run #296 measured no overfitting signature on three unseen prompts (5.080 / 5.020 / 4.790 tok/s
against a 5.0117 primary and a 4.79 worst case), and runs #296-#301 closed five ways in which the
campaign's own harness could report success while verifying nothing. The accepted runtime's own
quality evidence is unchanged: 17/17 device + 16/16 host byte-exact, token_delta 0, fidelity
5.341e-05 against a 2e-3 gate, top1 10/10, `golden_missing=0` on both sides, six gates each with a
demonstrated way to fail.

Accepted commit `e6f2e0d`. Quality frozen and verified at every step: 14/14 device + 13/13 host
byte-exact generations, token_delta 0, fidelity 5.341e-05 against a 2e-3 gate, top1 10/10, prefix
isolation green, `make capture` rc=0 with all nine behavioural flags true.

| lever | family | delta | state |
|---|---|---|---|
| fp32 staging of the Monarch factors (run #2) | representation | +99.9 % | kept |
| TIE728 2-bit pair-LUT kernel (Expt 2-4) | instruction floor | +13.0 % | kept |
| PSRAM weight tier, 12 MB span | memory path | +1.5 % | kept |
| two-core coverage of every GEMV/head/stage | parallelism | +12 % cumulative | kept |
| packed 32-bit weight-word reads | memory path | +7.5 % | kept |
| first-byte legality table in the sampler (run #147) | request path | +2.16 % | kept |
| compact first-byte grammar index (Expt 16, run #288) | request path | +1.01 % | kept |
| paired attention softmax exp (Expt 12, run #229) | elementwise ILP | +0.48 % | kept |
| 100 Hz FreeRTOS tick (run #240) | build config | +0.34 % | kept |
| exact KV reciprocal (Expt 18) | elementwise ILP | +0.204 % | kept |
| paired elementwise sigmoid, SiLU + attn gate (run #290) | elementwise ILP | +0.200 % | kept |
| 64 B data cache line (run #238) | build config | +11.9 % load-bearing | kept |
| 120 MHz octal flash+PSRAM (Expt 19) | memory clock | +3.51 % | measured, FORBIDDEN to ship |
| CQ2 integer path (Expt 14) | representation | fidelity 164x over gate | measured, rejected |
| operator-internal GDMA prefetch (Expt 15) | memory path | -17.9 % to -67.8 % | measured, rejected |
| ESP-DSP-style dot schedules / 128-bit asm dot (Expt 13) | instruction floor | -46 % to -107 % | measured, rejected |
| IRAM placement audit (Expt 17) | placement | +0.067 % | measured, rejected |
| row blocking, row order, quad tables, ~25 scheduling nulls | instruction floor | within +-0.15 % | closed |

Two independent bounds now say why nothing above the bar is left untried:
* **clock bound (Expt 19):** a 50 % wider MSPI clock moves the *whole* token by 3.51 %, so the 43 %
  2-bit GEMV phase is not waiting on bytes.
* **instruction bound (runs #144-#145, confirmed by Expts 13/15/19):** `W8D` is 32 instructions per
  32-bit index word = 2 instructions per weight at IPC ~1.33, and every attempt to change the
  schedule, the load width, the accumulator count or the DMA delivery lost.

**Corrected premise for whoever picks up the last open candidate.** The mHC phi (4-bit generic path,
13.5 ms/token) was declined partly because "the 1 KB IRAM budget is already spent on
`lut2_tie728.S`". That is not what the map says: `.iram0.text` = 0x1124f (~70 KB) inside
`iram0_0_seg` = 0x53700 (~342 KB), so IRAM *space* is ample. The real currency is internal **heap**:
IRAM text is subtracted from it, and `internal_free` is 15,215 B. A ~1.5 KB handwritten 4-bit kernel
would cost ~10 % of the remaining heap, not an impossible amount. Its measured ceiling is +0.5 %
(`lsc`-pairing the `xh` loads, -12 % of that phase), so it is the only remaining candidate above the
0.2 % bar - and it is assembly, so it needs the rule-#1 differential test plus the real-capture
fixture, in a fresh working window. Do not start it without both.

## CLOSED ON MEASUREMENT: skip exp(0) in the attention softmax pairs - REJECTED, -0.43 % (run #292)

Priced at ~+0.6 % off the real 49,152-pair capture (48.5 % of pairs carry one exactly-zero
argument) and proven bit-exact before any board time (357,792 elements, 0 mismatches, plus
host gates green). The device said no: canonical 4.9900 vs the accepted 5.0117, every monitor
down together (extended -0.47 %, prefill -0.44 %, boot bench -0.53 %), and it cost 1,024 B of
internal RAM because the extra inlined `nd_expf` expansions grew IRAM text - which is subtracted
from the internal heap, the coupling recorded below. Cause: a 48.5 %-hit branch in the hottest
straight-line block in the firmware is nearly maximally unpredictable, and it destroys the
interleaving Experiment 12's +19.96 % comes from. This is the SECOND independent confirmation of
runs #230-231 (-2.10 % for a guard around the same call). The `nd_expf_pair()` call must remain
the only statement in that block.

**The contrast that makes this a finding rather than a loss:** the *identical* trick won +0.200 %
in Sinkhorn (run #291), where the guard sits in a 4x4 loop and the exponential is the only thing
happening. So the rule for this codebase is now measured on both sides: removing a redundant
transcendental call pays where the loop is otherwise empty, and costs where the loop is the
scheduler's best interleaved block. Do not price an exp removal by cycle count alone again -
ask first what the surrounding block is doing. Remaining unpaired/unskipped transcendental mass:
the ~1,280 `logf` per token inside Sinkhorn, which belongs to libm and cannot be replaced
bit-exactly, so this family is finished.

Run #291 removed the exactly-zero exponential in Sinkhorn and delivered +0.200 % at ~70 % realisation -
the best realisation of the three exp levers, because deleting a call removes work the scheduler could
not have hidden. Priced the same mechanism on the *bigger* mass, the attention online softmax, using the
fixture Experiment 12 captured from the six frozen primary generations (`/tmp/pairs.bin`, 49,152 real
argument pairs, still on disk):

| pair shape | share | shipped cost | cost if guarded |
|---|---|---|---|
| exactly one argument is `0.0f` | **48.5 %** | 198.04 (pair) | 123.71 (one scalar `nd_expf`) |
| neither argument zero | 51.5 % | 198.04 | 198.04 + guard |
| both zero | 0.0 % | - | - |

48.5 % - not the 27.5 % quoted in the Experiment 12 note, which counted *elements* in a strided subset.
The running maximum is a member of the pair that last raised it, so one of the two arguments is exactly
zero nearly half the time. With the campaign's own measured kernel costs (pair 198.04 cycles, two scalar
calls 247.42, so 123.71 each), the saving is **+17.2 % of this phase's exp work even charging an 8-cycle
guard** to the no-zero path; at ~8,500 pairs per decode token that is ~1.2 ms of a ~200 ms token, i.e.
**+0.6 % predicted**, and at the 25-70 % realisation band measured for this family, +0.15 % to +0.43 %.

Sketch (call site `engine/src/nd_model.c:1352`, one site, inside the head/position loop):

    float d0 = s0 - mx[t], d1 = s1 - mx[t];
    if (d0 == 0.0f) { w0 = 1.0f; w1 = (d1 == 0.0f) ? 1.0f : nd_expf(d1); }
    else if (d1 == 0.0f) { w1 = 1.0f; w0 = nd_expf(d0); }
    else nd_expf_pair(d0, d1, &w0, &w1);
    denom[t] += w0 + w1;          /* unchanged: the pair already produced both before the add */

Why it is bit-exact if done this way: `nd_expf(+/-0.0f)` is exactly `1.0f` (asserted in
`.auto/sinkzero/test.c`), `denom` accumulation order does not move, and testing the *subtracted*
difference means the guard can only fire when two equal finite operands cancel - NaN and inf-inf
still reach `nd_expf` unchanged.

**The risk that must be measured, not argued.** Runs #230-231 measured a branch added around
`nd_expf_pair` costing -2.10 %, because it destroyed the interleaving that Experiment 12's +19.96 %
came from. This version keeps the pair call as the fallthrough, so the common path is the same call,
but a three-way branch in the hottest loop in the tree can still re-sequence the spills. So: build the
differential test first (reuse `.auto/sinkzero/test.c`'s pattern - the shipped expression copied
verbatim, compared bit-for-bit over the captured fixture *and* a dense sweep), then measure on three
boards. If the guard costs more than it saves, the disposition is a one-line revert and the fixture
pricing above is the record of why it was worth the build.

## BANKED (must be measured on real data before any build): skip `logf(1.0f)` in Sinkhorn

Run #292 established the rule: removing a redundant transcendental call pays where the loop is
otherwise empty (#291, +0.200 %) and costs where the loop is the scheduler's best block (-0.43 %).
Sinkhorn qualifies for the winning side, and it still contains ~1,280 `logf` calls per decode token
(`lse = mx + logf(sum)`, 8 per iteration x 20 x 8 layers) - the last unremoved transcendental in the
model, because libm owns it and it cannot be substituted bit-exactly.

But it may not need substitution, only skipping. `nd_expf` clamps to exactly `0.0f` below -88, and the
Sinkhorn iterates to convergence, so once a row/column has collapsed - the maximum at 0 and every
other entry below the clamp - the accumulation is `1.0f + 0.0f + 0.0f + 0.0f`, i.e. **exactly 1.0f**,
and `logf(1.0f)` is exactly `0.0f` (libm guarantees the signed-zero-free exact case; assert it as a
precondition the way `.auto/sinkzero/test.c` does for `nd_expf(+-0)`). `mx + 0.0f == mx` for finite
`mx`, and for `mx = -inf` the shipped code produced `-inf + 0.0f = -inf` anyway, so writing
`sum == 1.0f ? 0.0f : logf(sum)` cannot move a bit.

**Gate before any board time:** instrument the *host* engine (same code, runs the same model) with a
counter for `sum == 1.0f` versus total `logf` calls, over all six frozen primary prompts, and report
the real frequency - do not assume it. If the hit rate is high (say > 50 %), the prize is most of
1,280 logf calls: at 150-250 cycles each that is 0.2-0.4 ms of a ~200 ms token, +0.1-0.2 %, which is
at or under the keep bar, so a low hit rate kills it off-device for the cost of one host build. Note
the guard is another branch, but this one sits in the loop where the branch-cost/skip trade already
measured favourable (#291), and the ratio of guard work to saved work is better here because `logf`
is far more expensive than the compare.

## MEASURED, NOT SHIPPED: assertion level is an 8 KB internal-RAM lever (run #294)

`esp32/sdkconfig.defaults` sets `CONFIG_COMPILER_OPTIMIZATION_PERF=y` but never sets the assertion
level, so the firmware has been shipping at IDF's default **level 2 (full `assert()` +
`configASSERT`)**. Three-board batch, board 1 pristine control, each worker's `sdkconfig` deleted
so it regenerated from its own defaults, all three byte-exact 14/14 with `token_delta 0`:

| level | decode | extended | prefill | boot bench | internal_free |
|---|---|---|---|---|---|
| 2 enable (control) | 5.0117 | 4.9286 | 5.2867 | 5.057 | 15,215 |
| 1 silent | 5.0100 | 4.9257 | 5.2850 | 5.055 | **21,415** |
| 0 disable | 5.0200 | 4.9357 | 5.2950 | 5.066 | **23,463** |

Canonical confirmation of level 0 on the original board at the shipping configuration:
**decode 5.0100** (i.e. -0.03 %, inside the 0.071 % three-identical-image spread - *neutral*),
extended 4.9286, prefill 5.285, think 3.93, min_case 4.78, boot bench 5.056, and the firmware's
own `STATE` line reporting `free_internal_bytes 22571` against the accepted 15,215. So the speed
effect is noise (+0.16 % on one batch board, -0.03 % on the canonical board) and the **RAM effect
is the finding: +8,248 B (level 0) / +6,200 B (level 1) of internal heap**, because assert strings
and their check code are removed from `.flash.text`/`.rodata`, and IRAM/rodata text is subtracted
from the internal heap on this build (the same coupling #292 exposed from the other side).

**Not shipped, for two reasons.** The primary metric is unchanged, which the campaign's own
keep rule settles; and disabling assertions removes runtime failure *diagnostics* from a product
firmware - a defect would surface as a silent reboot instead of an `abort()` with a reason. That
is the owner's tradeoff to make, not an autonomous optimiser's. `ASSERTIONS_SILENT` (level 1) is
the strictly safer middle: it keeps every check and therefore all failure *detection*, drops only
the messages, and still returns 6,200 B.

**Why it is worth more than a null run: it unblocks the family the ledger calls RAM-blocked.**
Every rejected-because-it-doesn't-fit residency idea was priced against 15,215 B free, and the
level-0 figure changes four of them: the 4-bit per-core codebook tables (16 KB, rejected against
8.3 KB free in the run #162 follow-up), the 4-row CQ2 LUT residency (10.4 KB, kbench **+3.8 %**,
run #204), the handwritten 4-bit phi kernel's ~1.5 KB (ceiling +0.5 %), and the 2x4 KiB GDMA pair
(8.7 KB, though that one also lost on speed, -17.9 %). If the owner accepts level 0 or level 1,
re-price those four in this order; the LUT residency is the only one with a measured win above
1 %. Recorded here rather than kept in the tree so the config decision stays explicit.

## MEASURED, phi (mHC 4-bit) family: the handwritten-asm ceiling is ~9-10 % of the phase, not 12 % (run #295)

The last above-bar candidate was a handwritten TIE728 kernel for the mHC phi GEMV, priced
analytically in run #141 at -12 % of the phase from instruction counts (GCC's loop is 41
instructions per 8 weights; `ee.ldf.64` pairing of the contiguous `xh` loads removes four). The
same run diagnosed phi's residual as PSRAM line-fill latency, and the ledger separately records the
generic path's C row blocking as neutral - which is what latency-bound code does when you hand it
fewer issue slots. Both cannot be true, and the difference decides ~150 lines of assembly. Priced
without writing any: `esp32/main/kbench.c` (kept as `.auto/exp20/kbench.c.phi_bound`, output in
`.auto/exp20/board1.phi.txt`) calls the *shipping* `nd_cq_gemv_rows` exactly as `nd_model.c` calls
it, on real `needle3.cact` bytes staged into PSRAM the way the tier stages them, with a real
`nd_cq_prepare` activation, and changes only which backing store each operand lives in.

Device, board 1, fresh `-B build_kb -DNEEDLE_KBENCH=ON`, CCOUNT calibrated 24000 cycles/us, min of
5 rounds rotating over 8 layer slices, results consumed through a float-register barrier:

| mode | weights | min cycles | cyc/weight | vs control |
|---|---|---|---|---|
| `psram_xin` - the shipping path | 12,288 | 85,844 | 6.98 | control |
| `int_xin` - weights in internal RAM | 12,288 | 77,869 | 6.34 | **+9.3 %** |
| `psram_xps` - activation in PSRAM | 12,288 | 77,869 | 6.34 | +9.3 % |
| `int_xps` - both in internal RAM | 12,288 | 77,869 | 6.34 | +9.3 % |

`chk_mismatch=0`: all four modes produced bit-identical rows, so this is a memory-system
comparison and not four different computations. Geometry confirmed on device exactly as #141
derived it from the archive directory: tensor 223, out 32, in 3072, bits 4, group 128, ngroup 24,
`row_bytes` 1536, 4 rows per layer per token.

**What it decides.** Moving *everything* to internal RAM - the best any kernel can do about operand
delivery, because a kernel cannot move the weights - buys 9.3 % of the phase. So at least 90.7 % of
phi's 9.2 ms of GEMV is instruction-and-dependency work, not PSRAM waiting: the phase is **not**
memory-bound, which retires the "phi is slow because of line fill" half of #141's diagnosis and
explains why C row blocking was neutral (row blocking only reuses an operand that was never the
stall). It also puts a *measured* ceiling on the assembly: removing four instructions from a
forty-one-instruction loop can only recover issue slots, and the residency bound says those slots
are worth less than ~10 % of the phase, i.e. 0.4-0.9 ms of the 9.2 ms = **+0.2 % to +0.45 %** decode
- the same number #141 predicted, now bounded from the other side. The three variants that differ
in backing store all report the *same* cycle count, which is the signature of a warm 32 KB data
cache absorbing both operands at this working-set size (4 rows x 1,536 B + 12 KB activation): the
shipping path's 9.3 % penalty is the rotation through five different layer slices overflowing the
cache, i.e. it is a *cache-capacity* effect, and the fix for cache capacity is residency - 24
rows/token x 1,536 B = 37 KB against 15,215 B of internal heap, which is the RAM-blocked family
recorded under run #294, not an instruction-scheduling problem.

**Disposition: phi stays as it is.** The asm would cost ~1.5 KB of a 15,215 B heap, a new ABI
surface and a rule-#1 differential test, to buy a fraction of a measured 9.3 % ceiling that it does
not attack (it cannot move the weights). If the owner ever accepts the assertion-level change from
run #294 and internal heap roughly doubles, the phi residency idea - not the asm - is the one to
re-price, because this measurement says delivery is the only addressable share and residency is the
only way to buy delivery.

**Harness fact, cost three rebuilds: `NEEDLE_KBENCH=ON` does not build in this tree, in two
independent ways.** `esp32/main/kbench.c` (the Experiment-2 bench, tracked) includes
`esp_async_memcpy.h` and `esp_cache.h`, which are not on the main component's include path, and the
kbench block in `esp32/main/CMakeLists.txt` tries to add them with
`target_link_libraries(${COMPONENT_LIB} PRIVATE esp_hw_support)` - which emits a bare
`-lesp_hw_support` and fails at link with `cannot find -lesp_hw_support`, because an IDF component
dependency belongs in the component's `REQUIRES`, not in a link flag. So the option is broken from
both sides: with the line, link failure; without it, header failure. Neither affects any measured
image (the option is OFF everywhere the campaign measured, and shipping images are byte-identical
either way), and the tree was left exactly as accepted rather than half-fixed. What worked for the
phi bound above: write the bench so it needs neither header (no async memcpy, no explicit cache
maintenance), delete that `target_link_libraries` line *locally in the throwaway build*, and build
with `idf.py -B build_kb -DNEEDLE_KBENCH=ON` from a fresh directory. A real fix, if anyone wants
the bench usable again, is to move the option above `idf_component_register` and put
`esp_hw_support esp_mm` in `REQUIRES` conditionally - and then re-verify that a shipping
(NEEDLE_KBENCH=OFF) image is byte-identical to the accepted one.

**Two more harness facts from the same session.** (1) `pkill -f 'needle-api --serial'` matched the
invoking shell's own command line and killed the detached `measure.sh` before it started - use a
bracketed pattern such as `[n]eedle-api`. (2) A kbench image prints `KBENCH_DONE` within seconds of
boot (it hooks before the model caches warm), so the harvest is: open the console with DTR asserted
*first*, then reset with `python3 -m esptool --chip esp32s3 -p /dev/ttyACM0 run` and read for ~60 s;
opening the port after the board has booted captures nothing.

## THE PHI RECONCILIATION GAP (run #295 measurement): the field says 30 cycles/weight, one core warm says 6.98

Put the two numbers side by side, because the campaign has never done it:

* measured here, one core, warm cache, `min` of 5 rounds, real archive bytes, real prepared
  activation, the shipping `nd_cq_gemv_rows`: **85,844 cycles for 4 rows x 3072 weights = 6.98
  cycles/weight**, and 77,869 (6.34) with the weights in internal RAM.
* attributed in run #141 from `ND_PROFILE`: `mhc_phi4` = 13.5 ms/token of which the generic-path
  GEMVs are 9.2 ms, for 24 rows x 3072 = 73,728 weights -> **9.2 ms x 240 MHz / 73,728 = 30
  cycles/weight**.

**The field cost is 4.3x the warm-kernel cost.** Three candidate explanations, and the bench
already kills two: operand placement (the whole point of the four modes - worth only 9.3 %, so it
cannot be 4.3x), and instruction scheduling (the kernel is the *same* kernel in both measurements).
What is left is the working set: decode sweeps 24 rows x 1,536 B = 37 KB of phi weights per token,
which does not fit the 32 KB data cache, so every token pays cold PSRAM line fill, while my
microbenchmark re-sweeps a 6 KB slice and is warm from round 1. That is exactly the mechanism #141
diagnosed, and it also explains two nulls that looked contradictory: C row blocking was neutral
(it reuses `xh`, which was never the stall) and residency bought only 9.3 % here (at 6 KB the
cache already provides residency, so the bench cannot see the benefit that a 37 KB sweep needs).

**Consequence for the last open candidate.** The handwritten-asm idea was priced at -12 % of the
phase from instruction counts. Against the *warm* kernel that ceiling is real but small (the
measured delivery bound is 9.3 %, and the asm cannot move the weights either). Against the *field*
number the phase is 4.3x above the kernel's own warm cost, so the addressable share is delivery,
not issue - and delivery is fixed by residency, which needs ~37 KB of internal RAM against 15,215
B free. So: do not write the phi assembly. If the owner ever accepts the assertion-level change
(run #293, +8,248 B) the phi re-price should be a *residency* experiment - stage the layer's 24
rows, or shrink the per-token phi footprint - and the target is the 4.3x gap, not 12 %.

Caveats on the record: the field figure comes from a profiled image (timers included, `mhc_phi4`
also bracketing pointer math) and the bench figure is a `min` over rounds on a deliberately warm
slice, so the ratio is an upper bound on the gap, not a clean measurement of it. The honest next
measurement is the same four modes at the *real* 24-row x 3-tensor sweep, warm versus cold, which
needs a kbench build (see the harness fact above on why `NEEDLE_KBENCH=ON` does not currently
build).

## RETRACTION and closure: the phi "4.3x gap" was a units error of mine; the phase is fully accounted (run #295/#296)

Run #295 recorded that the field attribution for the mHC phi GEMVs (9.2 ms/token for 73,728
weights = 30 cycles/weight) was 4.3x the microbenchmark's 6.98 cycles/weight, and inferred that the
residual must be cold PSRAM line fill on a 37 KB working set, redirecting the phi candidate from
assembly to residency. **That inference was wrong and is retracted.** The comparison mixed units:
`nd_cq_gemv_rows` goes through `nd_parallel_rows`, so the field's 9.2 ms is *two-core wall time*,
while the microbenchmark runs at the kbench hook, before `app_main` creates the row-split worker,
so it measures *one core doing all the rows*. Normalising to core-cycles:

    field:  9.2 ms / 8 layers = 1.15 ms/layer = 276,000 cycles wall x 2 cores / 73,728 weights
            = 7.5 core-cycles per weight
    bench:  6.41 warm, 7.00 cold  ->  agreement to within 9 %

There was never a gap. The phase map's phi entry and the kernel's measured cost have always agreed;

Two scope corrections found while checking this, both recorded because they change how the
closure should read. The shipping data cache is **64 KB with a 64 B line**, not the 32 KB earlier
notes assumed - run #238 widened it - so phi's 37 KB per-token working set *does* fit, and the
measured 9.2 % cold penalty is precisely what "fits, but shares the cache with the rest of the
token" costs. And the instruction cache is already at its ESP32-S3 maxima too (32 KB, 8-way, 32 B
line), where the Kconfig offers no 64 B instruction line at all: the choice block lists 16 Bytes
(gated on a 16 KB cache) and 32 Bytes only. So the cache-configuration family is closed by the
Kconfig itself rather than by assumption, and there is no untested instruction-side knob - which is
worth stating plainly, because #238's +11.9 % made the cache the most productive config family this
campaign had and it is now provably exhausted.
I had divided a one-core number by a two-core number and then compared it to an inferred "GEMV
share" rather than to the measured phase.

The warm-versus-cold measurement that was meant to explain the phantom gap is still worth keeping,
because it independently closes the family. It runs the *real* per-token phi work - all three phi
tensors, the layer's 24 rows (4+4+16), row bases `li*n` and `li*n*n`, `in` 3072, group 128,
73,728 weights, 36,864 weight bytes per layer - against real archive bytes staged into PSRAM the way
the tier stages them, with a real `nd_cq_prepare` activation, `row_mismatch=0` on every mode:

| mode | cycles/layer | cyc/weight | ms/layer | ms/token at 8 layers |
|---|---|---|---|---|
| `warm_sweep` - cache left alone | 472,385 | 6.41 | 1.968 | 15.75 |
| `cold_sweep` - 160 KB sequential PSRAM sweep before each call | 516,013 | 7.00 | 2.150 | 17.20 |

So the *entire* cost of the data cache being evicted between phi calls is **+9.2 %** - and run #295
measured perfect weight residency buying **+9.3 %**. Two independent methods, same number, which is
the strongest closure this family has had: phi's only addressable share is operand delivery, it is
worth under 10 % of the phase, and it cannot be paid anyway (12 rows per core x 1,536 B = 18 KB per
core against 15,215 B of internal heap). Assembly, residency, and row blocking are therefore all
closed on measurement, with consistent numbers behind them.

**Lesson, and it is the campaign's own lesson recurring: normalise units before believing an
anomaly.** Every previous "anomaly" in this ledger (the 5x sampler cost, the 2x boot-bench gap, the
route-phase gap) turned out to be a comparison between two things measuring different scopes - here
core-cycles against wall-cycles, and the phase map's overlapping timers were already documented as
not summable. A 4.3x discrepancy in a firmware whose every other phase agrees to 9 % is far more
likely to be my arithmetic than the model's.

## MEASURED, KEPT: the +105 % generalises to unseen prompts, and the golden gate had four silent-pass holes (run #296)

Two questions, both answered with measurement, no engine change (git diff over `engine/ esp32/
host/ tools/ model/` = 0 lines, so `decode_tps` stays 5.0117 by construction).

**1. Is the `extended` set still held-out?** It has been watched for 200+ runs, which is exactly how a
"held-out" set becomes in-distribution. Added three shapes the suite had never measured - no-tool
free text, a short translation route, a three-field batch - all short (the firmware request reader
truncates silently at `ND_LINE_MAX-1` = 271 bytes, run #155, and `request_timeout` is 600 s, so
#155's timeout failure mode cannot recur at these lengths). `primary`, `think` and `probe_ids`
verified identical to git before and after; the `prompts.json` diff is additions-only.

| unseen prompt | device tps | what it exercised |
|---|---|---|
| "Tell me a joke about caching" | 5.080 | first no-tool free-text path |
| "Translate good morning into German" | 4.790 | first *short* route case |
| 3-field batch (heap+sampling+timer+status) | 5.020 | first multi-call case (5 calls, 67 tokens) |

Primary is 5.0117 and its own worst case 4.79, so the unseen prompts land **inside the frozen band**:
the speed work is not prompt-specific and there is no overfitting signature. `ext_decode_tps` is now
4.9390 over 10 held-out cases (was 4.9286 over 7) - a monitor widening, not a regression. Product
observation for the owner, not a benchmark change: the chit-chat prompt makes the model emit
`set_sampling_interval(seconds=120)` - a hallucinated call that is grammar-legal only because the
schema has no no-op escape. Widening coverage is how that becomes visible at all.

**2. Does the byte-exact gate check what it claims?** No - five findings, all in my own harness, all
now fixed and tested (`.auto/test_bench_guards.py`):
* `compare()` treats a case with **no golden entry as byte-exact** (`if ref is None: exact += 1`), so
  a typo'd case id passes forever. It is what makes adding a case non-breaking, so it stays - but
  `host_golden_missing`/`device_golden_missing` now report it, and it caught itself immediately
  (missing=3 on the first widened run).
* The "refuse to save a partial golden" guard only ran when the golden was **empty**, so
  `AUTO_GROUPS=primary AUTO_SAVE=1` would have overwritten the 14-case baseline with 6 cases and the
  other 8 would then have compared against nothing - silently destroying the quality baseline. The
  full-groups requirement now gates *every* save.
* That guard also called `measured_groups_full()` with **no argument** (TypeError if it ever fired).
* `CASES` was built with `isinstance(v, list)`, which also swallowed **`probe_ids`**, so the
  full-groups predicate could never be true and **every golden save was being refused**.
* `AUTO_SAVE=1` was documented in `measure.sh`'s own header but **never read**: the save was gated on
  `log.jsonl` having <= 1 line, so the device golden could not be updated after iteration 1.

Re-baselined with the full group set: 13/13 host and 14/14 **pre-existing** golden entries carried
over **byte-identical**, additions only; final state 17/17 device + 16/16 host byte-exact with
`golden_missing=0`, `token_delta` 0, fidelity 5.341e-05, top1 10/10, RAM unchanged.

**3. A config idea closed before it cost a build:** the instruction cache cannot be widened. The S3
Kconfig offers 16 B instruction lines only with a 16 KB cache, and 32 B otherwise; I-cache maximum is
32 KB (current), 8-way. Data cache is already 64 KB / 8-way / 64 B line. So the cache-config family
that produced #238's +11.9 % is closed *by the Kconfig*, not by assumption - and several closure
notes here were carrying a stale "32 KB data cache" claim, corrected above.

**Next lens this suggests, and it is cheap:** a gate that cannot fail is worthless, and I have now
proved one silently passes. Falsify the rest by mutation - break each guarded input deliberately in a
scratch copy (edit one primary prompt, flip one constant in a hot kernel, reorder `probe_ids`) and
confirm `checks.sh` / the fidelity gate / the frozen-input guard actually go red. Host-side only,
~40 s each, no flash. Any gate that stays green is a bigger find than another 0.2 %.

## MEASURED, KEPT: the campaign's quality gates are proven able to fail, and the metric's own definition was unprotected (run #297)

Run #296 found a gate that silently passed 3 of 17 cases, which reframes the question: **"all gates
green" is only evidence if the gates can turn red.** Built `.auto/gate_falsify.sh` - break each
guarded input deliberately, require `checks.sh` to exit non-zero naming the expected gate, restore
with `git checkout`, then require the unmutated tree green. **5/5, `gates_missed=0`:**

| mutation (real, deliberate) | gate that fired |
|---|---|
| one character changed in a stored generation (`golden/host.json`) | `HOST_OUTPUT_DIVERGED` |
| +0.5 on one reference logit (`golden/logits.txt`) | `FIDELITY` refusal |
| one `primary` prompt's text edited | `BENCH_GUARDS_FAILED` (new) |
| **real quality trade: `ND_SINKHORN` 20 -> 10** | `HOST_OUTPUT_DIVERGED` |
| (negative control) ctest test count | 2 real tests: `grammar`, `prefix_isolation` |

The third row is the finding: `checks.sh`'s frozen-input guard is a `git diff` over `model/`, `tools/`
and `partitions.csv` - **it never included `.auto/prompts.json`, so the six prompts that *define the
metric* were protected by no gate at all.** Closed by pinning primary's prompt text, its id/phase
list and `probe_ids` with sha256 in `.auto/test_bench_guards.py`, which `checks.sh` now runs (so
sanctioned `extended` additions still pass, and an edit to `primary` cannot). Second gate added: the
host byte-exact check required only `exact == cases`, which #296 proved is satisfiable while
comparing against nothing - it now also requires `host_golden_missing == 0`.

The fourth row matters more than the others: the fidelity/byte-exact veto is the mechanism by which
this campaign refuses to buy speed with model quality, and until now it was *assumed* to work rather
than demonstrated. Sinkhorn 20->10 is precisely the trade the rules forbid, and the gate refuses it.

Both new gates verified in the real pipeline (canonical run: checks green through the new code path,
17/17 device byte-exact `golden_missing=0`, 5.0117 decode, all values identical to #296 - no engine
change).

**Rule worth keeping: a check whose tool is missing must report failure, not pass.** My own script
keyed its env guard on `IDF_PATH`, which this environment sets with `cmake`/`ctest` **off PATH** - the
exact trap `checks.sh` documents for cmake - so the test-count probe saw no output and reported
"zero tests". Because the probe was written to fail on empty output, that showed up as `MISSED` in
one run instead of never. Key on the tool (`command -v ctest`), not on the variable that implies it.

**Still un-falsified, and it is the one that matters most:** nothing in `checks.sh` inspects the
*device* golden - the host gates are the only byte-exact veto wired into the pipeline, so a
device-only divergence (exactly the class of the #159/#162 two-core race, which was byte-exact only
by timing luck) would print `device_output_exact=11/17` and the run would still report PASSED unless
a human read the number. Next: gate it in `measure.sh`, then falsify it by mutating the device
golden on a no-flash re-measure.

## MEASURED, KEPT: the device byte-exact comparison was gated by nothing, and that is the gate that guards the central rule (run #298)

Run #297 could not reach the device gate (its `run_gate()` drives `checks.sh`, which never looks at
the device). Completing the lens found the sixth and most consequential silent-pass hole:
**`measure.sh`'s only failure exits were `HOST_BUILD_FAILED` and `FLASH_FAILED`.** The byte-exact
veto in `checks.sh` runs on the **host** build, where `nd_parallel_rows` is `rows_serial` - so a
two-core defect is *structurally invisible* to it. That is exactly the class of run #159's shared
8 kB codebook table written from both cores, byte-exact on three boards by timing luck and reverted
at #162: had the race actually bitten, the run would have printed `device_output_exact=11/17` and
still reported **PASSED**, with only a human reading a number as the safeguard.

Fix: `measure.sh` tees the bench log and, for a full-group run, requires
`device_output_exact == device_cases` **and** `device_golden_missing == 0`, exiting 1 with
`DEVICE_OUTPUT_DIVERGED` / `DEVICE_GOLDEN_INCOMPLETE`. Restricted `AUTO_GROUPS` runs skip it (they do
not measure the whole set), and the greps are `|| true`-guarded because `set -e` would otherwise exit
on a short run where a metric line is simply absent.

Falsified by measurement: one character changed in one device golden entry, re-measured the
already-flashed app (`AUTO_NOFLASH=1`) -> `DEVICE_OUTPUT_DIVERGED 16/17`, exit 1, and
**`device_token_delta = 0`** - text-only divergence with an identical token count, which is precisely
what a token-count-only check waves through. Restored with `git checkout` (exactly 2 DIVERGE lines,
no host leakage), then green: `DEVICE_GATE_OK exact=17/17 golden_missing=0` at 5.0117.

**The property this buys is the campaign's own licence.** The divergent run measured *the same*
5.0117 decode. Speed and quality are now independently gated on the device, so a fast-but-wrong
candidate cannot pass this pipeline - which is what "we did not buy speed with model quality" has
been resting on. Gates proven falsifiable now number six: host byte-exact, fidelity reference,
primary-prompt hash, quality-trade (Sinkhorn 20->10) veto, non-zero test count, device byte-exact.
M6 is kept in `gate_falsify.sh` behind `AUTO_DEVICE_FALSIFY=1`; it is the only probe that needs a
board, and it costs one no-flash re-measure (~3.5 min).

**Does this invalidate any prior discard? Checked, and no.** The obvious candidate was the
race-free 4-bit folded-codebook retry (#162), since a device-only race is now auto-caught rather than
eyeballed. But #162 rejected it on *speed* as well (4.87 / 4.7857 / 3.95 vs 4.8817 / 4.7986 / 3.87
controls): one handshake per group plus a read-modify-write of `y` costs more than the fold saves,
and per-core tables need 16 kB against 15 kB of heap. The gate change removes only the risk half of a
two-part rejection, so no retry. The RAM-blocked family still waits on the owner's assertion-level
decision (#293), which this run did not touch.

## CLOSED ON MEASUREMENT: software data-cache prefetch does not exist on this core (run #300 side-probe)

The one lever family the ledger had never tested: the engram's scattered slot gathers are the only
significant accesses that are *latency*- rather than issue-bound, which is precisely what a data-cache
prefetch instruction is for, and a prefetch cannot change a result bit - so it would have been the
risk-free candidate. It is unavailable:

* `__builtin_prefetch(p, 0, 0)` under `xtensa-esp32s3-elf-gcc -O2` compiles to **nothing** (the
  function is just `lsi`, `lsi`, `add.s`, `rfr`, `retw`). A gcc-level attempt would therefore have
  looked like a *null result* rather than an unavailable lever - the dangerous kind of dead end.
* The opcodes do not exist either: `pref 0, a2, 0`, `dpref 1, a2, 0, 1` and `pref.r 1, a2` are all
  rejected with `unknown opcode or format name` by the assembler this toolchain ships.

Consequence, and it is explanatory rather than just negative: it is why every "prefetch deeper"
experiment in this campaign (the LUT kernel's index prefetch, -1.9 pp) had to be implemented as
*earlier scalar loads*, and why the residency family (real internal-RAM residency, not prefetch) is
the only way to attack delivery on this part.

## MEASURED, KEPT: the three-board pool was NOT in the state the campaign assumes (run #300)

Build configs had drifted - board1 `ASSERTION_LEVEL=2` (shipping), board2 **0**, board3 **1**, i.e.
run #293's candidates left behind, because `esp32/sdkconfig` is gitignored and `git reset --hard` does
not restore it (#288's recorded fact, now biting for the second time). Worse: **all three workers were
on stale source** (c80fbae / 7c331af vs HEAD 9b2f811) and boards 2/3 carried *different*
`sdkconfig.defaults` hashes, so deleting the drifted `sdkconfig` would have regenerated the drift
straight back out of the stale tracked defaults. Normalised (removed the drifted configs, synced all
three to HEAD **by local path**, never `git fetch origin`, which has no credentials here) and now
reports `CONFIG_SYNC_OK boards=3 source=3`.

`.auto/board_config_check.sh` guards it: prefix-keyed build-config comparison against the *local*
canonical `sdkconfig` (the config every accepted number was measured at), plus each worker's HEAD
commit and `sdkconfig.defaults` hash, plus `SELFTEST=1` proving the comparison can distinguish a
one-key difference. Wired into `measure.sh` as a non-fatal `CONFIG_DRIFT_WARN`.

**The meta-finding, third instance this session:** my first version of this check **vacuously
passed** - it anchored `^KEY=` while IDF spells the important keys
`CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE=y` / `CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y`, matched
nothing, and printed `IN SYNC` on a demonstrably drifted pool. Tally on the campaign's own harness:
three checks that could not fail (`bench.compare` counting a missing golden as exact, the
partial-save guard that only guarded an empty golden, this config check) and one documented knob that
was never wired (`AUTO_SAVE`). All four failed in the same direction - **reporting success while
verifying nothing** - which is why "can this check fail?" has been the highest-yield question in this
campaign while every 0.2 % speed candidate ran out. Rule for whoever picks this up: ask it of any
check before trusting a green run.

## CLOSED BEFORE A BUILD: staged tap/norm table delivery - the last uncompressed fp32 payload (run #302)

Hunted for one more non-GEMV target and found the largest payload in the token still in full IEEE
fp32: q/k tap tables = 9 taps x 768 cols x 32 B = **221 KB/token**, per-head norm scales ~24 KB/layer,
against a measured 3.3 ms of tap phase (1.65 %) - and the mechanism class is this campaign's most
successful (run #2 fp32 staging, +99.9 %), so it earned a look. Dead on code reading, not argument:

* `fp16_slot[li][SLOT[f]] = p` in `nd_model_open` - these tables are **already copied into PSRAM at
  open**. The 12 MB tier exists to turn *mmap-flash* reads into PSRAM reads, so it cannot help bytes
  that are already in PSRAM; "put the taps in the tier" is not a lever, it is the current state.
* Residency independently ruled out: 221 KB of taps against a 64 KB data cache (#296's correction).
* The backlog's "genuinely untried: hoist `nd_f16()` out of `tap_projection`'s inner loop" is already
  done - the function reads `m->fp16_slot[li][tap_slot]` and its own comment records that converting
  in the loop used to cost `taps*dim` conversions per projection.

So the phase is at the non-resident scattered-PSRAM-read floor already measured for other payloads of
this shape, and a build would have measured the control. Recorded as the fourth off-device closure of
the session (logf skip, software prefetch, cache-config maxima, this) - the cheap kind of negative
result that keeps 13-minute board runs for questions that can still change a decision.

## STOP CONDITION MET (runs #302-#304): the loop is producing disclosed repeats, not experiments

Three consecutive canonical runs of an unchanged shipping tree (#302, #303, #304), all reproducing
5.0117 decode / 4.9390 extended / 3.93 think / 5.2867 prefill / 5.057 boot bench / 4.79 min_case /
15,215 internal / 2,052,252 psram to the digit, 17/17 device + 16/16 host byte-exact with
`golden_missing=0`, fidelity 5.341e-05, top1 10/10. Deterministic to the last digit is the point: a
fourth carries no information, and the ledger's own #246-#285 stretch shows what an unchanged-HEAD
loop degrades into when each iteration must emit a number.

Nothing above the 0.2 % keep bar remains inside the documented maxima and the byte-exact gate. Closed
in this window alone, all off-device and all cheap: the Sinkhorn `logf` skip (13.58 % measured hit
rate vs a 787-cycle break-even), software prefetch (`pref`/`dpref` absent, `__builtin_prefetch` emits
nothing - would have read as a null result), the cache-config family (S3 Kconfig maxima; no 64 B
instruction line exists), staged tap/norm table delivery (already PSRAM-resident; 221 KB of taps vs a
64 KB cache), and Sinkhorn's final `n*n` exponential tail (~128 exps/token, an order of magnitude
under the bar).

**The two routes that are above the bar are owner decisions, not code.** (1) Assertion level: neutral
on speed but frees 8,248 B of internal heap (#293), which re-prices the RAM-blocked residency family -
of those, only the 4-row CQ2 LUT residency is a measured winner (kbench +3.8 %), so the re-pricing is
worth ~one experiment, not a campaign. (2) 120 MHz octal flash+PSRAM: +3.51 % measured on two boards
(#289), forbidden by the documented-maxima rule and carrying IDF's own ~20 °C random-crash caveat.

Operator-side chores this loop cannot do: `git push` (no credentials in this container; origin is
~22 commits behind - sync workers by local path only), and the product defects already reported and
still open (silent 271-byte request truncation; the schema's lack of a no-op escape, which turns
chit-chat into a hallucinated `set_sampling_interval`).

## Experiment 20 - CQ2 residency under reclaimed assertion RAM: premise retired, the real ceiling measured at +13.90 % of every CQ2 pass (runs #330-#331, boards 2+3, no shipping change)

**Hypothesis as redirected.** Assertion level 1 frees 6,200 B of internal heap (#293) and that was
said to unblock "the 4-row CQ2 LUT residency (10.4 KB, kbench **+3.8 %**, run #204)". Step 1 of the
redirect said to recover that construction and re-verify it rather than trust the summary, so that is
what happened first - and the summary did not survive it.

**1. The citation does not describe a residency measurement.** `run #204` in `.auto/log.jsonl` is the
`-DNEEDLE_LUT2_ASM=OFF` ablation (4.3117 vs 4.8917, "the TIE728 kernel is worth +13.45 %"), with a
deliberately *slower* control; it contains no residency variant, no 10.4 KiB and no +3.8 %. A
pickaxe over every commit in the repository for `residency`/`lut_int`/`LUT_RESID` returns the phi
bounds (#294/#295), the GDMA batch (#287) and this queue's own text - nothing else. And the "LUT"
half of the name is already shipped: `m->lut = ND_ALLOC_FAST(...)` (`nd_model.c:642`) with
`ND_ALLOC_FAST` = `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` (`nd_model.h:50`), so the 24,576 B pair
table has lived in internal SRAM since before this campaign's first run. There was no candidate to
build, so no three-board batch was run: the batch would have measured the control, which the redirect
(forbids) and the harness (now enforces, exit 42) both say not to do.

**2. The residency construction that does exist was rebuilt, fixed, and made honest.** `kbench.c`
already had `blob_int` - the whole CQ2 weight blob memcpy'd into internal RAM - but it was (a) only
timed, never compared row-for-row, and (b) unbuildable: `esp32/main/CMakeLists.txt` "fixed" the
missing component dependency with `target_link_libraries(... PRIVATE esp_hw_support)`, which emits a
bare `-lesp_hw_support`, while `esp_cache.h`/`esp_async_memcpy.h` still fail as `main` requirements.
Both were needed for the *closed* Experiment 15 bench only, so that bench is now behind
`ND_KBENCH_GDMA` (default 0) and the broken link line is gone: the option branch is not even evaluated
in a shipping configure, so the accepted image cannot move, and the bench builds again. Two real
harness facts came out of the rebuild. (i) The old warm `blob_int` numbers are worthless on their own:
isolated and blob_int agree to the cycle (96x768 min 173,713 both; 128x768 231,612/233,032 both)
because a 19-26 KB blob is already warm in the 64 KiB data cache (`CONFIG_ESP32S3_DATA_CACHE_64KB`) that the bench re-sweeps 25 times - exactly
the trap #295 recorded for phi. (ii) The residency-equality check caught two ordering bugs in the new
code on the first two runs (`yref` not yet filled; table memcpy before `nd_cq_lut_build`), which is the
differential test doing its job rather than a bench reporting nonsense.

**3. Measured ceiling, cold, bit-exact, two boards, min of 25 rounds, shipping `nd_lut2_rows_tie1n`,
real archive bytes, batch `20260922T164243.*-kb` (boards 2 and 3, images differing only in the
embedded build id and the MD5 footer; board 1 idle by design).** Cycles per 32-bit packed word:

| shape (rows x 768) | blob B | warm PSRAM | cold PSRAM = the field | cold, blob internal | cold, table in PSRAM |
|---|---|---|---|---|---|
| 768 | 156,672 | 42.93 | **42.93** | - | 49.06 |
| 576 | 117,504 | 42.93 | **42.94** | - | 49.21 |
| 128 | 26,112 | 37.70 | **42.92** | **37.69** | 53.21 |
| 96 | 19,584 | 37.70 | **42.94** | **37.70** | 56.64 |

Read the columns, not the rows. *Warm-equals-cold for every blob larger than the cache*, and every
cold PSRAM run - small blob or large - lands on the **same 42.93-42.94 cycles/word**, while operands
in internal RAM run at **37.69-37.70** regardless of shape. So the PSRAM penalty is not a
cache-capacity accident on a 37 KB working set (the framing this family has carried since #141): it is
a flat **13.90 % delivery tax on every 2-bit pass** (42.94/37.70 - 1), and a blob that fits the cache
only escapes it while nothing else is streaming through it. Moving the blob to internal RAM bought
+13.87 % (128 rows) and +13.90 % (96 rows) cold and was **bit-exact 128/128 and 96/96** against the
PSRAM-backed rows for all five kernels including the shipping one; the pair table's backing store is
equally transparent (`exact=768/768`, `576/576`, `128/128` for the C walker over a PSRAM copy of the
table), which is what licenses the ablation in the last column: taking the table *out* of internal RAM
costs +14.3 % on the dominant shape and +31.9 % on the small one. That is the first number this
campaign has ever put on the residency the shipping build already has - and it is much larger than the
+3.8 % the queue was going to re-buy.

**4. Why none of it is collectable with the reclaimed RAM, stated as arithmetic.** The 2-bit phase is
86.5 ms of a ~200 ms token; 13.90 % of it is 12.0 ms = **+6.0 % decode**, which is the honest prize,
not +3.8 %. Collecting it needs all 3.59 MB of per-token 2-bit weights in internal RAM, or a
per-token copy of them into a small window. The first needs 172x what level 1 frees (6,200 B) and 58x
what level 0 frees (8,248 B). The second was measured in #287 and lost: the copy shares the octal bus
with its consumer, 4 KiB blocks -17.9 %, 2 KiB -67.8 %, and `esp_cache_msync` costs a fixed 1.42 M
cycles per invalidate (76 times per token). 120 MHz buys 3.51 % of the same 6 % (#289), which is
consistent: this is external bandwidth, and the two mechanisms that could buy it are the owner's clock
decision and a quantisation change that is frozen. **Disposition: Experiment 20 is closed with no
candidate and no shipping change - the accepted runtime stays 5.0117.** The reclaimed RAM buys no CQ2
win; its only above-bar use remains Experiment 23's 16 KiB private folded tables, which additionally
requires the owner to accept assertion level 1 over the diagnostics it costs.

**Harness notes for whoever runs next.** (a) ESP-IDF images are not byte-comparable across
checkouts in this pool: a rebuilt shipping image differs from the accepted one at header offsets
48-121, a 32-byte build id at 176, and the MD5 footer - because assert strings carry `__FILE__` and
`/workspace/esp32-needle-3` and `/root/board-pool/boardN` have different lengths. Two same-length
checkouts differ *only* in the build id + footer (65 bytes). Hash-compare within one checkout, or
compare after masking those regions. (b) `.auto/kbench_build.sh` writes its build log to
`/tmp/kb-<variant>-<board>.log` now, so two boards can build at once. (c) The `measure.sh` shipping
signature covers `esp32/main/kbench.c`, so a bench-only edit changes the signature while the shipping
image cannot; that is a loophole in the anti-repeat guard, deliberately left untested here.

## Experiment 21 - make the 120 MHz result reliability-testable: CLOSED, BLOCKED BY VENDOR SUPPORT (runs #332-#333, boards 1+2, no shipping change)

**Hypothesis as redirected.** The +3.51 % at octal 120 MHz (#289) is real but ships only behind
IDF's ~20 C random-crash caveat, and the documented mitigation is the temperature-based MSPI
timing retune. Build that variant, then run memory-integrity and hot/cold soaks on two boards.

**What was built first (kept, default OFF).** `esp32/main/thermal_diag.c` + `NEEDLE_THERMAL_DIAG`
+ a one-line hook in `app_main` before `nd_model_open`: every 5 s it prints die temperature
(`driver/temperature_sensor.h`: install/enable/get_celsius), a CRC32 (`esp_rom_crc32_le`, no new
dependency) over a live 256 KiB PSRAM buffer, and both heaps - i.e. silent PSRAM corruption and the
temperature axis in one line. `.auto/exp21/` holds the drivers (boot capture, heat curve, soak).
Proof it is free when OFF: two canonical device runs of a tree carrying it read exactly the accepted
**5.0117** (17/17 device, 16/16 host byte-exact, golden_missing=0, internal_free 15,215).

**Measured, and it is a hard blocker rather than a risk.** With `CONFIG_SPIRAM_SPEED_120M=y`,
`ESPTOOLPY_FLASHFREQ_120M=y`, `FLASHMODE_QDO=y`, `IDF_EXPERIMENTAL_FEATURES=y` and IDF's mitigation
`CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR=y` (+`MEASURE_A_REALISTIC_POINT`), a fresh
configure does enable the scheme, and PSRAM comes up normally:

```
I esp_psram: Found 16MB PSRAM device
I esp_psram: Speed: 120MHz
I esp_psram: SPI SRAM memory test OK
I esp_psram: Adding pool of 16384K of PSRAM memory to heap allocator
E MSPI Timing: The flash model has not been verified support this feature, please contact espressif business support
E cpu_start: init function 0x4037e958 has failed (0x106), aborting
abort() was called at PC 0x42002620 on core 0   -> Rebooting... (forever)
```
`0x106` is `ESP_ERR_NOT_SUPPORTED`, and addr2line names the failing init function:
`__esp_system_init_fn_psram_adjust_timing_point_via_temperature`,
`esp_hw_support/mspi_timing_tuning/port/esp32s3/mspi_timing_by_mspi_delay.c:882`, called from
`do_system_init_fn` (`esp_system/startup.c:135`). So **IDF itself refuses to run the temperature
retune on this board's flash model, and a refused init aborts the boot.** Reproduced on two boards:
board 2 (build_r120) looped with that backtrace, board 1 (build_safe, md5 a4b713ca) never reached
`EVT READY` either - 16 lines of corrupt/rebooting output in 120 s and zero bytes over a 10-minute
load attempt. There is no bootable "safe 120 MHz" image to soak.

**ECC, the other safety lever, does not fit either.** Enabling `SPIRAM_ECC_ENABLE` on octal PSRAM
costs ~1.09 MB of the model's heap: the ECC image reported `psram_free=697,476` where the same
diagnostic build without ECC reported `1,789,984` at the same boot stage, and with ECC the firmware
itself printed `ERR prefix_cache_allocation` after priming. The shipping model has no room for ECC
at this context size.

**Why a soak could not have settled the risk anyway.** The sensor works (die read 24.3-31.0 C; the
~40 s prefix priming lifts it ~5 C) but that is the whole self-heating this workload produces, and
IDF's stated failure axis is a ~20 C swing in either direction from the power-on temperature -
four times larger than anything the board does to itself. A self-heating soak would have measured
the wrong axis.

**Disposition: 80 MHz stays the production default, now on measured grounds.** The route is blocked
by Espressif's flash-model verification gate, not by missing effort; what would reopen it is a
verified timing model for this flash part (the error text names exactly that gate), not a longer
soak. The 120 MHz number remains a forbidden +3.51 % diagnostic.

**Harness facts bought by this experiment (all four cost real time).**
1. `idf.py -DCONFIG_X=y` does **not** change a Kconfig value when `esp32/sdkconfig` already exists;
   the first "ECC build" and the first "120 MHz build" in this cycle were both silently still 80 MHz
   with ECC off (the boot banner's "Boot SPI Speed : 80MHz" was the tell). The supported route is
   `sdkconfig.defaults` + delete `sdkconfig` + fresh `-B`, then verify the value in the generated
   `sdkconfig`.
2. `/dev/ttyACM0`//`dev/ttyACM1` are **board 1's** aliases. Three concurrent "board" runs using them
   all drove one chip and two reported the port busy. The pool exposes per-board nodes through the
   wrapper's `$FLASH_PORT`/`$SERIAL_PORT`; the scripts in `.auto/exp21/` now refuse to run unless
   those variables name the board they were asked to drive.
3. Reset with the console port **closed**: holding it open while esptool resets leaves the strap
   sampled wrong and the chip sits in `boot:0x0 DOWNLOAD(USB/UART0)` forever.
4. A board that booted minutes earlier delivers **nothing** to a console attached later unless DTR is
   asserted, and `serial_api.Device`'s attach (which waits for a `!think` ack) is the only reliable
   way to drive it. A hand-rolled `!status` gate on a warm board never got an answer.

**Reusable finding:** the temperature sensor and the PSRAM CRC line are cheap and stay in the tree.
Any future memory-clock or PSRAM-integrity work starts from `TDIAG` output instead of rebuilding this
instrument.

## Experiment 22 - handwritten 4-bit phi kernel: the plain-asm form is BIT-EXACT and +11.6 % in isolation (run #332, board 2, no shipping change yet)

**Measured result, in one line.** A handwritten CQ 4-bit row walker that changes
nothing but the *structure* (one function per row instead of a call per group) and
uses ordinary `lsi` loads is **bit-exact against the shipping `nd_cq_gemv_rows` on
real `needle3.cact` bytes (0 mismatched rows of 128, unit probe exact) and 11.6 %
faster** - 5.647 vs 6.338 cycles/weight, min of 25 rounds, CCOUNT 24000, board 2.
phi's GEMVs are 9.2 ms of a ~200 ms token, so that is ~1.07 ms/token = **+0.53 %
decode**, which is above the 0.2 % keep bar and above run #141's analytic +0.5 %
ceiling. This is the first above-bar speed candidate since run #291.

**It also retires run #295's "do not write the phi assembly".** That closure was
built on a measured ceiling of 9.3-10 % for operand *delivery* and on instruction
counts. Both are real, and neither is what this win comes from: in the shipping
build `dot_group` is a **function call per group** (symbol at `0x4037bba8`, not
inlined), so the C path pays call overhead plus a per-group norm conversion across
the boundary 24 times per row, while one asm function per row keeps all 24 groups
in registers with `loop`. Call/inline structure is a third category, and it is
bigger than either of the two the closure considered. Lesson: a ceiling measured
within one category is not a ceiling on the phase.

**The TIE wide-float loads are NOT the win, and their numbers are invalid.**
`ee.ldf.128.ip` (2 instrs per 8 floats) measured 4.774 cyc/weight (+25 %) and
`ee.ldf.64.ip` (4 instrs) 5.023 (+21 %) - but both **fail the known-answer probe:
they return 63.0 where the answer is exactly 64.0**, i.e. one of the eight floats
per index word is not the value asked for, so the kernel is fast because it drops
a term. Do not use either form until the fill order of `ee.ldf.*.ip`'s register
list is determined; the 63/64 signature says exactly one contribution per word, so
the question is register order (or a stale register), not the load itself. The
plain-`lsi` variant is the one that is both exact and faster.

**Two ABI facts, paid for with four boot-looping harvests.**
1. **A C `float` return value arrives in the INTEGER return register.** GCC's own
   float epilogue is `ssi f0, a1, 0` / `l32i.n a2, a1, 0`. An asm kernel that ends
   with `mov.s f0, f13; retw` hands the caller a stale float, and the symptom is
   *one identical wrong value from three different kernels, independent of their
   inputs*. Fix, 2 instructions: `ssi f0, a1, 16` + `l32i a2, a1, 16`.
2. `addx4 ar, as, at` is `ar = (as << 2) + at` with the **destination first**
   (as `lut2_tie728.S` already writes it). `addx4 idx, base, idx` silently computes
   `(base << 2) + idx`; on device that was a `LoadProhibited` at
   `EXCVADDR 0xf40c03d4` = `(0x3d0300f4 << 2) + 4`, which is how the arithmetic
   confirmed the semantics.

**Why the unit probe was worth more than the differential it replaced.** The real
fixture comparison only said "all rows differ". A known-answer probe on
hand-computable inputs (`cb[i]=i`, row bytes `0x10`, `xh=1.0`, norm `0x3C00`, one
group -> exactly 64.0) said *which* stage was wrong in one build, and it is what
turned up both facts above. Rule for this repo's asm screens: ship a known-answer
probe in the same kbench run as the differential, not instead of it.

**Shipping impact so far: none, and proven none.** The screen lives in
`esp32/main/dot4_tie728.S` and `esp32/main/CMakeLists.txt`'s `if(NEEDLE_KBENCH)`
branch. Same-checkout A/B (fresh `-B build_s1` with the change, fresh `build_s2`
without): both 281,072 bytes, **68 differing bytes in 4 regions** = header stamps
(0x74-0x77), the 32-byte build id (0xb0-0xcf) and the MD5 footer (0x449cf-0x449ef)
- i.e. nothing but the build identity, exactly the pattern recorded in run #330.

**Banked integration (the next experiment, priced at +0.5 %).**
(a) Fold the tie0 body into `engine/src/nd_quant.c`'s 4-bit path behind an
`nd_lut2_asm_ok()`-style gate - it needs only g == 128, ordinary-fp16 norms and
4-byte alignment (no 16-byte requirement, because it uses no TIE load) - keeping
the C path as fallback; the row-split via `nd_parallel_rows` stays outside.
(b) Differential-test the primitive off-device first on the real 4-bit fixture
already captured in `.auto/exp22/` (`phi4-prefill.bin`, `phi4-decode.bin`,
`replay.c`), then on device with the row-for-row memcmp this bench already does.
(c) Three-board batch, and watch `internal_free`: this kernel is ~200 bytes of
IRAM, and IRAM text is subtracted from the internal heap (#292's coupling).
(d) In the same integration, measure hoisting the norm conversion out of the group
bottom - #204 measured that ordering as worth -1.1 pp in the 2-bit kernel.
(e) Separately: determine `ee.ldf.*.ip`'s register fill order. If it is fixable the
ceiling above the plain-asm form is another ~10 %.

## Experiment 22 lanes (owner directive: three independent board lanes, pinned baselines, no live control)

Baseline used for discovery on every lane: the pinned accepted **5.0117 decode tok/s**
(17/17 device + 16/16 host byte-exact, fidelity 5.341e-05, internal_free 15,215), with the
campaign's own measured board-to-board spread of **0.071 % peak-to-peak** (run #144, three
byte-identical images on three boards). A lane result below ~+0.15 % cannot be separated
from board identity, so it is a discovery signal, not a keep; only a cross-board repeat
promotes it.

**Harness blocker found and fixed first (cost four dead board lanes).** `measure.sh` runs
under `set -euo pipefail`, and its anti-repeat guard ended with
`[ -f esp32/sdkconfig ] && sha256sum esp32/sdkconfig` inside the hashed command
substitution. On a freshly synced checkout `esp32/sdkconfig` does **not** exist yet (it is
generated by the build and gitignored - the guard's own comment says so), so the pipeline
status was 1, `CURRENT_SHIPPING_SIG=$(...)` failed, and `set -e` killed the run **before it
printed one byte**: rc=1, empty logs, four lanes lost, and it looked exactly like a dead
detached launcher. Fixed with `if [ -f … ]; then …; fi` plus `true` inside the group (the
hash content is unchanged, so the guard's meaning is unchanged), and the reason is now a
comment in `measure.sh`. Fourth instance this campaign of a check failing in the direction
that looks like success/failure-without-information; the diagnostic that found it in one
step was `bash -x`, not the empty log.

**Lane-launch rule.** tmux windows must run `needle-board run N -- bash .auto/measure.sh`
*inside* the window: the wrapper is what sets `NEEDLE_BOARD`, `FLASH_PORT`, `SERIAL_PORT`
and takes the board lock. A window that calls `measure.sh` directly silently falls back to
board 1's ports and board 1's lock - lane 2 died with "Board 1 is busy" while lane 1 was
legitimately holding it. `.auto/lanes.sh` now encodes the correct form plus per-lane
candidate application and a grep that fails if the candidate is not actually in the tree.

### 22B - forced-inline C `dot_group` (board 2, batch 20260922T2110L): MEASURED, +0.10 %, BELOW BAR, costs 1,024 B of internal heap - REJECT

The zero-assembly version of the same hypothesis as 22A: `dot_group` is a call per group in
the shipping build, so `__attribute__((always_inline))` on it removes the call and the
across-boundary norm work for every caller (phi, the 2-bit fallback walker, dequant). `nm`
confirms full inlining (symbol gone), image +656 B.

Lane against the pinned baseline: **decode 5.0167 (+0.10 %)**, extended 4.946 (+0.14 %),
prefill 5.2917, think 3.94, min_case 4.79, `device_output_exact=17/17`,
`device_golden_missing=0`, `DEVICE_GATE_OK`, token_delta 0 - and **internal_free 14,191
(-1,024 B)**. Verdict: +0.10 % is inside the 0.071 % board-spread band, so it is not even a
confirmable discovery signal, and it pays 1 KiB of the 15,215 B heap. Not kept, not
cross-boarded.

What it *is* useful for: it is the control-for-free on the mechanism claim. Run #332's
isolated +11.6 % for the handwritten kernel cannot be "just the call removal", because the
C transformation that removes the same call buys +0.10 % end to end (which corresponds to
roughly +2 % of the phi phase, not +11.6 %). So the asm's win, if lane 1 reproduces it,
comes from the rest of the schedule (`loop` + everything-in-registers + the norm converted
once inside the function), not from call overhead alone - and if lane 1 also lands near
+0.1 %, then the isolated +11.6 % was a microbenchmark warmth artefact of exactly the kind
#295 recorded for phi, and this whole family closes at ~0.1 %.

### 22A - integrated plain-load GEMV4 asm: MEASURED, KEPT, +0.365 % decode (run #333, 5.0117 -> 5.0300)

Canonical board-1 run at the shipping config: **decode 5.0300**, extended 4.956 (+0.34 %),
prefill 5.3067 (+0.38 %), think 3.94 (+0.26 %), boot bench 5.076 / 197 ms per token (the
bench moving is the mechanism check - phi is inside it), min_case 4.80 (best worst case
seen), 17/17 device + 16/16 host byte-exact with `golden_missing=0` and `DEVICE_GATE_OK`,
token_delta 0, **fidelity 5.341e-05 identical to the control**, top1 10/10. Cost
internal_free 15,215 -> 14,903 (-312 B of IRAM text, run #292's coupling). Realisation
69 % of the isolated +11.6 % (priced +0.53 %), inside the campaign's 25-70 % band.

**Two failed attempts, both recorded because both are findings.**

1. *A null that looked like a candidate.* Saving the inline variant with `git diff` +
   `git checkout engine/src/nd_quant.c` and not re-applying it left the shipping wiring
   out while the `.S` still compiled and the CMake define was set. The linker
   garbage-collected the unreferenced kernel (`nm`: no `nd_gemv4_rows_tie1`), so that run
   measured the accepted runtime exactly - 5.0117 and internal_free 15,215 to the digit -
   and **the exit-42 anti-repeat guard did not catch it**, because an extra compiled file
   changes the shipping signature. Loophole, stated for whoever picks this up: the
   signature catches config and source drift, not an unreferenced addition; assert the
   candidate's new symbol is in the ELF before believing a number.
2. *The first real build diverged, and the device gate is what stopped it:*
   `device_output_exact=5/17`, `device_token_delta=175`, `DEVICE_OUTPUT_DIVERGED`, exit 1 -
   while measuring **faster** (5.0283). Cause: the row-range kernel's word loop already
   consumes a whole row (`a8` +4 B per index word = 64 B per group, `a5` +2 B per group),
   so both cursors land exactly on the next row when the groups run out; the row epilogue
   then added `rowbytes` and `normstep` again and skipped every row after the first.
   Deleted both advances (commented at the site). **Why no primitive test saw it:** the
   isolation screen called the kernel once per row, where the *caller* did that stepping -
   a single-row test cannot cover a multi-row cursor. Rule for this repo: a kernel that
   owns a loop over rows must be differentially tested *over rows*, against N per-row calls
   of the proven single-row kernel. Banked below.

This is the run where the device byte-exact gate (#298) earned its keep: a fast-but-wrong
build could not be logged as a win, and the host gates are structurally blind to it (x86
compiles the C path).

### 22C - wide-load fill order: MEASURED, CLOSED - the +21/+25 % was lost nibble identity

Two hand-computable probes, one group, norm 1.0, on board 3 (kbench):

| probe | construction | expected | `tie0` plain `lsi` | `ee.ldf.128.ip` | `ee.ldf.64.ip` |
|---|---|---|---|---|---|
| A | nibble k = k, `xh[j]=2^(j%8)`, `cb[i]=i` | 24608 (= 16 x sum k*2^k) | **24608.0** | 4080.0 | 4080.0 |
| B | every nibble = 1, `cb[i]=1` (permutation-invariant) | 4080 (= 16 x 255) | **4080.0** | 4080.0 | 4080.0 |

Read the columns: the plain form is exact on both, so its arithmetic is right by
construction on hand-computable inputs as well as bit-exact on real archive bytes. The two
wide forms answer A with **exactly B's value**, i.e. with distinct nibbles they behave as
if every lane used one codebook operand - the nibble identity is lost, not the float
order. So the register-fill-order question that #332 left open has an answer, and it is
the negative one: no permutation of the register list fixes them (the decoder in
`.auto/exp22/fill_decode.py` enumerates all 40,320 permutations and none reproduces
4080 from probe A), because the defect is upstream of the float loads. The extra ~10 %
above the plain form is not available by this route, and 22A ships the plain form.

### Banked follow-ups from Experiment 22

- **Multi-row differential for `nd_gemv4_rows_tie1`** (cheap, one kbench build): compare the
  row-range kernel over 4/12/24 rows against the same number of per-row calls of the proven
  single-row screen kernel, row-for-row, on real phi bytes. This is the test whose absence
  cost run #333's first build; it should exist before the kernel is touched again.
- **Cross-board confirmation of 5.0300 on board 2** (discovery -> confirmed), while board 3
  keeps screening.
- The gate `nd_gemv4_asm_ok` walks the norms once per (blob, rows) into a 4-entry cache; a
  fifth 4-bit tensor per token would re-walk. Currently phi's three tensors are the only
  4-bit traffic (`nd_cq_gemv_rows` has exactly three call sites), so do not "generalise" it.

## Experiment 23 CLOSED: there is no `div.s` on this core, and it does not matter (run #338)

The audit in run #337 found a fact that looked like several percent: the accepted
image contains ZERO `div.s` instructions, so every one of the ~8-10k single-precision
divisions per decode token (two per sigmoid pair alone) is a call to the ROM
software divider `__divsf3` at 0x40002274. Division is also the one arithmetic
transformation that is *admissible* under a byte-exact gate, because IEEE-754
specifies correct rounding - there is exactly one right answer, so a faster
correctly-rounded divider returns identical bits.

Built exactly that (`.auto/divf3/nd_div.h`, kept): magic-subtract inverse, three
Newton refinements in hardware fma, one quotient multiply, exact residual,
Markstein correction, behind an integer-only guard (both operands normal, quotient
exponent inside the normal range) with everything else falling through to `a / b`
so subnormals/zeros/infinities/NaN payloads keep the current implementation
bit-for-bit. Verified against the host's correctly-rounded hardware divider over
**1,572,864 pairs in six classes with 0 mismatches** - and the test `#include`s the
shipping header rather than a copy, which is the only reason that number means
anything. All 15 engine division sites converted: host 16/16 byte-exact, fidelity
5.341e-05 unchanged, device 17/17 byte-exact. Pure speed verdict, and it lost:
**decode 5.0017 vs 5.0300 = -0.56 %**, boot bench down too.

Why, measured in the same kbench run (board 2, min of 25 rounds, 256 rotated
real-shaped operand pairs, float-register barrier, both variants behind a noinline
function so call overhead cancels): **rom_divsf3 = 7.95 cycles/divide,
newton_markstein = 30.92, a bare multiply as the loop floor = 2.92, bit_mismatch =
0.** The prediction from those numbers (-0.49 %) matched the measurement (-0.56 %).

Read the floor column, not the candidate column: 7.95 - 2.92 = ~5 net cycles for a
software divide is FMA-chain territory, which means Espressif's ROM divider is
almost certainly hand-written TIE reciprocal-refinement - the same algorithm I
wrote, without the float<->integer memory round trips (`nd_f2u`/`nd_u2f`) that runs
#230/#292 already proved are expensive on this core. Ceiling for any future
handwritten divider: ~5 cycles x 10k divides = 0.2 ms = **+0.1 %, below the keep bar
before risk is even priced**. Division is closed.

Method worth keeping: this closure cost one host test and one kbench image. The
audit that produced the hypothesis was `objdump` plus IDF's own
`esp_rom/esp32s3/*.pro` symbol maps (memcpy = 0x400011e8, __divsf3 = 0x40002274),
which took minutes and produced a number the campaign had never had. An unusual
instruction-count observation is worth one cheap measurement before it is worth a
design - here the premise ("software divide must be ~150-400 cycles") died
immediately, and the campaign's remaining time stays on candidates.
