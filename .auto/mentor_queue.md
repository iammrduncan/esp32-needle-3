# Needle 3 mentor queue

Mentor refresh **2026-09-26 00:19 UTC**. This replaces the stale chronological
appendices; experiment history and assets remain in `.auto/log.jsonl` and
`.auto/exp88`–`exp94`. Preserve dirty workers, locks, the anti-repeat guard,
frozen quality gates and 240/80 MHz. Researcher implements and measures.

## Current evidence and priority change

**Accepted remains 5.3033 tok/s (bundle5, 20/20 device, 19/19 host).** Best
proposal is **6.1117** on B3 (#762), prefill 6.4533, ext 6.0415, think 4.68,
min 5.89, host 19/19 / delta 0 / fidelity 5.341e-05. Device is still **18/20,
delta 52**, so it is not quality-approved. Ring correlation (#647) neither
proves the claimed timer mechanism nor permits replacing goldens.

Group hardware LOOP + outlined `qk_dot8` compose: B3 6.1100 screen / 6.1117
breadth; B2 **6.1083** screen (#766). **Correction to #758/#764:** their cited ROWRANGE/E49 differentials exercise
`nd_gemv4_rows_tie1` (4-bit), not the changed CQ2 `nd_lut2_rows_tie1n`.
`kbench.c:1649,1688,1812` proves the dispatch; rowbytes=384 for in_pad=768
is another clue. Both inspected `build_kb/build.ninja` kbench compile lines
omit `ND_KBENCH_ASM`, whose source default is 0. Do not call those cited lines
CQ2 coverage. Couple the actual CQ2 differential to B2's NEW experiment below;
do not launch another all-board verification cycle. The separate QK differential
remains owed as bounded supporting work. #769/#770 are recovery observations, not new timings;
ignore carried metrics when identifying the newest experiment.

**New decisive EG2 evidence:** `/tmp/eg2dbg.txt` in the container, lines around
2180–2196 and 4480–4496, prints `ERR prefix_cache_allocation` immediately after
213/213 priming, then StoreProhibited with **EXCVADDR=0**. Earlier lines show
the intended second-site pointers and successful K/V calls. The complete log
settles the first failure: prefix allocation, not a demonstrated GEMV fault.
The final panic path after the error is a separate issue; no new call-site
instrumentation is needed to establish the allocation failure.

Read-only inspection found **two whole EG2 allocation blocks** in the saved
`.auto/exp94/nd_model.c.eng2seed` (around 880–937 and 938–999), also present in
the live diagnostic before restoration. Both allocate 313,344 B; the second
sets `m->eg2_psram=NULL` and leaks the first. The capture prints two `EG2 ok=1`
lines each boot. Open free falls 2,062,492 -> 1,423,508 B (~639 KB). This is a
concrete defect, not a layout theory. Seed `prefix_copy` also still copies the
full window: **B1's compact-prefix change was never ported to seed**. Even one
313 KB copy needs an actual post-prime budget, not the open-time 2 MB print.

## Next three board lanes

| Board | Next work | Own comparison / availability |
|---|---|---|
| **B2 now** | **Amortize CQ2 long-loop setup across rows** (below). | 6.1083 composed pin. Usable and idle at inspection; saved firmware is sufficient as a reference. |
| **B3 next** | **FP32 norm-sidecar probe on one real CQ2 tensor.** | EG2+compact-prefix screen completed at 6.1217; post-prime 517,028 B free. Preserve this tree; measure a distinct change. |
| **B1 NOW — recovered** | **Port only CQ2 group LOOP to the 5.8633 wide-phi/compact-prefix tree.** | Existing B1 source pin 5.8633; do not import seed NF16V (lost 1.11% here). Flash was replaced by the known-good B2 image during recovery. |

**B1 recovered at 00:16 by the corrected attach. All three boards are usable.**
The tmux result is `STATE: {device: esp32-s3, uptime_ms:55239,
free_internal_bytes:11183, free_psram_bytes:50088, ...}`. No physical power
cycle was needed. Retire the hardware-outage claim and start its distinct
branch-only LOOP experiment. The old recovery scripts opened serial with
DTR asserted; `serial_api.Device`'s proven closed-constructor/false-before-and-
after-open discipline succeeded. Actual API: `Device(port,115200,180,20,False)`
and `state()`. Use locked per-board ports, not invented keyword arguments.
[Espressif boot-mode reference](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

**Newest real performance result, B3:** completed `R-eg2cp-b3.log`, engine
`33f133d26421`, app `4386510750ec`: **decode 6.1217**, prefill 6.4633,
min 5.90, 6/6 exact, delta 0. Actual post-prime STATE free PSRAM **517,028 B**;
open-time metric 1,743,000 is not the spendable budget. Compact prefixes + one
EG2 copy fix the allocation failure. Host prefix isolation and 19/19 exact,
delta=0, missing=0, fidelity 5.341e-05 / top1 10/10 passed on this tree.
Compare to **B3's** composed 6.1117 breadth (+0.164%) / 6.1100 screen (+0.191%),
not B2's 6.1083 (+0.22% would be a misleading cross-board keep claim).
This is modest/sub-0.2%, not an accepted runtime; preserve as a possible bundle
component/capacity enabler. No automatic breadth or cross-board repeat for it.
**B3 next: representative FP32 norm-sidecar probe below**, now capacity exists.

**B2 correction built at 00:18, unmeasured:** mentor independently checked ELF:
LBEG 0x4037dc6c is `l16ui a8,a6,0`; epilogue checks last row before rearm,
then WSR.LCOUNT/ISYNC and jumps exactly to 0x4037dc6c. The failed first screen
and the `.Ltn_row` link failure provide no speed evidence. Run the actual CQ2
differential then primary; B1/B3 need independent work concurrently. No board
hardware blocker remains, and saved images suffice as references.

## B2: reuse LBEG/LEND, reload only the count between rows

The initial wrong-LBEG draft reset the LUT every group; the first attempted
repair left a stale `.Ltn_row` jump and failed to link. Both are now fixed in
source and the ELF (see current state above). Neither is a negative mechanism
result. Do not repeat them or preserve their abandoned launchers as live work.

New premise: #756 measured a real +0.44% from group control flow, overturning the
claimed CQ2 floor. Its long-loop setup now costs **nine emitted instructions
per row**, at 0x4037dc51..0x4037dc69 in B2's inspected ELF (`nd_lut2_rows_tie1n`).
It is LOOP, RSR.LEND, WSR.LBEG, L32R, NOP, WSR.LEND, ISYNC, RSR.LCOUNT, ADDI.
The no-call group body and its endpoints are invariant across every row.

Keep the assembler's proven long-loop setup for the **first** row. For subsequent
rows, restore the table base as before and reload **ngroup-1 into LCOUNT**, with
**ISYNC**, then jump to the unchanged `.Ltn_group`. LBEG/LEND remain those of
the first row. Preserve every packed/norm cursor, W8D0, FP partial, fold and
output order; retain the ordinary outer row loop. This is a setup-amortization
hypothesis, not another inner gather schedule. Inspect ELF control flow before
running: all later rows must skip the long setup and reach the same group body.

[Cadence ISA tables 129–131, pp277–279](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf)
require ISYNC between a write of LCOUNT and a possible loopback. Keep it;
not replacing that requirement with timing assumptions. The local IDF
`components/xtensa/xtensa_context.S` saves/restores all three loop registers
(lines 134–139 and 275–280); the kernel has no callees or nested hardware loop.
[Binutils long-loop expansion](https://sourceware.org/legacy-ml/binutils/2019-04/msg00014.html)
explains why the first-row setup is longer than the source `loop`.

Use existing CQ2 `bench_shape`/`num_check` and real 2-bit tensors, with the
candidate `nd_lut2_rows_tie1n` actually registered (`ND_KBENCH_ASM=1`, verified
in emitted compile flags and `KB CFG ... asm=1`). Require `KB NUM ...
kernel=tie1n ... bitexact=1`, not ROWRANGE/E49 or the generic Gather comparison
(which deliberately has a different reduction graph). Add only the small
range/count cases this change needs: one/six groups, zero/one/many rows,
nonzero r0, real PSRAM, then production split. In particular
check count=1, first row, second row, and exit count=0. Retain the 64-byte ABI
frame (the old 32-byte frame corrupted return state). Count instruction savings
as a prediction only: ISYNC and fetch placement can erase them. One primary
screen decides speed; a loser gets no breadth repeat. If setup preparation
blocks, B2 can take the norm-sidecar probe below instead of sitting idle.

## B3: finish the memory candidate, with capacity actually present

Preserve the failing asset/log, then make a corrected candidate with **one**
owned allocation and one cleanup path. Restore the original single value-GEMV
call and remove the diagnostic per-token print spam. Both copies must remain
byte-compared; selection stays by offset, with the original tier/overlap guard:
engram[1] K [15,707,584,15,864,256), V [15,864,256,16,020,928), total 313,344 B.
The first site is already contained; do not select it. The live call sites
already use `nd_tier_ptr`, and norm offsets are relative to the supplied blob.

Port only the compact-prefix helper/copy/restore logic from `.auto/exp88`, not
the whole B1 file. It reclaimed **768,508 B post-prime** on B1 (#748), with prefix
isolation green and unchanged decode. Restore prefix metadata BEFORE the compact
copy, retain both schemas and every live state byte. Recheck host prefix isolation
on this receiving tree. Print actual post-prime free/largest block and check
both prefix allocations; one open-time `EG2 ok` is not proof of capacity.

Then screen relocation against B3's composed pin: this is still unmeasured
performance, not a negative mechanism. If it fails, capture the **first ERR and
EXCVADDR** from the full log once, not a tail of successful GEMVs. Do not spend
another three-board crash/recovery cycle or replace a measured allocation error
with speculative norms corruption. No new general-purpose crash harness.

## Ready follow-on: FP32 group-norm sidecar

Move up for B3 now that compact-prefix capacity is proven; use a small
representative tensor before considering full integration. #452's five-to-three-instruction NF16V hoist was null, so removing the
conversion may be off the critical path. Test rather than assume. Keep packed
2-bit indices and preconvert immutable norms using existing `nd_f16`; an FP32
load replaces the halfword load plus conversion, with identical norm bits and
FP graph. Dense 40 projections + four engram projections: 130,560 norms,
522,240 B sidecars. Group traffic 32+2 -> 32+4 B (+5.88%), not 4x indices.

Start with one actual tensor (Q sidecar 13,824 B or 768x768 sidecar 18,432 B),
existing real-row kbench, distinct group norms, nonzero row offsets, cold PSRAM,
both cores. Charge the extra stream; no tiny shared SRAM table like #745.
Integrate only a positive probe with a demonstrated post-prime budget. The
compact-prefix capacity can fund EG2 OR all sidecars comfortably on B1's measured
budget, not blindly both (313,344+522,240 exceeds its 818,596 free before overhead).

## Keep these lessons; retire stale advice

- `.auto/exp90` = group loop; `exp91` = qk_dot8/composed B2 asset; `exp93` =
  composition evidence; `exp88` = compact prefixes; `exp92/94` = EG2 drafts.
  A main keep commit does not necessarily contain worker code; preserve/export
  the actual worker assets and provenance when a candidate wins.
- #752's zero-spill claim was false: QK had indirect a1+0x470 accesses. The helper
  won, so register-allocation boundaries remain real research levers. Do not
  re-add the already-tested handwritten QK, narrow counted QK, or outlined P.V.
- LSX byte addressing works (#744); #745 was tiny SRAM only; CV3W expansion is
  low priority after its traffic budget and broken probe. SCROW/CV2W/kron2,
  broad alignment sweeps, private LUTs and printing hunts remain down.
- #754 proved sign inversion across trees. B1's branch-only LOOP port is a
  distinct receiving-tree test; keep NF16V and unrelated changes out of it.
- Silent boards need actual boot evidence. Use `$FLASH_PORT`/`$SERIAL_PORT`
  supplied by the lock, not hardcoded ACM aliases, and the proven console/reset
  discipline. `EVT  READY` has two spaces in the serial handshake. Do not invent
  an outage from node mtimes or an empty deferred boot log.
- No quality waiver, golden rewrite, repeated control, or all-board verification
  batch. Complete a winner's necessary evidence once while other lanes discover.

Next mentor: did B2 actually measure setup amortization; did corrected EG2 get
past BOTH prefix allocations and finally produce a speed; is B1 still blocked
on a confirmed boot/strap state; and are 18/20 proposals still being mislabeled
accepted? The newest meaningful result must come from a completed device log,
not dashboard carry-forward fields.
