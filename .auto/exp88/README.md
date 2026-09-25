# exp88 - compact saved prefixes (the +768,508 B reclaim)

**Asset:** `nd_model.c.prefixkv` = the full `engine/src/nd_model.c` of the shippable line's
compact-prefix tree, taken from board 1 (engine md5 `c69648d2784a`).

**Why this file exists here:** the change was developed in the *worker* checkout, so the
`log_experiment` "keep" commit (#748) recorded a commit in the MAIN workspace whose tree did not
contain the code. A worker reset would have lost it - this campaign has lost lane state that way
twice already. Always re-read the file from the main checkout after a keep, and export an asset
when the change only exists in a worker.

**What the change is** (self-contained, ~45 lines, all in `nd_model.c`):
1. `prefix_kv()` - a strided copy helper for one KV buffer compacted to the prefix's live slots.
2. The four KV `PREFIX_BUFFER` lines in `prefix_copy()` replaced by four `prefix_kv()` calls with
   `live = snap_pos + 1` clamped to `window`.
3. `nd_model_prefix_restore()` assigns `snap_pos`/`snap_eg_pos`/`n_sink` **before** `prefix_copy()`,
   because the live count is the PREFIX's own (it used to be assigned after).
4. A `PREFIXKV` evidence print at save time (its two size figures were both computed through the
   compacted path - fix by deriving the full size independently).

**Evidence:** host prefix-isolation test PASS (rc=0); device 6/6 exact, decode 5.8283 unchanged
(the restore is outside the decode clock); full 20-case gate 18/20 with token_delta 52 = the two
#647 goldens only; **post-prime `free_psram_bytes` 818,596 against the 50,088 baseline**.
