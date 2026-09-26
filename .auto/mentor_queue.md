# Needle 3 mentor queue

Mentor refresh **2026-09-26 12:43 UTC** (pass began 12:30). Researcher owns all implementation,
builds and measurement. Preserve worker dirt, frozen goldens, locks, anti-repeat
history, assertions, and supported 240/80 MHz. Read this at lane turnover.

## Actual state

At 12:30–12:32 UTC **all three boards were idle**: no build, flash, benchmark,
or gate process; latest batch log ended 11:05. `pi:agent` is alive but reports
**“Autoresearch auto-resume limit reached (200 turns)”**. Mentor restarted the
idle turn via tmux and the researcher resumed. Owner disposition is not a
prerequisite for safe, unpromoted experiments.

- Owner-accepted **5.3033 tok/s**. Per-board research pin **6.1550**, engine
  `d6b8014fd2fb`, #872/#874/#880. These are different statuses.
- B1/B3 starting bases were final-norm conversion hoist `c59669b4d143`:
  **6.1617 / 6.1650**, #907/#909/#910. Raw B1/B3 gates match the pin's known
  **22/24, delta52, 99 primary tokens**; host 23/23, fidelity 5.341e-05.
- B2's starting base was mHC conversion hoist #919: **6.1583**, `B2b4r.log`, host
  `HOSTGATE-B2b4r.log` RC=0, device 22/24/delta52/99, internal_free 10623.
- The two frozen #647 failures still block adoption. Matching their aggregate
  count alone does not prove candidate identity: inspect the same failing cases
  and outputs. Never call 22/24 a full pass or silently change the goldens.

**12:43 progress:** B1 composition and B2 gate hoist are running under their
locks, benchmark PIDs 393037 / 393100, with growing `B1comp.log` / `B2gate.log`.
Primary readings **6.1667 / 6.1617**, both 99 tokens; full device/host gates
still pending. B3's revised factor build is ready, excluding head slots 7/8 to
avoid its double-add defect; launch is queued. B3 still has the per-element
mode branch described below, so this is a restricted first implementation.
Do not edit these live workers. B1/B2's owner-pointer-only caches still need
the documented same-address reopen/lifetime fix at turnover, before promotion.

## Important premise correction: the norm pool really is writable fp32

**The appended archive-pointer claim and #913/#917 closure are false.** In the
actual B1 worker `nd_model.c:701`, `fp16_pool = ND_ALLOC16(sizeof(float)*total)`;
line 705 sets **p = m->fp16_pool**; line 712 sets **h = nd_cact_data(...)**;
line 714 sets the slot to **p**, and 726 fills **p[k] = nd_f16(h[k])**.
`p` and `h` are different pointers. Slots 0/7/8/11/13 own staged float storage.
The proposed precompute is **once at model open**, not once per layer per token.
One call site therefore does not refute reuse across every decode token; head
norms additionally reuse a scale across heads. No new 3 KB destination is needed.
Correct the historical claim by appending evidence, not rewriting old entries.

Also correct the mHC conversion census: this code runs once per layer, not once
per lane. With n=4 it reads **3 + 4 + 4 + 16 = 27 constants/layer, 216/token**,
not 736. This does not invalidate #919's measured speed; it limits the causal
story. “Conversions win, cheap ALU work cannot” is not established by two small
positives and an unbuilt, incorrectly specified add-hoist.

## Current three lanes

| Board | Candidate and comparison | Reason |
|---|---|---|
| B1 | Compose final-norm + mHC conversion hoists; compare to its 6.1617 final-norm base and 6.1550 pin | Both constituents have measurements; price the combination without duplicating it on other boards. |
| B2 | Hoist immutable `sigmoidf_(fp16_get(attn_gate,0))` to model open; preserve #919 and compare to its 6.1583 base | Eight repeated exp/div/conversion computations per token need only eight floats; no activation caching. |
| B3 | Precompute rounded `1+scale` in existing norm-only float slots at model open, on its 6.1650 final-norm base | Reopens an executable idea invalidated only by a misread pointer and wrong time scope; no extra allocation. |

Preserve measured artifacts before edits; do not copy main's stale engine over
workers. Launch a ready lane while preparing the others. After launch confirm
real child processes and growing, nonempty logs; freeze each worker until its
job ends. Host checks must run **inside** `needle-board run N --`, not as a
shell command accidentally chained outside the wrapper. Read each run's own
metrics. If blocked, substitute a concrete candidate; no identical controls,
anti-repeat override, finish essays, or open-ended verification loops.

**B1 composition detail.** #919 currently uses fixed-size process globals with
`s_mhc_ready` filled lazily in the token path and no reset at model open/close.
Before carrying that into a new candidate, bind the constants to the model
lifetime, validate each tensor's nbytes and n_layers/lanes, and initialize after
binding at open (or retain a correct generic fallback). Do not reuse stale
values when a different model is opened. Preserve exactly `nd_f16(raw[i])` and
consumer indexing; shape[0] alone caused #918's 14/24 failure. Do not fuse bias
and lane offsets: that would change rounding. These bounded lifecycle checks
belong to this implementation, not a new harness campaign.
An owner-pointer comparison alone is insufficient: reopening the same `m`
address must invalidate/refill the cache too. Fill at open after binding, or
explicitly reset validity at every open and retain bounds/fallback checks.

