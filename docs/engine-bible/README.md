# Needle engine bible

This directory is the durable engineering record for the Needle 3 inference
engine work. It is written so that another inference-engine project can reuse
the mechanisms, measurement discipline, and failure knowledge without needing
the original agent conversations or the ESP32 board pool.

The bible is not a changelog and it is not a claim that every preserved
candidate is shipped. It separates four states that the research history often
had alive at the same time:

1. **Accepted** — passed the campaign's required gates and was promoted as the
   comparison pin.
2. **Candidate** — measured and preserved, but still needs an owner decision or
   a named gate.
3. **Diagnostic** — valid evidence about a mechanism, but not a shippable speed
   measurement.
4. **Rejected/closed** — measured, falsified, unsafe, below the keep bar, or
   blocked by a documented hardware/vendor constraint.

## Snapshot and headline result

This edition was frozen at **2026-09-25 21:42 CDT** from repository `HEAD`
`82a3cf1`, the working-tree research ledger through run **#805**, all
documentation and preserved experiment assets in this checkout, the available
Claude/Pi/Codex session stores, and the external sources listed in
[sources.md](sources.md). The Pi and scheduled-Codex stores were live during
the audit; their exact, earlier census cutoffs are recorded in
[session inventory](appendices/session-inventory.md).

**Identity warning:** the cutoff checkout `82a3cf1` is a mixed research/evidence
tree. It is neither accepted commit `2c79104` nor final candidate A, B, or C,
so a build of it must not be labeled 5.3033 or 6.1433 tok/s. The live branch
may also advance beyond the frozen cutoff. See the
[candidate identity manifest](appendices/candidate-manifest.md) for exact
hashes and the limits of repository-only reconstruction.

The numbers that are easiest to misquote are:

| State | Decode rate | What it means |
|---|---:|---|
| Original device baseline, run #1 | 1.2217 tok/s | Unmodified eight-layer port before the first large staging win. |
| Later campaign baseline, run #2 | 2.4417 tok/s | FP32-staged MLP-factor tree; the `2.44` baseline used by later campaign summaries. |
| Accepted pin, commit `2c79104` | 5.3033 tok/s | 20/20 device exact, 19/19 host exact, fidelity and top-1 gates green. |
| Best candidate tree A, board 3 | 6.1433 tok/s | +15.8% over the accepted pin; 18/20 device exact on the disputed pair, plus host/numeric/top-1, own-tree CQ2, 9/9 capture, and repeat-soak evidence. |
| Conservative candidate tree C, board 1 | 5.9033 tok/s | +11.3% over the pin; same two-case owner decision. |

After the durable handoff, run #805 repaired a double-applied-scale bug in a
QK-outline port on tree C and measured **5.9233 tok/s** (`+0.339%` over that
tree) with host checks and a restricted 6/6 device screen. It did not yet have a
new full 20-case gate at this edition's cutoff, so it is a provisional derivative
and does not replace the fully evidenced table above. A separate B2 CQ4
codebook-residency screen was still running and has no verdict in this edition.

The final campaign recommendation was to adopt tree A, or tree C when the more
conservative lineage is preferred, after deciding how to treat
`heldout_interval_one` and `heldout_long_tools_note_only`. A pure transport
change flips exactly those cases with the engine bytes unchanged. Run #718
reproduced the exact `interval_one` answer on the host and the same repeated-tool
tendency—not byte-identical output—on the long-tools prompt; ordinary B3 prompts
were repeat-deterministic in the run #797 soak. This is an
**oracle/re-baseline decision**, not permission to weaken the gate silently. See
[performance history](esp32/performance-history.md) and
[transport](esp32/serial-transport.md).

## Reading map

### General inference-engine knowledge

- [Engine anatomy](general/inference-engine-anatomy.md) — loader, model state,
  forward pass, tokenizer, grammar, sampler, and host/target seams.
- [Needle model pipeline](general/model-pipeline.md) — mHC lanes, engrams,
  gated GQA attention, ZC-RMS norms, Monarch Hadamard MLP, KV state, and prefix
  caches.
- [`.cact` model format](general/cact-format.md) — zero-copy layout, tensor
  directory, slicing, manifest pinning, and validation.
- [Quantization and kernels](general/quantization-and-kernels.md) — Cactus
  Quants, FWHT preparation, pair LUTs, 2-bit and 4-bit walkers, and portability
  boundaries.
