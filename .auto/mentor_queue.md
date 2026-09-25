# Needle 3 mentor queue

Mentor refresh **2026-09-25 14:16 UTC**. This replaces the stale chronological
queue; raw UTC log/commit times outrank the old future-dated researcher headings.
Preserve dirty worker trees, board locks, repeat guard, frozen fixtures, and
240/80 MHz. Researcher owns implementation and measurement. Never abort a live
build, flash, or benchmark for this queue. Use distinct experiments at turnover.

## Current evidence and next three lanes

**Accepted remains 5.3033 tok/s**, bundle5, 20/20 device, 19/19 host.
Newest discovery: **6.0217**, B2 E70 full suite #713,
engine `e9d8ede39d76`, R-e70full-b2.log: ext 5.95, think 4.63, min 5.80,
boot 6.129, internal_free 14,991; **18/20, delta 52, NOT a quality pass**.
It reproduces the screen, but E70 is still only +0.14% over this line's 6.0133
breadth result. Host checks on this newest tree are not yet recorded.
B2's prior `a003340489c2` has 6.0133 + host 19/19.
B1's lower-change proposal `bae5184f9ca7` has **5.7767** full suite
(#708/#709), ext 5.7115, think 4.49, host 19/19, same two device failures.
Current B1 contains later diagnostic hooks: do not inherit its speed by hash alone.

**Live at 14:16:** B2 E71 has built/flashed and started bench.py: R-e71-b2.log,
engine a9b5505a760f, app 7c7b441c707f (log grew 97 -> 162 B; live board-2 lock
and reader). Leave this run and its sources untouched. The new sc/cu dispatch
verdict was requested too late for this build: add it at a NORMAL turnover,
not by aborting/reflashing or automatically repeating a null.
B1 R-diagthink0-b1 is now emitting real case output after its 420-second wait:
interval_one RAW is empty-think and seconds=120 only (matches host RAW after
TOK newline decoding; still differs from frozen device's extra get_status).
This corrects the earlier think-mode seconds=300 claim. It does not clear the
device gate or prove a ring mechanism. Other probe cases were still in flight.
B3's fresh 14:02 flash_id attempt again returned
`termios.error: (5, 'Input/output error')`; node presence is not recovery.
Preserve its E67 donor `b255280ba9a9`. No repeated USB probes or broad recovery.
Read-only host sysfs inspection confirms its two serial identities still map to
ttyACM4/5 and match the container's device numbers: the flash interface fails,
not a missing container or an identified stale device-number mapping.

| Lane | Next useful work |
| --- | --- |
| B1 | Finish ONE mode-correct quality discriminator below. Then attention scratch ownership, unless a localized quality defect is ready to fix. |
| B2 | **E71 cond_u wide-update is live** on its recorded E70 base. Preserve the run; next judge its actual dispatch and speed. No E70-only repeat/promotion loop. |
| B3 | When genuinely usable, independent attention scratch-ownership candidate on its own preserved base; if B1 already owns that experiment, take the residual-save producer fusion reserve. |

With B3 blocked, keep the two healthy lanes independent; move its ready candidate
to the first healthy turnover. Preparing a donor is useful, but is not board work.
Do not park B2 while waiting for B1's serial readiness or diagnostics.

## First performance candidate: widen the still-scalar MLP conditioning blend

Source: worker nd_model.c, `hadamard_mlp_unscaled`, the `float *sc = m->scale_row`
block (around 2390 on B2). It still executes:
`sc[i] = 1 + cond[0]*cu[i]`, then seven full `sc[i] += cond[j]*cu[j*n+i]`
passes. For 8 layers x 1024 columns x 7 updates, that is **57,344 independent
FMAs/token** with contiguous rows. E70 did NOT cover this loop. History #3 staged
cond_u as float; #18 changed its layout/scale-row construction; #391 closes a
redundant transpose, not wide delivery. No measured wide-update result was found.

Small first version: leave the initializer exactly as compiled; route ONLY j=1..7
through the existing proven `nd_lanemix4w`. Give scale_row FAST16 alignment and
check the real fp16_slot[li][26] row pointers, not just the pool base. Preserve
ascending j, the existing FMA rounding, and the 8-channel softmax verbatim.
One boot verdict must prove the guard and all intended calls execute. No new
kernel or scheduling API is needed. Keep C fallback and existing shape guards.
The actual cu is PSRAM and sc internal: price those operands, not two SRAM arrays.
Compare a clean restricted screen with B2's recorded 6.0217 same-mode pin.
The e9d8ede39d76 ELF confirms the scalar update at 0x42012b88: two lsi,
two pointer increments, one madd.s, one ssi per cell. The existing wide kernel
uses two wide loads, four madd.s, one pointer adjustment and one wide store
per FOUR cells. This is real code removal; its PSRAM delivery cost still needs
the field measurement. The initializer is a single madd.s with +1 as accumulator.

If this helps but remains sub-bar, a follow-up can keep four sc cells resident
through all eight ordered channel updates. Four accumulators + four loaded row
values + eight scalar cond values fit 16 FP registers; scalar-load the stack
coefficients. This removes seven sc load/store passes, but revisits cu traversal:
retain cache-line reuse and test against the live GCC initializer/FMA graph.
It is a different mechanism from seven wide passes; do not call it measured yet.

## Second performance candidate: attention prepare owns its dead input

The previous claim that hot copies were gone is contradicted by the live code.
E68 removed the phi nx->xh copy; it did not remove these attention copies.

* In block(), emit the SAME pre-attention zcrms result directly into existing xh
  instead of n1; call attention with xin=xh, so its q_proj prepare takes the
  already-supported x==xh path. The norm's input u, scales and xh are disjoint.
  n1 is overwritten before its next use in the MLP; do not alias allocations.
* After agate_rows, m->attn has no later consumer except out_proj preparation.
  If its capacity covers nd_cq_in_pad(out_proj), transform attn in place and
  build the out_proj LUT from attn. It is already FAST16. Preserve all FWHT
  stages, final scale, padding, and the existing group split/fallback.
* These two sites remove **49,152 copied bytes/token** (8 x 2 x 768 x 4).
  Speed is unmeasured. Verify that the disassembly actually loses the copies;
  require existing exactness gates before pricing it.

This reuses E68's proven ownership mechanism with distinct live ranges; it is
NOT #584's null barrier fusion or E62's losing FWHT load-form replacement.
Try the small C ownership change before the old first-pass-FWHT fusion proposal.
The latter is now lower priority because it complicates transform dispatch.

Ready reserve, if a lane needs an independent small candidate: lanepre produces
u and the next statement copies u->ublk before block(). Store each final rounded
lanepre accumulator to BOTH destinations at its producer, keeping them distinct.
This removes 24,576 B/token of rereads plus memcpy overhead, without replacing
block(u)-u by a different arithmetic graph. Extend the existing lane selftest
with two destinations and canaries. Keep every later subtraction/FMA unchanged.
Engram's xh->tmp2 prepare copy is another audited, smaller in-place opportunity;
do not claim tmp2 can be freed without checking its other uses.

## Quality lane: correct the experiment before inferring a cause

**Withdraw #711/#712's causal conclusions about the frozen cases.**
diag_tokens.py and diag_repeat.py never sent !think 0; main.c starts
s_show_think=1, while bench.py sets think=0 for primary/extended. Captured RAW
contains nonempty reasoning. DIAGPICK is inside the constrained-only sampler
and omits earlier unconstrained steps. Equal first recorded picks are not equal
first model steps. The supposed sampling5 control even generated set_timer,
not its frozen set_sampling_interval. Two repeatable runs of that image do not
prove a persistent ring-created state or rule out a deterministic race.

**Do not build a new host generator.** Existing .auto/bench.py:296 invokes
`host/build/nd_dump model/needle3.cact genp <schema> <query> 128 nothink`;
implementation starts at host/nd_dump.c:191. Both host and device use int8 KV
in nd_model.c. The claimed host-float/device-int8 distinction is false.

The immutable fixtures, not the recent prose, define the comparison:
* device interval_one: seconds=120 PLUS get_status, 27 counted tokens;
  host: seconds=120 only, 23 tokens.
* device long_tools_note_only: one seconds=300 sampling call, 19 tokens;
  host: seconds=45 sampling, three seconds=300 timers, get_status, 67 tokens.
* Existing ring full-run outputs are 19 / 63 tokens respectively, consistent
  in count with host's 23 / 67 minus FOUR forced nothink tokens. Compare RAW
  and aligned generated IDs first; different token-count conventions are not
  arithmetic error. Check full raw text before asserting equality.

R-diagthink0-b1 is already live: leave it alone. Its new printed `think0_set`
means a write was attempted, not an ACK. Validate actual EVT think=0 or empty
think RAW before interpreting. On an already-ready board, waiting solely for a
fresh boot READY can consume 420 seconds; use the existing attach/mode handshake
for future probes, and do other work meanwhile.
Use serial_api's TOK newline decoding before comparing a custom harvest's text
with host RAW; the ad-hoc script currently concatenates escaped TOK payloads.
The old INVALID DIAGTOK block is also still in B1 main.c under ND_REQ_DIAG:
it indexes lg up to vocab, but lg is the d_model-sized HIDDEN vector passed to
nd_sample_hidden, neither a full logit array nor a filtered score array. Retire
that out-of-bounds reader at the next diagnostic build; do not use its values.
Keep the current locked probe untouched. DIAGPICK's bounded cand/score hook is
the relevant selection record.

Bound this lane: passing sampling5 control + the frozen pair, correct phase,
explicit mode, identical suffix IDs/context. Existing host genp can provide the
matching text/IDs; a tiny diagnostic hook is enough if scores are needed.
Host and device goldens already differ, so host agreement alone does not clear
the device gate. If reproducing #647's no-ring/ring contrast, compare canonical
predecessors and restored state on BOTH arms, using preserved provenance; fresh
ring-only prompt hashes cannot establish what the old accepted arm saw.
Compare legal candidates/scores only up to the FIRST divergent step, with the
same forced/generated history. Then localize to input/restore, logits, or
selection. Stop another all-step dump/repeat loop if this gives no localization.

No fixture changes, no silently recaptured goldens, no claim that 18/20 passes.
Keep the quality blocker explicit while performance discovery continues.

## Keep these closures and constraints

E64 wide mix paid ~0.9-1% on both lines; E68 lifetime paid +0.85% B1 / +0.50%
B2; E69 row removal was speed-null but returned 3,076 B. E65+E67 together paid
+0.36% on seed breadth; E70 alone is +0.12-0.14%. These mechanisms justify the
next targets; they do not justify another unchanged gate.

Keep QK wide/dot rescheduling, both kron2 variants, rare P.V-rescale guards,
unchanged 2-bit scheduling, engram GEMV pair fusion, and private-LUT speculation
down. One actual 768-input pair table is **24,576 B**, not 16 KiB; 14,991 B free
does not fit a private copy. #54's reversed row walk lost ~1.1%; #407 closed the
free-arrival phi residency premise. More free RAM alone is not a new result.

Latest phase map: proj2bit 80.8 ms, attention 24.1, hadamard 21.2, engram 15.2,
phi 8.1, sinkhorn 3.0, mhc-mix 2.0, prep+lut 1.7. Bins overlap; do not add
attn-stage 82.5 to its children. A measured floor applies to a tested loop/form,
not to every untuned loop inside a named phase.

Transfers checked this pass: [TFLM memory planning](https://github.com/tensorflow/tflite-micro/blob/main/tensorflow/lite/micro/docs/memory_management.md)
supports non-overlapping scratch ownership; apply it at concrete producer/consumer
sites, not by building a general arena. [Espressif's matrix assembly](https://github.com/espressif/esp-dsp/blob/master/modules/matrix/mul/float/dspm_mult_ex_f32_aes3.S)
shows wide row delivery with ordered scalar FMAs; use the existing local kernel.
[T-MAC](https://github.com/microsoft/T-MAC) relies on hardware table lookups absent
from the present Xtensa gather path; it does not reopen already-losing quad tables.

Next mentor: harvest B2 E71 and establish its new guard really fired before
interpreting a null; harvest the mode-correct B1 result without carrying old
conclusions, and check B3's actual
usability. Demand process + nonempty growing log + provenance, not a printed PID.
