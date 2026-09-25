# Needle 3 mentor queue

Mentor refresh 2026-09-25 01:53 UTC. Research resumed after all three boards
had been idle since ~00:01. Keep three DIFFERENT lanes moving. Preserve worker
dirt, source snapshots, locks, repeat guards, frozen fixtures and 240/80 MHz.
Only the researcher implements, builds, flashes and measures.

## Evidence and pins

**Accepted shipping remains 5.3033 tok/s**, bundle5 `2c79104`, canonical engine
`0c1a6272cd01`, device 20/20. Faster discovery is not shipping acceptance.

| Lane / completed screen | Decode tok/s | Own comparison | Quality / memory |
|---|---:|---|---|
| B1 bundle5 + attention composition + ring (#639) | 5.4083 | B1 starting pin | Full 18/20, delta 52; heap 5679 |
| B2/B3 seed-era composition (#635/#640) | 5.7467 | Each board's starting pin | Full 18/20, delta 52; heap 4823 |
| B1 staging helper (#643), `5a9b75f4c948` | **5.4650** | **+1.048%** vs B1 pin | Primary 6/6, delta 0, rc=0; heap 6191 |
| B2 nonpositive exp (#644), `b580af16a62c` | 5.7367 | -0.174%; retire this form | Primary 6/6, delta 0; heap 4575 |
| B3 paired rescale exp (#645), `18527a7a80c9` | 5.7150 | -0.552%; retire this form | Primary 6/6, delta 0; heap 4063 |
| B2 staging transfer, `91e79eeeb304` | **5.8083** | **+1.072%** vs B2 pin | Primary 6/6, delta 0, rc=0; heap 5087 |

All four new screen trees passed host 19/19, fidelity 5.341e-05, top1 10/10.
Primary-only screens have NO new extended/think result. B2 5.8083 and B1 5.4650
are now the preserved discovery pins for further isolated changes.

Logs: `/root/board-pool/batches/M-{kvst-b1,npexp-b2,rsp-b3,kvst-b2}.log`.
Sources: `/root/board-pool/preserved/b1-recipeB/`,
especially `nd_model.c.{cmp-d8sr,kvst-b1,npexp,rspair}` and
`nd_quant.h.npexp`; preserve B2's winning tree before its next edit.

## Current three lanes and next turnover

1. **B1: accepted bundle5 + ONLY late RX ring, one full frozen gate.**
   Live device benchmark at 01:53; `M-ringonly-b1.log`, engine
   `d6c8c1039f67`. Mentor verified diff versus 2c79104: ONLY main.c UART
   ring install/use plus esp_driver_uart CMake dependency; all engine files
   unchanged. No attention composition or staging helper in this image.
   This is a deliberate attribution exception: does the 18/20 failure pair
   follow the transport/build change alone? Leave the real job running.
   If the SAME pair fails, stop blaming attention for that blocker and preserve
   the discriminating evidence; it still does not authorize changing goldens.
   If 20/20 passes, attention/candidate changes remain implicated; choose the
   earliest differing runtime boundary, not another unchanged full gate.
   Afterwards return B1 to new performance work: add a winning B3 cursor to
   its preserved 5.4650 staging tree, or take compact specialization below if
   the cursor is null/negative. Do not leave the board idle for owner admission.

2. **B2: one isolated P.V call-boundary screen against 5.8083.**
   Staging transfer is COMPLETE, six cases exact. Preserve its source, then
   add `noinline` ONLY to existing `pv_pair2`, keeping ND_HOT, signature,
   selective rescale branches, four-cell width and every arithmetic operation.
   In the winning ELF pv_pair2 has no symbol: it is absorbed into attn_heads.
   A smaller allocator scope now has supporting evidence from staging, but
   this callee has 13 arguments: extra stack/call traffic may easily lose.
   Inspect the parent/callee frame and hot-loop traffic, then primary-screen
   ONCE. No cursor/composition changes in this screen. If it loses, preserve
   the evidence and move on; do not iterate equivalent helper spellings.

3. **B3: sink-aware slot cursor on its 5.7467 composition pin.**
   Host checks green; device benchmark live at 01:53. `M-cur-b3.log`,
   engine `cdb6ba4ea983`. Original ELF executes two remu per position pair
   (0x4037c158/0x4037c176). Seed each sink/recent run with actual
   `kv_slot(m,base)`; step `s+1 == window ? n_sink : s+1`; retain the odd
   tail and pair across wrap. The researcher checked slot equivalence on
   small windows, all sink counts, and 384/512 geometries over multiple wraps.
   Old #42 used WRONG base%window, so it was not a valid timing rejection.
   Harvest this independent screen before composing it with staging.
   If positive, B1 can test its transfer while B3 takes the reserve below.

Use the existing wrapper and `checks.sh && measure.sh` so failed checks cannot
flash. Confirm actual child processes AND growing nonempty logs. These precheck
launchers do NOT emit CHECK_RC: read their host/fidelity results and the lane's
METRIC/LANE_RC. Do not spend board time on duplicate controls, poll loops or
result prose while another lane has a ready experiment.

## Why the priority changed; ready reserve

The staging win is concrete codegen evidence, on two runtime stacks. In the old
attn_heads, K unpack stored/reloaded four integer byte temporaries at frame
+0x464..470 every loop; V had three similar spills. Conversion was already
native float.s. The 120-byte `kv_stage_pair` now uses a 32-byte frame, hardware
LOOP and ZERO inner-loop stack traffic. Parent remains frame 0x580; combined
parent/helper text shrank 309 bytes. Both winning ELFs have attn_heads size
0x1852. This supports controlled allocator-scope experiments, not a blanket
claim that all outlining wins.
[GCC noinline](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Common-Function-Attributes.html#Common-Function-Attributes).

**Reserve: compact actual-shape specialization with separate cold fallback.**
Guard qk=48/v=64/rep=6 and full KV-group head-range boundaries. Keep generic
odd-head behavior in a separate cold helper, preferring a shared body with
constant parameters to repeated manual transcription. Inspect map, frame and
hot object, then measure against that worker's own preserved staging pin
(or B3's own current composition pin if staging has not been transferred).
Do not repeat the previous failed outline-generator repair marathon. One lane
owns this reserve at a time; if mechanically blocked, preserve it and pick a
distinct ready mechanism instead of parking the other boards.

## Quality attribution and measured closures

Host and device goldens already differ for the frozen failing cases
`heldout_interval_one` / `heldout_long_tools_note_only`. New device counts
19/63 correspond to host 23/67 minus four forced tokens; visible calls agree.
That is a clue, not proof. Host 19/19 does not prove identical target arithmetic.
#633 changed BOTH attention and RX ring, so it did not separate those causes.
The router dispatch occurs AFTER generation; no actual timer-state-to-input
path was demonstrated. Old M-rxlate2-b2.log retained RXQ only for 7/8-byte
control commands, not failing-prompt hashes: drop=0 there is not input identity.
No oracle waiver, replacement, pacing change or signature-guard bypass.

Keep #624 P.V width8 null, #626 final-normalization pairing null, #628 QKTILE2
null, #634 counted QK negative, #636 counted P.V negative, and twice-negative
rescale/P.V fusion closed as measured forms. New #644 NP exp and #645 rescale
pairing are also negative. The NP guard correctly caught the first draft's sign
error before build; corrected actual-helper comparison passed 25.6M values.
No evidence established how often both rescale flags occur: #645's speed loss
and code growth are measured, its co-occurrence explanation is a hypothesis.

[FlashAttention-2](https://tridao.me/publications/flash2/flash2.pdf) was useful
as a prompt to examine scalar overhead and operand movement, not as a GPU
speedup claim or permission to reorder reductions. Needle already normalizes
only at the end. Only ~40 KB PSRAM remains in runtime STATE after both prefix
snapshots; do not budget a whole fp32 KV mirror from the ~2 MB boot figure.
Keep unsafe clocks, approximate math, FP16 taps, exhausted LUT/prefetch variants
and harness expansion down.

Next mentor: first harvest B1's ring-only failure attribution, then B3 cursor
and B2 P.V call boundary. Keep accepted **5.3033** distinct from discovery
**5.8083**, and turn over all three lanes instead of declaring research finished.
