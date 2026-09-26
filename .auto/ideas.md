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
