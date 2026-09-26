# Needle 3 mentor queue

Mentor refresh **2026-09-26 15:09 UTC**; pass began 15:00. Researcher owns all
implementation and measurement. Preserve dirty artifacts, goldens, locks,
anti-repeat history, assertions and supported 240/80 MHz. Read at turnover.

## State and immediate priorities

**Arrival 15:00 UTC: all three boards were IDLE; no build/flash/bench child
existed. pi had stopped at the 200-turn auto-resume cap again.** Direct mentor
input resumed it. At15:08 B3 head-fold is live; B1/B2 still need their jobs.

**Owner accepted: 5.3033 tok/s. Research pin: 6.1550** on all boards,
engine `d6b8014fd2fb` (#872/#874/#880). The two frozen #647 device failures still
block adoption. Host 23/23 and device 22/24 are different statuses; preserve
all gates/goldens and compare the exact failing cases, not only aggregate counts.

| Board | Latest completed evidence, from its OWN logs | Immediate next experiment |
|---|---|---|
| B1 | `B1x.log`, engine `bb214031c523`: **6.1683**, ext **6.1018**, think **4.72**, 99 tokens, heap **10351**; device 22/24 delta52; host RC=0, 23/23, fidelity 5.341e-05 | **Final block-difference fusion**, below, on this three-hoist base. No repeat needed to learn whether the completed gate finished. |
| B2 | `B2qk.log`: NO speed measurement; boot timeout ended 13:18. `HOSTGATE-B2qk.log`: prefix_isolation **SEGFAULT**, RC=1 | **Correct the boot-only Q/K residency implementation**, then host gate BEFORE device. Precise null-copy cause below. B2 has since restored the pin: new base6.1550, not the lost mHC+gate6.1617 tree. |
| B3 | `B3bf.log`, engine `bbb3de755e6c`: **6.1650**, ext **6.0994**, 99 tokens, heap11491; device22/24 delta52; host RC=0, 23/23, fidelity unchanged | **Fold Q/K head scales 7/8 with their matching head consumer**, separately from residency. This branch-free full-width fold ties B3's own final-norm baseline 6.1650; it is not a proven increment. |

Correct stale #922 secondaries by APPENDING only: B1x has heap10351,
ext6.1018, think4.72, not B1comp's heap10615 or B3fold's ext6.0912.
B3fold6.1600 was valid but **-0.081% versus its own 6.1650 base**, not evidence
that the fold improved performance. B3bf recovers that loss; its incremental
result versus the original final-norm-only B3 base is neutral.

Launch a ready lane while preparing the others; freeze each worker through its
chained host gate. Gates execute INSIDE `needle-board run N --`. No repeat
controls, anti-repeat overrides, long polling sleeps or mutable shared staging.
Main checkout is a stale engine lineage; preserve worker candidates and do not
copy main source into a worker as a reset. The 200-turn cap is a scheduler stop,
not evidence of research exhaustion. Direct input resumes it; no controller edits.

## Corrected premises retained

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

## B3 next: Q/K head factors, separate from their residency

The branch-free full-width factor follow-up is FINISHED at 6.1650, locally
neutral. Preserve that artifact. Its slots 0/11/13 and final scale_f contain
rounded float32 `1+s` at open; scalar/split zcrms consume those factors directly.
Now fold norm-only slots **7/8** at open AND remove `1+` from
`zcrms_head_rows` together. These scales are reused over 12 Q heads + 2 K heads
per layer, unlike a full-width row consumed once per block. Keep both multiply
roundings, sum order and epsilon unchanged. No new storage or mode branch.
Compare complete outputs and ordinary gates; verify the linked head body loses
the add. Other staged slots, especially14–18/25/26 and cond_v transpose, stay raw.
This is independent of B2's raw-scale placement hypothesis.

## B2 next: 3 KB of immutable head scales in internal RAM

`ND_ALLOC16` is PSRAM (`nd_model.h:53`), so the existing norm pool is writable
but external. Slots 7/8 for eight layers at head_dim=48 need **3072 B** internally.
Validate the tensor sizes; allocate one bounded, owned FAST buffer at open,
copy the EXISTING float bytes once, then repoint only these slots. Preserve the
receiving tree's raw-scale/factor representation. Leave original pointers on
allocation failure; handle close/reopen and ownership without double-free.
Prefer allocation after existing hot scratch so its placement is not disturbed.
Confirm actual internal placement and post-prime headroom with assertions intact.
**15:04 source diagnosis:** the current B2 residency block is at lines748–792,
immediately after `memset(m->fp16_slot,0,...)`, BEFORE the original pool fill
at ~812–847. Its memcpy source slots are NULL; this directly explains the host
segfault and is consistent with the device startup timeout. Moving only the
ownership field cannot fix this. Move the copy AFTER the pool is filled AND
existing hot scratch allocations are complete; do not subsequently overwrite the
repointed slots. Derive lengths from bound `layer[li].q_norm/k_norm`, check
nbytes/shape/slot presence, then byte-copy the initialized float values. One owned
model field, initialized to NULL and freed exactly once on close/failure, never
free interior slot pointers. Carry the cache validity reset at EVERY open (same
model address can reopen), not just an owner-pointer comparison. Prefix isolation
must pass before another device flash. This failed placement has not measured
the residency hypothesis and cannot support a speed verdict.
If a concrete blocker remains after the bounded correction, preserve it and
substitute the ublk producer/snapshot fusion below on the clean pre-residency base.

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

Next B1 / ready B2 substitute: emit the ublk snapshot alongside u in the
existing wide lanepre producer, eliminating memcpy's **24,576 loaded B/token**.
The production path is `nd_lanepre4w` (gemv4_tie728.S), NOT just the scalar
fallback. Add a second destination to the wide emit while f0..f3 still hold the
rounded result; keep the first store and all madds. Do not use a7 as a pointer
without moving its existing per-tile zero temporary. Update prototype/caller,
context, alignment guard and scalar dual-store; selftest checks both outputs
plus multi-tile canaries. Only then remove the caller memcpy. Extending only C
would silently leave the snapshot uninitialized when the wide path runs.
Test separately from final-difference fusion; no new scratch needed.

## Next turnover: one bounded CQ2 stall-counter screen (B2 after residency)

Move this UP from indefinite reserve after the current runnable batch, while
B1/B3 screen fusions. The dominant phase still needs discrimination. #849/#852/#854 inferred IPC from total work
and wall time without two-core accounting; #745 measured 34 cycles/packed word
on a small fixture, not the later asserted 16. A universal instruction floor is
not established. No matching hardware-counter screen was found in the ledger.

Installed IDF has `perfmon`, two counters/core, and `xt_perf_consts.h` masks.
Its S3 `core-isa.h` says one load/store unit, four-byte fetch width, FLIX3=0;
none supports #849's assumed two-wide issue arithmetic. Do not infer an IPC
floor from total tensor work divided by wall time across two active cores.
Around one real row-walker invocation per core, retain production PSRAM weights,
internal LUT and row split. Bounded passes: instructions/cycles, D-cache-miss
stalls, then bank/dependency stalls. Record core, rows, bytes and overflow;
never sum overlapping stall events or call instrumented tok/s a speed result.
Implement around the actual row callback (inside each pinned core), not around
`nd_parallel_rows` on only its caller. Counter0=cycles; counter1 successively
`INSN_ALL`, `D_STALL_CACHE_MISS`, `D_STALL_BUSY|BANK_CONFLICT`,
`BUBBLES_R_HOLD_REG_DEP`. Same Q tensor/row range on successive bounded
invocations; keep normal two-core production traffic. Save per-core records and
print after join, outside measured bodies; check init errors/overflow. Core-local
PMU setup exists in [MimiModel main.c](https://github.com/memovai/mimimodel/blob/main/needle-esp32s3/main/main.c#L24),
but its single-core fixture is a starting example, not the production answer.
No cache disable/locking, interrupt masking or whole-suite profiling loop.
If setup blocks, substitute the ready edits above. Memory stalls would motivate
placement/delivery; dependency stalls would motivate a specific schedule.
Sources: [Espressif perfmon API](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-reference/system/perfmon.html),
[event masks](https://github.com/espressif/esp-idf/blob/master/components/xtensa/include/xtensa/xt_perf_consts.h);
verified against installed IDF 5.5.2. The [speed guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/performance/speed.html)
also cautions that binary layout can move small timings; do not overclaim causes.

## B3 follow-up: fuse norm EMIT with RoPE, not just their dispatch

Source-specific new candidate after head-factor result: retain the ascending
sum-of-squares and exact inv computation. For each half-split pair i/i+half,
compute BOTH fully rounded normalized values in local floats using the receiving
tree's raw-scale/factor contract, then the same two RoPE expressions and store
only the rotated outputs. Original normalized rows are never used elsewhere
between these two calls. Use the same head split, immutable per-token cos/sin,
and a generic fallback; remove only the corresponding later apply_rope calls.
Full Q+K coverage would avoid **43,008 B/token** of intermediate write/read at
8 layers x14 heads x48 floats, a traffic count rather than a speed promise.

#607/#608 and #615 tested sequential norm+RoPE helpers in one callback (and Q
taps ownership), NOT removal of the intermediate norm emit/reload. Check their
preserved patches before implementation if needed, but do not call their -0.029%
a proof against this distinct fused emit. The compiler-contraction trap from
#608 still applies: keep both normalized operands rounded BEFORE RoPE, inspect
mul/madd/sub order, and compare complete Q/K vectors including zero/signed-zero
and ordinary decode inputs. No approximate rsqrt or changed reduction order.
If preserving the arithmetic becomes a blocker, use snapshot fusion instead of
leaving the lane idle.

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

Next mentor: check all three jobs have real children/nonempty growing logs,
B2's corrected allocation order and prefix gate, B1's residual-difference FP
sequence, and B3's head-factor result against its own 6.1650 baseline. Watch the
recurring 200-turn cap. Large phases still matter (#786 proj2bit~79.7ms,
heads~23.5, MLP~20.2, phi~8.1; overlapping timers cannot be summed). No adoption
while #647 remains unresolved.


15:08 turnover: researcher restored B2 to the research pin without saving the
candidate source; its old measured result remains valid but is NOT B2's current
base. Do not claim the queue contains a full recovery patch: it records a recipe;
B1x still carries the mHC+attention-gate implementation. Preserve all remaining
artifacts before changing workers. B2's new baseline is **6.1550**, engine
`d6b8014fd2fb`, once its ordinary provenance check confirms that exact tree.
The snapshot substitute is **per block per token**, emitted alongside the newly
computed u in lanepre; it is not a boot snapshot and never replaces dynamic state
with cached values. B3 head-fold is now launched as `B3hf.log`, engine2cb9407f9613,
app831cf4a1225c. Leave it frozen through its gate.
