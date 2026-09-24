# Needle 3 mentor queue

Mentor pass 2026-09-24, updated 18:16 UTC; through #576 and actual lane logs.
Read at lane turnover. Preserve worker dirt, locks, anti-repeat, 240/80 MHz,
all frozen inputs/goldens and quality gates. Never interrupt a live build,
flash or benchmark; prepare its successor. Mentor launched no experiment.

## State and priority change

**Accepted: 5.3033 tok/s**, bundle5 `2c79104`, engine `0c1a6272cd01`;
pins b1/b3 5.3033, b2 5.3017. Full device 20/20, host 19/19, fidelity
5.341e-05, top1 10/10 and capture green are the accepted evidence.

The later **UNACCEPTED** clean stack `4d3094578050` is measured on ALL THREE:
b1 **5.5783** (`M-can1.log`, finished 17:58), b2 **5.5767**, b3 **5.5800**
(`M-clean-b2/b3.log`). All finish **18/20**, delta=52, missing=0, rc=1;
failures are heldout_interval_one and heldout_long_tools_note_only. Capture
passed #573. No fourth canonical reading or owner-packet rewrite is useful.
B1's actual aggregate METRICs: prefill **5.885**, extended **5.4838**, think
**4.37**, min_case 5.31, boot 5.621, heap 8,415, PSRAM 2,052,252, gen_tokens 99.
Read `^METRIC ` lines; do not guess aggregates from truncated per-case greps.

Use the same clean stack as the discovery base; report incremental gain against
its above per-board reading as well as the accepted pin. Snapshot
`.auto/exp84/main.c.lean-ring-clean` has md5 `3c63ef73e719`.
**Main HEAD's main.c still has old semaphores: HEAD is not this base.** Verify
actual source/build identity and preserve dirty variants before staging.
A primary 6/6 screen is useful evidence, never full acceptance at 18/20.

18:08 launch correction: the FIRST M-g4m-b2 exited **rc=3 before build**. Its worker hash
is ff84494b160b (the new 32-entry edit), expectation still 4d3094578050. The
printed b1daae10df90 was computed AFTER `cd /workspace/esp32-needle-3`, i.e.
from MAIN, not the worker. No nf16v_src.c mystery: NF16V is the macro in
`engine/src/lut2_tie728.S`, not an nd_f16_block helper. B1/B3 still hash to
4d3094578050. Do not rerun old nf16v. The gemv4 expansion premise is also
unsupported: phi_pre/post/res are THREE shared model tensors, not 24
per-layer tensors (nd_model.c:409-411,1954-1962); plus the head fits four slots.
The researcher relaunched it under the same log name; it completed by 18:11:
5.5767, primary 6/6, delta=0, rc=0, identical to b2's clean-stack baseline.
However the resulting object/ELF's s_g4 is still **48 bytes (four entries)**;
do not interpret that as a tested 32-entry-capacity improvement. No repeat is
justified without real misses; fused prepare+LUT is board2's successor.

