# Agent session inventory

This appendix records the agent-history material that was locally available
when the engine bible was assembled. It exists to prevent two attribution
errors: treating a configured model name as evidence that the model ran, and
treating an agent's narration as stronger evidence than the resulting source,
run ledger, board log, or binary.

The short result is unambiguous:

- **Claude:** no local Claude transcript was available, and no actual Claude
  inference occurred in the documented router/demo path. `Claude Opus` and
  `claude-opus` are selection labels, not executed providers.
- **Pi:** seven project-session JSONL files were available. Six were fixed at
  the audit cutoff and one was still growing during an unfinished QK retry.
- **Codex:** 30 rollout JSONL files existed at the audit cutoff. Twenty-eight
  had the project as their working directory; two used the home directory, of
  which one concerned the project through Herdr and one was unrelated.

These are filesystem-snapshot counts, not lifetime account totals. Deleted
sessions, cloud histories, browser sessions, histories on other machines, and
unmounted account exports cannot be inferred from their absence here.

## Snapshot boundary

The repository evidence is the edition identified in the bible index: Git
`HEAD` `28bd4b4`, with the run ledger read through run #797. The mutable agent
stores needed separate cutoffs:

| Corpus | Audit boundary | Material visible at that boundary |
|---|---|---|
| Claude | Local-machine search during the 2026-09-25 bible audit | Zero native Claude transcripts. |
| Pi | 2026-09-25 21:32:07 CDT (2026-09-26 02:32:07 UTC) | Seven project JSONLs; the seventh was active and growing. |
| Codex | Immediately after the 2026-09-25 21:30 CDT mentor run began | 30 rollout JSONLs, 120,438,357 bytes, and 17,817 JSONL records. |

Anything appended after those boundaries is outside this edition. In
particular, the three Codex subagent rollouts started around 21:36 CDT while
this appendix was being produced are not part of the 30-rollout census.

## Claude: availability and negative finding

### What was searched

The audit checked for all of the following:

- `~/.claude/`, `~/.claude.json`, a workspace `.claude/`, and a project
  `CLAUDE.md`;
- Claude desktop state, a `claude` executable, shell-history invocations, and
  Anthropic environment variables;
- accessible home-directory JSONL files for Anthropic provider names and
  Claude model identifiers;
- Git refs and reflogs for Claude/Anthropic authors, trailers, commit text, or
  historical filenames;
- Herdr's current session record and agent-detection log;
- every Pi project session's provider/model events.

No native Claude Code history, Claude desktop history, Anthropic-backed event,
Claude shell invocation, or Claude-authored Git metadata was found. The seven
Pi sessions use only the providers/models listed in the Pi section below.
Herdr recorded Codex and Pi detections, with no Claude detection.

`git log -S Claude` found only commit `9bd0c333`, authored by Shannon Duncan,
which introduced model-router demo references. Generic installed support—such
as a Herdr Claude detector, Pi's `claude-code` adapter, Anthropic SDK packages,
or Codex plugin examples—is capability scaffolding, not project-session
history.

### No Claude inference in the product path

The repo's `research_and_plan` route maps to `claude-opus` with
`execution: handoff` in [`tools/model-catalog.json`](../../../tools/model-catalog.json).
The host bridge maps the route name to that catalog entry, but a handoff ends
with `external_selected`; it does not call a provider. The project
[`README.md`](../../../README.md) explicitly says the names are deployment/demo
policy labels rather than benchmarks and that the demo calls no external LLM.

The current [`demo/recording.json`](../../../demo/recording.json) confirms that
boundary for the architecture example: the ESP32 selected
`research_and_plan`, the presentation label was `Claude Opus`,
`remote_called` was false, and only one local inference pass ran. Its timing is
Needle inference and host orchestration timing, **not Claude latency**. There is
no Claude response, endpoint trace, exact Claude model revision, Claude quality
result, or Anthropic billable request in the available evidence.

Consequently, this bible attributes no kernel, hardware, numerical, or
optimization finding to Claude. The useful Claude-related finding is negative:
the router can select an external handoff label without executing that external
model.

### Availability limit

Absence is scoped to the accessible machine state. The host's `/root` was not
readable to the audit, and browser/cloud histories, other accounts, other
machines, deleted files, and an exported `~/.claude/projects` tree were not
available. A later Claude export would be a new source; it would not make the
current label-only demo an actual Claude inference.

## Pi: seven project sessions

### Store and exact census

The native Pi project store was:

```text
/home/mbench01/.local/share/pod-pi/needle/home/.pi/agent/sessions/--workspace-esp32-needle-3--/
```

All seven JSONL files in that directory were inventoried. The first six were
fixed-size files at the cutoff and total 610,496,023 bytes and 17,963 records.
The active seventh file measured 273,917,799 bytes at the cutoff, making the
observed seven-file total 884,413,822 bytes. No stable record count is reported
for the seventh file because it was being appended while the audit ran.

