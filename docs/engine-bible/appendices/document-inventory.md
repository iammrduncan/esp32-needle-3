# Repository document inventory

This is the audit map for the prose evidence that existed before the engine bible was
assembled. It covers every project-authored Markdown file, extensionless `README`, and
research backup found outside generated/vendor trees at the snapshot below. The bible
itself is an output of that audit, so it is indexed by
[`../README.md`](../README.md) rather than recursively treated as source evidence here.

Snapshot: worktree at `82a3cf1551292c43007c245e016ce281c0a0f159`, with the research
ledger observed through run #805 on 2026-09-25 21:42 America/Chicago. This repository
was being updated by a live research loop while the inventory was written. Re-run the
commands at the end before claiming that this is the latest tail.

## How to read the authority labels

| Label | Meaning |
| --- | --- |
| **Contract** | Defines product behavior, benchmark rules, or a frozen input. It is authoritative for that scope even when its performance number is old. |
| **Primary evidence** | Records a measurement or reproducible artifact directly. Prefer the JSONL ledger when the prose and ledger differ. |
| **Current synthesis** | A maintained summary of primary evidence. Useful for orientation, but it can lag a live ledger. |
| **Live/volatile** | An operational queue or backlog that may change while it is being read. Never cite it as the sole proof of a result. |
| **Historical** | Correct for its dated checkpoint, but superseded as a statement of current state. |
| **Prepared** | Describes a proposed or built experiment; it is not proof that the experiment ran. |

The practical precedence order is: frozen benchmark/product contract; raw
`.auto/log.jsonl` record and preserved raw artifacts; exact-tree gate output; durable
handoff/acceptance synthesis; live queue; dated handoff; idea or proposal. A later
narrative does not erase an earlier failure, and a proposal does not become a result
because its asset directory exists.

## Product-facing documentation

| Document | Role | Authority and staleness |
| --- | --- | --- |
| [`README.md`](../../../README.md) | Product description, two-pass router semantics, scope, hardware, build/flash/API/test instructions, original capture, model provenance. | **Contract/current product guide.** Its 2026-09-18 capture is historical performance evidence, not the decode campaign's latest speed. Its statement that external model choices stop at selection is the product contract. |
| [`demo/README.md`](../../../demo/README.md) | Seven-scenario capture and rendering guide, including what the saved media does and does not claim. | **Contract/current guide.** The included recording is a dated hardware capture. VHS 0.11.0 is the tested renderer; the file explicitly says this is curated demonstration evidence, not a general routing benchmark. |

## Campaign control, synthesis, and research history

