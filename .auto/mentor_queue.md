# Needle 3 mentor queue

Mentor refresh **2026-09-26 15:22 UTC**; pass began 15:00. Researcher owns implementation and
measurement. Preserve dirty artifacts, locks, anti-repeat history, quality gates,
frozen goldens, assertions and supported 240/80 MHz. Read at lane turnover.

## Current direction and evidence

At arrival all three boards were idle at pi's 200-turn auto-resume cap. Direct
mentor input restarted work repeatedly. Three distinct experiments have now
completed. Their latest results change the queue: do not keep scalar residual
fusion or spend 3 KB on neutral residency merely because they remove operations.
Use the next three lanes below; no repeated controls and no controller edits.

**Owner accepted 5.3033 tok/s; research pin 6.1550 on all boards**, engine
`d6b8014fd2fb`. The frozen #647 pair still blocks adoption. B1x/B3bf diverge only
on `heldout_interval_one` and `heldout_long_tools_note_only`, delta 52. Device 22/24
is not a full pass, even with host 23/23 and fidelity 5.341e-05/top1=10/10.

| Board | Latest completed measurement | Next distinct lane |
|---|---|---|
| B1 | `B1fus.log`, engine `91cf398ec14c`: **6.1650 vs B1x 6.1683**, ext 6.1000, think 4.71, heap 10351, 99 tokens; device 22/24 delta 52; host RC=0 | Preserve the small regression; restore the saved B1x constituent at `/tmp/B1x_engine_snapshot_1512`. **Dynamic snapshot dual-store** in lanepre. |
| B2 | `B2res.log`, engine `18d25dac6a1a`: **6.1550 vs pin 6.1550**, ext 6.0876, heap 8415, 99 tokens; QKRES 8 layers / 3072 B; device 22/24 delta 52; host RC=0 | **Preserved PMU draft blocked**; the second edit failed compilation. Use a ready fusion while its bounded repairs wait. Prefer lean pin for later performance work; preserve residency artifact. |
| B3 | `B3hf.log`, engine `2cb9407f9613`,app `831cf4a1225c`: **6.1683 vs head-fold base 6.1650**, ext 6.1018,think 4.72,heap 11491, 99 tokens; device 22/24 delta 52; host RC=0 | Preserve the small candidate. **Norm emit/RoPE fusion**, distinct from previous dispatch-only fusion. |

All three host gates finished (B3 ~15:13, B2 ~15:16, B1 ~15:19), host 23/23,
fidelity 5.341e-05/top1=10/10. They retain exactly the two blocked #647 cases.
These are candidate results, not owner acceptance. B1x was three independent
constant hoists, engine `bb214031c523`, ext 6.1018/think 4.72/heap 10351 at 6.1683.
Two unrelated candidates printing 6.1683 do not establish a mechanism; compare
each against its own base. Researcher acknowledged that correction in #926.

**Correction to #924's proposed B1 edit: KEEP the snapshot memcpy.** Only the
later subtraction pass is removed. Snapshot removal belongs to a separate
producer-fusion experiment and is never a boot-only cache of dynamic u.

Read each run's own metrics. #922 carried B1comp's heap and B3fold's extended
speed into B1x; correct by appending only. B3fold 6.1600 was valid but -0.081%
against its own 6.1650 pre-fold base. B3bf recovered that loss and was neutral
versus that same base; neither justifies a blanket claim that add-hoisting wins.

## B1 completed: final block difference in the residual emit

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

## B2 completed: initialized, owned Q/K-scale residency

Hypothesis: slots 7/8 reused across 12 Q + 2 K heads/layer may benefit from 3072 B of
immutable internal-RAM scales. The existing model-owned fp16_pool is writable
FLOAT storage but ND_ALLOC16 puts it in PSRAM. Copy the existing float bytes
ONCE at open, preserving the receiving tree's raw-scale/factor representation.

