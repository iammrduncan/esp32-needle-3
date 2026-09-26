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