| Start (UTC), session ID | Provider/model | Bytes | Records | Boundary state |
|---|---|---:|---:|---|
| `2026-09-18T06:22:46.196Z`, `01a0b32e-87b4-7a29-96f9-6d2ab90b4f70` | `sp02 / qwen3.8-flash-next` | 27,892,930 | 6,151 | Fixed; last record is an assistant tool call, with no terminal answer in the file. |
| `2026-09-20T18:01:02.959Z`, `01a0bffa-8aef-7ec7-ad4a-959c7182e302` | `sp02 / qwen3.8-flash-next` | 60,820,313 | 2,327 | Fixed; last record is an assistant tool call after a new-iteration request. |
| `2026-09-22T03:32:11.922Z`, `01a0c72b-ce12-7064-8e49-250f9d4d05ef` | `sp02 / qwen3.8-flash-next` | 30,396,158 | 1,320 | Fixed; ends with an assistant `stop`. |
| `2026-09-22T16:03:51.321Z`, `01a0c9db-f799-7d45-9fa3-4d0093b9253c` | `sp02 / qwen3.8-flash-next` | 99,625,278 | 4,286 | Fixed; ends with an assistant `stop` after the user asked whether it was stuck. |
| `2026-09-24T05:55:46.613Z`, `01a0d1fb-f935-7be9-b928-e723e6b2f341` | `sp02 / qwen3.8-flash-next` | 285,137,101 | 2,635 | Fixed; ends in an assistant error after an aborted command. |
| `2026-09-24T22:16:16.083Z`, `01a0d57d-a3d3-7131-9a07-fc5a7b59313d` | `sp02 / qwen3.8-flash-next` | 106,624,243 | 1,244 | Fixed; ends with a new user iteration request after the preceding assistant error. |
| `2026-09-25T05:42:53.807Z`, `01a0d716-8a6f-7ece-a4e6-9c3c4e619070` | `nous / deepseek/deepseek-v4.1-flash` | 273,917,799 at cutoff | Mutable | Active; a QK retry was unfinished at the cutoff. |

“Fixed” means only that the file was no longer growing; it does not retrofit a
successful completion onto an abrupt tool call, error, or dangling user turn.
This distinction matters when reconstructing whether a proposed experiment was
actually harvested and logged.

The Pi configuration exposed only `sp02` and `nous` providers. The first six
sessions selected Qwen 3.8 Flash Next; the active restart selected DeepSeek
V4.1 Flash through Nous. There were zero Anthropic-provider or Claude-model
events.

### Related Pi material and duplicate prevention

The audit also used operational context outside the session JSONLs:

- `/home/mbench01/.local/share/pod-pi/needle/SETUP.md`;
- `/home/mbench01/.local/share/pod-pi/needle/home/.pi/agent/AGENTS.md`;
- `/home/mbench01/.local/share/pod-pi/needle/home/.bash_history`;
- `/home/mbench01/.local/share/pod-pi/needle/home/board-pool/batches/` and its
  per-board logs/results;
- Pi's provider configuration in `.pi/agent/models.json` and default settings
  in `.pi/agent/settings.json`.

The preserved checkpoint
`/home/mbench01/.local/share/pod-pi/needle/home/research-checkpoints/20260918-153904-paused-for-usb-hub/pi-session.jsonl`
is a prefix/copy of the first Qwen session, not an eighth independent session.
Board-pool files and shell history are supporting execution traces, not agent
sessions; they were not added to the seven-session count.

### What the Pi corpus contributed

Pi is the principal conversational record of the long autoresearch execution:
hypothesis selection, code changes, builds, board-lane allocation, flashing,
serial harvests, quality-gate failures, retries, and the transition from Qwen
to the fresh Nous/DeepSeek context. It is especially useful for recovering why
an experiment was attempted, why a lane was abandoned, and whether an apparent
silence was a board/tool/process problem.

It is not the final authority for a speed or correctness claim. Large tool
outputs are embedded in these transcripts, sessions can end mid-call, and the
active file is mutable. Claims taken from Pi were accepted into the bible only
when the exact result was corroborated by `.auto/log.jsonl`, a preserved board
artifact, a commit, the measured source tree, or another evidence class defined
in [the evidence policy](../evidence-policy.md).

## Codex: 30 rollout files at cutoff

### Stores and exact census

The native rollouts were under:

```text
/home/mbench01/.codex/sessions/YYYY/MM/DD/rollout-*.jsonl
```

At the Codex cutoff, 30 rollout files occupied 120,438,357 bytes and contained
17,817 JSONL records. Their `cwd` fields split as follows:

- 28 rollouts used `/home/mbench01/github/esp32-needle-3`;
- one home-directory rollout was a project-adjacent Herdr task;
- one home-directory rollout was an unrelated Tailscale task and contributed
  no engine-bible fact.

The 30 files are accounted for below. IDs are sufficient to recover the full
filename because each filename also contains its local start timestamp.

