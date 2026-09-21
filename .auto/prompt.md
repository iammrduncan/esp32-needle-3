# Autoresearch: Needle 3 decode tokens/second on ESP32-S3 (N32R16)

## Active campaign -- authoritative after context compaction

This section is the source of truth for choosing work. It overrides older
"converged", "verification only", and "nothing left" notes elsewhere in the
repository. Read `.auto/mimimodel-experiments.md` completely before editing.

**CURRENT STATE (run #201, authoritative).** Accepted runtime: **4.8917 decode tok/s (+100.5 %
over the 2.44 baseline)**, 4.81 extended, 3.87 think, 5.2167 prefill, boot bench 201 ms/token,
14/14 device + 13/13 host byte-exact, fidelity 5.341e-05 / top1 10/10, internal_free 15759.
Seven three-board batches agree to the last digit, so there is no board drift and the metric is
deterministic to ~0.04 % (one quantisation tick), not noisy.

The campaign's productive lens at the end was **request-path work the boot bench cannot see that
runs while core 1 is idle**; it yielded #147 (first-byte legality table, +2.16 %) and #176
(two-core legality filter, +0.20 %). Everything else in the token is now two-core or at a named
floor, and the closures with evidence are in `.auto/ideas.md`. Cadence rules that apply now:
run `make capture` **when the shipping code changes** (green at #188 on this code; a periodic
re-run with no code change adds cost and no information), keep the three-board batch every ~5
cycles for drift, and treat any candidate below the 0.2 % keep bar as not worth a build.

**NEXT: no open experiment remains.** The MimiModel queue is dispositioned (Experiments 1-6:
1 complete, 2-4 complete and *shipped* as the TIE728 CQ2 kernel at +13.0 % decode, 5 rejected as
structurally invalid, 6 not applicable; 7-11 closed against the measured phase map). Since then the
**sampler** turned out to be a real lever, because the boot bench never runs it: a first-byte
legality table for the constrained sampler took decode from 4.7783 to **4.8817 tok/s (+2.16 %**,
byte-exact 12/12 device + 11/11 host, extended +2.13 %, worst case +2.42 %, confirmed on two
independent boards against a control that reproduced the old value exactly). That closes the last
phase that was not at a named floor.

