# exp90 - CQ2 group hardware loop (KEPT, B2, +0.44 %)

**Asset:** `lut2_tie728.S.grouploop` = board 2's live file (engine PROVENANCE at the run's start).
**Change:** in the LIVE kernel `nd_lut2_rows_tie1n` (labels `.Ltn_row/.Ltn_group/.Ltn_gend`),
`loop a2, .Ltn_gend` before `.Ltn_group`, and the group tail's
`addi.n a2,-1` / `beqz a2,.Ltn_gend` / `j .Ltn_group` deleted.
**Measured:** decode **6.0883** vs the board's 6.0617 pin = **+0.44 %**; prefill 6.4283;
min_case 5.86; 6/6 exact, delta 0; internal_free 11,795.
**Trap to remember:** the `.Lt1_*` kernel at lines 270-320 is **not dispatched** - editing it is a
silent null. The live labels are `.Ltn_*`.
**Owed:** the bitwise row differential (multi-row, real fixtures) and a breadth promotion.

# exp91 - QK dot outlined to noinline qk_dot8 (KEPT, B3, +0.22 %)

**Asset:** `nd_model.c.qkdot8` = board 3's live file.
**Change:** `attn_heads`' 8-column four-accumulator QK dot (sixteen statements per 8 columns) moved
into `static ND_HOT __attribute__((noinline)) void qk_dot8(...)`, caller keeps the four scale
multiplies in place. Statement-for-statement identical arithmetic.
**Why:** the live ELF's QK loop spills two float values through `a11 = a1 + 0x470` inside every
8-column iteration (0x4037c77c/78b/793, reload 0x4037c7b3; same slot at 0x4037c803/847). This also
**corrects the earlier zero-spill claim**, which came from a regex that only matched literal `a1`
operands and so missed indirect frame accesses.
**Measured:** decode **6.0750** vs 6.0617 = **+0.22 %**; prefill 6.41; min_case 5.85; 6/6 exact,
delta 0; internal_free 12,035.
