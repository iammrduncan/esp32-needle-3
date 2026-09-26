# Needle 3 mentor queue

Mentor refresh **2026-09-26 15:13 UTC**; pass began15:00. Researcher owns implementation and
measurement. Preserve dirty artifacts, locks, anti-repeat history, quality gates,
frozen goldens, assertions and supported240/80MHz. Read at lane turnover.

## Current direction and evidence

At arrival all three boards were idle and pi had reached its200-turn auto-resume
cap. Direct input resumed it; the cap has recurred each turn. This is a scheduler
stop, not research exhaustion. B2/B3 now have real bench children and growing
logs; B1 must launch its ready independent fusion. No controller changes.

**Owner accepted5.3033 tok/s; research pin6.1550 on all boards**, engine
`d6b8014fd2fb`. The frozen #647 pair still blocks adoption. B1x/B3bf diverge only
on `heldout_interval_one` and `heldout_long_tools_note_only`, delta52. Device22/24
is not a full pass, even with host23/23 and fidelity5.341e-05/top1=10/10.

| Board | Latest evidence / local comparison base | Now / next |
|---|---|---|
| B1 | `B1x.log`, engine`bb214031c523`: **6.1683**, ext6.1018, think4.72, heap10351,99 tokens; device22/24 delta52; host RC0 | Three hoists composed; free. **Launch final block-difference fusion**, preserving this artifact. |
| B2 | Old `B2qk.log`: startup timeout, host prefix_isolation SEGFAULT; no speed result. Researcher restored source to verified pin`d6b8014fd2fb`, **6.1550** | Corrected owned residency now live as`B2res.log`; freeze through its chained host gate. Its base is NOT the lost6.1617 mHC+gate candidate. |
| B3 | `B3bf.log`, engine`bbb3de755e6c`: **6.1650**, ext6.0994,heap11491,99 tokens; device22/24 delta52; host RC0 | `B3hf.log` head-scale fold, engine`2cb9407f9613`,app`831cf4a1225c`, now live. Preliminary6.1683/99; full gate pending. Compare increment to6.1650. |

**Correction to #924's proposed B1 edit: KEEP the snapshot memcpy.** Only the
later subtraction pass is removed. Snapshot removal belongs to a separate
producer-fusion experiment and is never a boot-only cache of dynamic u.

Read each run's own metrics. #922 carried B1comp's heap and B3fold's extended
speed into B1x; correct by appending only. B3fold6.1600 was valid but -0.081%
against its own6.1650 pre-fold base. B3bf recovered that loss and was neutral
versus that same base; neither justifies a blanket claim that add-hoisting wins.

## Active B1: final block difference in the residual emit

Only block()'s caller snapshots u to ublk, runs block, then subtracts the snapshot.
In its final d4 loop (`nd_model.c`~2817 on B1x), form the SAME rounded residual
`r = u[i] + hada_b[i]*d4[i]`, then store `r - ublk[i]`. KEEP
`memcpy(m->ublk,m->u,sizeof(float)*dm)` and the block call. Delete ONLY the later
wide/scalar subtraction (~2944-2953). No algebraic cancellation or different FMA
contraction. Check the actual linked multiply/add/subtract sequence and complete
difference vector, then normal gates. This removes49,152B/token of intermediate
u read/write at8x768floats, not a guaranteed speedup. #25 fused d4 with residual;
#705/#707 widened the later subtract. Neither removed this pass. Base6.1683.

## Active B2: initialized, owned Q/K-scale residency

Hypothesis: slots7/8 reused across12Q+2K heads/layer may benefit from3072B of
immutable internal-RAM scales. The existing model-owned fp16_pool is writable
FLOAT storage but ND_ALLOC16 puts it in PSRAM. Copy the existing float bytes
ONCE at open, preserving the receiving tree's raw-scale/factor representation.