The failed version copied immediately after slots were memset NULL, BEFORE the
pool fill. A second unlaunched edit landed inside the fill loop with broken braces.
Those failures do not measure residency. Current B2res has its owned
`qk_resident` field, copy at the end of open after scratch/selftests, and one free in
close; build RC=0. It finished valid and neutral. QKRES reports
all 8 layers / 3072 B, heap 8415 after priming. Preserve its artifact before turnover.
At any follow-up use bound q_norm/k_norm sizes and validate slot presence/lengths;
preserve original pointers on allocation failure and never free interior pointers.
Do not reintroduce a static shared allocation. Its pre-hoist pin needs no cache
lifetime repair; when later composing B1x caches, retain reset at every open,
actual bounds and fallback. mHC reads 27 constants/layer, 216/token, not 736.

B2's earlier 6.1617 mHC+gate source was overwritten by the researcher without a
snapshot. The measurement survives in its logs; the queue is a recipe, not a full
recovery patch. B1x still carries that implementation. Preserve remaining images.

## B3 completed: Q/K head factors

Fold rounded float32 `1+s` into norm-only slots 7/8 at open AND remove `1+` from
`zcrms_head_rows` in the same change (done in B3hf). Sum order, epsilon and both
multiply roundings stay unchanged. No new storage or per-element predicate.
Slots 0/11/13 and final scale_f were already factors; all other staged slots,
especially 14-18/25/26 and cond_v's transpose, remain raw. Unlike full-width norm
rows, each head-scale row is reused by many heads. This is independent of B2's
raw-scale placement experiment. Its gates finished; turnover immediately.

## B2 bounded CQ2 counters: valuable, but PARK the broken draft at this turnover

15:23 disposition: no PMU device run exists. First draft had a shared-counter
race; second failed `/tmp/b2_pmu2_build.log` with unterminated#if at line3. It
also still latches s_pmu_ptr in workers, prints from workers, never selects the
576-row Q in the caller, leaves global rows/ngroup zero, and does not record
init failures. Preserve this work; do not treat it as counter evidence or an
unsupported-hardware finding. With all boards idle, ready performance fusions
now take precedence. A later bounded repair can measure cycles + ONE specific
event first, then extend events only after that record is interpretable.

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
Preflight of first B2 draft: shared `s_pmu_cnt++` races between cores; alternating
(cycles, insns) with (other bubbles, dependency bubbles) also loses the matched
cycle denominator and mixes Q/K/V shapes. Replace with a sample index and four
records PER CORE, counter 0 always cycles, specific event in counter 1. Select
one 576-row Q packed pointer on the caller before dispatch; target that pointer
only. Printing belongs after the existing join, never inside one worker while
the other is still counted. Read init/overflow status. Do not interpret the draft.

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

## Next B1 (or free B2 substitute): emit the dynamic snapshot with lanepre

Add ublk as a second destination in the existing wide `nd_lanepre4w` emit while
f0..f3 hold the rounded result; keep its first store and all madds. This snapshots
EACH block's newly computed u, never boot state. Update context, prototype, caller,
alignment guard and scalar dual-store. Do not use a7 as the new pointer without
moving its existing per-tile zero temporary to unused a12 (check the current body). Check both outputs and multi-tile
canaries, then remove only the caller memcpy. Editing only the C fallback leaves
ublk stale when the wide path runs. No extra scratch; saves 24,576 loaded B/token.
Test separately from the residual-difference fusion and against that board's base.

## Next B3: norm emit fused with RoPE

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
mentor should inspect whether the next three lanes really launched, the corrected
PMU's per-core denominators/rows/errors, and new fusion outputs. The prior batch
is finished; do not re-read it as live. The 200-turn cap requires direct continuation; finish prose is not work.

At15:23 the mentor confirmed no build/flash/bench was alive, interrupted pi's
stalled turn, and redirected to B3 norm-emit/RoPE plus B1 snapshot fusion. This
pass has six minutes left; consolidate queued work, do not expand instrumentation.