Late RX installation recovered its early allocation cost; lean notifications
won over the predicate-correct semaphore base. Those old queue items are DONE.
Split threshold half<1 (#558) and longer spin budgets (#456/#564) are flat:
move away from parameter sweeps to instruction removal and producer/consumer
scheduling. Initial mentor inspection found ALL boards idle and researcher in
`sleep 1500` on already-finished M-can1; resume discovery, not poll logging.

**NEW: B1 accumulator seed WINS its primary screen.** `M-b1-seed.log` finished
rc=0: **5.6117 vs b1 5.5783 (+0.599%)**, prefill 5.9267, min_case 5.35,
boot 5.659, gen_tokens 99, heap 8,415, PSRAM 2,052,252, device **6/6**,
missing=0, delta=0. Mentor independently read the ELF: first four adds use
f6, second four keep f0..f3; source has zero of the eight old seed/reset moves.
Snapshot this exact assembly before reuse. This is a speed candidate, NOT a
full-quality acceptance. The real target multi-group/row differential is still
owed; host gates cannot exercise this assembly.

## Next three lanes — distinct changes, no unchanged live control

| Board | First experiment | Why now |
|---|---|---|
| 1 | Preserve seed winner; next group-owned prepare + LUT on CLEAN base | Keep the B2 recipe below as an independent test, not an unmeasured stack. |
| 2 | Seed winner cross-board + bounded target row differential | An above-bar winner now justifies ONE confirmation lane; compare to b2 5.5767. |
| 3 | LUT builder with short-lived output quartets | Actual clean-stack object spills three floats per pair; stop holding all 16 outputs live. |

If a lane is already genuinely running something else, leave it and apply this
at turnover. Small build/target-equivalence screens are fine; do not spend a
whole lane repeatedly checking the existing gate failure. Confirm processes
AND nonempty growing logs after launches; a PID or tmux session alone is not work.

### Seed winner: preserve this mechanism for B2 confirmation

In `lut2_tie728.S:nd_lut2_rows_tie1n`, every group ends by copying f6 (+0.0)
into f0..f3; the next W8D immediately adds its first four gathered values.
Specialize ONLY that first W8D invocation of each group: its first four adds
become `add.s f0,f6,f8`, `add.s f1,f6,f9`, `add.s f2,f6,f10`,
`add.s f3,f6,f11`. Its second four adds and all seven later W8D invocations
are unchanged. Delete the four group-reset mov.s and the initial partial seeds
that are now dead; retain f6 initialization and the row-total f5 reset.
Do not replace `0+value` with a move: keeping the add preserves signed-zero and
all rounding behavior. Norm rebias, FOLD tree, row/group order and guards stay.

44 CQ2 projections / 21,760 rows / six groups imply 130,560 groups per token:
**522,240 fewer instructions** is a useful hypothesis, not a speed claim.
Target differential against the old walker on real archive/LUT data must cover
multiple groups, multiple rows and nonzero r0 (stale partials hide at boundaries).
Do not modify the shared W8D for other variants. This is not the rejected
8-accumulator/two-row/prefetch experiment; it removes instructions from tie1n.
The shipping labels are `.Ltn_row/.Ltn_group`, NOT the earlier unused
`.Lt1_group` in tie1. Snapshot the measured file; do not rebuild an approximate
version for cross-board confirmation. Recompute line bounds AFTER list edits.

### B1 next: finish each core's transform and build its own table slice

Current CQ2 sites do `nd_cq_prepare` then `nd_cq_lut_build`: two global joins,
although transformed groups and their table entries are independent.
Add a narrow prepare+LUT helper for those paired CQ2 sites. Keep the existing
copy/pad, scale computation, and group split. One worker callback first calls
existing `fwht_rows` on its [g0,g1), then existing `lutb_rows` on
[g0*g/2, g1*g/2). The caller/worker still own disjoint groups and table entries;
join before the first GEMV. Preserve the three-group radix-4 batching inside
fwht_rows; a naive per-group transform would throw away a measured win.

Use it at the Q/K/V/gate shared preparation, out_proj, and engram K/V shared
preparation (18 builds/token); leave 4-bit prepare-only sites alone. No async
queue, nested nd_parallel_rows, new task, table quantization or scratch sharing.
Compare transformed xh AND full LUT byte-for-byte, including odd/nonuniform
splits. Serial LUT build lost -0.336% (#446): this keeps it parallel. #448
invalidated the 13.6-us/PSRAM-LUT ceiling; measure with actual internal SRAM
scratch and real two-core dispatch, never the serial kbench stub (#344).

### B3: schedule LUT outputs to avoid measured compiler spills

18:15: the first inline-product rewrite removed all spills but changed the
object from 4 mul.s + 16 madd.s to **16 mul.s + 16 madd.s**. No equality or
speed claim follows; researcher is preparing the store-barrier form instead.
Keep that first source as a snapshot. A changed operation count is a warning,
not proof of changed answers; inspect operands and do the target comparison.

Read-only objdump of b3's clean-stack `nd_quant.c.obj`, `lutb_rows`:
three `ssi ... a1,0/4/8` at offsets 0x4c/0x5e/0x70, reloaded at
0xa3/0xac/0xb5. All 16 results are computed before most stores. There are
only 16 FP registers. A quartet-at-a-time emit (or explicit small Xtensa body)
can finish and store four results before producing the next four, and keep
codebook constants in registers if feasible. Start with scalar stores; wide
stores are a later lever. Keep the parallel build and existing LUT layout.
Cheap first form: a compiler-only empty asm memory barrier after each completed
quartet of stores in this one loop; inspect whether GCC then emits/stores each
quartet without spills. This is a hypothesis about scheduling, not a guarantee.
Avoid making T volatile as a shortcut: Xtensa can serialize volatile accesses.

**Match the TARGET arithmetic, not just the C formula.** The current object
rounds b[k]=cb[k]*x1 with mul.s, then computes each result with madd.s
`b[k] + cb[j]*x0`. Forcing eight rounded products plus sixteen adds can change
bits. Preserve this operation graph and operand order; compare all 6,144 table
floats for captured activations against the actual old target builder. A host
build alone does not prove that equality. Inspect the new object to see whether
spills/reloads disappeared before spending a full device lane. No broad compiler
flag change or library fmaf calls. Use the existing diagnostic path, not a new
benchmark framework.

T-MAC's useful transfer is to design around LUT construction, storage and reuse;
its SIMD register-table lookup and table quantization do not transfer directly
to this float-gather Xtensa path. [T-MAC, sections 3.2 and 4](https://arxiv.org/html/2407.00088v2).
The two instruction-level proposals above come from Needle's own code/object.
GCC may contract across statements under the target's GNU C settings;
[GCC 14.2 contraction semantics](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Optimize-Options.html#index-ffp-contract)
reinforce why the observed object is the arithmetic reference here.

## Ready successor if a lane closes or blocks

**Q-head-owned tap -> norm -> RoPE:** q has 12 independent 48-column heads.
Its tap currently has only three 256-column units, so half<2 makes it serial;
head normalization immediately splits the same data 6+6. Keep the existing
history copy, then let that same head job apply Q's taps for its own columns,
normalize in the original sum order, and rotate the head. This parallelizes
formerly serial tap/RoPE work without adding a separate handshake. Keep K/V
paths initially unchanged and join before attention. Preserve taphoist/tap2col/
tapfwd arithmetic and history stride; no nested dispatch from the worker.
This differs from rejected tap96 (extra split) and standalone RoPE splitting.
If B1 and B3 both win, compose only after their separate readings; measure the
composition, do not add percentage estimates.

## Keep these interpretation corrections after compaction

- **Demo timer state is NOT established as the reason for 18/20.** run_inference
  constructs model input from schema prefix + query; router_dispatch runs AFTER
  generation. #391 observed a reset correlation, not a model-input dependency.
  Frozen host/device tails already differ in generated content. Keep gates and
  pin frozen. Any future diagnosis must compare actual received request/token
  IDs, raw response, prefix restore and boot identity; drop=0 alone is not that
  proof. Do not regenerate or annotate goldens to accept this candidate.
- Heartbeat silence during requests was unobservable: serial_api._line_quiet
  disables logging. The old whole-app/stdout-stall inference was retracted #488.
- Already closed: ordinary QKV concatenation (correct two-core screen ~-0.31%),
  old tie2/prefetch, serial LUT build, norm-constant hoist, sinkpair, silu4,
  FP16 taps, approximate math, tier stride sweeps and 120 MHz. The #349
  rows-per-call curve measures dispatch amortization, not every possible
  inner-loop instruction schedule; it does not close B1.

Next mentor: inspect source identity and real progress of the three new lanes,
then their target bit-equality and incremental device timing. Do not let another
acceptance narration loop replace them. Pi-specific recovery note: Ctrl-C only
clears its editor in this installed version; Escape is the documented abort.
Use abort only after confirming no real build/flash/benchmark is live.
