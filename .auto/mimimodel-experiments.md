# MimiModel follow-up experiment queue

## Execution status

Historical queue: Experiments 1-4 were completed with real device work;
Experiment 5 was tested and rejected; Experiment 6 was not applicable. The
later "Experiments 7-11 CLOSED" table records useful phase-map predictions, but
Experiments 7-9 and 11 were not all built as their original specifications
required. In particular, Experiment 8's requested integer numerical/device
microbenchmark was not performed.

**The top of `.auto/prompt.md` is the sole current authority after compaction.**
It reopens the missing concrete screen as Experiment 14 and defines new
Experiments 12-19, their order, three-board protocol, quality gates, and stop
conditions. Do not use this historical document to select "Experiment 2 only"
or to resume unchanged verification cycles.

Prepared 2026-09-19 from `memovai/mimimodel` at commit
`1a19329707c5ca8b9833ef9f079c796647887eba`.

Primary references:

- https://github.com/memovai/mimimodel/blob/main/README.md
- https://github.com/memovai/mimimodel/blob/main/docs/esp32s3-tie728-audit.md
- https://github.com/memovai/mimimodel/blob/main/docs/esp32s3-overlap-audit.md
- https://github.com/memovai/mimimodel/blob/main/needle-esp32s3/main/cq2_dot_tie728.S

## Correction and scope

MimiModel does **not** report Needle 2 at 10 tok/s. Its current README reports
2.11 prefill tok/s and 1.73 decode tok/s on ESP32-S3. The 9.9 tok/s figure in
its credits belongs to `slvDev/esp32-ai`, a different 28.9M-parameter model
using Per-Layer Embeddings. Do not use 10 tok/s as an apples-to-apples target.

Several MimiModel ideas are already present here: dual-core row splitting,
hidden-only prefill/no full logits head, fast internal scratch, and a dynamic
PSRAM weight tier. Do not repeat those as if they were new experiments.

Two MimiModel techniques have **not** been tested equivalently here:

1. Its CQ2 TIE728 kernel is handwritten Xtensa assembly with aligned
   `ee.ldf.128.ip` float loads and eight independent float accumulators over two
   output rows. Previous Needle 3 C row blocking and packed-word-load tests are
   not an equivalent test.
2. Its cross-operator scheduler leaves a worker job in flight while the other
   core performs an independent operator. Needle 3 currently splits an
   individual operator and joins inside `nd_parallel_rows`; this is not
   cross-operator overlap.

## Operating rules for this queue

- Stop running unchanged plateau re-verifications. The accepted control is
  commit `c03d1dd`, approximately 4.185 tok/s, unless a later accepted commit
  replaces it.
- Run novel candidates for the next several experiments. Use all three boards:
  one control and up to two candidates in a batch whenever candidate builds are
  ready. Serial candidate preparation is acceptable; device measurement should
  be parallel.
- Keep the existing quality gates: device output 12/12 exact, host output 11/11
  exact, token delta 0, logit max delta within the current tolerance, top-1
  match, and all tests green.
- Use two timing cycles for screening. Repeat enough cycles to distinguish a
  candidate from the observed noise only after it shows a plausible gain.
- One behavioral idea per candidate. Revert rejected code before the next
  candidate. Record measured sub-kernel timing where possible, not only total
  tok/s.
- Do not weaken model quality, prompts, generated-token count, or benchmark
  coverage to improve the score.

## Experiment 1: profile the actual CQ2 hot path

Hypothesis: the TIE728 work is worthwhile only if the CQ2 LUT GEMV still owns a
large enough fraction of decode time.

Instrument representative decode tokens with low-overhead aggregate timers.
Separate CQ2 projection time from attention, MLP/Kronecker, mHC, and scheduler
overhead. Within CQ2, sample q/k/v/gate/out and the mHC projections. Run the
instrumented build on one candidate board while another board runs the clean
control. Remove instrumentation after collecting the totals.

Decision: continue with Experiments 2-4 if CQ2 projection time is material.
Otherwise record the profile and prioritize Experiment 5.

## Experiment 2: isolated TIE728 microkernel proof

Hypothesis: an assembly inner loop matching Needle 3's actual CQ2/LUT layout
can beat the current C `lut2_rows` loop without changing its arithmetic.

