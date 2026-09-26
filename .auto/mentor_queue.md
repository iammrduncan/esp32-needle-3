# Needle 3 mentor queue

Mentor refresh **2026-09-26 15:13 UTC**; pass began 15:00. Researcher owns implementation and
measurement. Preserve dirty artifacts, locks, anti-repeat history, quality gates,
frozen goldens, assertions and supported 240/80 MHz. Read at lane turnover.

## Current direction and evidence

At arrival all three boards were idle and pi had reached its 200-turn auto-resume
cap. Direct input resumed it; the cap has recurred each turn. This is a scheduler
stop, not research exhaustion. B2/B3 now have real bench children and growing
logs; B1 fusion has built RC=0 and is launching; keep three independent lanes occupied. No controller changes.

**Owner accepted 5.3033 tok/s; research pin 6.1550 on all boards**, engine
`d6b8014fd2fb`. The frozen #647 pair still blocks adoption. B1x/B3bf diverge only
on `heldout_interval_one` and `heldout_long_tools_note_only`, delta 52. Device 22/24
is not a full pass, even with host 23/23 and fidelity 5.341e-05/top1=10/10.

| Board | Latest evidence / local comparison base | Now / next |
|---|---|---|
| B1 | `B1x.log`, engine `bb214031c523`: **6.1683**, ext 6.1018, think 4.72, heap 10351, 99 tokens; device 22/24 delta 52; host RC=0 | Three hoists composed; free. **Launch final block-difference fusion**, preserving this artifact. |
| B2 | Old `B2qk.log`: startup timeout, host prefix_isolation SEGFAULT; no speed result. Researcher restored source to verified pin`d6b8014fd2fb`, **6.1550** | Corrected owned residency now live as `B2res.log`; freeze through its chained host gate. Its base is NOT the lost 6.1617 mHC+gate candidate. |
| B3 | `B3bf.log`, engine `bbb3de755e6c`: **6.1650**, ext 6.0994, heap 11491, 99 tokens; device 22/24 delta 52; host RC=0 | `B3hf.log` head-scale fold, engine `2cb9407f9613`,app `831cf4a1225c`, now live. FINISHED 6.1683 / 99, ext 6.1018, think 4.72, heap 11491; host RC=0, device 22/24 delta 52. Compare increment to 6.1650; ready for turnover. |

**Correction to #924's proposed B1 edit: KEEP the snapshot memcpy.** Only the
later subtraction pass is removed. Snapshot removal belongs to a separate
producer-fusion experiment and is never a boot-only cache of dynamic u.

Read each run's own metrics. #922 carried B1comp's heap and B3fold's extended
speed into B1x; correct by appending only. B3fold 6.1600 was valid but -0.081%
against its own 6.1650 pre-fold base. B3bf recovered that loss and was neutral
versus that same base; neither justifies a blanket claim that add-hoisting wins.

## Active B1: final block difference in the residual emit

Only block()'s caller snapshots u to ublk, runs block, then subtracts the snapshot.
In its final d4 loop (`nd_model.c`~2817 on B1x), form the SAME rounded residual
`r = u[i] + hada_b[i]*d4[i]`, then store `r - ublk[i]`. KEEP
`memcpy(m->ublk,m->u,sizeof(float)*dm)` and the block call. Delete ONLY the later
wide/scalar subtraction (~2944-2953). No algebraic cancellation or different FMA
contraction. Check the actual linked multiply/add/subtract sequence and complete
difference vector, then normal gates. This removes 49,152 B/token of intermediate
u read/write at 8 x 768 floats, not a guaranteed speedup. #25 fused d4 with residual;
#705/#707 widened the later subtract. Neither removed this pass. Base 6.1683.
Mentor linked-body check 15:15: residual `madd.s` at 0x420130b1, separate
`sub.s` at 0x420130ba, then one store; source keeps the snapshot memcpy.
This confirms the contraction sequence, not a substitute for quality gates.

