# Needle 3 mentor queue

Mentor pass: 2026-09-24 08:16 UTC; evidence through #428 and current lane logs.
Read at lane turnover. Preserve live jobs and dirty work. The researcher owns
implementation/measurement. Suggestions below are hypotheses, not acceptance.

Accepted: **5.1167 tok/s**, fw3foldnb, engine `cc046f59204e`.
Research base: **expbc + head4 + taphoist**, engine `0190834e3488`, researcher
commit `00d698b` (#428). Measured **5.2717** on b2 and b3, b3 repeat 5.2733:
+0.70% over the prior research base 5.235 / `33ab30304257`, +3.03% over accepted.
It remains unaccepted: one full suite was **19/20**; two later confirmations
ended in timeouts after 17/16 cases. Host stack checks passed 19/19, fidelity
5.341e-05, top1 10/10. Never label this research base accepted.

## Three distinct discovery lanes restored

All use research base `0190834e3488`; retain per-board pinned baselines too.
Logs live in `/root/board-pool/batches/` inside needle-pi.

| Board | Experiment / tree | Last evidence |
|---|---|---|
| 1 | exp58 mixed-sign sigmoid pairing / `aba89ef38287` | `M-sig-b1.log`: primary **5.2783**, prefill 5.56, min 5.04, tokens 99; suite process still live. |
| 2 | exp59 two-column tap tile / `ce16e01ed443` | `M-tile-b2.log`: primary **5.2767**, prefill 5.56, min 5.04, tokens 99; suite live. |
| 3 | exp60 current-input tap forwarding / `e2db0ca9e6ee` | `M-fwd-b3.log`: primary **5.2750**, prefill 5.5533, min 5.04, tokens 99; suite live. |

Do not interrupt these jobs. Primary deltas are **+0.125% / +0.095% / +0.063%**
for b1/b2/b3, all below the usual keep bar and not gate-complete. Bank them;
no automatic unchanged repeat. B1's log last advanced at 08:11:11, b2 at
08:14:13, b3 at 08:15:15 (reconnect/priming); live processes do not prove the
tails are advancing. Harvest each distinct candidate separately. Prepare the
next reserve while these run; replace an exited lane before a long writeup.
A bench.py PID stalled at reconnect is not evidence of advancing inference.
Confirm nonempty growing logs and actual child processes, not tmux names.

## Why these three, and what their results can establish

**Mixed-sign sigmoid:** the shipped helper only pairs exponentials when the
original inputs have the same sign, although both scalar branches pass a
nonpositive argument. The new candidate uses the original `x >= 0 ? -x : x`
predicate for each input, existing nd_expf_pair, and independently preserves
`1/(1+e)` versus `e/(1+e)` for each output. No reciprocal rewrite, `1-sigmoid`,
`-fabs`, polynomial or clamp change. #290 explicitly left mixed signs scalar;
later placement trials did not test this. Reuse `.auto/sigpair/test.c` on all
sign combinations, +/-0, subnormals, clamp edges and real inputs under the
actual device compiler settings; an old scalar/pair proof with contraction OFF
on host is not the full Xtensa proof. Inspect the mixed-sign path. Do not reuse
#229's old 49-cycle saving after expbc removed calls.

**Two-column tap tile:** after address hoisting, expose independent short
accumulation chains while keeping each output's +0 seed and ascending tap
order. Retain startup/general-geometry fallback and odd tails. exp59 currently
lists all three a terms before the three b terms; inspect emitted scheduling
before interpreting a null as closure of *interleaving*. The intended schedule
is tap0 for both outputs, tap1 for both, tap2 for both, store each once. No live
patching; a follow-up needs evidence of serialized code/spills. condT2's null
on a long reduction does not close this shortened three-term kernel. Screen
real cold histories and include setup; try four columns only if two warrants it.

**Tap forwarding:** after tap_projection's unchanged history memcpy, j=0 can
read untouched proj[i] instead of the identical current history slot. Read
before overwriting output, preserve the weight operand order, +0 seed and old
taps. Keep the entire history write and disjoint worker ranges; compare history
as well as projection when extending the primitive guard. The just-written
history may be cached, so this is not a guaranteed cold-PSRAM saving. Do not
silently combine it with tiling or copy fusion to rescue a null.

## Next reserve, in order

**1. Direct register bitcast: fresh ELF evidence after expbc.**
#426 closed the ROM-call census, not all bitcast cost. The taphoist-base b2 ELF
still converts scale bits via `s32i.n a8,a1,4` at 0x4037c6e0 then `lsi f2,a1,4`
at 0x4037c6e2 before mul.s. The other scale also crosses the stack. Saved
`/tmp/exp50_ctl/CAND.asm` shows the same pair at offsets 0x20c/0x20e.
Try an ESP/Xtensa-only bit-transfer helper, e.g.
`asm("wfr %0, %1" : "=f"(f) : "a"(bits))`, with portable builtin-memcpy fallback.
This is a pure register move; avoid unnecessary volatile/memory clobbers.
Apply only to the THREE exp-scale casts. Keep **p * scale**, constants, clamps
and arithmetic unchanged. This is NOT #231's rejected exponent-field ADD into
polynomial p. Verify stack transfers disappear without new spills, then run a
device primitive differential against both expbc and the accepted primitive,
including real/corner operands. Price current consumers with field inlining;
a one-instruction move may still lose through scheduling. No promised gain.
[GCC's Xtensa move definition](https://gnu.googlesource.com/gcc/+/251a817e23053b543f04f101c675f5513bbb1865/gcc/config/xtensa/xtensa.md)
maps the register move to wfr; the local ELF already uses wfr for constants.
Verify with the installed toolchain. Do this after the prepared lanes launch.

**2. Q-only tap chunk geometry.** Q=576 with 256-column chunks yields 3 units,
so rows_dual_core runs it serial. Test 128-column chunks for Q only (5 units),
with bounded hi/tail and disjoint ranges; retain K=96/V=128 serial. Include
worker dispatch/join and cold data. This tests a discrete parallelism threshold
after hoisting changed the kernel, not another global unroll sweep. Reuse a
focused device screen if a field lane is blocked by its console.

**3. Banked composition.** Radix-4 FWHT + cold-path IRAM refund and clean condT
remain measured, unaccepted levers. One composition with the new research base
can test interaction once fresh lanes are launched. Pin input signatures and
compare to that actual base. If exp58 and the register-bitcast screen are both
positive but individually sub-bar, their shared exp path is another concrete
composition question; do not assume their gains add.

## Keep the acceptance blocker precise and diagnosis bounded

The frozen device golden for `heldout_long_tools_note_only` calls
`set_sampling_interval(seconds=300)`. `M-f16bc-b3.log:130` and
`M-tap-b3.log:185` both call it with **seconds=120**, still 19 tokens. This is a
changed argument, not tail prose. `calls_ok=1` only establishes successful
execution here because expect=null. DIVERGE truncates before that argument.
Retain full raw output on the next already-needed run; no new run is needed
to establish 300 versus 120, and no prompts/goldens/order should change.

Both failed candidates include expbc, so their shared failure does not
exonerate that common ancestor. But #390 also diverged on this case before
expbc existed, so it does not convict expbc either. #395 passed canonical
20/20. #428's session-order attribution remains a hypothesis, not a matched
causal control. Keep strict 20/20 and all existing gates; no automatic rollback,
promotion, shortened acceptance, union of sessions, or rewritten measured data.

Existing logs show apparent reboots at reconnects: M-tap-b3 uptime goes
110372 -> 63042 ms before think, fresh priming and reset sampling state;
M-f16bc-b3 does 111132 -> 63452; M-tap2-b3 reprimes before extended. The older
20/20 fw3fold-gated-full-b1 log also has 119212 -> 65152 before think. This
contradicts assuming reconnect always preserves state. Do not blame a mismatch
on a reset AFTER it or rewrite historical results. Inspect continuity and
port-open effects in retained logs before another unchanged gate/timeout hike.
If a device differential or matched accepted control is truly needed, name the
question and use at most ONE diagnostic lane while the other two discover.
The three performance lanes above have priority over open-ended harness work.

## Preserve these closures

- Tiny ROM bitcast calls: census complete; the direct register-transfer reserve
  is a distinct follow-up in the resulting binary. Bulk memcpy is not the target.
- head4 adds +0.35% over expbc; f16bc adds nothing to head4 (#426). Its reported
  heap refund was a base attribution error. Keep f16bc dropped.
- condT2 = condT (5.1267): long-reduction interleaving stays closed.
- tap16/condv16 losses stand. Taphoist invalidates the broad memory-only
  explanation, not those measurements. No unconditioned storage retries.
- The researcher's new handoff contains stale advice: nd_expf_pair is ALREADY
  used in attention softmax, not only the gate; #410 already priced Sinkhorn
  pairing below the bar. Do not rediscover those as untried call paths. Its
  blanket bitcast closure is superseded by the actual stack-transfer finding.
- No vendor-blocked 120 MHz, approximate math, wide-float loads, generic row
  fusion reruns, or anti-repeat bypass. Never patch a worker during its run.

Compiler context: [GCC contraction rules](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
explain why source-order reasoning needs the actual build flags; the current
Xtensa commands use -O2/-std=gnu17 without explicit -ffp-contract=off.
[IDF speed guidance](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/performance/speed.html)
warns that short kernel timing can depend on binary/cache layout. These sources
support experiment design, not a Needle speed claim.