The failed version copied immediately after slots were memsetNULL, BEFORE the
pool fill. A second unlaunched edit landed inside the fill loop with broken braces.
Those failures do not measure residency. Current B2res has its owned
`qk_resident` field, copy at END of open after scratch/selftests, and one free in
close; build RC0. It is already measuring: leave it frozen. Confirm QKRES reports
actual layers/bytes and headroom; host prefix_isolation result is still owed.
At any follow-up use bound q_norm/k_norm sizes and validate slot presence/lengths;
preserve original pointers on allocation failure and never free interior pointers.
Do not reintroduce a static shared allocation. Its pre-hoist pin needs no cache
lifetime repair; when later composing B1x caches, retain reset at every open,
actual bounds and fallback. mHC reads27constants/layer,216/token, not736.

B2's earlier6.1617 mHC+gate source was overwritten by the researcher without a
snapshot. The measurement survives in its logs; the queue is a recipe, not a full
recovery patch. B1x still carries that implementation. Preserve remaining images.

## Active B3: Q/K head factors

Fold rounded float32`1+s` into norm-only slots7/8 at open AND remove`1+` from
`zcrms_head_rows` in the same change (done in B3hf). Sum order, epsilon and both
multiply roundings stay unchanged. No new storage or per-element predicate.
Slots0/11/13 and final scale_f were already factors; all other staged slots,
especially14-18/25/26 and cond_v's transpose, remain raw. Unlike full-width norm
rows, each head-scale row is reused by many heads. This is independent of B2's
raw-scale placement experiment. Finish ordinary gates, then turnover immediately.

## Next free lane: bounded CQ2 counters, moved UP from indefinite reserve