## Active B2: initialized, owned Q/K-scale residency

Hypothesis: slots 7/8 reused across 12 Q + 2 K heads/layer may benefit from 3072 B of
immutable internal-RAM scales. The existing model-owned fp16_pool is writable
FLOAT storage but ND_ALLOC16 puts it in PSRAM. Copy the existing float bytes
ONCE at open, preserving the receiving tree's raw-scale/factor representation.

The failed version copied immediately after slots were memset NULL, BEFORE the
pool fill. A second unlaunched edit landed inside the fill loop with broken braces.
Those failures do not measure residency. Current B2res has its owned
`qk_resident` field, copy at the end of open after scratch/selftests, and one free in
close; build RC=0. It is already measuring: leave it frozen. QKRES reports
all 8 layers / 3072 B; preliminary decode 6.1550/99 is neutral against its 6.1550 base.
Full device/host gates and post-prime headroom are still owed.
At any follow-up use bound q_norm/k_norm sizes and validate slot presence/lengths;
preserve original pointers on allocation failure and never free interior pointers.
Do not reintroduce a static shared allocation. Its pre-hoist pin needs no cache
lifetime repair; when later composing B1x caches, retain reset at every open,
actual bounds and fallback. mHC reads 27 constants/layer, 216/token, not 736.

B2's earlier 6.1617 mHC+gate source was overwritten by the researcher without a
snapshot. The measurement survives in its logs; the queue is a recipe, not a full
recovery patch. B1x still carries that implementation. Preserve remaining images.

## Active B3: Q/K head factors

Fold rounded float32 `1+s` into norm-only slots 7/8 at open AND remove `1+` from
`zcrms_head_rows` in the same change (done in B3hf). Sum order, epsilon and both
multiply roundings stay unchanged. No new storage or per-element predicate.
Slots 0/11/13 and final scale_f were already factors; all other staged slots,
especially 14-18/25/26 and cond_v's transpose, remain raw. Unlike full-width norm
rows, each head-scale row is reused by many heads. This is independent of B2's
raw-scale placement experiment. Finish ordinary gates, then turnover immediately.

## Next free lane: bounded CQ2 counters, moved UP from indefinite reserve

