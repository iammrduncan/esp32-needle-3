# Autoresearch shutdown handoff (owner stop directive)

Campaign stopped on the owner's instruction. No new experiments, lanes, builds or measurements
were started after the directive. The two lanes already in flight were allowed to finish and are
recorded below from their own logs.

## Verified final best

| item | value |
|---|---|
| **fastest fully-gated reading** | **6.2200 decode tok/s** (B1w3 tree) |
| extras | ext 6.1500 · host gate RC=0, 23/23, fidelity 5.341e-05, top1 10/10 |
| prefill / min_case / think | 6.5267 / 5.94 / 4.72 |
| vs owner-accepted 5.3033 | **+17.3%** |
| vs session start 2.44 tok/s | **+154%** |
| device quality | 22/24 byte-exact, token_delta 52 — the two frozen #647 cases only |

## Final in-flight results (own logs)

* **B2resid2** (W3 residual emit): **6.1900**, ext 6.1229, gen 99, device 22/24 delta 52,
  host RC=0, heap 7647 — **−0.053%** against its own 6.1933 base, i.e. the per-store `q < dm`
  guards in the linked code cost slightly more than the removed intermediate traffic saves.
  *Queued follow-up, not started: row-bounded 32x24 residual callback (avoids per-store guards).*
* **B3p1fix** (P1 consumer fusion on numeric metadata): **6.2033**, gen 99 — **+0.46%** against
  its own 6.1750 metadata base; six-crate wiring verified by real anchors (dtype guards before
  allocation, mHC FP16 refusal, P1 fused with only the `!m_meta` FP32 fallback, P2 untouched).
  Pre-flash host gate green (fidelity/top1); device gate captured where present.

## Preserved work

* Tree snapshots and engines: `/tmp/B1x_engine_snapshot_1512`, `/tmp/B1rope_engine_preserved`,
  `/tmp/B1p1_nd_model_preserved.c`, `/tmp/B1nofb2_nd_model_preserved.c`,
  `/tmp/B2p1fix_nd_model_preserved.c`, `/tmp/B2pre2_donor_nd_model.c`, `/tmp/B2p1fix_base_for_resid.c`,
  `/tmp/B3meta2_nd_model_preserved.c`, `/tmp/B3inv_broken_draft.c`, `/tmp/B1_broken_charat_31.c`,
  `/tmp/B1x_nd_model_pristine.c`, `/tmp/B1x_asm_pristine.S`, `/tmp/B1x_gemv4_preserved.S`.
* Workers: board1 = B1w3 (6.2200 gated) or B1p1 lineage; board2 = B2resid2 tree; board3 = B3p1fix tree.
* Prepared, unapplied patches: `/tmp/patch_b1_w3pre.py`, `/tmp/patch_b2_resid.py`.
* Lanes/logs: `.auto/log.jsonl` plus `batches/*.log` on the pool host.

## Unresolved gates / carried decisions (owner)

1. **The two frozen #647 cases** (`heldout_interval_one`, `heldout_long_tools_note_only`) fail on every
   candidate: 22/24 with token_delta 52. Verified cause: they are the only two cases whose device and
   host goldens disagree; the ring-carrying device reproduces the HOST answer byte-for-byte. Options:
   scoped re-capture of those two device ids, drop the lossless RX ring, or keep as blockers.
2. **Assertion-level RAM** (+8,248 B at level 0/1) — the owner's call; it would reopen residency ideas.
3. **Adoption**: the accumulated hoists + unused-output elimination are gated and unpromoted; pin of
   record is `d6b8014fd2fb` at 6.1550 and the business value is 5.3033 until the owner accepts a tree.
4. **B1 W1 zero-tail/replay** and the **row-bounded B2 residual** remain specified but were explicitly
   cancelled by the stop directive.

## Method notes that survived the campaign

* Device metrics are read only from the run's own log; never from a pin or an earlier gate.
* Any ESP-conditional edit must be diffed against previously unconditional code (host gates compile the
  other arm).
* Never reinterpret a pointer's dtype without reading its declaration (P1 is FP32; a uint16 cast
  compiled silently and read wrong indices).
* A guard/assert must be evaluated where it can be true, and a failed check must abort rather than
  print-and-proceed; offsets must never be reused across two edits of one source.
* Fixed array sizes are not bounds checks; use real consumer counts with fallback.