| Document | Role | Authority and staleness |
| --- | --- | --- |
| [`.auto/HANDOFF-2026-09-28.md`](../../../.auto/HANDOFF-2026-09-28.md) | Compact end-state handoff: three candidate trees, reproduction recipes, measured levers, closures, and owner decision. | **Current synthesis** through run #800. This is the best short narrative entry point, but later ledger/queue entries can supersede an open item. It preserves the crucial distinction between the 5.3033 accepted pin and the 6.1433 proposal. |
| [`.auto/OWNER_ACCEPTANCE_PACKET.md`](../../../.auto/OWNER_ACCEPTANCE_PACKET.md) | Append-only owner packet spanning the 5.3033 accepted state, later candidate stacks, console-stall attribution, seed stacks, and attention proposal. | **Mixed current/historical synthesis.** The newest section is at the top, while lower sections preserve old decisions. Use it for the evidence chain; use the later durable handoff for the final three-tree table. |
| [`.auto/prompt.md`](../../../.auto/prompt.md) | Autoresearch objective, metric definitions, frozen inputs, quality gates, files in/off scope, hardware limits, and successive operator redirects. | **Contract** for invariant benchmark rules; **historical** for queues and point-in-time performance. Its top active block supersedes lower queue blocks, but the immutable constraints remain binding. |
| [`.auto/mentor_queue.md`](../../../.auto/mentor_queue.md) | Mentor priorities plus a chronological stream of researcher-state updates, evidence tables, final decision pages, and subsequent reopened work. | **Live/volatile.** It was modified during this audit. It is often newer than the committed handoff and excellent for locating a lane, but claims must graduate into the ledger and exact-tree artifacts before being treated as settled. |
| [`.auto/ideas.md`](../../../.auto/ideas.md) | Pruned live backlog: premise-gated opportunities, sub-bar banked work, and method rules. | **Live/volatile current backlog.** Refreshed after run #801; it deliberately omits the historical narrative. “No above-bar candidate” was true at that refresh, not a timeless theorem. |
| [`.auto/ideas.md.pre-2026-09-28.bak`](../../../.auto/ideas.md.pre-2026-09-28.bak) | Verbatim 3,396-line pre-prune research notebook: phase maps, hypotheses, measurements, retractions, failure analysis, external cross-checks, and run-linked rationale. | **Historical research archive.** Indispensable for why a family was closed and for negative results, but its opening accepted-rate statements are hundreds of runs stale. Search by run number or mechanism; never read top-to-bottom as current state. |
| [`.auto/mimimodel-experiments.md`](../../../.auto/mimimodel-experiments.md) | Audit and transfer plan from MimiModel/ESP32-AI, then measured disposition of TIE728, scheduling, attention staging, overlap, integer, and LUT ideas. | **Primary evidence/current for its bounded campaign**, especially runs #137-#142; **historical** for overall state. It records why the assembly CQ2 transfer worked and why several superficially similar ideas did not. |
| [`.auto/hourly_mentor.md`](../../../.auto/hourly_mentor.md) | Operating charter for a scheduled research mentor: authority, safety boundaries, three-board topology, queue hygiene, and reporting. | **Operational contract**, not measurement evidence. It explains who was allowed to edit/measure and why queue text may be advisory rather than implemented. |
| [`.auto/handoff_20260924b.md`](../../../.auto/handoff_20260924b.md) | Post-run-#428 continuation state, live board lanes, launcher recipe, and then-current 5.1167/5.27 state. | **Historical checkpoint.** Useful for reconstructing expbc/head4/taphoist provenance and worker-tree hazards; all headline performance figures are superseded. |
| [`.auto/restart_handoff.md`](../../../.auto/restart_handoff.md) | Fresh-session directive around runs #421-#423 after a context loop. | **Historical checkpoint.** Preserves the failed exp-scale build proof and the exact three-lane recovery instructions; its 5.1167 state is superseded. |
| [`.auto/restart_handoff_20260924_1715.md`](../../../.auto/restart_handoff_20260924_1715.md) | Fresh-session directive after an aborted long foreground sleep, through run #612. | **Historical checkpoint.** Strong evidence for the 5.3033 accepted pin versus 5.61 discovery seed distinction and for the “never block on a board job” rule. |
| [`.auto/restart_handoff_herdr_20260925.md`](../../../.auto/restart_handoff_herdr_20260925.md) | Load-form-day results, four-condition cost model, worker-tree hazards, and next candidate classes. | **Historical synthesis.** Its measured wide-P.V/kron1 results and build hazards remain useful; its 5.8967 best was later surpassed. |
| [`.auto/restart_handoff_nous_20260925.md`](../../../.auto/restart_handoff_nous_20260925.md) | Fresh Pi-session directive around runs #665-#669, including the surviving lane and three-board operating direction. | **Historical checkpoint.** The named model/provider is session metadata, not Claude evidence and not a claim about the firmware. The 5.84 best was later surpassed. |

## Experiment-local prose and preserved result notes

Experiment directory numbers are asset identifiers, not ledger run numbers. These files
are valuable because they preserve exact patches, fixtures, or failure mechanisms that a
one-line result cannot. Their local “owed” lists are frozen at file creation and can be
closed later in the ledger.