**B2 immutable gate detail.** `block()` still computes
`g = sigmoidf_(fp16_get(m, &L->attn_gate, 0))` before the residual emit.
Compute exactly this expression once per model/layer after binding, with the
same sigmoid implementation and FP semantics; consume g unchanged. Store per
model or otherwise handle reopen and supported shapes safely. Eight floats
for this model, and no cached dynamic sigmoid inputs. Keep the engram alpha,
attention gates derived from activations, and mHC nonlinearities unchanged.
No matching attention-gate hoist was found in the ledger search.
The scalar is **`m->layer[k].attn_gate` element 0**, not a model-level flat
`m->attn_gate` tensor (that member does not exist). Preserve per-layer binding.

**B3 scale detail.** During the existing pool fill, for norm-only slots
**0/7/8/11/13**, store the float32-rounded `1.0f + nd_f16(h[k])` in p[k].
Keep all other slots raw, especially 14–18/25/26, and retain cond_v transpose.
Use factor*x*inv in **all three** emit implementations (`zcsplit_rows`, scalar
`zcrms`, `zcrms_head_rows`), preserving both multiply roundings and reduction
order. B3's already-hoisted final `scale_f` must also contain the factor at
open before the common zcrms contract changes. Audit all consumers including
host/fallback paths, and compare complete outputs against the original form.
Do not precompute per token, combine inv with the factor, or edit mapped data.
This is an independent incremental experiment, not a repeat of #917 (unbuilt).
**12:41 pre-flash review:** B3's first build incremented slots 7/8 but left
`zcrms_head_rows` adding 1 again — fix before measuring. Its added `pre` flag
also compiled to `beqz+j` inside each element of zcsplit_rows, replacing one
removed add with two branches. All callers now consume factors: use that
uniform contract, or select entire loops once, not an elementwise mode test.
If that first image is already running, leave it alone and mark its data void.

## Next turnover reserves

At turnover: **B1 -> final-difference fusion; B2 -> internal Q/K scales;
B3 -> branch-free factor contract, then fold head scales with their consumer.**
Keep raw constituent artifacts and compare each incremental change to its own
measured base as well as the 6.1550 per-board pin. The optional counter screen
is a substitute when a larger-phase decision needs evidence, not a pause on
the other two boards. No matching buffer-pass/residency trials were found;
#25 fused d4 scaling with residual addition, and #705/#707 only widened the
later subtraction.

1. **Fuse block-difference subtraction into the final d4 residual emit.**
   `step_hidden` copies u to ublk, calls block, then traverses u again to do
   `u -= ublk`. In block's last loop, first form the SAME rounded residual
   result `r = u[i] + hada_b[i]*d4[i]`, then store `r - ublk[i]` directly.
   Remove only the now-redundant later subtraction for this caller; preserve
   the ublk snapshot and generic call semantics. Do not algebraically cancel
   ublk or merge the subtraction into a different FMA: both original roundings
   matter. This trades the current wide subtract for no extra u read/write
   pass, saving 8*768*8 = 49,152 B/token of internal u traffic. Inspect generated
   FP order and compare the complete block difference before field timing.
2. **Boot-resident Q/K norm scales in internal RAM.** Actual `ND_ALLOC16` is
   PSRAM (`nd_model.h:53`), so the existing norm pool is writable but external.
   Only slots 7/8 for all eight layers need **2*8*48*4 = 3072 B** internally;
   validate tensor nbytes/head_dim rather than assuming shapes. Copy their
   current float bytes once at open, repoint only these slots, and preserve
   the receiving tree's raw-scale or factor representation unchanged. Keep
   ownership/close/failure paths correct; if allocation fails leave the original
   pointers. Prove the allocation is internal and check post-prime headroom
   with assertions intact. This tests cache lookup/competition on scales reused
   by 12 Q heads and 2 K heads, with NO per-token copy. #10 staged fp16 into
   float storage; #911 copied phi every layer; neither tests this residency.
   It may be neutral because the scales already hit cache: measure, do not infer.

3. **One bounded CQ2 counter screen, if dominant-phase work is being rejected
   by the old “instruction floor”.** #849/#852/#854 infer IPC from total work
   and wall time without two-core accounting; #745 actually measured 34 cycles
   per packed word on a small fixture, not the later asserted 16. This is not
   a proof that every schedule is exhausted. Use the installed IDF `perfmon`
   component around one real row-walker invocation per core, with production
   PSRAM weights/internal LUT and row split. Two counters per core are available:
   bounded passes for retired instructions/cycles, D-cache-miss stalls, and
   data-bank/dependency stalls, using local `xt_perf_consts.h` masks. Record
   core, row count, bytes and overflow; do not sum overlapping stall categories
   or call instrumented tok/s a speed result. No cache disable/locking, interrupt
   masking, whole-suite profiling loop or new harness. If counter setup blocks,
   use either ready performance edit above. A memory-stall result would justify
   a placement/delivery experiment; dependency stalls would justify a narrowly
   targeted schedule. Stop after this discriminating screen.