- [Constrained decoding](general/constrained-decoding.md) — byte grammar,
  vocabulary filtering, subset logits, deterministic argmax, and tool-call
  validation.
- [Numerical correctness](general/numerical-correctness.md) — equivalence
  classes, accumulation order, host/device limits, and the layered gate.
- [Optimization method](general/optimization-method.md) — how to profile,
  price, screen, falsify, promote, and stop.
- [Reusable lessons and dead ends](general/lessons-and-failures.md) — what
  worked, what did not, and why.
- [Porting guide](general/porting-guide.md) — how to carry these mechanisms to
  a different model, CPU, memory system, or runtime.

### ESP32-S3-specific knowledge

- [Hardware platform](esp32/hardware-platform.md) — the N32R16-class target,
  LX7 cores, memory hierarchy, cache, flash/PSRAM, and supported clocking.
- [Memory and bandwidth](esp32/memory-and-bandwidth.md) — placement rules,
  byte budgets, hot scratch, PSRAM tiers, cache interactions, and capacity.
- [Xtensa kernels](esp32/xtensa-kernels.md) — TIE728 assembly, ABI rules,
  hardware loops, load forms, and disassembly-based proof.
- [Dual-core scheduling](esp32/dual-core-scheduling.md) — row splitting,
  handshake cost, shared-state hazards, thresholds, and failed overlap.
- [Firmware runtime](esp32/firmware-runtime.md) — partition mapping, prefix
  priming, generation, router/tool execution, and HTTP bridge behavior.
- [Serial transport](esp32/serial-transport.md) — UART framing, DTR/RTS,
  console fatigue, the RX-ring discriminator, and state-sensitive goldens.
- [Build, flash, and debug](esp32/build-flash-debug.md) — ESP-IDF settings,
  reproducible builds, model flashing, kbench, profiling, and board recovery.
- [Performance history](esp32/performance-history.md) — accepted milestones,
  candidate trees, phase maps, and the final owner decision.
- [ESP32 failures](esp32/lessons-and-failures.md) — board-time traps and their
  prevention rules.

### Evidence and lookup material

- [Evidence policy](evidence-policy.md) explains what counts as proof and how
  contradictory records are resolved.
- [Source catalog](sources.md) records external and internal sources, pins, and
  what each source actually supports.
- [Session inventory](appendices/session-inventory.md) records which Claude,
  Pi, and Codex histories were available and how they were searched.
- [Repository document inventory](appendices/document-inventory.md) accounts
  for every Markdown research document in the checkout.
- [Experiment ledger](appendices/experiment-ledger.md) indexes the 805-run
  JSONL ledger, major result ranges, preserved `exp*` assets, and closed
  families.
- [Candidate identity manifest](appendices/candidate-manifest.md) separates the
  accepted commit, mixed edition checkout, and transient final worker trees,
  including what can and cannot be reconstructed exactly.
- [Reproduction recipes](appendices/reproduction.md) gives safe commands and
  evidence expectations.
- [Research operations](appendices/research-operations.md) covers agent
  compaction, mentor/researcher roles, three-board scheduling, worker provenance,
  service persistence, queue hygiene, and safe handoffs.
- [Glossary](appendices/glossary.md) defines project shorthand such as CQ2,
  TIE728, pin, breadth gate, and token delta.

## Source-of-truth order

When two records disagree, use this order rather than choosing the newest prose
blindly:

1. raw artifact from the exact measured image (board log, capture JSON, kernel
   differential, ELF/disassembly, hash);
2. `.auto/log.jsonl` entry that names the run, provenance, metrics, and gates;
3. commit diff/message for the exact tree;
4. current source code and frozen fixtures;
5. acceptance packet and experiment README;
6. chronological notes, handoffs, mentor queues, and session narration.

Newer is not automatically stronger. Several late notes deliberately retract
earlier causal stories, and several valid candidates live outside the current
checked-out engine files. [evidence-policy.md](evidence-policy.md) lists the
known corrections.

## Maintenance rule

Add new facts with all of: target/tree identity, workload, metric, correctness
gate, and a pointer to raw evidence. Record a rejected idea with its changed
premise or reopening condition. Never replace a frozen golden merely because a
candidate differs; first decide whether the candidate is wrong, the oracle is
stateful, or the product behavior intentionally changed.
