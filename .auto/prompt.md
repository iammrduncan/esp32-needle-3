# Autoresearch: Needle 3 decode tokens/second on ESP32-S3 (N32R16)

## Active campaign -- authoritative after context compaction

This section is the source of truth for choosing work. It overrides older
"converged", "verification only", and "nothing left" notes elsewhere in the
repository. Read `.auto/mimimodel-experiments.md` completely before editing.

**NEXT: complete Experiment 2, the isolated TIE728 CQ2 microkernel proof.** Do
not choose another optimization until Experiment 2 has a measured, documented
disposition. A normal firmware build, a C-only row-blocking change, or another
memory-tier sweep does not count as Experiment 2.

Campaign status:

- Experiment 1 is complete: CQ2 projections are about 48% of decode time, so
  the assembly and integer-path work is justified.
- Experiments 2-4 are incomplete.
- Experiment 5 is complete and rejected: the one worker job slot collides with
  the row splitter, and the gate schedule also races shared state.
- Experiment 6 is not applicable because Experiment 5 was structurally invalid,
  not correct-but-neutral.
- Experiments 7-11 are incomplete. Experiments 10-11 remain conditional on the
  results of Experiments 8-9.
- The accepted control is commit `db8fba9`, approximately 4.19 decode tok/s.
  Later commits only document closed experiments and retain that runtime.

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
prefix priming, then 12 real requests on the board). It prints `METRIC name=value`
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