The accepted control is the current HEAD at **4.8917 decode tok/s** (+100.5 % over the 2.44
baseline): 4.8817 plus the two-core sampler-filter split (#176, confirmed by batch at
#177 with the control reading 4.8817 exactly). The 4-bit folded codebook that briefly reached 4.8917 was withdrawn at #162:
as implemented it wrote one static table from both cores, which `rows_dual_core`
runs concurrently with no barrier - byte-exactness passed by timing luck. The
race-free variant measured worse. **Audit rule now in force: any kernel handed to
`nd_parallel_rows` must be checked for shared mutable state, not only for matching
numbers** (the audit of every kept split kernel is in `.auto/ideas.md`; all clean); every phase above 0.5 % of the ~205 ms token is now at a floor - 2-bit GEMV ~53 %
(TIE728 instruction floor), attention heads ~18 %, MLP `kron_apply` ~11 %, engram ~8 %, 4-bit mHC
phi ~6.7 % (non-resident PSRAM working set), constrained sampler ~1 % (was 7 %), subset logits
~3.5 %.

Work that is still worth spending a run on, in order:

1. **Verification cycles** on HEAD (the metric is stable to +-0.05 % across
   boards, so a control that reads low is a build-integrity failure - fresh
   configure every board and check `compile_commands.json`).
2. **`make capture`** (`demo/capture.py --out /tmp/recording.json`, driven
   through `tools/serial_api.py` on the board's console port) every ~10 kept
   changes: it is the only behavioural check of routing, second-pass tool
   execution and timer expiry, which the 14-case byte-exact suite does not
   cover. Last green: run #143 tree, all 7 scenarios; due again shortly.
2a. **`make capture` is green again (run #169, previous #157)** on the 4.8817 runtime: 7 real
   scenarios, all 9 verification flags true (routes_match, tools_match,
   requests_succeeded, no_external_calls, local_has_two_passes,
   external_stops_at_selection, telemetry_progressed,
   sampling_interval_applied, timer_expired). Run it inside the board namespace
   with `needle-api --serial /dev/ttyACM1` - **/dev/ttyACM0 is the flash port**,
   and pointing the API at it produces write timeouts that look like a dead
   console. Due again in ~10 kept changes.
2b. **Closed now, but the pattern is the one to reuse:** the sampler win came
   from *repeating the cheap part of a hot predicate per small domain* (256 byte
   values per state) instead of trying to skip it. Memoizing the same work on the
   grammar state measured exactly zero, because the state changes on essentially
   every accepted token. Two corollaries recorded in `.auto/ideas.md`: microbench
   a predicate on the states the real loop sees (a fresh-open grammar state made
   my estimate 3x optimistic), and the paths the boot bench cannot run - sampler,
   per-token emit - are the only territory that was still unexplored.
2c. **The request-path phase map is now measured on the shipping tree (run #166), and it
   closes the sampler family on data rather than arithmetic**: `sample` is 8.9 ms of the
   ~202 ms request token and 5.7 ms of that is the subset-logits projection, so there is
   ~3 ms of sampler arithmetic left and nothing above it is unaccounted. Reproduce with
   `AUTO_PROFILE=1` plus a direct `tools/serial_api.py` `Device` drive - and pass
   `think=False` or send `!think 0` first, or you will measure the 8192-row
   full-vocabulary path instead (that trap showed `logits4` at 63 ms and looked like a
   30 % phase).

2d. **The bench-invisible part of the request path is where the wins are, and it has now
   yielded two (#147 first-byte table +2.16 %, #176 split legality filter +0.20 %).** The
   rule that found both: after the boot-bench phase map is exhausted, look for request-path
   work that (a) the bench cannot see and (b) runs while core 1 is idle. Sampling is exactly
   that point, because the next token's projections depend on the token being sampled. What
   is left there is under 0.8 % of the token.

2e. **Two more candidates closed by analysis, so do not spend a run on them.**
   `kv_store_int8` (1.1 ms, 0.55 %) is divide-bound - one software `__divsf3` per element,
   ~340 cycles/element implied - but it has only 2 units per call (k and v per KV head) so
   `rows_dual_core` would run it on one core, and re-rolling it as one 4-unit split per layer
   costs 8 handshakes a token against a 0.55 ms prize, which the measured +0.04..0.13 % band
   for splits of this size says loses. Replacing `x / scale` with `x * (1/scale)` would be
   fast, and the fidelity probe would probably allow it, but it changes the int8 rounding of
   the KV cache - the model's memory - and 14 prompts cannot certify that. Not shipped.

2f. **Tier family is closed on every axis, including low-end coverage (run #206).** The
   logits embedding was outside the staged span and could be pulled in for free; staging it
   changed decode by exactly nothing (and think too, which reads all 3.14 MB per token). The
   tier's value is specific to big sequential per-layer projection reads. `ND_TIER_TRACE=1` is
   the knob that answers residency questions in one build - use it instead of reasoning.

3. **Sub-1 % candidates, only if a floor is shown to be wrong**: `lsc`-pairing
   the 4-bit loop's activation loads, an asm `kron_apply`, asm around online
   softmax. Each needs a *new* measurement that contradicts the floor before it
   is worth a build.

If a future archive changes phi's `in_pad`/`group`, or the PSRAM tier or
fp32-pool geometry changes, re-measure the phase map once (`AUTO_PROFILE=1`,
~8 min) before choosing.

Closed families -- **do not build, flash, or measure these again** unless this
section is deliberately updated first:

- PSRAM tier span, stride, copy order, copy limit, or allocation ceiling;
- unchanged plateau/control verification;
- C row blocking or row-order variants;
- 32/64/128-bit packed-load-width sweeps;
- async cross-operator overlap with the existing worker slot.

Experiment 2 must produce all of the following:

1. A device-side microbenchmark comparing the existing C `lut2_rows` kernel
   with handwritten Xtensa/TIE728 assembly.
2. Needle 3's real LUT/index/group layout and dominant projection shapes,
   including 768x768; do not copy MimiModel layout assumptions.
3. Isolated kernel timing, maximum/mean numeric error, alignment requirements,
   and an inspection of generated/handwritten instructions.
4. A measured disposition: faster and eligible for Experiment 3, or rejected
   with evidence. If assembly syntax, ABI, or hardware support blocks the test,
   record the exact blocker instead of silently switching experiments.
5. A concise result in `.auto/ideas.md` and `.auto/log.jsonl` before selecting
   any next experiment.

### Three-board measurement protocol

Use all three ESP32-S3 boards concurrently whenever device images are ready;
never serialize three independent full device suites.

- Build candidate images first, using a fresh build directory per compile-time
  variant, and verify that intended flags appear in `compile_commands.json` and
  that meaningfully different variants do not accidentally have the same hash.
- For an isolated Experiment 2 microbenchmark, assign board 1 to the existing C
  control, board 2 to assembly candidate A, and board 3 to assembly candidate B
  or a duplicate control/noise check. Start them together with
  `/root/bin/image-batch.sh 1=<control.bin> 2=<candidate-a.bin> 3=<candidate-b.bin>`.
- For a promising full-model candidate, use one control and two identical
  candidate images in the first batch. Swap board assignments in the repeat so
  a board-specific effect cannot masquerade as a win.
- Use short, microbenchmark-specific firmware runs for kernel screening. Spend
  the 12-case full suite only after an isolated candidate is correct and faster.
- Never allow multiple processes to access one board outside `needle-board run`;
  the batch helper already acquires one lock per board.
- Record image hashes, board assignments, batch directory, raw kernel timings,
  quality results, and the candidate/control delta. A build-only result is not
  evidence.

Before every build, state the active experiment and hypothesis in the run notes.
If the proposed command belongs to a closed family above, abort it and return to
Experiment 2.

## Objective

Make the shipping inference path on the attached board decode faster, without
buying speed with model quality. The workload is one greedy, grammar-constrained
decode of the Needle 3 tool-calling model (8 layers, d_model 768, 12 heads / 2 KV
heads, 4 mHC lanes, Monarch Hadamard MLP width 1024, engram sites at layers 4 and
7, context 384) over a flash-mapped `.cact` archive. Weights stream through the
cache straight out of flash; there is no copy into RAM.

Baseline at session start: **~1.23 tok/s decode, ~1.26 tok/s prefill**, 8 layers,
16,155,796-byte archive, ~37 KB internal heap free, ~13.6 MB PSRAM free. Host
(x86, single core) runs the same engine at ~86 tok/s, so the device is not
compute-limited in the same way — do not trust host speed as a proxy.

## Metrics

- **Primary**: `decode_tps` — mean of the device-reported `EVT done ... tps=` over
  the six frozen `primary` prompts (higher is better).
- **Secondary monitors**: `ext_decode_tps` (5 held-out/extra prompts),
  `think_tps` (unconstrained full-vocabulary path — a different code path),
  `prefill_tps`, `boot_bench_tps` (firmware's own 6-token boot bench),
  `min_case_tps`, `gen_tokens`, `internal_free`, `psram_free`,
  `device_output_exact`/`device_token_delta`, `host_output_exact`,
  `logit_max_delta`, `logit_top1_match`.
- Secondaries are tradeoff monitors. They never justify keeping a primary
  regression, and the quality ones can veto a primary win (see Constraints).

## How to Run

`./.auto/measure.sh` (~10-13 min: host pre-check, `idf.py build`, flash, boot +
prefix priming, then 14 real requests on the board (6 primary + 7 extended + 1 think)). It prints `METRIC name=value`
lines plus `CASE`, `GROUP`, `BOOT`, `PROF` diagnostics.

`./.auto/checks.sh` runs the correctness/quality gates (host build, repo tests,
frozen-input guard, byte-exact generation, forward-pass numeric fidelity).

Useful env knobs on `measure.sh` (none of them change what the metric means):
`AUTO_PROFILE=1` for the per-phase breakdown, `AUTO_NOFLASH=1` to re-measure the
already-flashed app, `AUTO_GROUPS=primary` for a short run, `AUTO_SAVE=1` to
rewrite the device golden (**only** for a documented re-baseline).

## Benchmark integrity — read this before touching anything

- The prompt set in `.auto/prompts.json` is frozen: `primary` defines the metric
  and must not be edited, reordered or trimmed. Adding coverage to `extended` is
  welcome; shrinking `primary` is cheating.
- Speed authority is the **device**. The host build is the *quality* authority.
- A candidate is keepable only if the generated text is **byte-identical** to the
  frozen goldens, on host (all 11 prompts) and on device (`device_output_exact`
  equals the case count). If text diverges, the change is a quality change:
  discard it, whatever it does to tok/s.
- `logit_max_delta` (fixed 10-step forward probe, full 8192-row logits) must stay
  under 2e-3; it separates "1 ULP from a reassociation" from "the model is now
  doing something different". It is a veto, not a licence to diverge.
- Forbidden as a means of going faster: changing layer count, the archive, its
  quantisation, the KV codebook/`kv_bits`, context length, vocabulary, the tool
  or route schemas, greedy sampling, the grammar, or the prompt set. Forbidden
  outright: overclocking beyond the documented 240 MHz / 80 MHz octal
  flash+PSRAM, benchmark-only fast paths, skipping warmup, or reporting a metric
  other than the device's own decode timing.
- The model quality that matters is real routing/tool behaviour, and the board can
  only be measured on a handful of prompts. That is exactly why the goldens are
  byte-exact and the held-out cases exist. Do not "improve" a case by changing
  what it expects.
- Beware the metric's own blind spots: tok/s is per-request and ignores prefill,
  and a change that only helps the 4-bit path can regress the dominant 2-bit path
  (and vice versa). `think_tps` and `ext_decode_tps` are there to catch that.

## Files in scope

- `engine/src/nd_quant.c` — CQ GEMV kernels: 2-bit pair-LUT path, 4-bit generic
  path, FWHT activation prep, LUT build, gather. Most decode time lives here.
- `engine/src/nd_model.c` — forward pass: mHC lanes, phi GEMVs, QKV convolution
  taps, per-head RMSNorms, RoPE, int8 KV cache, online-softmax attention,
  Monarch Hadamard MLP, engram gather, Sinkhorn, confidence pooling.
- `engine/src/nd_sample.c` — constrained greedy sampling, grammar-candidate
  enumeration, subset logits.
- `engine/src/nd_grammar.c`, `nd_tokenizer.c`, `nd_cact.c` — schema-constrained
  decoding, SentencePiece lookup, archive reader.
- `engine/include/*.h` — kernels/structs; `ND_PROFILE` phase enum lives in
  `nd_model.h`.
- `esp32/main/main.c` — dual-core row splitter worker, prefix priming, request
  loop, boot bench.
- `esp32/sdkconfig.defaults` — cache size/line, CPU 240 MHz, octal flash/PSRAM
  80 MHz. (`esp32/sdkconfig` is gitignored: always edit the defaults file, then
  delete `esp32/sdkconfig` to regenerate.)
- `esp32/main/router.c`, `esp32/partitions.csv`, `tools/`, `host/` — read freely;
  touch only to fix a real bug, and `partitions.csv`/schemas are frozen.

## Off limits

`model/manifest.json`, `model/needle3.cact`, `tools/demo-tools.json`,
`tools/model-routes.json`, `tools/model-catalog.json`, `esp32/partitions.csv`,
`.auto/prompts.json` (`primary`), `.auto/golden/*`.

## Constraints

- `make test` equivalents must pass (10 Python + grammar + prefix-isolation tests)
  — the prefix-isolation test is the one that proves schema switching does not
  leak KV/conv state, keep it green.
- No new dependencies. No new threads beyond the existing two-core split.
- Internal SRAM headroom is ~37 KB. Anything sized in internal RAM must fit.
- Keep the `ponytail:`-free, heavily commented style of the engine: comments in
  this repo explain *why* a kernel is shaped the way it is, and several record a
  measured dead end. Preserve them; add new ones for new constraints.
- Device changes are expensive to test (~12 min/run). Think and use the host
  build + `AUTO_PROFILE=1` before spending a device run.

## What's Been Tried

(Update as experiments accumulate.)
