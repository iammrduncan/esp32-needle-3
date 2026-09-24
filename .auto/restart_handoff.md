# Fresh Needle 3 autoresearch handoff — 2026-09-24 05:55 UTC

The previous Pi conversation exhausted roughly 91% of its context and fell into
a repetitive planning loop. This is a new session. Do not resume or reconstruct
that conversation. Continue the performance campaign from durable evidence.

Read `.auto/mentor_queue.md` completely first. It contains Astra's current
research and priority decisions. Then read only the active operator directives
at the top of `.auto/prompt.md` and the last few records of `.auto/log.jsonl`
(especially #421-#423) needed to establish provenance. Do not delay launches for
broad canonical rereading, historical narration, methodology work, or another
full audit of already-closed experiments.

Current facts at handoff:

- Accepted shipping rate is 5.1167 tok/s with canonical engine signature
  `cc046f59204e`.
- Clean condT measured 5.1267 tok/s (+0.196%) and completed primary plus 13/13
  extended cases before the think-group serial handshake timed out; it remains
  unaccepted.
- Fusion+condT reached 5.1567 on two boards but remains gated. Do not mix bases.
- All three boards currently report connected. There are no live board jobs and
  no board-lock owners. Old tmux lane sessions are inert shells, not work.
- `.auto/exp49/` contains the prepared condT+two-accumulator correctness guard;
  its randomized equivalence check passed with zero mismatches.
- The first exp-scale bitcast generator FAILED: its occurrence assertion counted
  `__builtin_memcpy` in its own comment, no candidate file was written, copy/build
  failed, and later checks ran on the unchanged baseline. Those checks are not
  candidate evidence. Repair it with fail-fast chaining, count actual call sites,
  verify the applied file differs, and prove the ROM calls disappear in the ELF.

Act now rather than narrating. Restore three independent lanes:

1. Board 3: launch the ready channel-major condT + two independent conditioning
   sums experiment, preserving each accumulator's ascending input order and the
   pinned accepted base.
2. Board 1: launch a JTAG/FLASH_PORT microbenchmark for the FP16 builtin-bitcast
   cost; follow with tap-offset/address hoisting. Do not leave it idle merely
   because its UART console was previously intermittent—the board is connected
   and the JTAG screen path was already proven.
3. Board 2: repair and launch the three exp-scale `memcpy` to
   `__builtin_memcpy` candidate on the accepted base. Keep the FP16 site separate.

Launch ready lanes while preparing the remaining lane. Within ten minutes there
must be three distinct live jobs with growing nonempty logs, or a concrete
per-lane hardware/build blocker plus an immediate substitute. Confirm processes
and log growth after launch; printed PIDs are not evidence.

Next prepared research direction after those lanes: Astra found that the
full-vocabulary output head appears to use `gemv_rows_generic` even though the
8192x768 4-bit/g128 embedding satisfies the guarded assembly kernel. Preserve
the full terminal-token prediction and generic fallback, explicitly initialize
the row context base, and gate multi-row cursor/equality before field testing.

Keep quality strict, preserve dirty work, and log measured results. Do not spend
the fresh context repeating the explanation above.
