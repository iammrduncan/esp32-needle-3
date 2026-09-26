# exp95 - EG2 (engram[1] K/V staged) + narrow compact prefix on the SEED line (WORKS, sub-bar)

**Assets:** board 3's `nd_model.c` + `nd_model.h` (single staging block, no debug prints, one value call).

**Measured (B3, own tree):** decode **6.1217** vs its composed pin 6.1117 = **+0.164 %** (or +0.191 %
vs the 6.1100 screen); prefill 6.4633; min 5.9; 6/6 exact, delta 0; post-prime STATE PSRAM 517,028.
**Disposition:** positive but **sub-bar** - preserved, no repeat, no breadth promotion.

**Why it now works (three earlier attempts failed):**
1. **PSRAM pressure** - the crash was a NULL store (`EXCVADDR 0`) after `ERR prefix_cache_allocation`:
   EG2 pins 313,344 B and the seed line lacked the compact prefix. Fixed by porting the narrow exp88
   `prefix_kv` (LIVE slots only; restore assigns snap_pos/eg_pos/n_sink **before** the copy).
2. **A duplicated staging block** - the saved seed carried TWO blocks; the second NULL-reset the first
   and leaked 313,344 B (hence the ~639 KB free-PSRAM drop, not 313 KB). Removed.

**Order of operations that made it work:** host quality gate on the tree (19/19, fidelity unchanged,
prefix isolation passes) -> screen -> own-tree comparison.
