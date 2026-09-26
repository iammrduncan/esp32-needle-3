# Ideas backlog (refreshed 2026-09-28; the pre-refresh file is kept as .auto/ideas.md.pre-2026-09-28.bak)

**Read `.auto/HANDOFF-2026-09-28.md` first** - it holds the result, the three trees, the working
recipes and every closure with its measured reason. This file is only the live backlog.

## Status: no above-bar candidate remains

The campaign's stop condition is met (no candidate that is not already measured or closed by byte
budget, gate or vendor support). The entries below are what a *premise change* would have to reopen,
in the order they would pay best.

## Needs a premise change to be worth anything

- **phi (4-bit) operand delivery** - the only measured addressable share in a phase above 8 ms:
  9.3 % of the phase (0.75 ms, ~+0.45 % token-wide) is a page-cache capacity effect. Blocked by
  ~18 KB/core against ~12 KB internal free. Reopens if internal RAM roughly doubles (i.e. the owner
  accepts assertion level 0/1 for +8,248 B) or if a smaller-footprint residency scheme appears.
- **120 MHz octal flash+PSRAM** - +3.51 % measured, vendor-blocked: IDF refuses the temperature
  timing retune on this flash model (`ESP_ERR_NOT_SUPPORTED`). Reopens with a verified timing model
  for this flash part, not with a longer soak.
- **Anything that changes the weight stream's bytes** (quantisation, codebooks, layout) - refused by
  the byte-exact/fidelity gate. Note for whoever proposes one: price it in BYTES per token FIRST.
  Two worked examples of that rule die immediately - the uint16 offset stream (4x the bytes against a
  bus already ~69 % utilised) and the fp32 norm sidecar (+6.25 % bytes = +5.1 ms vs 1.4 ms saved).
- **The two frozen heldout cases** (`heldout_interval_one`, `heldout_long_tools_note_only`) - not
  arithmetic; a transport change flips exactly that pair. Owner decision: re-baseline/replace, drop
  the ring, or block. Nothing in the loop can settle it.


## NEW IDEA (priced, needs a premise change): phi residency + asymmetric row split

The phi blocker is usually stated as "~18 KB/core against ~12 KB free". But that framing assumes both
cores get residency. The measured facts: phi's addressable share is **9.3 % of the phase** (0.75 ms,
~+0.45 % token-wide) and it comes from the *data cache* being shared with the rest of the token.

**Variant nobody has measured:** stage only **one** core's rows in internal RAM - 12 rows x 1,536 B =
18 KB is too much, but a *half* of them (6 rows, 9 KB) fits in the ~12 KB free - then rebalance the
split so both cores finish together. With a 9.3 % delivery advantage on the staged half, the
equal-finish split is roughly 54.5/45.5, and the net gain is the *balance* gain, not the residency
gain: ~4.5 % of the phase = 0.36 ms = **~+0.22 % token-wide, i.e. right at the bar, not safely over**.

**Why it is not being built now:** it needs a second walker variant that reads the packed stream from
internal RAM (a new asm entry point plus a split policy change), and the payoff sits exactly on the
0.2 % bar with no margin. **What would make it pay:** either the assertion-level RAM decision (which
frees 8,248 B and would allow the full per-core half, ~18 KB, for the full 9.3 % on one side), or a
measured delivery advantage larger than 9.3 % on this tree. Whoever tries it should price it on B2/B3
with the per-tree CQ2 differential first, since the walker is the same kernel that carries the
amortised loop.


## CLOSED TWICE (do not revive): de-splitting the LUT build

The old banked note said "run `lutb_rows` serially for +0.03..0.07%" on the premise of a 13.6 us
build. **That premise was wrong** - the measured build is ~135.8 us - and the experiment was already
rejected at #446 (lutserial, **-0.335%**). It was re-run by mistake on the amortised tree (#813) and
lost **-0.54%**, i.e. the split is now *more* valuable, not less (with the amortised loop the walker's
own setup is cheaper, so the build holds a larger share of the critical path). Canonical source:
`R-lutserial-b2.log`. **Before banking a de-split idea, check the measured build/phase size in
`.auto/log.jsonl` and whether the family is already closed** - the same trap cost this window a lane.

