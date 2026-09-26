# Needle 3 mentor queue

Mentor refresh **2026-09-26 12:51 UTC**; pass began 12:30. Researcher owns all
implementation and measurement. Preserve dirty artifacts, goldens, locks,
anti-repeat history, assertions and supported 240/80 MHz. Read at turnover.

## State and immediate priorities

At arrival all boards were idle and pi had reached its **200-turn auto-resume
limit**. Direct mentor input restarted it; the cap recurred after the first
batch, so a second continuation was sent. This scheduler limit is still active;
do not mistake it for exhaustion of research. No controller files were changed.

**Owner accepted: 5.3033 tok/s. Research pin: 6.1550** on all three boards,
engine `d6b8014fd2fb` (#872/#874/#880). The two frozen #647 device failures still
block adoption; no rebaseline or golden changes. Host 23/23 and device 22/24
are different statuses. Compare exact failing cases, not only aggregate counts.

| Board | Evidence from its own logs | Current / next work |
|---|---|---|
| B1 | `B1comp.log`, engine `0ef0b105716d`, **6.1667**, ext 6.1012, min 5.94, 99 tokens, heap **10615**; device 22/24 delta52; `HOSTGATE-B1comp.log` RC=0, 23/23, fidelity 5.341e-05 | Building **final-norm + mHC + constant attention-gate** composition, adding B2's measured hoist to this two-hoist base. This is a reasonable priority adjustment; let it finish. Next: block-difference fusion below. |
| B2 | `B2gate.log`, engine `49b3077740e9`, **6.1617**, ext 6.0929, min 5.94, 99 tokens, heap **10359**; device 22/24 delta52; `HOSTGATE-B2gate.log` RC=0, 23/23, same fidelity | Idle after its gate. **Start internal Q/K-scale residency now**, independently of B1/B3. Its own incremental base is 6.1617 (mHC + attention-gate), not B1's composition. |
| B3 | FINISHED `B3fold.log`, engine `1c41c9b080fc`, app `3791e18fd8b8`: **6.1600 / 99 tokens**, ext 6.0912, heap 11491, device 22/24 delta52; `HOSTGATE-B3fold.log` RC=0, 23/23, fidelity unchanged | Now free. Start the branch-free factor follow-up immediately; later fold head scales only with their matching consumer. |

B1/B2 host gates finished at **12:44:55/56**, not still pending. #920/#921
carried old internal_free=10623; actual values are above. Correct by appending,
not rewriting historical results. Starting bases: B1/B3 final-norm-only
`c59669b4d143`, 6.1617 / 6.1650 (#907/#910); B2 mHC-only 6.1583 (#919).

**12:51 turnover:** B1's three-hoist `B1x.log` is now measuring, benchmark
PID 398210, app `0c6b48ef328d`, with growing boot/priming output. Do not interrupt
it. B3's host gate finished at 12:49:16; B2's ended six minutes ago. These two
free boards must prepare/launch replacements while B1 runs, not wait on B1.
B3's valid -0.081% versus its 6.1650 base leaves the branch-free variant open.

Launch ready work while preparing another lane. Check real child processes and
growing nonempty logs, not printed PIDs. Freeze a worker through its chained
host gate; checks run INSIDE `needle-board run N --`. Avoid long polling sleeps
while another board is idle. No duplicate controls, anti-repeat overrides or
finish essays in place of the next experiment. Preserve each constituent image.

## Corrected premises that matter

- **Norm scales ARE model-owned float storage.** Arrival B1 `nd_model.c:701`
  allocates fp16_pool; :705 sets p=fp16_pool; :712 sets h=archive data; :714
  assigns slots=p; :726 fills p[k]=nd_f16(h[k]). #913 confused p with h.
  The hoist is once at OPEN, not per layer per token; #917's single-consumer
  argument misses reuse across tokens and heads. No new 3 KB buffer is needed
  for `1+scale`. #913/#917's off-device rejection is withdrawn by the researcher.
- mHC reads **3+4+4+16 = 27 constants/layer, 216/token**, not 736. Its measured
  win remains real; do not turn the mistaken count into a universal claim that
  conversions pay while cheap ALU work cannot.
- B3's first unlaunched build folded slots 7/8 while head norms still added 1.
  The launched form EXCLUDES 7/8, so it avoids that correctness defect. If its
  gate matches, **6.1600 is valid for that branch-bearing implementation**;
  inefficiency does not void data or close the cleaner hoist. Researcher has
  acknowledged this correction to #920's initial INVALID label.
- B1/B2 caches used an owner pointer without resetting on same-address reopen.
  B1's current composition adds invalidation at every open. Carry that repair
  into B2 at turnover too, validate actual nbytes/shape bounds, and retain a
  generic fallback. Fixed global arrays are not automatically model-local.
  `attn_gate` is **m->layer[k].attn_gate element 0**, not a flat model tensor.

## B3 next: make the factor contract uniform

Slots 0/11/13 and final scale_f already contain rounded float32 `1+s` at open.
All four zcrms call sites consume factors. Delete the `pre` member/argument,
ternaries and now-useless s_fnorm_pre mode; both scalar and split emits become
`(s[i] * x[i]) * inv`. Keep reduction and multiply roundings unchanged.
Current ELF has `beqz+j` INSIDE every element at zcsplit_rows +0x3b/+0x3d;
that is not the intended add removal. Verify the next linked body loses them.
Do not add duplicate helpers or a new per-element predicate for nonexistent
raw-scale callers. zcrms_heads is a separate helper and can remain unchanged.

After this isolated result, optionally fold norm-only slots **7/8** at open AND
remove `1+` from zcrms_head_rows together. Other staged slots, especially
14–18/25/26 and cond_v's transpose, remain raw. No reassociation, mapped-data
writes or per-token precompute. Compare complete outputs, then the normal gates.

## B2 next: 3 KB of immutable head scales in internal RAM

`ND_ALLOC16` is PSRAM (`nd_model.h:53`), so the existing norm pool is writable
but external. Slots 7/8 for eight layers at head_dim=48 need **3072 B** internally.
Validate the tensor sizes; allocate one bounded, owned FAST buffer at open,
copy the EXISTING float bytes once, then repoint only these slots. Preserve the
receiving tree's raw-scale/factor representation. Leave original pointers on
allocation failure; handle close/reopen and ownership without double-free.
Prefer allocation after existing hot scratch so its placement is not disturbed.
Confirm actual internal placement and post-prime headroom with assertions intact.
Pre-flash review at 12:52: the first B2 prototype declares res_pool only as a
local, then loses its owning pointer after slot repointing. Store that allocation
in nd_model and free it exactly once in close/failure handling; do not free the
interior slot pointers. If already live, freeze it and record this lifecycle
limitation for turnover. The fixed-model timing is not automatically void.

Hypothesis: eliminate cache lookup/competition on scales reused by 12 Q heads
and 2 K heads, with no per-token copy. It may be neutral because they already
hit cache. #10 staged half-to-float; #911 copied phi every layer. Neither tests
this bounded, boot-only residency. Measure instead of applying the old universal
copy-cost claim. No new timing harness is needed for this candidate.

## B1 next: fuse the final block difference into the residual emit

The only block() caller snapshots u into ublk, runs block, then traverses u to
subtract ublk. In block's final d4 loop first form the SAME rounded residual
`r = u[i] + hada_b[i]*d4[i]`, then store `r - ublk[i]`. Remove only the now-
redundant later subtraction. Retain the snapshot and both original roundings;
never algebraically cancel ublk or fuse the subtract into a different FMA.
Inspect actual FP instruction order and compare the complete difference vector.
This trades the current wide subtraction for no extra u read/write pass:
**49,152 B/token** removed for eight 768-float blocks, not a promised speedup.
#25 fused d4 with residual addition; #705/#707 widened the later subtraction.
Neither removed this pass. Compare with B1's own new composition if it succeeds.

Lower reserve: emit the ublk snapshot alongside u in the existing wide lanepre
producer, eliminating memcpy's **24,576 loaded B/token**. Preserve the wide path
and its scalar fallback. Test separately from final-difference fusion.

## Larger-phase reserve: one bounded CQ2 stall-counter screen

Use only when the next dominant-phase hypothesis needs discrimination, with
other boards screening candidates. #849/#852/#854 inferred IPC from total work
and wall time without two-core accounting; #745 measured 34 cycles/packed word
on a small fixture, not the later asserted 16. A universal instruction floor is
not established. No matching hardware-counter screen was found in the ledger.

Installed IDF has `perfmon`, two counters/core, and `xt_perf_consts.h` masks.
Around one real row-walker invocation per core, retain production PSRAM weights,
internal LUT and row split. Bounded passes: instructions/cycles, D-cache-miss
stalls, then bank/dependency stalls. Record core, rows, bytes and overflow;
never sum overlapping stall events or call instrumented tok/s a speed result.
No cache disable/locking, interrupt masking or whole-suite profiling loop.
If setup blocks, substitute the ready edits above. Memory stalls would motivate
placement/delivery; dependency stalls would motivate a specific schedule.
Sources: [Espressif perfmon API](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-reference/system/perfmon.html),
[event masks](https://github.com/espressif/esp-idf/blob/master/components/xtensa/include/xtensa/xt_perf_consts.h);
verified against installed IDF 5.5.2. The [speed guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/performance/speed.html)
also cautions that binary layout can move small timings; do not overclaim causes.

## Retained results and next mentor check

- #911 corrected four-row phi staging: **6.1417 vs 6.1650**, valid local -0.378%.
  Retire that implementation; it does not price memcpy or all residency methods.
- #915 phi dispatch fusion and the RMS pair: **6.1550**, neutral versus pin.
- #897/#901/#903/#904 were withdrawn correctness failures (missing device
  preparation; pin metrics copied into candidate entries), acknowledged #905.
- Keep specific measured negatives closed: kron2 register rename, four-pass
  Sinkhorn exit, four-output wide QK, 36-byte records #868, LUT de-split,
  private codebook, uint16 offsets and row-owned Kron fusion. Unsupported
  120 MHz and assertion-level RAM changes remain unavailable.

Next mentor: check B2 is actually occupied, B3's branch-free follow-up and raw
full gate, B1's three-hoist composition versus 6.1667, and cache-lifetime bounds.
Watch the recurring 200-turn cap: direct prompts resumed work but did not reset
it. Large phases remain relevant (#786 proj2bit ~79.7 ms, heads ~23.5, MLP ~20.2,
phi ~8.1; overlapping timers cannot be summed). No adoption while #647 remains.
