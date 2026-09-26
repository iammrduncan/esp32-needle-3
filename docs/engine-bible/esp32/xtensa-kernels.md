# Xtensa LX7/TIE728 kernels

The target assembly work succeeded when it was built around the *actual*
ESP32-S3 core configuration and Needle 3 data layout. Copying an instruction
sequence from another Xtensa product or another model packing was not safe.

## Production CQ2 kernel contract

[`engine/src/lut2_tie728.S`](../../../engine/src/lut2_tie728.S) implements the
pair-table CQ2 row walkers. For Needle 3:

- group size is 128 weights;
- one group has 32 LSB-first index bytes;
- pair `p` represents weights `p` and `p+64`;
- each pair owns 16 FP32 table entries selected by one packed nibble;
- the table span per group is `64 * 16 * 4 = 4096` bytes;
- one FP16 norm follows per row/group in the separate norm blob;
- tensor blobs are 64-byte aligned and row strides satisfy the caller checks.

The address is data-dependent, so this is a gather kernel. The available
64/128-bit TIE float loads are contiguous loads and do not replace the
`extui + addx4 + lsi` gather sequence.

## Why the pair LUT works

The activation is transformed once into the quantizer's Hadamard domain. For
each group, precompute the contribution for all 16 combinations of a pair of
2-bit codes. Each packed byte/word then indexes contributions instead of
decoding and multiplying every scalar weight. The table is shared across rows,
so build cost is amortized over an output range.

The accepted assembly keeps four FP accumulator chains and folds them in the
same order as the C reference. It performs the ordinary-positive FP16 norm
conversion inline and falls back when the one-time tensor predicate finds a
zero, subnormal, infinity/NaN, or negative norm outside that fast domain.

## Measured variants

- The initial single-row TIE path was about `+32.4%` in the isolated kernel.
- The retained `tie1n` form was about `+33.4%` versus C in the tested sweep and
  delivered roughly `+13%` end-to-end on the model tree.
- A plain hardware group loop added about `+0.44%` on its tree.
- Amortizing loop setup once per call, with `wsr.lcount` plus `isync` per row,
  added `+0.27%`, `+0.35%`, and `+0.34%` on the three final trees.

Those values are not additive across arbitrary lineages. Use the per-tree
measurements and re-run the bit-exact differential after composition.

## ABI and frame lessons

ESP32-S3 uses the Xtensa windowed ABI. The assembly must respect register
windows, call frames, callee expectations, alignment, and exception spill
areas. A 32-byte frame appeared sufficient for two locals but overlapped the
window handler's spill use; return restored a corrupted address. A 64-byte
frame fixed the actual ABI requirement.

Reusable rule: a kernel that computes correct output and then faults on return
is often an ABI/frame problem, not arithmetic. Inspect `a0`, `a1`, exception
cause, prologue/epilogue, and generated compiler assembly for a comparable C
function.

## Hardware loops

Xtensa zero-overhead loops are not free to configure. Assembler-generated long
loop setup can erase a small-body win. The **final candidate asset**
[`exp96/lut2_tie728.S.amortised`](../../../.auto/exp96/lut2_tie728.S.amortised)
pays setup once per call and updates loop-count state per row. It uses the
instruction-sequencing requirements documented by Cadence, including `isync`
after loop-register reconfiguration. The linked current-checkout
[`engine/src/lut2_tie728.S`](../../../engine/src/lut2_tie728.S) is a different,
pre-amortized research state; do not infer candidate-A code from that link.

Proof for a loop change includes linked disassembly: the loop instruction must
target the intended `LBEG`, control flow must cover every group and tail, and
the body must survive relaxation/linking. Source syntax alone is insufficient.

## Instruction facts established by assembly

On the audited toolchain/core configuration:

- no useful scaled/indexed integer load was available for the nibble gather;
- no integer post-increment load used by the proposed body was available;
- `lsi` has a limited immediate reach that shaped table-base scheduling;
- there are 16 scalar single-precision FP registers;
- proposed packed/quad FP mnemonics were rejected;
- wide contiguous TIE loads do not accelerate random table gathers.

Always test candidate instructions with the exact target assembler and inspect
the emitted opcode. ISA-family documentation can describe optional extensions
that this configured core does not implement.

## Correctness proof

The CQ2 assembly is held to bit identity against `nd_lut2_rows_c`, using
`memcmp`, across the shape sweep. Required coverage includes:

- actual widths such as 128 and 768;
- row counts including odd tails;
- multiple starting rows and both split halves;
- all tensors eligible for fast norm conversion;
- C fallback for unsupported shape/alignment/norm domains;
- the exact candidate tree's build and dispatch.

The final-tree evidence includes `exact=768/768 bitexact=1` on each tree. That
does not replace host/device goldens; it isolates the kernel claim.

## Failure catalogue

| Attempt | Why it failed |
|---|---|
| Direct MimiModel kernel copy | Different model packing, stride, and call contract |
| Two-row frame at 32 bytes | Window spill collided with locals; corrupt return |
| 64/128-bit gather replacement | Production addresses are data-dependent and sometimes misaligned |
| More FP SIMD | Requested optional instructions absent on this core |
| Hoisted FP16 rebias constant | Exactly null; conversion off critical path |
| Norm FP32 sidecar | Instruction saving smaller than added memory traffic |
| QK outline transplant | +0.22% on seed; first shippable ports double-applied scales and diverged. Run #805 fixed the transcription and reached +0.339% on a restricted 6/6 B1 screen; full breadth remained due. |
| New object/flag only | Link GC or stale build can leave production path unchanged |

## Porting recipe

1. Derive packing, strides, norm domain, and tails from the local reader.
2. Compile a scalar C oracle with an explicit accumulation order.
3. Query the exact assembler for instruction availability.
4. Write the smallest one-row kernel; preserve the ABI conservatively.
5. Differential-test bits across production shapes and alignments.
6. Confirm dispatch and symbol in the linked ELF/disassembly.
7. Measure integrated request throughput, not only cycles/row.
8. Re-run the differential after every loop, scheduling, or composition change.
