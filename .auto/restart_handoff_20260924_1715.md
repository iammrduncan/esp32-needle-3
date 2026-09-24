# Needle 3 autoresearch restart — 2026-09-24 22:15 UTC

This is a fresh Pi session after the previous session exited on an aborted
foreground `sleep 560`. Do not resume or reconstruct its 285 MB conversation.
Durable evidence is authoritative.

Read `.auto/mentor_queue.md` completely first, then only the active top of
`.auto/prompt.md` and log entries #607-#612. Do not reread the full historical
ledger before acting. Preserve worker dirt, board locks, provenance, anti-repeat
history, and every quality gate.

## Immediate state

- Accepted shipping image: **5.3033 tok/s**, bundle5 `2c79104`, engine
  `0c1a6272cd01`, device 20/20, host 19/19, capture green.
- Unaccepted discovery seed: provenance `61861dd9886c`; board pins are B1/B2
  5.6117 and B3 5.6133. It is not the accepted shipping image.
- All three boards are connected, idle, and unlocked at this restart.
- Runs are durably logged through #612.
- Corrected recipe A3 on board 2 FINISHED after Pi died:
  `/root/board-pool/batches/M-a3-b2.log` reports **5.6467 tok/s**, +0.624%
  against board 2's 5.6117 seed, prefill 5.9583, min case 5.38, boot 5.691,
  heap 8,415, token delta 0, missing 0, restricted device exact 6/6,
  `LANE_RC=0`. Provenance is `9e658137ba25`. Extended/think and the full
  original quality breadth are not measured. This is a successful candidate,
  not yet accepted.
- Astra already verified A3's target instruction graph: separate rescale
  `mul.s`, then two ordered `madd.s` per cell, four-wide hardware loop, no
  loop-body spills. Do not repeat that proof.

## Act immediately

1. Harvest and log the completed A3 number without re-running it. Snapshot its
   exact source from board 2 before changing that worker. Prepare its target
   differential and one permitted full original quality-breadth gate.
2. Put free board 1 on mentor recipe B: share each V load across two heads while
   preserving each output's term order. Start narrow enough to avoid register
   spilling; inspect the built loop.
3. Put free board 3 on mentor recipe C: full Q tap/norm/RoPE ownership with the
   two extraction fixes documented in `.auto/mentor_queue.md`. The earlier small
   norm+RoPE run did not test Q taps; the failed extraction is unmeasured.
4. Board 2 may run A3's quality breadth after its source is snapshotted. A later
   turnover may cross-board-confirm A3; do not waste B1/B3 on duplicate A3 while
   B and C are ready.

Launch B and C while preparing A3's gate. Verify live processes plus advancing,
nonempty logs. Do not merely print PIDs.

## Never block the session waiting

Do not issue foreground sleeps longer than 20 seconds, including `sleep 560`,
`sleep 620`, or a long shell poll with a large tool timeout. The Pi harness has
aborted such calls at roughly 90 seconds and treats the abort as a fatal session
error. Board jobs must run in their own tmux/background lanes. While they run,
prepare the next candidates or return from the tool call; inspect them later
with short bounded polls. A finished lane must be harvested from its log, never
waited on speculatively.

If context pressure rises, update `.auto/mentor_queue.md` and start a clean
handoff before 80%; do not continue into another 230k-token session. Avoid
methodology narration. Execute, measure, log, and keep three distinct lanes
moving.
