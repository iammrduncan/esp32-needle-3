# exp94 - engram[1] narrow staging on the SEED line (implemented, UNMEASURED)

**Assets:** board 3's `nd_model.c` + `nd_model.h` after the rewrite (the seed line's own guarded
`EGRELOC` draft was replaced, not patched).

**Change (per the queue's spec):** the engram K/V pair that whole-tensor containment leaves OUTSIDE
the tier span gets its **own narrow `ND_ALLOC` copy** - not the span tail (measured unused space
there: 132,032 B vs this pair's 313,344 B). Selection is **by offset, never index** (the first site
is already contained and must not be selected); both copies are byte-compared; an
`EG2 ok=1 bytes=... k=[..) v=[..)` proof line prints what was chosen. The dispatch serves it from
`m->eg2_psram`, and the engram GEMVs already fetch `nd_tier_ptr(m, kp/vp)`, so the path is reached.

**Status:** builds clean (app `98c47d4f349e`), flashed, then the lane produced **no output for ~10
minutes** - the same silent-wedge signature board 1 shows. So **board 3 is also suspect** for the
same strap/host condition and this candidate is **UNMEASURED**, not disproven.
