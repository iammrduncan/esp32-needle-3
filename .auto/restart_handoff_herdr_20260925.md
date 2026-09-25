# Handoff after the 2026-09-25 load-form day (Herdr restart)

Read `.auto/prompt.md` and the researcher-state block at the end of
`.auto/mentor_queue.md`, then the closing section of `.auto/ideas.md`
("the load-form program closes with a cost model"). Everything else is history.

## What this day produced (all measured, all logged)

| lever | result | gate |
|---|---|---|
| wide P.V update (`nd_pv_pair2w`) | **+0.63 %** on BOTH bases | full gate both lines |
| wide kron1 factor row (`nd_kron1_w`) | **+0.36 %** (b1) / **+0.34 %** (b2) | full gate both lines |
| QK DOT8W wide kernel | -2.1 / -1.9 % - CLOSED with a price | #670, #671 |
| kron2 factor rows (two forms) | -0.30 / -0.18 % - CLOSED | #674, #680 |
| P.V rescale sweep wide | -0.36 % - CLOSED (guard cost) | #681 |

Accepted pin is untouched at **5.3033**; the campaign now has two fully breadth-gated
lines, each with only the two `#647` demo-timer goldens outstanding:
* **shippable + notif + P.V + kron1 = 5.6383** (b1 and b3, engine `24d6ce2ce19b`)
* **seed-era + composed attention + wide phi + P.V + kron1 = 5.8967** (b2,
  engine `58bee4d1cde9`) - the best reading in the campaign, +11.19 % over the pin.

Every worker tree is parked at the state it measured (verified by hashing the engine
inside the board directory). Boards are idle and free.

## The cost model that decides load-form candidates (do not re-litigate)

A hand-written wide-load kernel pays only if **all four** hold:
1. the compiler re-reads data it cannot keep (row transport, call-per-group structure);
2. the arithmetic is independent per accumulator (no serial FMA chain - the QK body's
   four chains made the same substitution 2 % slower, with the C body at 49 cycles per
   chunk against the kernel's 80 and the call's 29);
3. the wide row fits alongside the accumulators (kron1: 14 of 16 registers; kron2: does
   not fit at all and both workarounds measured negative);
4. the guard is not on a rare path (the rescale sweep fires 13.2 % of the time but its
   guard ran 9216 times per token and cost the whole 0.35 %).

**Print the guard verdict at boot in every such lane.** A sanctioned-but-unused kernel
measures as a perfect null - that is exactly what happened to the first kron1 run.

## Next candidate directions (inventories, not completed experiments)

* The `dot_group` call-per-group structure inside `gemv_rows_offset` is the same shape
  that made the P.V win (redundant transport the compiler cannot remove) - price it on
  the 2-bit path, which is the dominant weight stream and has never had an asm
  transport change of this kind.
* The FWHT first stage operates on adjacent pairs, so a 128-bit load covers two
  butterflies; rule 2 is satisfied (butterflies in one stage are independent). Check
  register pressure and guard placement first.
* Anything else must be checked against the four rules above BEFORE a board is spent,
  and priced with a boot-time diff+cycle harness in the same image if it is novel.

## Harness facts paid for today

* `cp -a` preserves mtimes: ninja then treats a copied source as up to date and links
  the OLD object - `touch` after copying (this cost one build).
* Never `git checkout -- <file>` in a worker: every worker carries its lane state
  uncommitted (this cost b3's original tree).
* Hash the engine from inside the board directory, exactly as `measure.sh` does; hashing
  absolute paths from elsewhere produces a fake mismatch (this cost 20 minutes).
* `checks.sh` (host gates) is NOT run by `measure.sh`: a full-gate device result does not
  carry host 19/19 or `logit_max_delta`. Both lines owe a `checks.sh` run on their parked
  trees before any owner submission.