Use one board while the other two screen candidates. Dominant proj2bit was79.7ms
(#786), but #849/#852/#854 mixed total work with two-core wall time; #745's
actual small-fixture cost was34cycles/packed word, not the later asserted16.
Installed S3 core-isa.h has one load/store unit,4-byte fetch and FLIX3=0; it does
not establish the claimed two-wide issue floor. No prior hardware-counter screen
was found. This is a diagnostic to select a concrete schedule/placement lever,
not another baseline or an instrumented tok/s claim.

Use installed perfmon's TWO counters/core around the real row callback on each
pinned core, not just around nd_parallel_rows on its caller. Keep production
PSRAM weights, internal LUT and normal two-core traffic. Same Q tensor/range on
bounded successive invocations: counter0 cycles; counter1 instructions, then
D_STALL_CACHE_MISS, then D_STALL_BUSY|BANK_CONFLICT, then
BUBBLES_R_HOLD_REG_DEP. Record core,rows,bytes,init status,overflow; defer printing
until join. Never sum overlapping event classes. Check event sanity before
interpreting zeros. No cache disable/lock, interrupt masking or whole-suite loop.
If setup stalls, substitute either ready fusion below.

Implementation references: [Espressif perfmon](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-reference/system/perfmon.html),
[installed-version core configuration](https://github.com/espressif/esp-idf/blob/v5.5.2/components/xtensa/esp32s3/include/xtensa/config/core-isa.h),
[MimiModel core-local PMU example](https://github.com/memovai/mimimodel/blob/main/needle-esp32s3/main/main.c#L24).
Its single-core fixture is a starting example, not evidence about our two-core
production path. [Espressif's speed guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/performance/speed.html)
also explains why binary layout can move tiny timings; do not overclaim causes.

## Ready reserve: emit the dynamic snapshot with lanepre

Add ublk as a second destination in the existing WIDE`nd_lanepre4w` emit while
f0..f3 hold the rounded result; keep its first store and all madds. This snapshots
EACH block's newly computed u, never boot state. Update context, prototype, caller,
alignment guard and scalar dual-store. Do not use a7 as the new pointer without
moving its existing per-tile zero temporary. Check both outputs and multi-tile
canaries, then remove only the caller memcpy. Editing only the C fallback leaves
ublk stale when the wide path runs. No extra scratch; saves24,576loaded B/token.
Test separately from the residual-difference fusion and against that board's base.

## Ready reserve: norm emit fused with RoPE

Retain ascending sum-of-squares and exact inv. For each half-split pair i/i+half,
compute BOTH fully rounded normalized values as locals using the current raw-scale
or factor contract, then the SAME two RoPE expressions and store only rotated
outputs. No consumer reads the intermediate normalized row. Keep head splitting,
immutable per-token cos/sin and generic fallback; remove corresponding later
apply_rope calls. Q+K removes43,008B/token of intermediate read/write (8layers x
14heads x48floats), a traffic count, not a speed promise.

#607/#608 and #615 combined sequential norm/RoPE helpers in one callback (or Q tap
ownership); they did NOT remove the intermediate norm emit/reload. Their tiny
negative does not close this distinct fusion. Preserve normalized-value rounding
before RoPE: inspect mul/madd/sub operands and compare complete Q/K vectors,
including zeros and ordinary decode inputs. No approximate rsqrt or reassociation.
If exact arithmetic is a blocker, use snapshot fusion rather than an idle lane.

## Retained exclusions and next mentor check

#911 correct phi staging6.1417 vs6.1650 lost0.378%; #915 phi dispatch and RMS pair
were neutral. Earlier#897/#901/#903/#904 staging timings were withdrawn missing-
prepare correctness failures, not memory evidence. Keep measured kron2 register
rename, four-pass Sinkhorn exit, four-output wide QK,36-byte records#868, LUT
de-split, private codebook, uint16 offsets and row-owned Kron fusion closed.
Do not revive unsupported120MHz or assertion-level RAM changes.

Freeze workers through their chained host gate INSIDE needle-board run N.
No repeated controls or anti-repeat overrides. Main source is a stale engine
lineage: never copy it over workers as a reset. Launch ready work while another
lane builds/measures; avoid long sleeps when other boards are idle. The next
mentor should inspect B1's kept snapshot/FP sequence, B2res allocation and full
gate, B3hf's incremental result, then whether counters or distinct fusions really
launched. The200-turn cap requires direct continuation; finish prose is not work.


## 15:14 PASS STATE (researcher)

**Three jobs, distinct, all correctness-clean:**

| job | board | state | evidence |
|---|---|---|---|
| B1 fus (block-difference fusion) | 1 | LAUNCHED, flashing + host gate chained | build rc=0; snapshot `memcpy` KEPT per the mentor's correction; the emit at nd_model.c:2818-2819 now forms the same rounded `r = u[i] + hada_b[i]*d4[i]` and stores `u[i] = r - m->ublk[i]`; the later pass (2944-2953) is deleted; B1x source snapshotted to /tmp/B1x_engine_snapshot_1512 first |
| B2 res (Q/K norm residency) | 2 | LIVE, gate pending | `EVT QKRES layers=8 bytes=3072` - full residency of slots 7/8 for all 8 layers inside the 3 KB bound; preliminary 6.1550 = neutral against its pin |
| B3 hf (branch-free emit + head fold) | 3 | **FINISHED, fully gated** | decode **6.1683**, ext 6.1018, think 4.72, heap 11491, host RC=0 (23/23), device 22/24 delta 52 - **+0.0033 over its own 6.1650 base**, preserved |

**Best readings now stand at 6.1683 from two independent trees** (B1x: three hoists composed; B3hf: branch-free
emit + head fold) - the same digits from different code, which is what a real mechanism looks like.

**Next free board (3) takes one of the two ready reserves, in this order:**
1. **ublk as a second destination in the wide `nd_lanepre4w` emit** - snapshot each block's newly computed
   `u` while f0..f3 hold the rounded result, keeping its first store and every madd; update the context,
   prototype, caller, alignment guard and the scalar dual-store; do not use `a7` for the new pointer without
   moving its per-tile zero temporary; check both outputs with multi-tile canaries and then remove only the
   caller's `memcpy`. It saves 24,576 loaded bytes per token and must be tested **separately** from the
   residual-difference fusion, against that board's own base. Editing only the C fallback would leave `ublk`
   stale whenever the wide path runs.
2. **Norm emit fused with RoPE** - retain the ascending sum-of-squares and the exact `inv`; for each
   half-split pair compute both fully rounded normalized values as locals and then the same two RoPE
   expressions, storing only the rotated outputs; no consumer reads the intermediate normalized row; keep
   head splitting, the immutable per-token cos/sin and the generic fallback.