Use one board while the other two screen candidates. Dominant proj2bit was 79.7 ms
(#786), but #849/#852/#854 mixed total work with two-core wall time; #745's
actual small-fixture cost was 34 cycles/packed word, not the later asserted 16.
Installed S3 core-isa.h has one load/store unit, 4-byte fetch and FLIX3=0; it does
not establish the claimed two-wide issue floor. No prior hardware-counter screen
was found. This is a diagnostic to select a concrete schedule/placement lever,
not another baseline or an instrumented tok/s claim.

Use installed perfmon's TWO counters/core around the real row callback on each
pinned core, not just around nd_parallel_rows on its caller. Keep production
PSRAM weights, internal LUT and normal two-core traffic. Same Q tensor/range on
bounded successive invocations: counter 0 cycles; counter 1 instructions, then
D_STALL_CACHE_MISS, then D_STALL_BUSY|BANK_CONFLICT, then
BUBBLES_R_HOLD_REG_DEP. Record core, rows, bytes, init status,overflow; defer printing
until join. Never sum overlapping event classes. Check event sanity before
interpreting zeros. No cache disable/lock, interrupt masking or whole-suite loop.
If setup stalls, substitute either ready fusion below.

**#926's claimed PMU blocker is withdrawn by source evidence, not a hardware
restriction.** Installed `/opt/esp/idf/components/perfmon/include/xtensa_perfmon_access.h`
and `components/perfmon/xtensa_perfmon_access.c` implement init/start/stop/value/
overflow. Add `perfmon` to the needle component REQUIRES. The installed
`components/xtensa/include/eri.h` explicitly describes ERI as internal to EACH
Xtensa core; this is not a shared MMIO window. No new raw-XDM driver is needed.


Implementation references: [Espressif perfmon](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-reference/system/perfmon.html),
[installed-version core configuration](https://github.com/espressif/esp-idf/blob/v5.5.2/components/xtensa/esp32s3/include/xtensa/config/core-isa.h),
[MimiModel core-local PMU example](https://github.com/memovai/mimimodel/blob/main/needle-esp32s3/main/main.c#L24).
Its single-core fixture is a starting example, not evidence about our two-core
production path. [Espressif's speed guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/performance/speed.html)
also explains why binary layout can move tiny timings; do not overclaim causes.

## Ready reserve: emit the dynamic snapshot with lanepre

Add ublk as a second destination in the existing wide `nd_lanepre4w` emit while
f0..f3 hold the rounded result; keep its first store and all madds. This snapshots
EACH block's newly computed u, never boot state. Update context, prototype, caller,
alignment guard and scalar dual-store. Do not use a7 as the new pointer without
moving its existing per-tile zero temporary. Check both outputs and multi-tile
canaries, then remove only the caller memcpy. Editing only the C fallback leaves
ublk stale when the wide path runs. No extra scratch; saves 24,576 loaded B/token.
Test separately from the residual-difference fusion and against that board's base.

## Ready reserve: norm emit fused with RoPE

Retain ascending sum-of-squares and exact inv. For each half-split pair i/i+half,
compute BOTH fully rounded normalized values as locals using the current raw-scale
or factor contract, then the SAME two RoPE expressions and store only rotated
outputs. No consumer reads the intermediate normalized row. Keep head splitting,
immutable per-token cos/sin and generic fallback; remove corresponding later
apply_rope calls. Q+K removes 43,008 B/token of intermediate read/write (8 layers x
14 heads x 48 floats), a traffic count, not a speed promise.

#607/#608 and #615 combined sequential norm/RoPE helpers in one callback (or Q tap
ownership); they did NOT remove the intermediate norm emit/reload. Their tiny
negative does not close this distinct fusion. Preserve normalized-value rounding
before RoPE: inspect mul/madd/sub operands and compare complete Q/K vectors,
including zeros and ordinary decode inputs. No approximate rsqrt or reassociation.
If exact arithmetic is a blocker, use snapshot fusion rather than an idle lane.

## Retained exclusions and next mentor check

#911 correct phi staging 6.1417 vs 6.1650 lost 0.378%; #915 phi dispatch and RMS pair
were neutral. Earlier #897/#901/#903/#904 staging timings were withdrawn missing-
prepare correctness failures, not memory evidence. Keep measured kron2 register
rename, four-pass Sinkhorn exit, four-output wide QK, 36-byte records #868, LUT
de-split, private codebook, uint16 offsets and row-owned Kron fusion closed.
Do not revive unsupported 120 MHz or assertion-level RAM changes.

Freeze workers through their chained host gate INSIDE needle-board run N.
No repeated controls or anti-repeat overrides. Main source is a stale engine
lineage: never copy it over workers as a reset. Launch ready work while another
lane builds/measures; avoid long sleeps when other boards are idle. The next
mentor should inspect B1's kept snapshot/FP sequence, B2res allocation and full
gate, B3hf's incremental result, then whether counters or distinct fusions really
launched. The 200-turn cap requires direct continuation; finish prose is not work.

15:17 update: B2res FINISHED6.1550, device22/24 delta52,99tokens,heap8415,
host RC0/23of23/fidelity unchanged. Residency is valid and neutral, using3072B.
Keep its artifact, favor the lean pin for subsequent B2 work; no repeat needed.
B1fus is measuring6.1650 versus B1x6.1683 (gate still pending).
B3 is free after its small head-factor increment. Equal6.1683 readings from
different candidates do not prove either mechanism; the researcher corrected
that overclaim in#926. Launch its next independent job rather than reciting reserves.
