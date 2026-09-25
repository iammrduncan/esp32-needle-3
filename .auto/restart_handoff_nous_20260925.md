# Fresh Nous autoresearch handoff — 2026-09-25

You are resuming the ESP32 Needle autoresearch campaign in a fresh Pi session after the prior session developed a stale `pi-goal-x` context. Continue doing real experiments immediately. Do not spend a turn re-reading the entire repository or replaying old canonical readings.

## Current benchmark state

- Accepted reproducible baseline: **5.3033 tok/s**, bundle5 `2c79104`, engine `0c1a6272cd01`, with full quality/capture.
- Unaccepted discovery seed `61861dd9886c`: board 1/2 **5.6117**, board 3 **5.6133**.
- Run #665: shippable lean notification scheduler, **5.5833**, passed full 20-case gate.
- Run #668: integrated wide-load phi; board 2 seed-era **5.8400**, board 3 shippable+notification **5.6033**, both 6/6.
- Run #669: wide-load QK kernel bench was **38.2% faster**. The board 2 E52 full candidate lane was still running when the old researcher was paused.
- Board 3 `M-widegate-b3` refused an unchanged shipping image; that is not a benchmark failure. A real candidate change is required before retrying that gate.

## Live-lane handoff

- The board 2 `E52-b2` tmux lane was deliberately left alive across the restart. Harvest `/root/board-pool/batches/e52_b2.log` and `.out` before deciding its disposition.
- Inspect board leases and tmux sessions before launching replacements. Do not interrupt a live independent lane.
- Keep all three ESP32 boards productively occupied with **different, independent candidate experiments** whenever there are enough testable hypotheses. Treat each board as its own experiment lane, not as replicas of one experiment unless a promising result needs confirmation.
- Launch long board work in independent tmux/background lanes. Never block Pi with a foreground sleep longer than 20 seconds. Poll briefly, then perform useful analysis or launch another independent lane.

## Operating direction

1. Read `.auto/mentor_queue.md` fully once, then inspect only the recent tail of `.auto/log.jsonl` covering runs #665–#669 and any entries added since this handoff.
2. Harvest the surviving board 2 lane and record it through the normal experiment log path.
3. Select the strongest untried hypotheses from the mentor queue and recent outcomes; use three-board parallelism to maximize genuinely distinct experiments.
4. Every candidate must make a real code/configuration change, preserve benchmark integrity, and be evaluated with the appropriate quality gate. No benchmark cheating or workload weakening.
5. Prefer discovery screens first, then promote only winners into full quality/reproducibility gates. Avoid repetitive verification loops on already-settled candidates.
6. Keep durable notes in `.auto/log.jsonl` and update `.auto/mentor_queue.md` when priorities materially change.

The active model is DeepSeek V4.1 Flash through Nous Portal at high reasoning. Start by checking the surviving lanes and taking the next concrete experimental action.
