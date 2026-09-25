# exp93 - composed tree: group hardware loop + qk_dot8 (KEPT, B3, 6.1100)

**Tree identity:** board 3, engine PROVENANCE in `R-both-b3.log`; app_md5 `5694807b27ff`.
Composition of `.auto/exp90` (CQ2 group `loop`, +0.44 %) and `.auto/exp91` (`noinline qk_dot8`,
+0.22 %), built by copying the cleaned `lut2_tie728.S` into board 3's tree, which already carried
`qk_dot8` in `nd_model.c`.

**Measured:** decode **6.1100** against the 6.0617 pin = **+0.80 %** (the sum of the halves would be
+0.66 %, so the pair is additive-to-slightly-super-additive - unusual for this campaign, whose
compositions are normally sub-additive). prefill **6.4483** and min_case **5.88** are both bests;
6/6 device byte-exact, token_delta 0; internal_free 12,035.

**Owed on this tree:** the full-group breadth promotion (restricted screen only so far), host gates,
and the row-level differential for the QK extraction (the loop half's differential is already green
at chunk 1/12/24).
