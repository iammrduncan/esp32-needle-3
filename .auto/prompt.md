# Autoresearch: Needle 3 decode tokens/second on ESP32-S3 (N32R16)

## Active campaign -- authoritative after context compaction

This section is the source of truth for choosing work. It overrides older
"converged", "verification only", and "nothing left" notes elsewhere in the
repository. Read `.auto/mimimodel-experiments.md` completely before editing.

**CURRENT STATE (run #241, authoritative).** Accepted runtime at commit `674b168`:
**4.9417 decode tok/s (+102.6 % over the 2.44 baseline)**, 4.8614 extended, 3.91 think,
5.2667 prefill, boot bench 198 ms/token, 14/14 device + 13/13 host byte-exact,
fidelity 5.341e-05 / top1 10/10, internal_free 15247. Experiment 12 is kept
(paired attention `nd_expf`, isolated +19.96 %, end to end +0.48 %); Experiment 13 is measured
and rejected; Experiment 17 is measured and rejected; Experiment 18 is kept (+0.204 % exact KV
reciprocal); the 100 Hz FreeRTOS tick is kept (+0.34 %) and its behavioural capture is green.
**Experiment 14 is measured and closed as rejected (run #286); Experiment 15 is measured
and CLOSED as rejected (run #287, three boards: overlapped -17.9 % to -67.8 %, internal-RAM
ceiling +0.11 %, one cache invalidate 1.416 M cycles); Experiment 16 is measured and KEPT
(+1.01 %, 4.9417 -> 4.9917, which is the accepted runtime). Only Experiment 19 remains
unperformed.** Their evidence and recipes are in `.auto/ideas.md`.

**Operator intervention after run #284.** Runs after the accepted gate fell into a canonical-repeat
loop: the same `674b168` image returned 4.9417 dozens of times with no hypothesis or code change.
These readings are not experiments and add no evidence. The exhausted agent session was terminated.
Never resume that loop, even if the generic recurring runner says to call `run_experiment` at the end
of an iteration. The next `run_experiment` invocation must contain a real candidate that differs from
`674b168`; otherwise continue implementing or screening off-device and do not measure.

**Superseded state (run #227).** 4.8917 decode, 4.81 extended, 3.87 think, 5.2167 prefill,
boot bench 201 ms/token, internal_free 15759.
Repeated three-board batches agree to the last digit, so there is no board drift and the metric is
deterministic to ~0.04 % (one quantisation tick), not noisy. Runs #222-#227 were unchanged
verification repeats and added no information; do not continue that pattern.

The campaign's productive lens at the end was **request-path work the boot bench cannot see that
runs while core 1 is idle**; it yielded #147 (first-byte legality table, +2.16 %) and #176
(two-core legality filter, +0.20 %). Everything else in the token is now two-core or at a named
floor, and the closures with evidence are in `.auto/ideas.md`. Cadence rules that apply now:
run `make capture` **when the shipping code changes** (green at #188 on this code; a periodic
re-run with no code change adds cost and no information), couple three-board controls to novel
candidates instead of running periodic baseline-only batches, and treat any candidate below the
0.2 % keep bar as not worth a build.

**Experiment 16 is measured and KEPT (run #288, +1.01 %: 4.9417 -> 4.9917 decode, 14/14 +
13/13 byte-exact, capture green).** Accepted runtime is now **4.9917 decode tok/s**.
**NEXT: Experiment 19, the 120 MHz octal-memory diagnostic - isolated, non-shipping by
default** (ESP-IDF labels 120 MHz DDR experimental; it is a thermal/stability campaign, not
a free clock, and its result must be measured against two 80 MHz controls with board
assignments swapped).
Experiment 14 is measured and CLOSED as rejected (run #286): the real layer-0 `q_proj` fixture
replayed bit-exact 1024/1024, int8 CQ2 on ONE tensor moved `logit_max_delta` to 0.3282 (164x the
2e-3 gate) and the best case of the whole integer family - int16 activation with an exact codebook
- still hit 0.01818 (9.1x), while the 13/13 byte-exact host goldens stayed green. Numbers, the
kept fixture and the harness trap are in `.auto/ideas.md`. Proceed 15, then 16, then the
isolated/non-shipping 19 diagnostic. The accepted control is **4.9417 decode tok/s**. Reaching
5.00 requires saving about 2.36 ms from the ~202.36 ms request token. Earlier analytical closures
are hypotheses, not substitutes for these concrete screens.

### Research loop rules

1. **No unchanged verification loops, with no per-iteration exception.** Do not run or log the
   accepted image by itself. A control belongs in the same concurrent batch as a novel candidate.
   Context pressure, a generic instruction to produce a run each iteration, or wanting a noise datum
   is not permission to run the control. Persist work and continue the candidate instead.
2. Work on the first unmeasured experiment below. Each experiment must end in a concise measured
   disposition in `.auto/ideas.md` and `.auto/log.jsonl`: hypothesis, implementation/variant,
   image hashes and board assignments, isolated timing where requested, full-model delta, quality,
   memory, and accept/reject reason. Then immediately advance to the next experiment.
3. Use all three boards concurrently whenever images are ready: board 1 control, boards 2 and 3
   two candidate variants or duplicate candidates. For a plausible winner, repeat with assignments
   swapped. Fresh-configure every compile-time variant and verify `compile_commands.json` plus
   image hashes before flashing.
4. Screen cheaply with real captured inputs and a device microbenchmark. Run the full device suite
   only for a correct, plausibly faster screen. A shipping candidate must retain 14/14 device and
   13/13 host byte-exact output, token delta 0, the existing fidelity threshold, top1 10/10, and all
   ordinary tests. Run `make capture` after a shipping code/config change, not periodically when the
   code is unchanged.
5. Change one lever per candidate. Revert rejected implementation code before starting the next
   experiment. Never share mutable static scratch between `nd_parallel_rows` workers. Record internal
   free RAM for every full candidate; only 15,759 bytes remain on the accepted control.
6. An analytical objection is a prediction to test, not a disposition. Stop an experiment without
   device measurement only for a concrete build/ABI/hardware blocker, and record that exact blocker.

### Ordered open experiments

**Experiment 12 -- paired/interleaved attention exponential.** Disassemble the current `nd_expf`
calls in `attn_heads`. Capture the real input pairs reaching the adjacent `w0`/`w1` calls. Build a
device microbenchmark for `nd_expf_pair(a,b)` that interleaves the two degree-5 polynomial dependency
chains while preserving each scalar chain's operation order, clamp behavior, and output bits. Try
carefully structured C and, if C does not schedule it, handwritten Xtensa/TIE728. Report cycles per
pair over captured inputs and bit mismatches versus two scalar calls. Integrate only a bit-exact,
faster kernel, then measure attention phase and end-to-end decode.

**Experiment 13 -- ESP-DSP S3 dot-product audit.** Benchmark Espressif's optimized
`dsps_dotprod_f32_aes3` against the current 64-wide attention Q.K dot and representative small
Kronecker dot shapes, using the real alignments. Also inspect/borrow its instruction schedule in a
specialized inline-free local kernel so component-call overhead does not decide the result. Report
isolated cycles and numeric deltas. Full-model-test only the shapes that win; reject any reduction
order that fails the existing output/fidelity gates.

**Experiment 14 -- perform the original CQ2 integer feasibility screen.** The old Experiment 8 was
not performed as specified and is reopened. Use real CQ2 shapes, codebooks, row norms, alignments,
and captured prepared activations. Measure both (a) packed 2-bit indices with int8 activation and
codebook arithmetic and (b) one representative tensor expanded to int8 rows in PSRAM. Test tensor
and per-group scales. Report quantization cost, bytes read, staging bytes, cycles, maximum/mean error,
and fidelity/top1. If a scalar representation passes the numeric screen, benchmark
`dsps_dp_s8_aes3` or a handwritten S3 integer dot before deciding whether the path is bandwidth-bound.
Do not expand to full integration unless the screen is both acceptable and faster.

**Experiment 15 -- operator-internal GDMA double buffering.** This is distinct from the rejected
cross-operator worker overlap. Use ESP32-S3 AHB GDMA async memcpy to prefetch the next sequential
PSRAM weight block into one of two small DMA-capable internal buffers while TIE728 consumes the
current buffer. Start with 2 KiB and 4 KiB buffers and representative dominant CQ2 shapes. Measure
copy-only bandwidth, compute-only time, overlapped time, wait time, and heap impact. Verify cache/DMA
coherency explicitly. Integrate only if overlap beats direct cached PSRAM reads and fits safely.

**Experiment 16 -- compact first-byte grammar index.** Build once a PSRAM-resident vocabulary index
of ascending `uint16_t` token IDs grouped by first byte plus 257 offsets. At each grammar state,
enumerate only allowed-byte buckets; preserve the exact legal candidate set and restore globally
ascending token-ID order before logits and tie-breaking. Do not repeat the rejected 8 KiB internal
first-byte cache. Microbenchmark table build, per-token filtering on captured real grammar states,
memory, and complete candidate-list equality before end-to-end measurement.

**Experiment 17 -- selective hot-code IRAM audit.** Use the map file and disassembly to determine
whether `attn_heads`, the hot grammar traversal, subset logits, or paired-exp helper execute from
flash. Move one measured hot function at a time to IRAM; record IRAM/DRAM movement and end-to-end
timing. Do not blanket-annotate functions and do not retain a placement that endangers the current
internal-memory margin.

**Experiment 18 -- exact KV reciprocal screen.** Microbenchmark Xtensa `recip0.s` plus Newton
refinement for the divisions in KV int8 storage and any similarly shaped measured division hotspot.
First compare produced int8 KV-cache bytes over captured real inputs; a KV candidate is eligible only
if every byte matches the control. Report cycles and full `kv_store_int8` phase time. Reject quickly
if the 1.1 ms phase cannot yield a measurable full-token improvement.

**Experiment 19 -- 120 MHz octal-memory diagnostic, isolated and non-shipping by default.** Build one
120 MHz octal flash/PSRAM candidate against two 80 MHz controls to test whether external-memory clock
is the remaining CQ2 limit. Record boot/config evidence, temperatures, phase timings, and correctness.
If it wins, repeat with board assignments swapped and perform a long hot/cold thermal soak with memory
integrity checks. ESP-IDF labels 120 MHz DDR experimental; do not call it shipping-safe or make it the
default unless the stability campaign passes and temperature tuning/recovery is addressed.

After Experiment 19, derive the next candidate from the measured winners and remaining phase map.
Do not fall back to baseline-only runs. If no experiment wins, write a final evidence table and stop
the campaign cleanly rather than manufacturing verification work.

Historical closed families remain closed unless an experiment above explicitly distinguishes itself:
PSRAM tier span/stride/copy-limit sweeps; C row-block/order variants; packed load-width sweeps; and
the old worker-slot cross-operator schedules. Experiments 15 and 19 are explicitly new memory-system
tests, not permission to repeat those old variants.

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