## Banked but sub-bar (kept in trees, not promoted)

- engram[1] narrow staging (EG2) **+0.164 %** on B3 - kept, needs the compact prefix to fit.
- The same amortisation pattern on other dispatched kernels: the 4-bit phi walker has only ~576
  groups/token and the remaining 2-bit variants are not on the hot dispatch, so the arithmetic says
  sub-bar; worth a re-check only if a future archive changes the call mix.
- `qk_dot8`-style outlining on the shippable line's QK body - the port **diverged** there (#779);
  a retry needs the whole region including its odd-remainder tail, plus a row differential first.

## Method rules this campaign paid for (do not relearn)

1. A lever's **sign**, and sometimes its correctness, is tree-specific: measure on the receiving tree.
2. Price a change in **bytes per token** before cycles per word; the octal bus is the binding resource.
3. A lost **JSON/probe result is not evidence** unless the probe has a control that can pass.
4. Differentiate the **integrated** function, over the real call shapes (multi-row, odd splits).
5. Respect the anti-repeat guard; a restricted screen consumes a tree's one repeat allowance.
6. Board attach: `serial_api.Device` with the constructor closed and DTR/RTS false around the open.
   Anything else can reset the chip into ROM download mode, which mimics a dead board.
7. Inside `needle-board run N`, `$FLASH_PORT`/`$SERIAL_PORT` are the only correct handles (the
   `/dev/ttyACM*` aliases are board 1's); expand them inside the script, not in single quotes.
8. Kill `serial_api.py` children by pid (`pkill -f needle-api` misses them and holds the console).

## CLOSED OFF-DEVICE by line arithmetic (do not build): merged 36-byte group records

Idea: merge each group's 32 packed bytes with its fp32 norm into a 36-byte record so the walker touches
one stream instead of two. **Refuted before any asm**: at 64 B line granularity today's layout costs
**0.531 lines/group (34 B)** - the packed side already puts two groups per line and the fp16 norms live
in a dense array that puts **32** norms per line - while a 36-byte record, whose start offset is uniform,
spans **two** lines 12.5 % of the time and costs **1.125 lines/group (72 B)**, i.e. **2.12x** the bus
traffic. **Rule: never split a dense sequential stream into fixed-size records unless the record divides
the cache line.** (Third worked example after the uint16 offset stream and the fp32 norm sidecar.)

## CLOSED as ALREADY-SHIPPED: "int8 K/V word reads (+6%)"

The old ledger banks this as a candidate. **It is in the tree already.** Evidence from the linked
image: `attn_heads` contains exactly **1** `l8ui` and 19 `float.s` / 56 `ssi`, and a per-function scan
of the whole ELF finds **no function with more than four `l8ui`** - nothing reads the int8 KV cache a
byte at a time. The conversion runs from packed 32-bit words (four int8 per load, expanded with shifts
plus `float.s`), which is what the item proposed. **Do not re-derive the +6%.**

**Method note (cheap and general):** before pricing a candidate from an old ledger entry, make the
linked image prove the code is absent - one `objdump` scan of the relevant opcode is usually enough,
and it has now caught two stale entries in one session (this one, and the "13.6 us LUT build" that was
really ~135.8 us).

## CLOSED by resource arithmetic: concurrent prefix priming / flash-cached prefixes

The other half of this campaign's oldest open item ("iteration speed, not the metric") is the ~5 min
per flash spent priming the two schema prefixes. Both proposed routes die on resources:

* **Priming both prefixes concurrently:** they are sequential phases of ONE model state (prefix 1 from
  a reset, then prefix 2 from a reset) - they are not independent work that can be handed to the second
  core. Running them in parallel would need two model instances, and a second instance costs ~13 MB of
  PSRAM against the **~517 KB** free after open. Impossible on this part.
* **Caching a primed prefix in flash:** needs its own partition, and `partitions.csv` is frozen.

So experiment latency stays at ~5 min/flash, and any future throughput work has to come from somewhere
else (fewer flashes per hypothesis, which the anti-repeat guard and the pre-screen rules already push
toward).

## ALREADY ENFORCED BY CODE (2026-09-28) - do not re-learn these by losing a run

This window turned five ways a number could silently change meaning into checks, each tested in both
directions. They are mechanisms now, not advice:

1. **`engine_md5` == `PROV_ENGINE`** - the metric line used to hash a different file set than the
   provenance line, so the two numbers for one tree never agreed (cost two investigations). Both now
   use the concatenation of `engine/src/*.c`, `*.S`, `engine/include/*.h`, `esp32/main/*.c`.
2. **Anti-repeat is enforced, not conventional** - a seen signature refuses with exit 42, no reason
   exits 43, a spent allowance exits 44, and every legitimate use prints `EXPLICIT_REPEAT_ALLOWED`
   with its reason. Do not assume you can re-measure a tree because you have a good reason.
3. **Golden saves are ADDITIVE** - existing entries are preserved verbatim, only missing ids are
   added, and any entry whose measurement differed is reported as `GOLDEN_PRESERVED`. Replacing one
   takes `AUTO_REBASELINE=1`. (A capture run once silently re-baselined the two frozen #647 entries
   and made a failing test pass.)
4. **Captures do not spend a tree's allowance** - `AUTO_CAPTURE_ONLY=1` requires `AUTO_SAVE=1`, prints
   a banner that its metrics are not evidence, and leaves no trace in the signature or repeat history.
5. **Captures must run worker-side** - main's `engine/` is a stale lineage (no EG2, no `qk_dot8`, no
   amortised loop), so goldens captured there would encode a different engine while looking valid.
   The provenance gate caught the attempt; do not repeat it.