| Class | Count | Session IDs | Disposition/contribution |
|---|---:|---|---|
| Unrelated home-directory task | 1 | `01a098a8-a990-7240-8bb3-4e715737092e` | Tailscale work; inspected for scope and excluded from project conclusions. |
| Completed interactive project work | 5 | `01a0b240-8def-77d2-8001-42321a6ee4a0`, `01a0b328-ae4a-7053-8c5d-d48645d219b7`, `01a0bff4-3851-7521-a786-329fa4702aa1`, `01a0c25f-e5e4-73d2-bf50-d00656cc3efc`, `01a0c759-6a53-7471-b69e-717551cd0a25` | Port/router work, lab and evidence audits, campaign method, and early mentor handoff. |
| Completed scheduled mentor runs | 18 | `01a0d1e1-bfc3-7542-86d5-85fcd3ce4e18`, `01a0d26d-beb2-7080-8db4-3b50fb060f5d`, `01a0d2f7-1221-7ca2-b9ee-e247c18b7602`, `01a0d380-66ad-7b03-a0f9-5fd3acd56488`, `01a0d409-bb27-7b03-bcb9-729960b30107`, `01a0d493-1224-7772-baaf-4691d9fea971`, `01a0d51c-64c9-73d1-abce-11e4441b30d7`, `01a0d5a5-b93c-77c2-9e21-aa5752c4cf77`, `01a0d62f-0ce0-7753-a519-83d68f32e3c3`, `01a0d6b8-607b-7721-b9b0-6a7456840e2b`, `01a0d741-b399-72a1-8984-324fe0c21122`, `01a0d7cb-07c8-7093-8c60-020de779b594`, `01a0d854-5ade-7b02-8001-09a45a983530`, `01a0d8dd-adb6-7a30-8762-ee311c8df463`, `01a0d967-00f2-7900-8d14-5852ae79643e`, `01a0d9f0-57b7-79a2-85f9-763ff5cb185a`, `01a0da79-aaee-7071-8a86-e2a7c3c4343e`, `01a0db02-ff3a-7f02-8b5f-0ba788b96628` | Periodic review of recent runs, corrections, ranked hypotheses, and guardrails written into the mentor queue. |
| Project-adjacent home-directory task | 1 | `01a0d77b-a226-7da0-b4c3-40128fc97687` | Herdr work; relevant to agent-detection provenance, not a kernel experiment. |
| Current bible root/audit workers | 4 | `01a0db84-4159-77c3-a650-9f1846414c69`, `01a0db85-3de4-7470-bc1e-a3ec45951d6a`, `01a0db85-49eb-7002-93a6-1657e4fe77a5`, `01a0db85-57d4-7a90-93ae-d319b0990273` | In progress at cutoff; compilation and source/session audits for this bible, not independent historical evidence. |
| Newly active scheduled mentor | 1 | `01a0db8c-558b-73e0-9008-d54cd3ac6ec9` | In progress at cutoff. |

The local Codex state database contained 30 thread rows, 28 with the project
working directory, and zero archived rows. Its 101 recorded turns were marked
70 complete, 26 interrupted, and five in progress—the four bible workers and
the newly active mentor. Those are **turn** statuses, not 101 sessions. The
state projected 5,907 rollout items. `~/.codex/session_index.jsonl` named only
seven sessions, so it was treated as a partial convenience index rather than
the census source. No archived-session directory was present.

### What the Codex corpus contributed

Three large historical rollouts carry most of the unique narrative:

- `01a0b240-…` covers the initial port and model-router/tool-schema work;
- `01a0b328-…` covers lab operation, capture, and evidence audits;
- `01a0bff4-…` develops the measurement method and mentor process.

The router-schema experiment in `01a0b240-…` is a useful example of why the
transcript was consulted. A single `select_model` enum collapsed unlike prompts
onto Qwen; vague bundled local routes still misclassified timer/status work;
six concrete capability tools fixed six of seven cases; and explicit action
language for complex research, design, architecture, and plans fixed the last
architecture case. That is Codex-assisted schema evidence about the local
Needle router. It is not a Claude quality result merely because the winning
route's presentation label says `Claude Opus`.

The 18 completed mentor sessions primarily contributed independent review:
they challenged causal stories, ranked next experiments, identified missing
proof, and updated `.auto/mentor_queue.md`. They did not make every suggested
hypothesis true. The Pi/board campaign still had to implement, measure, and
gate each proposal.

The four in-progress bible sessions contributed organization and audit work to
this documentation. They are declared explicitly to avoid circular provenance:
this appendix cannot serve as independent confirmation of a claim merely
because the same audit session wrote it down.

## Cross-corpus attribution rules

Use the corpora to find evidence, not to replace it:

1. A model/provider label identifies the agent that produced a recorded turn;
   a route label identifies only the policy choice encoded by Needle.
2. A transcript statement that an experiment passed is a lead. The matching
   ledger record, hashes, board output, and source tree establish the result.
3. An incomplete session can still contain valid tool output, but no unlogged
   proposed change is promoted as completed work.
4. The Pi corpus documents most hands-on experiment execution. Codex documents
   port/router work, audits, methodology, and mentor review. No available
   corpus supports attributing project work to Claude.
5. Duplicate exports and checkpoint prefixes are counted once. Supporting
   shell histories and board logs are evidence artifacts, not sessions.
6. Current documentation sessions are disclosed and are not treated as
   independent historical corroboration.

For claims distilled from these histories, follow the source hierarchy in
[the evidence policy](../evidence-policy.md) and the concrete paths and external
references in [the source catalog](../sources.md).
