#!/usr/bin/env python3
"""Candidate `tap96`: the Q tap projection gets a 96-column partition so its second core runs.

The column-range split of the qkv tap convolutions uses a fixed 256-column unit: `nd_parallel_rows`
is handed `(dim + 255) / 256` units. For the Q projection `dim = n_heads * qk_head_dim = 576`, that
is THREE units, `rows_dual_core` computes `half = 3/2 = 1`, and its `half < 2` guard sends the whole
job back to the calling core - so the largest of the three tap projections, the one run #413 measured
at 73 % of the tap phase, has been running single-core. K (dim 96) and V (dim 128) are one unit and
are genuinely too small; the 768-column gate and out_proj already get four units and do use both
cores. Carrying an explicit chunk width gives 576 six units (three per core, 288 columns each)
without touching a single arithmetic expression.

Bit-exact by construction, and the argument is entirely about ranges: each output column depends
only on its own column of `proj`, the history rows and the weight rows, so any partition of the
column range produces the same values; `nt`, the ascending tap sum, the `+0.0f` seed, the two-column
inner pair and the tail clamp are all inside `tap_rows` and unchanged. Only dim == 576 takes width 96;
every other geometry keeps width 256 and therefore the identical dispatch it has today (where the
count stays below the parallel threshold the caller runs all units serially, and `b1 * chunk >= dim`
holds for either width, so coverage is identical). The host build's `nd_parallel_rows` is `rows_serial`
so the goldens verify the arithmetic; the device goldens are what test the split, which is why the
device gate is not optional here.
"""
import hashlib, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "0d639424f636"        # accepted (run #431) engine/src/nd_model.c

EDITS = [
    ("STRUCT", "                 uint32_t taps, pos, dim; } tap_ctx;",
               "                 uint32_t taps, pos, dim, chunk; } tap_ctx;"),
    ("ROWS", """    uint32_t        i, j, dim = c->dim;
    uint32_t        lo = b0 * 256, hi = (b1 * 256 < dim) ? b1 * 256 : dim;""",
             """    uint32_t        i, j, dim = c->dim;
    /* The unit width is the dispatcher's, not a constant of this loop: see
     * tap_projection for why 576 columns are split six ways instead of three. */
    uint32_t        lo = b0 * c->chunk, hi = (b1 * c->chunk < dim) ? b1 * c->chunk : dim;"""),
    ("CALL", """        tap_ctx tc = { projection, layer_history, weights, taps, m->pos, dim };
        nd_parallel_rows(tap_rows, &tc, (dim + 255) / 256);""",
           """        /* A 256-column unit gives dim=576 three units, and rows_dual_core's
         * `half < 2` guard runs three units on the calling core alone - the Q
         * projection has never used the second core. Width 96 makes it six
         * units (288 columns per core). Column ranges are disjoint outputs, so
         * no value can move; other geometries keep the accepted width. */
        const uint32_t chunk = (dim == 576u) ? 96u : 256u;
        tap_ctx tc = { projection, layer_history, weights, taps, m->pos, dim, chunk };
        nd_parallel_rows(tap_rows, &tc, (dim + chunk - 1u) / chunk);"""),
]


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    for name, old, _ in EDITS:
        if text.count(old) != 1:
            print(f"{name}_ANCHOR count={text.count(old)} -> refusing"); return 2
    new = text
    for _, old, repl in EDITS:
        new = new.replace(old, repl, 1)
    if new.count("c->chunk") != 3 or new.count("chunk = (dim == 576u)") != 1:
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print("APPLIED target=" + PATH + " md5=" + hashlib.md5(new.encode()).hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main())
