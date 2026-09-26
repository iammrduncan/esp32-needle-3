# Autonomous research and board-pool operations

The campaign was long enough that orchestration became part of result quality.
Agent compaction, service restarts, three mutable worker trees, scarce serial
ports, multi-minute prefix priming, and scheduled review can all create false
evidence even when the kernel code is sound. This appendix preserves the
operating method recovered from the Pi and Codex histories.

## Authority and durable state

Conversation context is not the campaign database. At any handoff, keep one
authoritative objective/metric contract and durable files for:

- frozen prompts, goldens, and quality thresholds;
- accepted pin and candidate state;
- append-only run results;
- next experiment and closed families;
- board/worker/image assignments;
- exact reproduction commands and unfinished obligations.

A Codex session found that compaction had reconstructed the wrong mission from
conflicting prose. The repair was one authoritative prompt, one concrete next
experiment, and explicit closed/deliverable lists. The later 3,396-line ideas
notebook was pruned only after being preserved byte-for-byte, and a compact
handoff was created so the result did not depend on a live queue surviving.

Use `.auto/log.jsonl` for the run record, `.auto/prompt.md` for invariant
contract, frozen JSON fixtures for oracles, and a dated handoff for the state
humans/agents must reload. Treat mentor queue and backlog as live routing aids.

## Roles

The observed workflow separated three functions:

1. **Researcher:** implements one hypothesis, runs its proof/gates, classifies
   it, and records the result.
2. **Mentor/reviewer:** audits recent evidence, challenges attribution, prices
   alternatives, and recommends the next discriminating experiment.
3. **Owner:** decides product/quality contracts, accepts pins, and authorizes
   premise changes such as re-baselining a golden or trading assertion safety
   for internal RAM.

The mentor should not turn an unmeasured proposal into a finding, and the
researcher should not resolve an owner-quality decision by editing an oracle.
Scheduled Codex reviews were useful because they detected repeated firmware,
wrong paths, invalid controls, and premature closures independently of the Pi
execution loop.

## Three-board discovery pattern

Use parallel boards for **different hypotheses with pinned controls**, not three
uncontrolled variations of one dirty tree. A productive cycle is:

1. assign each lane a named base, one patch, expected mechanism, and abort gate;
2. lock the physical board and resolve its stable flash/console pair;
3. build fresh, prove image/source identity, flash, and capture a restricted
   screen;
4. reject losers cheaply;
5. cross over the winner to another board/base when board spread or composition
   matters;
6. run full host/device/product gates only on candidates worth preserving;
7. export the patch/source/proof before reusing the worker.

The accepted pin's 5.3033/5.3017/5.3033 cross-board readings established a
normal reporting spread near one quantum. Cross-board agreement is still not a
provenance substitute: all lanes can flash the same stale binary.

## Board ownership and safe mutation

- A board lane owns both flash and console ports for the full build/flash/run.
- Expand `FLASH_PORT`/`SERIAL_PORT` inside the locked wrapper, not before it.
- Use stable `/dev/needle-pi/boardN-*` identities, not changing ACM numbers.
- Stop API/monitor children by recorded PID and verify the console is free.
- Keep DTR/RTS false around ordinary attach; use reset only through the
  deliberate same-board procedure.
- Never flash a board another lane may be timing.
- Record when a board holds kbench/profile rather than product firmware, and
  restore/verify the product image before handoff.

Hardware access was the scarce and state-changing part. Host checks,
disassembly, byte budgets, reference differentials, and source audit should run
before occupying a board.

## Worker-tree discipline

Each worker is a mutable experiment workspace, not a canonical base. Before a
lane:

- resolve the requested base commit from the main repository;
- verify the worker's `HEAD`, status, and engine hash;
- preserve any valuable prior patch before replacing it;
- apply exactly the intended candidate and inspect the diff;
- use a unique build directory or prove invalidation;
- save app/model/engine hashes with the result.

Restoring a worker from its own `HEAD` contaminated several comparisons with an
unaccepted base. Later results were relabeled rather than silently kept. A dirty
tree can be a valid composed candidate, but its complete diff—not its proposed
experiment name—is its identity.

## Job liveness

Do not equate “command was launched” with “experiment is running,” or “pane is
quiet” with “board is hung.” A live job needs:

- a live process owned by the expected lane;
- a nonempty log whose size/timestamp advances when progress is expected;
- correct board lock/port ownership;
- a terminal exit code and parsed result.

Multi-minute foreground sleeps proved fragile: the supervising agent/container
could abort while the board job continued. Prefer detached, externally owned
jobs plus short polls that return control often enough to report status. Silence
during cold prefix priming is normal only when boot progress or process/log
evidence supports it.

## Service persistence and restart

User systemd services disappeared after SSH logout while linger was disabled.
Enabling user lingering plus systemd supervision fixed process persistence, but
a restarted service did not inherently resume the research loop. The agent's
auto-continuation also had finite crash/continuation limits and triggered only
after turns that met its experiment criteria.

Operational policy:

1. make persistence a host-service property, not a chat promise;
2. verify the service, agent session, worker job, and board lock separately;
3. keep a restart handoff that is executable without prior conversation;
4. after restart, reconcile running jobs before launching replacements;
5. never claim “runs forever”—document watchdog, retry, token, and crash limits.

## Review and queue hygiene

A scheduled mentor should read only a bounded recent window plus durable state,
then write a concise finding with:

- observed result and provenance gap;
- correction/retraction, if any;
- one ranked next experiment with a falsifying control;
- expected prize, cost, quality gate, and changed premise;
- explicit “no experiment needed” when the stop condition is met.

Queues grow stale. Periodically synthesize accepted/candidate state, move
valuable history to an immutable/dated document, prune dead next actions, and
retain the previous file. A backlog should answer “what changed premise makes
this worth doing?” rather than list every imaginable optimization.

## Anti-activity safeguards

The campaign repeated an unchanged image many times before adding a shipping
signature guard. Automation should refuse:

- an already measured binary without a stated repeat question;
- a candidate whose diff is empty or whose claimed macro is absent;
- a result missing its expected case count or terminal status;
- a “new” experiment that only renames an old closed family;
- a board lane after the optimization stop condition unless it tests a named
  new premise.

Legitimate repeats include noise characterization, cross-board confirmation,
thermal stability, deterministic soak, or reproduction after a toolchain
change. Label the reason.

## Handoff checklist

- [ ] Objective, primary metric, keep bar, and quality contract are current.
- [ ] Accepted pin and every live candidate are distinguished.
- [ ] Latest complete run number and active/incomplete jobs are stated.
- [ ] Board → worker → image/tree mapping is verified.
- [ ] Consoles are free or ownership/PIDs are named.
- [ ] Diagnostic images are restored or prominently flagged.
- [ ] Exact next action has base, patch, command, control, expected result, and
      abort condition.
- [ ] Closed families include why and a changed-premise reopening condition.
- [ ] Raw logs, patches, hashes, differentials, and failures are preserved.
- [ ] Owner-only decisions are separated from researcher work.
- [ ] Stop condition is evaluated honestly.

This operating layer is reusable because it protects the same thing as a
numerical differential: the identity of what was actually tested.

