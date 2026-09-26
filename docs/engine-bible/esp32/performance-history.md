# Performance history and present decision

This chapter is a state history, not a claim that every faster number is on the
current branch. The campaign maintained accepted, preserved candidate, and
diagnostic trees simultaneously—often on different boards. Always pair a rate
with its tree, workload, and gates.

## Headline lineage

| State | Decode rate | Delta | Evidence/disposition |
|---|---:|---:|---|
| Original eight-layer port, run #1 | 1.2217 tok/s | — | Starting device implementation |
| FP32 factor staging, run #2 | 2.4417 tok/s | +99.9% | Became the later `2.44` campaign baseline |
| Accepted pin `2c79104` | 5.3033 tok/s | +117.2% vs 2.44 | 20/20 device, 19/19 host, fidelity/top-1, capture, cross-board |
| Final tree C / board 1 | 5.9033 tok/s | +11.3% vs pin | Conservative shippable-line candidate; 18/20 |
| Final tree B / board 2 | 6.1250 tok/s | +15.5% vs pin | Seed-line variant; 18/20 |
| Final tree A / board 3 | **6.1433 tok/s** | **+15.8% vs pin; +151.6% vs 2.44** | Recommended candidate; 18/20 plus complete supporting evidence |

The original `1.2217` and later `2.44` are different baselines. Quoting the
candidate as `+151.6%` is valid only against the latter; against the original it
is roughly a fivefold absolute throughput result, but that mixes a long series
of system and harness changes.

## Accepted pin

Commit `2c79104` is the last owner-accepted comparison pin:

- decode 5.3033 tok/s;
- device exact 20/20 with no missing golden and token delta zero;
- host exact 19/19;
- maximum logit delta `5.341e-05` against `2e-3`;
- top-1 10/10;
- all nine behavioral-capture flags;
- three-board readings 5.3033, 5.3017, and 5.3033.

The final accepted bundle included explicit exponent-scale bitcasts, CQ4 logits
head dispatch, QKV tap address hoisting, register transfer, paired sigmoid,
tap-column/fwd refinements, and conditioning layout work. Several were below
the campaign's 0.2% individual bar and were accepted only as a measured bundle.

## Major mechanism milestones

The full ledger contains 800+ iterations; the reusable arc is:

1. **Correct port and model depth.** A four-layer slice ran but made semantic
   tool errors, so the 8-layer/384-token archive became the product baseline.
2. **Stage high-reuse FP32 factors.** Nearly doubled decode to 2.44.
3. **Pack and pair quantized work.** Pair LUTs, packed K/V and CQ2 walkers cut
   decode arithmetic/data overhead.
4. **Exploit row independence.** Persistent dual-core output splitting helped
   large projections, with thresholds for small shapes.
5. **Place selectively.** A 12 MiB PSRAM weight tier added ~1.5%; cache and
   scratch placement were tuned without copying every tensor.
6. **Use actual TIE728 assembly.** The bit-exact `tie1n` CQ2 kernel improved the
   isolated body ~33.4% and delivered about 13% end to end on its adoption tree.
7. **Remove call/address overhead.** Eligibility memoization, exact bitcasts,
   tap hoists, 4-bit head dispatch, paired nonlinear work, and attention
   composition moved the engine past 5 tok/s.
8. **Fix operational bottlenecks.** RX ring, stale-line handling, build/image
   signatures, full gate enforcement, and serial attach discipline converted
   incomplete or ambiguous measurements into reproducible sessions.
9. **Finish the hot loop.** Plain then amortized hardware group loops, compact
   prefix, narrow engram staging, and selected QK outlining produced the final
   preserved trees.

## Final candidate tree composition

The durable handoff records:

- **Tree A (B3):** seed-era stack, composed attention
  (B4W/DOT8W/SELRES), EG2, compact prefix, plain group loop, noinline QK dot,
  and amortized group loop; engine fingerprint `391b89a4c159`.
- **Tree B (B2):** seed line, plain/amortized loop and QK dot;
  `cbc403eec029` family.
- **Tree C (B1):** shippable bundle, composed attention, RX ring, plain loop,
  EG2, and amortized loop; `1ece8792b190` family.

These are preserved worker-tree/image identities, not a statement that current
[`engine/src/nd_model.c`](../../../engine/src/nd_model.c) exactly equals tree A.
Reconstruction must start from the per-experiment assets and hashes, then re-run
all gates.

## Final-tree incremental evidence

| Lever | Measured value | Qualification |
|---|---:|---|
| Plain group hardware loop | +0.44% | Device breadth plus own CQ2 differential |
| Amortized group loop | +0.27% B2 / +0.35% B3 / +0.34% B1 | Three-tree result; ELF/disassembly and per-tree bitwise sweep |
| Noinline QK dot | +0.22% | Seed line only; diverged twice on shippable line, not portable yet |
| EG2 narrow engram staging | +0.164% | Below direct bar, kept; enabled by compact prefix |
| Compact prefix | ~768 KiB reclaimed | Quality-neutral capacity enabler |

Anti-overfit monitors moved with the primary metric. On B3, decode
6.1117→6.1433 accompanied prefill 6.4533→6.4833, held-out `ext`
6.0415→6.0731, and unconstrained `think` 4.68→4.70. B2 showed the same
direction. This supports a shared kernel mechanism rather than a constrained
sampler timing artifact; it remains evidence on those workloads, not a universal
model benchmark.

## Late phase map

On the best tree, approximate overlapping diagnostic shares were:

- attention stage 81.2 ms/token (50.6%);
- CQ2 projection 79.7 ms (49.7%);
- attention heads 23.5 ms (14.6%);
- Hadamard 20.2 ms (12.6%);
- engram 14.6 ms (9.1%);
- phi 8.1 ms (5.0%);
- Sinkhorn 3.1 ms (1.9%);
- mHC mix 2.0 ms (1.2%);
- preparation/LUT 1.7 ms (1.1%);
- tail and RoPE below 1 ms combined.

The categories overlap; summing them is invalid. The map was used to price the
next lever, and the final sweep found no unmeasured above-bar target.

## Why 6.1433 is still a candidate

All final trees fail the same two device fixtures with total token delta 52:
`heldout_interval_one` and `heldout_long_tools_note_only`. Every other device
case has token delta zero; host is 19/19; fidelity equals the accepted pin;
top-1 is 10/10; each tree has its own bit-exact CQ2 differential; behavioral
capture is green; and B3 completed 10/10 soak requests with 5/5 repeated calls
byte-identical.

A transport-only RX-ring change flips exactly the two fixtures. The correct
next step is an owner definition of the oracle/product behavior, not another
kernel experiment. Options:

1. keep lossless long-request handling and deliberately re-baseline/replace the
   two state-sensitive fixtures;
2. drop the ring and its >128-byte request fix to retain the old 20/20 oracle;
3. leave the cases as blockers and ship neither candidate.

Until that decision and an explicit promotion, the accepted performance remains
5.3033 tok/s.

## Stop condition

The campaign closed after per-tree differentials, behavioral capture on the
best tree, repeat soak, pool restoration, an updated phase map, and quantitative
closures for every remaining above-bar hypothesis. Run #803's exhaustive sweep
again found no such candidate. A later shippable-line QK retry remained an open
diagnostic at the documentation snapshot; it does not change the packet.

See [experiment ledger](../appendices/experiment-ledger.md) for the raw-run
index and [reproduction](../appendices/reproduction.md) for commands.

