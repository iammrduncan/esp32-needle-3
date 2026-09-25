# exp89 - wide phi on the shippable line (kept, +0.57 %)

**Assets:** the three files of board 1's wide-phi tree, taken live from the worker:
`gemv4_tie728.S` (carries `nd_gemv4_rows_tie1W`), `nd_quant.c` (carries
`gemv_rows_offset_asmW` + the alignment-aware selection), `nd_quant.h` (its declaration).
Worker PROVENANCE after the port: `ac2387bfb11ea0a0fb9bdf197c9ef44f`.

**Measured:** decode **5.8633** vs that board's own 5.83 tree = **+0.57 %**, prefill 6.165
(+0.60 %), min_case 5.65 (+0.53 %), 6/6 device exact, token_delta 0, internal_free 10,767.

**Why this asset exists:** `log_experiment` commits in the MAIN workspace while the change lives
in the worker checkout (the #748 lesson). Re-read the file from main after every keep and export an
asset when the change only exists in a worker.

**Port mechanics (both cost a build and are worth remembering):**
1. Extract the kernel **to its next top-level directive** (2,406 bytes here), not "from the comment
   to EOF" - the latter drags in lane-mix kernels the receiving file already defines.
2. The kernel's local labels (`.Llp`, `.Llp_end`, `.Llm`, `.Lmu`, `.Lpvchunk`, `.Lk1`, ...) collide
   with the receiving file's own; rename only the colliding ones (`.LW*`) and the assembler's
   `loop target does not follow loop instruction` error disappears.
3. The register list must be written `f7,f6,f5,f4`: the 128-bit extension forms are post-increment
   and the **last named register receives the lowest address** (TRM 1.8.25/1.8.65).
4. Check the receiving tree already has 16-byte-aligned `xh` (`ND_ALLOC_FAST16`); if not, the
   kernel's own guard falls back silently and the port measures null.

**Owed:** full-group gate + host gates on this tree.