Build a device-side microbenchmark around the exact shapes and alignment used
by `nd_cq_gemv_lut2`. Implement a narrow Xtensa assembly prototype that:

- consumes Needle 3's real LUT/index/group layout rather than copying
  MimiModel's layout assumptions;
- processes two output rows together;
- uses aligned 128-bit float loads where alignment is guaranteed;
- maintains enough independent accumulators to hide load/FPU latency;
- preserves the current accumulation order closely enough to pass the numeric
  and output gates.

Measure current C versus assembly for at least 768x768 and the other dominant
projection shapes. Validate against the C result before integrating it.

Decision: integrate only if the microkernel is consistently faster and its
error stays inside the existing gates. A build-only result is not evidence.

### Measured disposition (run #137, 2026-09-20): FASTER, integrated.

`engine/src/lut2_tie728.S` + `esp32/main/kbench.c` (`NEEDLE_KBENCH` +
`NEEDLE_KBENCH_ASM`), three boards, Needle 3's own pair tables and real blobs,
isolated and two-core-split timings:

| kernel | structure | saving vs C, split mode |
|---|---|---|
| `tie1`  | one row, 4 partials, 8 pairs (16 weights) per gather batch | **+32.4 %** |
| `tie2`  | two rows interleaved, 4-deep | +25.7 % |
| C       | `nd_lut2_rows_c` | - |

`exact=768/768 bitexact=1` on 768x768, 576x768, 128x768 and 96x96 - the assembly
is byte-identical to C, not merely inside the fidelity tolerance, because it
keeps C's nibble order, C's four-partial fold and C's FP16->FP32 bit arithmetic.
Alignment: needs the 4-byte packed-row alignment every CQ row already has, and
group 128 (32 packed bytes, 64 pairs); `nd_lut2_asm_ok()` also refuses any norm
whose FP16 exponent is 0 or 31, because the kernel inlines `nd_f16()`'s normal
path only. Cost: 1 KB of internal RAM for the IRAM kernel.

Two real bugs were found only by *synthetic* mapping checks (pair table set to
1.0 and norms set to 1,2,3,... so a stride or order mistake shows up as an
arithmetic difference, not a rounding one): `tie2` advanced its row cursors by
one rowbytes per row *pair*, and a debug probe writing through `ctx->y` was
silently corrupting the reference rows. Build traps worth not re-deriving:
`call8` needs a `(16 + n_out_regs) * 4`-byte frame (64 bytes here corrupted
a12/a13 and died in `retw` as a double exception with no usable panic), and a
`.S` file cannot reference a `static` C function (`-mlongcalls`/LTO reports it
as an undefined `ldext.S`).

## Experiment 3: integrate the winning TIE728 kernel narrowly

Hypothesis: a microkernel win survives call overhead and the full decode path.

Dispatch to the assembly kernel only for supported aligned CQ2 shapes. Keep the
C path as fallback. First test the hottest projection class found in Experiment
1; do not convert every call site at once. Compare candidate versus control on
separate boards in the same batch.

If it wins, expand the dispatch to the next compatible hot shape in a separate
experiment. If the full model does not improve, profile alignment fixups,
dispatch overhead, cache behavior, and dual-core contention before rejecting
the assembly idea.

## Experiment 4: TIE728 scheduling variants

Hypothesis: the best accumulator count and row blocking for Needle 3 differs
from MimiModel because the tensor shapes and LUT layout differ.

Starting from the correct microkernel, compare these as separate candidates:

- two rows / eight accumulators;
- one row / eight accumulators;
- two rows with a small prefetch distance or reordered LUT/index loads.

Keep instruction counts and alignment explicit in the notes. Use the third
board for a second candidate only when both variants are ready; otherwise keep
it as an additional control/noise check.

### Measured disposition (run #138, 2026-09-20): one row wins; hoist the load only.

Same microbenchmark, same image on three boards, all candidates bit-exact
(`exact=768/768 bitexact=1`), board-to-board spread <= 0.1 pp on the 768x768
split-mode number. Saving vs the C kernel:

| variant | what changed | split-mode saving |
|---|---|---|
| `tie1n` | norm halfword `l16ui` moved to the top of the group | **+33.4 %** |
| `tie1`  | accepted baseline (one row, 4 partials, 8-deep) | +32.4 % |
| `tie1m` | whole FP16->FP32 conversion moved to the top | +32.3 % |
| `tie1p` | four index words in flight through a8/a9 | +30.6 % |
| `tie2`  | two rows interleaved, 4-deep | +25.7 % |

