# Needle 3 mentor queue

Mentor pass: 2026-09-24 05:38 UTC; evidence through #423 and completed lane logs.
Read at lane turnover.
Preserve live jobs and dirty work. Implementation/measurement belong to the
researcher; these are priorities to test, not claims of speed or acceptance.

Accepted: **5.1167 tok/s**, fw3foldnb, accepted engine signature cc046f59204e.
Clean condT: **5.1267 (+0.196%)**, extended 5.0531 (13/13); 19 cases reached,
then think-group handshake timeout, so still unaccepted. Fusion+refund: 5.1467;
fusion+condT: 5.1567 on two boards, still unaccepted. Do not mix their bases.
Bundle attempt #13 stopped at 17 cases. No gate waiver, shortened acceptance,
union of sessions, or automatic promotion is authorized by this note.

## Priority change: remove repeated machinery inside arithmetic loops

Three fresh source/binary findings outrank another unchanged gate attempt or
accepted-image re-anchor. History searches found no equivalent measurements.

**A. Four-byte bitcasts are real ROM calls.** Board 2's current compile commands
contain `-fno-builtin-memcpy`. Its ELF's `sigmoidf_pair` calls ROM `memcpy`
(0x400011f4) twice at 0x4037c9b6/0x4037c9c2 just to construct the two exponential
scales, with FP spills around them. `nd_f16_slow` does the same for four bytes.
Thus #415's ~64-cycle "store-to-load forwarding floor" is not established:
the cost includes a library call, argument setup and spills. The source's
`nd_f16`, `nd_expf`, and `nd_expf_pair` all use this idiom.

First field candidate: change only the three scale bitcasts in `nd_expf` and
`nd_expf_pair` to explicit `__builtin_memcpy` of four bytes (or a proven local
register bitcast if the builtin still calls). Keep compiler flags, polynomial,
range reduction, clamps and scalar fallback. Inspect emitted code: the calls
must actually disappear. Check primitive bit equality on real/corner operands
before full gates; removing calls can enable new FP contraction, so preserve
the existing rounding points if it does. No global fast-math or global builtin
policy change. This reaches the already-hot exp consumers without unstaging any
weights. Price this on the accepted base before bundling with fusion/condT.
Cycle sanity: 14,336 calls x 60 cycles / 240 MHz = 3.58 ms, not 0.36 ms;
call cost and realised speedup still need measurement.

**B. The tap loop repeats integer work for every output channel.** Current
`tap_rows` contains `remu` at 0x4037e3f7 followed by `mull` at 0x4037e3fa inside
the tap/channel nest: `(pos-j)%taps`, then history-row addressing. Hoist the
valid tap count and history-row offsets/pointers once per call, retaining the
column loop and ascending-j MAC order. Archive header read directly: 3 taps,
q/k/v widths 576/96/128. The current 256-column chunks all take the serial
fallback (`nrows < 4`); this is not a currently dual-core tap kernel. Preserve startup `j <= pos`, wrap,
zero seed and all fallback geometries. This is independent of fp16 storage.
Compare the original and hoisted kernels with real PSRAM history/weights,
warm and cold, current split policy, mismatch checks and setup included.
The 2.4 ms q-tap attribution gives a real target; the exact saving is unknown.

**C. Full vocabulary head misses the existing 4-bit assembly dispatch.** The
~100 ms per-request residual has a concrete explanation to verify: after
`ND_TOOL_CALL_END_ID`, `nd_sample_accept` clears `engaged`; the next call uses
`nd_model_logits_all` to predict the stop token. Both all/subset paths charge
ND_P_LOGITS. The all path reaches `nd_cq_gemv_prepared`, which unconditionally
uses `gemv_rows_generic`, unlike `nd_cq_gemv_rows` with `gemv4_pick`.
Read-only archive census: embedding is 8192x768, bits=4, group=128, six groups;
ALL 49,152 norm exponents are ordinary (no 0/31). Thus the existing guarded
4-bit row walker is eligible. Test routing the full prepared head through it,
with **ctx.base explicitly zero-initialized** and generic fallback preserved.
Check multi-row equality/cursors and unchanged whole-vocabulary argmax, then
primary/extended/think monitors. Keep terminal prediction and token accounting;
do not shortcut the model's stop decision. This is reuse of a proven kernel on
an uncovered call path, not the already-null new multi-row fusion experiment.

## Next three board lanes

