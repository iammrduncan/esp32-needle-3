# Experiment 35 — unroll the remaining element-per-cell loops (queued, gated)

## Why this family is open again

Run #362 shipped the per-group rescale in `fwht_rows` unrolled by 4: **+0.332 % decode**,
bit-exact, gate-clean. Run #361 measured the same loop at **+263 %** in isolation and
run #358 had measured a *restrict* annotation on it as exactly zero. So the compiler was
leaving 2.6x on the table in a loop the ledger had implicitly treated as "already
optimal elementwise work", and the annotation that looks like the fix is a null while the
unroll that looks trivial is the fix.

The ledger's ~20 "scheduling nulls" were unrolls of **GEMV inner loops**, which are
issue-floor-bound (2 instructions per weight, `W8D`). They are not this class. Loops whose
body is one independent multiply/add per element with no accumulation order are a
different animal, and the measured precedent now says they carry headroom.

## The design rule this experiment must obey

Run #363 lost a board because the kbench screen tested **the bench's transcription** of a
kernel instead of the shipping function, and the transcription had the bug. So E35 does
not copy any body into the bench:

1. each candidate variant goes **inside `engine/src/nd_quant.c` behind `#if defined(ND_E35_*)`**,
   default OFF so a shipping configure never sees it;
2. kbench measures the **engine's own function** under `ND_E35=off` and `ND_E35=on` in two
   builds and prints a checksum of the output array;
3. bit-exactness is then the two builds' checksums matching (engine vs engine), and speed
   is the same two builds' cycle counts — no transcription anywhere in the chain.

## Ranked candidates (all bit-exact by construction: one independent op per cell)

| # | loop | cost basis | why it might pay |
|---|---|---|---|
| A | `nd_cq_lut_build` pair loop (8 mul + 16 add + 16 stores per pair, contiguous) | #344: 32,598 cycles/call, i.e. 0.136 ms; at ~16 calls that is ~1 % of the token, though the phase map's `prep+lut` 3.6 ms cannot reconcile with prepare's own 3.93 ms — measuring A settles which number is right | same shape as the rescale loop that gave +263 % |
| B | prepare's activation copy (3.3 % of the call by #351) | 1,954 cycles/call | probably already a `memcpy`; if so it is a null and A/B says so for one build |
| C | the `1/sqrtf(ss/n+eps)` emit loops in `rms_unit`/`zcrms_head_rows` | per-head-row elementwise | `restrict` was measured null there, unroll was never tried |

## Discard-before-this

Nothing. Note for the owner: the sibling lever that is *measured and shelved* is
`sigmoidf_pair` IRAM placement (#336/#342, +0.18 % mean, -768 B) — it needs the keep bar
at 0.15 %, not another measurement.