So on this board: the *index* stream is already covered by the hardware load
queue (deeper software prefetch actively hurts), row blocking loses a row stream
worth more than the extra reuse, and the only latency that is not hidden is the
per-group norm halfword on the *second* stream - moving just its load (not its
conversion, which occupies issue slots and registers) buys +1.0 pp of saving,
about +1.5 % kernel time. `tie1n` is the shipping kernel (run #138).

## Experiment 5: real async cross-operator overlap

Hypothesis: useful single-core bookkeeping can hide beneath an independent
worker projection, avoiding the immediate join in `nd_parallel_rows`.

Add a minimal worker API with explicit `submit` and `wait`, while retaining the
current synchronous row-split API. Test one schedule at a time. Good candidates
from the dependency graph are:

1. Start independent mHC `phi_post`/`phi_res` work on the worker while core 0
   computes `phi_pre`, its postprocessing, and other work that does not consume
   post/res. Join before the next operation that needs the worker or those
   results.
2. After V projection, start a leading slice of `gate_proj` on the worker while
   core 0 performs Q/K normalization, RoPE, and KV quantization/storage. Join
   before attention consumes the gate, then finish any remaining gate rows.

The first attempt should overlap enough work to exceed task/semaphore overhead.
Instrument worker busy time, core-0 overlap time, and wait time. Guard against
both cores writing shared scratch, starting a nested `nd_parallel_rows` job, or
reusing LUT/work buffers concurrently.

Decision: retain only a schedule that improves full-model timing and passes all
quality gates. A synchronous split followed by a join does not count as this
experiment.

## Experiment 6: overlap granularity sweep

Hypothesis: the schedule is sound but its worker slice needs tuning.

If Experiment 5 is correct but neutral, sweep only the submitted row count or
the join point. For the gate schedule, test a few coarse leading slices such as
32, 64, and 128 rows. For mHC, test one versus two projections per async job.
Reject variants that increase the critical-path wait or starve the normal
dual-core GEMVs.

## Experiment 7: narrow compiler floating-point candidate

Hypothesis: MimiModel's use of `-ffast-math` points to a smaller safe compiler
win in a measured hot translation unit.

Do not enable `-ffast-math` globally. On one candidate board, first try a narrow
flag set on the measured CQ2 kernel translation unit, such as contraction and
finite-math assumptions only where the data guarantees them. Inspect generated
Xtensa assembly and run all numeric/output gates. Test full `-ffast-math` on
that one unit only as a diagnostic candidate if the narrow flags do not change
code generation.

Reject immediately on output-gate failure. Keep it only on a repeatable full
decode improvement; compiler flags without changed code or timing are a logged
dead end.

## Required reporting after several experiments

After Experiments 1-3 and at least one Experiment 5 schedule, update
`.auto/ideas.md` with:

- source commit and the benchmark correction above;
- exact candidate commits and board assignment;
- sub-kernel and end-to-end deltas;
- quality results;
- accepted/rejected outcome and the next dependency-driven experiment.

Do not return to empty plateau re-verification while any experiment in this
queue remains untested or has a concrete follow-up supported by measurements.

## ESP32-AI follow-up (audited 2026-09-19)

Additional primary references:

- https://github.com/slvDev/esp32-ai
- https://github.com/slvDev/esp32-ai/blob/main/RESULTS.md
- https://github.com/slvDev/esp32-ai/blob/main/runtime/llm.h
- https://github.com/slvDev/esp32-ai/blob/main/firmware/esp32_tinystories/esp32_tinystories.ino

The reported 9.88 tok/s is valid for that project's attached-serial run, but
it is not a speed result for Needle 2 or Needle 3. Its dense core is only about
559K parameters (`d_model=96`, 6 layers); roughly 25M of the advertised 28.9M
parameters live in a Per-Layer Embeddings flash table from which only a few
rows are read per token. PLE would require architecture changes and retraining,
so it is outside this inference-only, frozen-model campaign.

The reusable runtime result is boot-time int8 staging plus int8 activation
quantization. ESP32-AI reports that this moved its dual-core exact-float-head
path from 139.4 ms/step to 102.9 ms/step, then its combined staging/placement
configuration reached 94.9 ms/step. It pre-unpacks selected int4 weights into
int8 PSRAM, converts fp16 group scales once, quantizes each activation once,
and lets both cores read the same quantized activation.

Needle 3 already implements the other transferable items: performance compiler
settings, hot scratch in internal SRAM, dual-core row splitting, RoPE tables
once per token, int8 KV storage, profiled PSRAM weight staging, and constrained
logits that avoid most vocabulary rows. Do not retry these under new names.

Needle 3's CQ2 format makes a direct copy inappropriate. The current pair-LUT
kernel streams packed 2-bit indices and selects float table entries. Its learned
2-bit codebook is approximately but not exactly symmetric, so an integer path
changes numerics. The following experiments are therefore quality-gated
prototypes after the MimiModel profiling/assembly/overlap work above.

## Experiment 3b follow-up: attention KV row staging (run #139, 2026-09-20)

Kept: `attn_heads` stages each position pair's K and V rows once per KV group and
shares them with the group's six query heads. +0.70 % decode (4.745 -> 4.7783),
byte-exact everywhere, fidelity probe unchanged. Two facts came out of it that
matter more than the number:

- `rows_dual_core()` in `esp32/main/main.c` runs the callback on **one core**
  whenever `nrows / 2 < 2`. Using KV groups as parallel units (nrows = 2) put the
  whole attention phase on core 0 and cost 12 % of decode. Any split with fewer
  than four units is silently single-core.
- The staged frame is ~1.4 kB, so the second core's task stack went 4096 -> 8192;
  at 4 kB the profiled build overflowed it and never printed EVT ready.

## Experiment 8: CQ2 integer-path feasibility microbenchmark

Hypothesis: quantizing the prepared Hadamard activation once can replace much
of the float pair-LUT traffic with compact integer dots, while remaining inside
the frozen quality budget.

Build a standalone device microbenchmark using the real CQ2 shapes, codebook,
row norms, alignments, and representative prepared activations. Compare the
current `lut2_rows` result and timing against two integer prototypes:

1. Keep packed 2-bit indices. Quantize each activation group and the four-value
   codebook to int8, accumulate `qcode[index] * xq[j]` into int32, then apply
   activation/codebook scales and the existing row-group norm.
2. At boot, expand one representative tensor's selected codebook values to
   int8 rows in PSRAM and stage its group norms as float. Use a plain
   int8-by-int8-to-int32 group dot.

For each, test tensor-wide and per-group activation scales. Report kernel time,
bytes read, maximum/mean output error, top-1 behavior on the fixed fidelity
probe, staging bytes, and quantization overhead. Do not integrate a prototype
whose error already makes the `logit_max_delta <= 0.002` gate implausible.

## Experiment 9: compact quantized pair-LUT

Hypothesis: a quantized pair table can preserve the packed 2-bit weight stream
while cutting table traffic from float32 to int16 or int8.

Build the same 16 entries per activation pair as today, but quantize entries
with one scale per CQ group. Accumulate selected int16 entries into int32 and
apply the table scale with the row norm once per group. Test int16 first for
fidelity; test int8 only if int16 is both accurate and faster. Keep lookup order
and row traversal identical to the control. Compare this separately from the
expanded-weight path in Experiment 8.

This is not equivalent to a generic int16 PIE result from another CQ layout.
Measure Needle 3's actual pair table and packed-index traffic before accepting
or rejecting it.

## Experiment 10: reuse one quantized activation across Q/K/V/gate

Hypothesis: integer-path setup amortizes only when the four projections sharing
the same prepared activation also share one quantization pass.

If Experiment 8 or 9 passes its numeric screen, integrate it first for one
layer's Q/K/V/gate group. Quantize `xh` once, then reuse it for all four
projections. Keep `out_proj`, mHC, engram, and logits on their existing paths.
Measure one clean control and two candidates in the same three-board batch:
the most accurate winning integer representation and the fastest acceptable
one. Run the full host and device quality gates before expanding coverage.

If one layer wins, expand by profiled projection cost in separate experiments.
Budget expanded weights against the existing 4.49 MB PSRAM tier and reported
free PSRAM; do not blindly retain both copies when replacing a tier entry would
fit better.

## Experiment 11: Xtensa SIMD integer dot and inlining audit

Hypothesis: after an integer path is proven correct enough, an LX7/TIE/PIE dot
kernel can reduce its remaining compute cost.

Only start this after a scalar integer candidate passes all quality gates.
Inspect generated assembly first. Benchmark a small handwritten SIMD dot
against the scalar integer kernel, with exact accumulator overflow bounds for
the largest group. Keep the hot ranged kernel out of line as a measured
candidate: ESP32-AI reports a severe regression when its `matvec_i8_range` was
inlined, so test `noinline` versus compiler choice rather than assuming either.

Reject any SIMD version that merely moves a bandwidth-bound path. Retain it
only when both isolated kernel time and end-to-end decode improve.

## ESP32-AI reporting rule

Record the comparison honestly: 9.88 tok/s comes primarily from a much smaller
dense core plus sparse PLE architecture. For Needle 3, report only measured
deltas from the accepted 4.185 tok/s control. If every integer prototype fails
the numeric gate, record that as the conclusion and continue the exact TIE728
and cross-operator experiments; do not loosen quality thresholds.


## Experiments 7-11 CLOSED against the measured phase map (runs #140-#142)

The campaign asked for a disposition of each remaining item, not another build.
Using the accepted tree's device phase map (201 ms/token, `AUTO_PROFILE=1`):

| Exp | Disposition | Measured reason |
|---|---|---|
| 7 compiler floating-point candidate | not applicable as a speed lever | Every phase is bound by something a compiler flag cannot touch: 2-bit GEMV 43% by PSRAM line-fill bandwidth, attention 18% by `exp()` plus KV line traffic, `kron_apply` 12% by FP latency (8 accumulators already break the dependency chains), engram 8% by flash gather latency. And `-ffast-math`-class changes break the byte-exact golden gate by definition. |
| 8 CQ2 integer path | **rejected on measurement** | The 2-bit projection phase is 86.5 ms for ~2.2 MB of packed weights per token, i.e. ~25-30 MB/s effective: the stream floor, not an arithmetic limit. Direct evidence that shortening arithmetic here buys nothing: row-blocking the generic path (#141) and 20+ instruction-scheduling nulls. Experiment 8's premise ("must now beat `tie1n`, not C") is answered - it cannot, because `tie1n` is already issuing ~3 instructions per weight against a bus that is the real limit. |
| 9 compact quantised pair-LUT | rejected | The 16 KB pair table is L1-resident; the streamed operand is the weights. Halving the table changes what is cached, not what is streamed, and quantising its entries changes products, so it fails the byte-exact gate before measurement. |
| 10 one quantised activation reused across Q/K/V/gate | **already true in shipping code, and tiny** | One `nd_cq_prepare` + one LUT build already serve q/k/v/gate/out_proj-class projections, and the whole PREP phase is 3.5 ms = 1.7% of the token. Prize was below the quality risk of touching quantisation. |
| 11 Xtensa SIMD integer dot | rejected | Same reason the independent MimiModel engine rejected int16 PIE assembly: the int8->int unpack dominates over the dot itself. The accepted kernel spends its instructions on the nibble->address step, not the multiply-accumulate. |

What remains identified is each sub-1% and each explained by a floor: `lsc`
pairing in the 4-bit loop (arithmetic is hidden behind PSRAM line fills, so ~0),
`kron` asm (FP-latency bound, GCC already at 1.75 instructions/product),
attention asm (needs a kernel around online softmax; `exp()` is not asm-fixable).

## Integrity: `make capture` on the accepted firmware (run #142 tree)

`demo/capture.py`, 7 real end-to-end scenarios through the API, board 1, against
the shipped path (TIE728 CQ2 kernel + shared KV row staging):

```
routes_match=true tools_match=true requests_succeeded=true no_external_calls=true
local_has_two_passes=true external_stops_at_selection=true
telemetry_progressed=true sampling_interval_applied=true timer_expired=true
```

Per case: translation/coding/architecture routed to the external models and
stopped at selection; status/sampling/timer/batch executed locally with the right
tool arguments. Latencies 11.1-17.7 s per scenario at 4.78 decode tok/s.
Run with `--out /tmp/recording.json` so the repo's committed recording is not
touched. This is the behavioural check that the 12-case byte-exact suite does not
cover (routing, second-pass tool execution, timer expiry).