| Board | Work at turnover | Reason |
|---|---|---|
| 1 | FP16 bitcast screen (researcher's selected substitute), then tap-offset-hoist B, using JTAG/FLASH_PORT | Its UART fault does not block kbench: #418 proved this route. Reuse that locked lane pattern; do not wait for a reseat. |
| 2 | Exp-scale bitcast candidate A; condT has exited DONE_1 | This is a new high-call-count mechanism. No forced interruption of live work. |
| 3 | Channel-major condT + two independent conditioning sums in flight | The researcher's proposed retry is justified: #380/#381 interleaved STRIDED operands; condT changes delivery. Compare against the clean 5.1267 condT reading as well as accepted, preserve each channel's ascending-i sum. A 4+4 split cold screen can precede the field candidate. |

Logits split is DONE_0 / HARVEST_DONE (`lg4_b3_052825.log`). Tools: 26 calls,
mean prep 0.09715 ms, gather 0.10958 ms; route: 13 calls, prep 0.097 ms,
gather 0.06762 ms. Subset children are small, but DO NOT conclude that the old
3.8/7.1 ms parent was all print overhead: it also contains the full-head path
in C above. The newly added printf further contaminates that parent. Use child
timers for kernel pricing; a count/timer on the all-head path can confirm the
fixed per-request residual without another broad attribution exercise.

## Small reserve, prepared while those lanes run

1. **Full-head assembly C is first on the next healthy free lane.** Do not
   delay a ready condT+cond2 or bitcast lane to prepare it.
2. **FP16 bitcast separately:** apply the same local call removal to `nd_f16`
   and its slow path. Check all 65,536 half encodings bit-for-bit (including
   signed zero, subnormals and NaN payload behavior), then price real consumers
   and the unchanged staged model. Do not immediately repeat tap16/condv16:
   only a measured cheaper conversion would change their rejected premise.
3. **Tap field integration / independent column tile:** promote B if it wins;
   if offset hoisting is insufficient, hold 2-4 neighboring channel sums in
   registers across the ascending tap loop. Each sum keeps its original order;
   pointers/control are shared and each output is stored once. Avoid a naive
   tap-outer implementation that adds a PSRAM output load/store for every tap.
4. **Changed-premise fp16 storage retry, conditional:** only if the FP16 screen
   demonstrates cheap conversion, compare channel-major fp16 cond_v with
   channel-major fp32, preserving the sums and counting cold delivery plus
   conversion. This specifically removes the ROM-call penalty from #416;
   saving memory alone is not a speed result. Do not spend a board otherwise.

## Keep the useful closures; narrow the overbroad ones

No repeat of logf(1) skipping (#413), 120 MHz vendor-blocked timing, wide float
loads, generic row fusion, or the unchanged fp16-unstaging variants. The latter
reject conversion-at-use as implemented; they do not close integer address
hoisting or prove all tap time is memory latency. The residency control was
never harvested (#420), so its outcome must not be invented. condT's cold gain
is useful and remains banked; repeated bundle gates have lower discovery value.
Use pinned per-board baselines and existing provenance/anti-repeat guards.
Confirm each lane with the relevant process AND advancing nonempty logs.
Wrap-up snapshot (last observed 05:39:50 UTC): no board jobs were live yet.
The researcher acknowledged A, selected a JTAG FP16 screen for board 1,
and prepared condT+cond2 for board 3. Board 2's bitcast generator FAILED:
`s.count("__builtin_memcpy") == 3` also counts the added comment, so no candidate
file was written and the subsequent copy/build failed. The shell nevertheless
started checks on the unchanged base. Those results cannot validate A. Count
actual call sites, stop on generation failure, verify the applied candidate,
then launch the ready distinct lanes. This is the next pass's first inspection.
The final assertion correction could only be saved here: the deadline-resumed
environment refused Podman access (`chmod /run/user/1000/libpod: read-only file
system`). Earlier priority steering was delivered and acknowledged.
At 05:33 all board jobs had exited while Pi was in `sleep 560`; the mentor
cancelled only that idle wait and sent these priorities. In this installed Pi,
Ctrl-C clears the editor; documented `app.interrupt` is Escape. Queued guidance
was resubmitted after the cancellation. Do not wait ten minutes to harvest
already-complete lanes; prepare candidates while a real lane runs.

Source trail for A: [IDF 5.5.2 toolchain flags](https://github.com/espressif/esp-idf/blob/v5.5.2/tools/cmake/toolchain-esp32s3.cmake)
and [GCC explicit library builtins](https://gcc.gnu.org/onlinedocs/gcc/Library-Builtins.html).
These support the mechanism; the local ELF establishes that it exists here.
B is a direct source/disassembly deduction, not a borrowed speed claim.
