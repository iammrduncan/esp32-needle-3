# exp96 - amortised group hardware loop (KEPT, B2, +0.27 %; CQ2 differential green)

**Assets:** `lut2_tie728.S.amortised` = board 2's live file; `kbench.c.sweep-hoisted` = the wiring
that finally produced the CQ2 differential (`ND_KBENCH_ASM=1` + the shape sweep hoisted above the
`#if/#elif` bench-selection chain, which never reached it).

**Change:** in the LIVE `nd_lut2_rows_tie1n`, the long-form loop setup runs **once per call**
(`l32i a2, a1, 20` ngroup, `l32i a4, a1, 16` first row's LUT base, `loop a2, .Ltn_gend` with LBEG at
`.Ltn_group`); each later row reloads the LUT base, re-arms `wsr.lcount = ngroup-1`, `isync`, and
jumps straight back to `.Ltn_group`. Rows are decremented and the exit tested **first**, so the last
row leaves LCOUNT at 0 and pays no re-arm.

**Measured:** decode **6.1250** vs B2's own 6.1083 = **+0.27 %**; prefill 6.4667; min 5.9; 6/6 exact,
delta 0; internal_free 12,003. **Verified:** `KB NUM shape=96x768 kernel=tie1n exact=96/96 bitexact=1
maxabs=0.000e+00` (plus tie1/tie2/tie1p/tie1m and all blob_int variants) and the ELF shows
`loop a2, 0x4037dc6c` with the epilogue jump landing exactly on that LBEG.

**Family so far (seed line):** plain group loop **+0.44 %**, amortisation **+0.27 %**.
**Not yet tried:** the same amortisation on the other dispatched 2-bit kernels or the 4-bit phi walker.
**Owed:** breadth promotion + host gates on this tree.
