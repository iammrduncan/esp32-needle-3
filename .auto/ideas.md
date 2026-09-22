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
