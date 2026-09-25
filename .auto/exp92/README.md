# exp92 - engram[1] K/V staged into its own narrow PSRAM copy (PREPARED, UNMEASURED)

**Assets:** board 1's `nd_model.c` + `nd_model.h` carrying the change (exports of the live worker files).

**Change:** the engram K/V pair that whole-tensor containment leaves OUTSIDE the tier span
(measured: dir233/234, `[15,707,584, 16,020,928)`, 313,344 B - exactly where the span's last
contained byte 15,707,584 ends) gets its own narrow `ND_ALLOC` copy, served by `nd_tier_ptr`
through two extra ranges. Selection is **by offset, never by index** (only tensors lying wholly in
the unstaged window are considered); both copies are byte-compared; a boot line prints what was
selected (`EG2 ok=1 bytes=... k=[..) v=[..)`) so a wrong pair cannot pass silently.

**Why the call sites reach it:** the engram GEMVs already fetch their pointers as
`nd_cq_gemv_lut2(kp, nd_tier_ptr(m, kp), ...)` and the same for `vp` (nd_model.c ~1428-1431), so the
dispatch is live for this pair - unlike the earlier draft, which selected `m->engram[0]` (already
staged) and could never fire.

**Status:** BUILD OK, screen launched on B1 (pin 5.8633) - the lane produced no `EG2` line and no
metrics within its window (~17 min, 3 processes alive), so the board/tree state is unresolved and
**no measurement exists**. Next window: read the B1 lane log, check whether the board is wedged on
this candidate or the lane simply outran its window, and either complete the screen or revert this
asset from the worker before any other B1 screen (the tree's provenance hash differs from the
wide-phi pin, so a later B1 number would otherwise be confounded).