**Suite counts also changed:** 24 cases everywhere now, so a full gate prints **22/24 with
`token_delta 52`** (was 18/20 with the same delta) and the host gate prints **23/23** (was 19/19). The
metric group is untouched. The four added cases are a fresh out-of-suite sample and landed in band
(6.09 / 5.92 / 6.23 / 6.11 against a 6.1450 primary mean).

## CLOSED BY PRINCIPLE: the dominant 2-bit phase's instruction budget is minimal (2026-09-28)

proj2bit is 79.7 ms = **49.7 %** of the token, so this is the one phase where a few percent would
change the campaign's headline. It is closed, and the closure is arithmetic rather than a list of
nulls. Per 32-bit index word (16 weights):

| category | count | why it cannot go |
|---|---|---|
| `add.s` | 8 | the accumulation itself - one per two weights, and the four-partial split is what hides the rest of the latency |
| `extui` | 8 | the ISA has no scaled-index load from a register; loading bytes instead of words gives two nibbles per load that still need shift+and, so extraction can only be moved, not removed |
| `addx4` | 8 | `lsi` takes base+immediate only, so the 4-byte scaling of the nibble must be materialised in a register |
| `lsi` | 8 | the table lookups, one per pair |
| `l32i` | 1 | the packed word itself |

That is 24 ALU ops, which at the S3's 2-wide issue is **12 cycles**, against ~16 measured - the extra
~4 being load-use and dependency slack. Which is exactly the campaign's measured IPC of 1.33:
32 instructions / 16 cycles / 2 wide. So the phase is issue-bound at a budget every instruction of
which is forced by the ISA's addressing model, and the single escape - a predecoded offset stream so
the lookup needs no scaling - costs **4x the bytes** and was measured dead against a bus already at
69 % utilisation (#749: 14.36 MB/token, 2.8x slower even at an unattainable 100 % bus).

**Consequence: no scheduling, unrolling, register, layout or load-width idea can move this phase, and
that is now a statement about the instruction set rather than about the ~20 nulls measured here.** The
only lever left would change what is stored, which the byte-exact and fidelity gates refuse.

## ALREADY IMPLEMENTED: floating-point fusion (checked in the linked image, 2026-09-28)

Hypothesis going in: the S3's FPU has `madd.s`, so every `a*b + c` in attention and the MLP could be
one instruction instead of two - a potentially large instruction-count win that the byte-exact gate
would forbid because fusion changes the rounding. **The check refutes the hypothesis at the first
step: fusion is already ON.** `attn_heads` in the linked image contains **119 `madd.s`** against 58
`mul.s` and 29 `add.s`, i.e. GCC has already contracted every pairable multiply-add, and the engine's
CMakeLists carries no `-ffp-contract=off` (the flag appears only in the campaign's *test* builds,
where it is needed to make bit comparisons meaningful). The residual 58 multiplies and 29 adds are
standalone operations, not halves of a pairable pair.

**So the FP instruction budget of the attention path is at its floor already**, and no "enable FMA"
proposal should be made again - it was never disabled. This is the third item this session found to be
already shipped rather than missing (after the int8 K/V word reads and the answer-side truncation
report), which is why "prove the code is absent before pricing it" is now the first step of any
candidate review here.

## VERIFIED IN THE LINKED IMAGE: the Hadamard MLP's inner loops are ~1 madd per product (2026-09-28)

The ledger claims `kron_apply` has no asm headroom at "1.75 instructions per product with
loop+lsi+madd.s". Checked against the ELF rather than trusted:

| kernel | madd.s | lsi | other | products per iteration |
|---|---|---|---|---|
| `kron1_blocks` | **8** | 6 | 8 ssi, 6 addx4, 7 add.n, 6 mov.n, 3 loop, 3 extui | 8 (the eight-accumulator pass) |
| `kron2_rows` | **17** | 19 | 9 ssi, 12 mov.n, 11 add.n, 4 addx4 | ~16 |

So there is **one fused multiply-add per product** and the rest is addressing, moves and the loop
itself - between about 0.75 and 1.5 overhead instructions per product, which is where the ledger's 1.75
came from and which is why the wider variants lost: `kron2` four-across measured -0.30 % and the
single-walk eight-across -0.18 %, both because a wide row costs four registers and the shipped body
re-reads 16 floats per j step *on purpose* - that is how it affords eight accumulators.

**Three largest phases, all now accounted for from the image rather than from a summary:**
* **proj2bit (49.7 %)** - 24 forced ALU ops per 16 weights, 12 cycles at 2-wide issue + ~4 slack = the
  measured 16, and the only escape costs 4x the bytes.
* **attention (14.6 %)** - FP contraction is already ON (119 `madd.s` vs 58 `mul.s` / 29 `add.s` in
  `attn_heads`), so the fused instruction budget is already spent.
* **hadamard (12.6 %)** - one madd per product with under 1.5 overhead, and the wider forms were
  measured negative for register pressure.

Together with the earlier per-phase closures (engram at the LUT-GEMV floor, phi delivery-bound and
unpaid, sinkhorn exp/log-bound, mix swept, transform measured on every axis), that is a complete
account of where 163 ms goes and why none of it is addressable without changing what is stored.

## SELF-CORRECTION: "one IPC across all phases" is NOT supported - only the two walker phases are

I nearly recorded a tidy global claim ("every phase runs at the same effective IPC, so there is one
bottleneck"), and the arithmetic refused it. Checked properly:

| phase | ms | work used | instr/product | Mcycles | implied IPC |
|---|---|---|---|---|---|
| proj2bit | 79.7 | 14.36 M weights | 2.0 | 19.13 | **1.50** |
| engram | 14.6 | 2.36 M weights | 2.0 | 3.50 | **1.35** |
| phi | 8.1 | 73.7 K weights | 4.1 | 1.94 | 0.16 |
| hadamard | 20.2 | 1.05 M products | 1.75 | 4.85 | 0.38 |

The last two rows are absurd, and the fault is mine, not the engine's: those phase timers **bracket
more than the multiply work** - `mhc_phi4` includes `nd_cq_prepare` and the LUT build alongside the
GEMVs, and my hadamard product count was simply wrong. This is the ledger's own standing warning
("the timers overlap; do not sum them") applied to a computation I was about to publish.

**What IS supported:** the two phases that are *pure* 2-bit walker work - proj2bit and the engram's
GEMVs - land at **1.50 and 1.35 IPC respectively**, i.e. the same kernel at the same measured rate
(1.33-1.5), which is exactly the 24-ALU-ops-plus-slack accounting of #849. The other phases cannot be
priced per weight from their timers at all, which is a statement about the instrumentation, not about
the code.

**Rule extracted:** never derive a per-operation rate from a phase timer whose bracket is not
single-purpose. If a phase looks anomalously slow per unit of work, suspect the attribution before
suspecting the kernel - this campaign has now done that twice (#295's phi "4.3x gap" was a units
error; this is the same class caught before publication).

## The QK dot is LATENCY-bound, quantified (measured 2.90 cycles/MAC)

Using only measured numbers, as the #852 rule requires (no rates derived from multi-purpose timers):
the campaign's own kbench reports the shipping 48-dim QK dot at **139 cycles**
(`KB DOT n=48 c4_pair_cyc=139`), i.e. **2.90 cycles per MAC against a ~1 MAC/cycle FPU**, so the
hottest kernel in the attention phase runs at **35 % of peak** and carries **1.9 cycles of exposed
latency per MAC** that only the compiler's interleaving hides.

That single number explains the whole measured history of this kernel:

| attempt | result |
|---|---|
| pair the two dots of a position pair | 404 cyc = **-46 %** |
| four accumulators, fold once (reorder) | 214 cyc = **-54 %** |
| 128-bit `ee.ldf.128.ip` asm | 286 cyc = **-107 %** |
| wide-load DOT8W in the field | **-2.1 % / -1.9 %**, because the hand-written body issued each madd immediately after its load, exposing exactly the latency the C body hides |
| exp pairing in the same phase (kept) | **+19.96 % isolated** - a latency-hiding win, not an instruction-count win |

**So the lever in this phase is latency hiding, and the compiler is already the best scheduler
measured here**: four hand-written schedules and one wide-load variant all lost to it. Beating it
would need a pipeline over the six 8-column chunks that the four attempts conspicuously failed to
produce - and the one place a hand-written pipeline *did* win in this campaign was the two-deep
codebook load in the wide phi (#806), whose payoff was +0.028 %, i.e. sub-bar. Do not re-open the QK
dot without a concrete schedule that differs from all four.

## NEW CANDIDATE (ready to build): head-blocked QK traverse - reuse kf across all heads

**Where it comes from.** The quantified model says the QK dot costs a *measured* 139 cycles for 48
MACs = 2.90 cycles/MAC against a ~1 MAC/cycle FPU, i.e. 1.9 cycles of exposed latency per MAC, and
that the compiler's interleaving is the best schedule measured for it (four schedules and a wide-load
variant all lost, #853). What those attempts had in common is that they attacked the *body*; none
changed *which operands are resident while the body runs*.

**The observation.** The dot is called per (head, position), and in the shipping traverse the loop is
*position-outer, head-inner*: for each position the two staged `kf0`/`kf1` rows (384 B) are loaded and
then used for **one** head's 48 MACs, and the same two rows are re-loaded for each of the twelve heads
that share that kv position. Twelve re-reads of an operand that changes once.

**The change.** Invert the block: for each kv position, stage `kf0`/`kf1` once and compute **all
twelve heads'** dots against them, i.e. 12 x 48 = 576 MACs per 384 B of loads instead of 48 MACs per
384 B. The `qh` rows for twelve heads are 12 x 192 B = 2.3 KB, which fits internal RAM comfortably.

**Why it should pay:** the dot is latency-bound on exactly those loads (#853), and this changes the
load-to-work ratio by 8x while leaving every head's accumulation sequence, operands and order
identical. It is the same shape as the two blocking changes that did win in this campaign, the wide
phi rows and `kron1`, both of which paid because the compiler was re-reading an operand it could not
keep.

**Bit-exactness argument:** each head's dot still sums its own 48 products in the same ascending order
into the same accumulator, and the softmax/max/denom/rescale per head still runs in the same order
afterwards - only the *order in which different heads are visited* changes, and heads are independent.
So the values are identical by construction, and the per-tree device goldens plus the fidelity probe
verify it.

**Risks to price, in order:** (1) register pressure - the blocked inner loop wants the 12 q rows and
the 2 kf rows live at once, which is far more than the register file holds, so the rows must be walked
from internal RAM rather than registers and the win depends on L1 hits rather than values; (2) the
parallel split - heads are already split across cores, so a head-blocked traverse has to be applied
*within* each core's head range, not across it; (3) the staging helper's current shape assumes
position-outer.

**Prize, priced from measured numbers:** if the blocked traverse lifts the dot from 2.90 to about
1.3 cycles/MAC, the QK portion of attention falls from 8.3 ms to ~3.7 ms, i.e. **+2.8 % of the token**.
That is the largest unbuilt candidate this campaign has had since the amortised loop, and the first
that attacks operand residency rather than the body.

## DOWNGRADED before building: the head-blocked QK candidate's premise does not hold

I proposed this candidate last window and priced it at +2.8 % on the argument that the two staged `kf`
rows are re-loaded for each of the twelve heads that share a kv position. **A code check refutes the
premise**, and it is worth recording that the check cost nothing while the build would have cost a
board lane and an hour.

What the code shows: `kv_stage_pair` (nd_model.c:73) is the noinline helper that converts four int8
values per 32-bit word out of the KV cache into **fp32 staging buffers**, and `qk_dot8` takes
`const float *kf0, *kf1` - i.e. **the dot consumes already-staged fp32 rows from a small internal
buffer, not int8 from the cache**. So the "twelve re-reads of an operand that changes once" are L1 hits
on a buffer of a few kilobytes, not memory traffic, and blocking the heads against it would change the
loop structure without changing where the data comes from.

Where the dot's 2.90 cycles/MAC then actually comes from is what #853 already established: the
`lsi`-to-`madd.s` dependency *inside* the body - register/FPU load-use latency, which is why four
hand-written schedules and a wide-load variant all lost to the compiler's interleaving, and why the
one hand-written pipeline that did win in this campaign (two-deep codebook loads) was worth 0.028 %.

**Disposition:** the candidate is withdrawn from the queue rather than built. If it is ever revisited,
the premise to test first is a *measured* stall attribution inside the dot (e.g. a kbench variant that
isolates load-use stalls from issue), not another loop-order argument - the same "measure the premise,
then price the lever" rule that has now caught this class three times in one session.

## CORRECTION + NEW CANDIDATE: the QK dot is ISSUE-bound (IPC 1.04), and the lever is fewer loads per MAC

**Correction to #853.** I recorded the dot as "latency-bound" on the strength of its 2.90 cycles/MAC.
It is not. Counting instructions: each of the 48 MACs needs `lsi qhA[i]` + `lsi kf0[i]` + `madd.s` = **3
instructions**, so 48 MACs is 144 instructions. At the measured 139 cycles that is an implied **IPC of
1.04**, i.e. the kernel is **issue-bound at essentially full issue for the instructions it executes** -
there is no stall to attribute and no schedule to fix. (The four failed schedules and the wide-load
variant remain explained: they either added instructions or changed nothing about the count.)

**The lever that follows:** fewer loads per MAC. 64-bit float loads fetch two floats at once, so a term
pair costs one load instead of two - about **1.5-2 instructions per MAC instead of 3**. The campaign
already tried that (`ee.ldf.64.ip`/`ee.ldf.128.ip` variants, then DOT8W in the field) and lost 2.1 %/
1.9 %, but the failure mode is known and specific: the hand-written bodies issued each `madd` immediately
after the load it depended on, which exposes the load-use latency instead of hiding it.

**The missing ingredient is already shipped elsewhere in this tree:** the two-deep pipelining technique
from the wide phi (#806), where each block's `extui`/`addx4`/`lsi` is issued *before* the previous
block's `madd.s`, with alternating registers (`f12`/`f14`) and an objdump check that the loads really are
in flight ahead of their consumers. Applied to a 64-bit-load QK body, that is the same transformation
with a much larger prize, because the dot has three instructions per MAC to remove while the wide phi
had almost none.

**Prize, from measured numbers:** 3 -> ~1.5 instructions per MAC means 139 -> ~70 cycles per dot, so the
QK portion of attention falls 8.3 -> ~4.2 ms = **+2.5 % of the token** - larger than anything kept since
the amortised loop.

**How to build it safely, in order:** (1) write the 64-bit-load body with the two-deep pipeline and
alternating registers; (2) **verify by objdump** that every load is issued at least one block ahead of
its consumer (this is the exact check that caught the unreachable-loop mistake in #806); (3) host-gate
the tree before flashing, since any re-association is a rounding change and must be caught there;
(4) the values must be bit-identical - each head still sums its own products in the same ascending
order, only the load width and issue order change - so the device goldens and the per-tree CQ2
differential decide it; (5) screen against the tree's own pin, never another tree's.

## CORRECTED PRICE for the wide-load QK candidate: ~+1.5 %, and it rests on a warm number

Two corrections to my own last two entries, made before building anything:

1. **The figure I used was a PAIR.** The kbench line says `KB DOT n=48 c4_pair_cyc=139` - that is the
   **two dots of a position pair, 96 MACs**, not one dot's 48. So the measured cost is
   **1.45 cycles/MAC, not 2.90** - my earlier arithmetic divided a pair figure by one dot's MAC count.
2. **The pair form already shares the `qh` load** across its two kf dots: 1 qh load + 2 kf loads +
   2 madds per 2 MACs = **2.5 instructions/MAC**, which at 1.45 cycles/MAC is an **IPC of 1.73**, i.e.
   **86 % of the S3's 2-wide peak**. The dot is therefore *nearly issue-saturated* in that measurement.

**Corrected prize for a 64-bit-load body:** 64-bit loads fetch two floats, so a term pair costs one load
instead of two - about **1.75 instructions/MAC** against 2.5, i.e. 139 -> ~97 cycles, QK 8.3 -> ~5.8 ms,
**about +1.5 % of the token** rather than the +2.5 % I first quoted. Still above the bar, but smaller.

**The caveat that matters more than the number:** 139 cycles is a **kbench** figure - warm, isolated,
back-to-back. This campaign's own rule, paid for three times (#295, #364, #745), is that a warm isolated
number prices nothing, and the field dot is called in a loop with staging and softmax around it. So the
+1.5 % is an **estimate from a measurement whose structure does not match the field**, and the honest
next step is not to build it but to **re-price it with the field's call structure** - a small kbench
variant that calls the dot once per group immediately followed by its consumer, as the transform screen
eventually had to do. Only then is the wide-load body worth writing, because its whole justification is
an instruction-count ratio against a number that may not hold in situ.

## CLEARED TO BUILD: the wide-load QK dot, bounded at +1.5 % to +3.6 % without a board

#858 held this candidate pending a field-structure re-price, because its 139-cycle basis is a warm
isolated kbench number. The bound can be had without a measurement, by subtracting the attention
phase's other attributed work from its measured total:

| attention phase (measured) | 23.5 ms |
|---|---|
| paired exp (8 500 pairs x 198 cyc / 2 cores) | ~3.5 ms |
| P.V wide kernel (ledger attribution) | ~3.7 ms |
| staging + bookkeeping | ~1.5 ms |
| **QK, as the remainder** | **<= 14.8 ms** |

against **8.3 ms** from the warm kbench (14 400 dots x 139 cycles). So the field QK cost lies between
8.3 and 14.8 ms, and a 64-bit-load body that cuts 30-40 % of the dot's instructions is worth:

| QK basis | -30 % (wide loads) | -40 % (wide loads + two-deep pipeline) |
|---|---|---|
| 8.3 ms (warm kbench) | **+1.53 %** | **+2.04 %** |
| 14.8 ms (field remainder) | **+2.72 %** | **+3.63 %** |

**Both ends are above the 0.2 % bar, so the candidate is cleared.** More than that: the warm-isolation
caveat that made me hold it in #858 cuts *in its favour* - this campaign has measured three times that
isolated numbers understate the field, so the likelier end of the range is the larger one.

**Build order (unchanged, plus one step):** (1) write the 64-bit-load body with the two-deep pipeline
and alternating registers, budgeting registers the way the wide phi does (its two-deep schedule uses
two register sets, so pipeline ONE row pair and keep the others scalar); (2) **objdump-verify** that
every load is issued at least one block ahead of its consumer; (3) **host-gate before flashing** - any
re-association is a rounding change and the host oracle is where it must be caught; (4) screen against
the tree's own pin; (5) if it lands, re-derive the attention phase to see which end of the bound the
field actually sits at, which is itself a useful number for every future attention idea.