| Document | What it preserves | Final reading at this snapshot |
| --- | --- | --- |
| [`.auto/exp16/SCREEN.md`](../../../.auto/exp16/SCREEN.md) | Off-device six-prompt/93-step screen for the compact first-byte grammar index. | **Primary evidence.** Zero candidate-set mismatches; only 1.62% of the old token-id walk touched. Promoted and kept at run #288. |
| [`.auto/exp35/README.md`](../../../.auto/exp35/README.md) | Gated design for unrolling remaining independent element loops without benchmarking a copied transcription. | **Historical prepared plan.** The family was measured in runs #362-#370; the rescale unroll won, while several nearby loops were null or negative. |
| [`.auto/exp49/README`](../../../.auto/exp49/README) | One-line note for the `condT+cond2` guard. | **Historical scratch note.** It is not the PSRAM-tier run #49 and not sufficient evidence by itself. |
| [`.auto/exp86/README.md`](../../../.auto/exp86/README.md) | Proposed heartbeat discriminator for console stalls. | **Prepared, then superseded by results.** Runs #483-#485 executed the discriminator family and narrowed the stall; the README's A/B decision tree is the plan, not the final conclusion. |
| [`.auto/exp88/README.md`](../../../.auto/exp88/README.md) | Exported compact-prefix source and its 768,508-byte reclamation, including the worker-checkout loss hazard. | **Primary asset note.** The capacity change is quality-neutral and enables EG2; later tree gates supersede its local “shippable line” context. |
| [`.auto/exp89/README.md`](../../../.auto/exp89/README.md) | Wide-phi asset, Xtensa register-list/label/alignment port mechanics, and +0.57% screen. | **Primary local result, historically incomplete.** Its “owed” gates were later addressed by composed-tree work; consult the ledger for the receiving tree. |
| [`.auto/exp90/README.md`](../../../.auto/exp90/README.md) | Plain CQ2 group hardware loop (+0.44%) and the seed-line noinline `qk_dot8` (+0.22%). | **Primary local result.** Later runs add own-tree differentials and breadth gates. The warning that `.Lt1_*` is not the dispatched kernel remains important. |
| [`.auto/exp92/README.md`](../../../.auto/exp92/README.md) | First narrow engram[1] K/V staging asset and an unresolved screen. | **Prepared/unmeasured at that point.** Do not interpret the missing output as a negative result; exp95 and later ledger entries resolve the mechanism. |
| [`.auto/exp93/README.md`](../../../.auto/exp93/README.md) | First composed plain-loop + `qk_dot8` tree at 6.1100. | **Primary restricted-screen result.** Its local quality debts were later paid on descendant trees. |
| [`.auto/exp94/README.md`](../../../.auto/exp94/README.md) | Seed-line EG2 rewrite that built but yielded no output. | **Prepared/unmeasured.** Later diagnosis showed the silence was not a clean performance verdict. |
| [`.auto/exp95/README.md`](../../../.auto/exp95/README.md) | Working EG2 plus compact-prefix tree, root cause of earlier allocation failures, and +0.164% result. | **Primary evidence.** Positive but below the 0.2% promotion bar; retained as an enabling component on candidate trees. |
| [`.auto/exp96/README.md`](../../../.auto/exp96/README.md) | Amortised CQ2 group loop, assembly shape, exact differential wiring, and +0.27% B2 result. | **Primary evidence.** Later runs #781/#783 confirm transfer to two more trees and #790/#791 close per-tree differential debt. |
| [`.auto/logfskip/RESULT.md`](../../../.auto/logfskip/RESULT.md) | Real-model hit rates and break-even analysis for skipping `logf(1.0f)` in Sinkhorn. | **Primary off-device closure.** The measured 13.58% hit rate implies an implausible ≥787-cycle `logf` break-even; closed without board time. |

## Structured evidence that is not prose

These are intentionally not counted in the 28-file prose inventory, but the bible uses
them and a documentation audit must not overlook them:

- `.auto/log.jsonl` is the complete append-only run ledger and the authority for counts,
  status, commit/provenance, metrics, descriptions, and many structured `asi` notes.
- `.auto/prompts.json` and `.auto/golden/{host,device}.json` define frozen cases and
  expected text. `.auto/golden/logits.txt` anchors numerical fidelity.
- `model/manifest.json` pins the upstream repository/revision, source and sliced archive
  hashes, dimensions, and output size.
- `.auto/exp*/` also contains patches, source snapshots, launchers, raw board captures,
  kbench fixtures, and differential tests. A README is not a substitute for those bytes.
- Source comments in `engine/`, `esp32/`, `host/`, and `tools/` preserve local invariants
  and measured traps. The engine bible's component chapters index those code paths.
- `demo/recording.json` is the machine-readable product capture referenced by the two
  product READMEs.

## Legal and attribution text

These three extensionless text documents are not research narratives and are
therefore outside the 28-file prose count, but they were audited because they
constrain reuse:

| Document | Role |
| --- | --- |
| [`LICENSE`](../../../LICENSE) | Repository Apache License 2.0 terms. |
| [`NOTICE`](../../../NOTICE) | Project/upstream attribution: the C engine, firmware scaffold, and host utilities derive from `andrisgauracs/needle-2-esp32` revision `61cafad7014a5664bb3ffd5f0c457ce5aa6598ae`; it also records Cactus Compute model provenance and VHS attribution. |
| [`engine/NOTICE-LICENSE`](../../../engine/NOTICE-LICENSE) | Embedded copy of the upstream engine's Apache-2.0 license/notice material, which must travel with redistributed derivatives as applicable. |

This inventory summarizes provenance; it is not legal advice and does not
replace reading the license and notices themselves.

JSON fixtures, shell/Python programs, raw `.log` files, build output, and `.venv` or other
vendored trees were excluded from the prose count. They are evidence or implementation,
not missing Markdown documents.

## Reproduce the inventory

From the repository root, this prints the source prose set. The explicit patterns catch
the extensionless experiment README and the pruned `.bak` notebook:

```sh
find . -type f \
  \( -iname '*.md' -o -iname '*.markdown' -o -iname 'README' \
     -o -iname 'README.md' -o -iname '*.md.*.bak' \) \
  -not -path './.git/*' -not -path './.venv/*' -not -path './venv/*' \
  -not -path './build/*' -not -path './host/build/*' \
  -not -path './docs/engine-bible/*' | sort
```

At this snapshot it resolves to 28 source prose files: two product documents, thirteen
campaign-level documents including the pruned backup, and thirteen experiment-local
notes. If the result changes, add the new document here with an authority/staleness
assessment rather than silently increasing the count.