The counter approach is supported by [Espressif's perfmon API](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-reference/system/perfmon.html)
and [event masks](https://github.com/espressif/esp-idf/blob/master/components/xtensa/include/xtensa/xt_perf_consts.h);
mentor verified both APIs/masks and two-counter configuration in installed IDF
5.5.2. Small timing shifts can also come from binary layout, as Espressif's
[speed guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/performance/speed.html)
explains; constant-hoist wins alone establish no universal ALU/conversion rule.

Lower reserve: emit the ublk snapshot alongside u in the existing wide
lanepre producer, eliminating the separate memcpy's 24,576 loaded B/token.
Keep this separate from final-difference fusion, and retain the wide path.

Test these separately against a documented receiving tree; compose only after
each has a result. They are traffic-removal hypotheses, not predicted speedups.

## What is actually closed; what remains open

- #911 corrected four-row phi staging **6.1417**, versus its B3 final-norm base
  6.1650: a valid local -0.378% result. Keep this implementation closed.
  Timing alone does not separately price memcpy, bank conflicts, or every
  residency strategy; its claimed universal ns/byte cost is not established.
- #915 fused 24-row phi dispatch **6.1550**, exactly neutral against the pin;
  RMS pair #908/#909 also neutral. Retire those specific implementations.
- #897/#901/#903/#904 are withdrawn correctness failures: ESP skipped the
  unconditional `nd_cq_prepare`, and ledger secondaries came from another run.
  #905 acknowledged this. They prove no copy or residency performance law.
- Retain measured negatives: kron2 register rename, four-pass Sinkhorn cycle,
  four-output wide QK, 36-byte records (#868), LUT de-split (#446/#813), private
  codebook (#808/#809), uint16 offsets (#749), row-owned Kron fusion (#664).
  Unsupported 120 MHz and assertion-level RAM changes remain unavailable.
- Large phases still dominate (last map #786: proj2bit ~79.7 ms, attention-head
  work ~23.5 ms, MLP ~20.2 ms, phi ~8.1 ms); do not turn constant hoists into a
  universal search rule. These three cheap builds buy useful time to find a
  concrete changed premise for a larger phase. Do not sum overlapping timers.

Next mentor: check that the stopped scheduler actually resumed with three
 distinct jobs, verify B3 used the allocated pool rather than another invented
 destination, inspect each new run's raw output/gate, and compare the composed
 hoist against B1's own constituent base. No adoption claim without resolving
 the frozen quality blockers.


## B3 STATUS at this turn: launched, result to be treated as INVALID, with two fixes owed

1. **Per-element mode branch (the reason the run is not a valid test).** The prescaled `1+scale` variants
   are written as `pre ? s[i]*x[i]*inv : (1.0f+s[i])*x[i]*inv`, and the current ELF shows that test compiled
   INTO the element loop - `zcsplit_rows` at 0x4037b56f/571 has a `beqz`+jump per element. A candidate that
   adds a branch per element cannot measure the removal of an add per element. **Fix:** choose the variant
   ONCE per call (whole-loop helpers: `zcrms_pre` / `zcrms` and `zcsplit_rows_pre` / `zcsplit_rows`), so the
   loop body is branch-free in both cases and the comparison is add-or-no-add, nothing else.
2. **Slot 7/8 consumer audit.** `zcrms_head_rows` (~line 269) still emits `(1.0f + c->s[i]) * v[i] * inv`.
   Any fold of slots 7/8 at open would double-add for Q/K. In the source as of the last edit the fold list is
   restricted to slots 0, 11 and 13 (plus `scale_f`), so the values are consistent - but the fold list and the
   consumer list must be re-audited together before any rerun, and the cleanest form is to fold all five
   slots AND convert `zcrms_head_rows` to the prescaled form in the same change.

**Measured while B3 ran (own logs, device, pending full gates):** B1 composed (final_norm + mHC hoists,
per-model cache) **6.1667** with `gen_tokens 99`; B2 attn_gate-sigmoid hoist **6.1617** with `gen_tokens 99`.
Both are above the 6.1550 pin and both show the correct token count, so both are correctness-plausible and
await their device gates; neither is promoted.

**B1/B2 lifetime note (mentor):** an owner pointer alone does not cover reopening the SAME `m` address.
The robust forms are (a) fill right after the tensors are bound at open, or (b) reset the validity flag at
every open and keep the bounds/fallback path. Do this at turnover, on the workers, not while their lanes run.
