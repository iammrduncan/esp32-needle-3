#!/usr/bin/env bash
# Build and run the fwscale equivalence guard against a scratch copy, so it can be
# checked while the main tree carries a DIFFERENT candidate (fw3res, measuring now).
#
# The oracle is the shipped, exported nd_fwht() from the engine library, not a copy of
# the transform: run the accepted transform + an elementwise scale, and require the
# fused candidate to match bit for bit. The candidate body is EXTRACTED from the
# patched file by this script rather than typed by hand, because hand-transcribed
# copies of a loop are the bug class that cost runs #333/#363 their first builds - a
# test that holds its own copy of the loop has only proved the copy correct.
set -uo pipefail
MAIN=/workspace/esp32-needle-3
S=/tmp/fws
rm -rf "$S"; mkdir -p "$S"
git -C "$MAIN" show HEAD:engine/src/nd_quant.c > "$S/nd_quant.c" || exit 1
echo "base_md5=$(md5sum < $S/nd_quant.c | cut -c1-12)"
python3 "$MAIN/.auto/exp45/apply_fw3fold.py" "$S/nd_quant.c" || exit 1

python3 - "$S" <<'PY'
import pathlib, sys
s = pathlib.Path(sys.argv[1])
text = (s / "nd_quant.c").read_text()
i = text.index("static ND_HOT void nd_fwht3s")
j = text.index("\nstatic ND_HOT void fwht_rows")
body = text[i:j].replace("static ND_HOT void nd_fwht3s", "void nd_fwht3s", 1)
(s / "fwscale_body.inc").write_text("#define ND_HOT __attribute__((hot))\n" + body + "\n")
print("extracted_lines=%d" % body.count("\n"))
PY

cat > "$S/test.c" <<'C'
/* fwscale vs the shipped transform: same bits, or the candidate is dead.
 *
 * Covers what the frozen goldens cannot reach: g values the model never uses, scales
 * that are not 1/sqrt(128), denormal and large operands, and exact ties, so a
 * rounding difference in the folded multiply would show up here rather than only in a
 * 19-case generation run that compares against outputs this phase never perturbs. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "nd_quant.h"

void nd_fwht3s(float *x0, float *x1, float *x2, uint32_t n, float scale);
#include "fwscale_body.inc"

static uint32_t st = 0x9E3779B9u;
static float rndf(void) {           /* arbitrary mantissas, mixed signs and exponents */
    st ^= st << 13; st ^= st >> 17; st ^= st << 5;
    union { uint32_t u; float f; } v;
    v.u = 0x3C000000u + (st % 0x02000000u);
    return (st & 1u) ? v.f : -v.f;
}

int main(void) {
    uint32_t gs[6] = { 2u, 4u, 8u, 64u, 128u, 256u };
    int bad = 0, cases = 0;

    for (int gi = 0; gi < 6; gi++) {
        uint32_t g = gs[gi];
        for (int trial = 0; trial < 400; trial++) {
            float a[256], b[256], c[256], ra[256], rb[256], rc[256];
            float scale = (trial % 7 == 0) ? 1.0f / sqrtf((float)g)
                        : (trial % 11 == 0) ? 0.0f : rndf();
            for (uint32_t i = 0; i < g; i++) { a[i] = ra[i] = rndf(); b[i] = rb[i] = rndf(); }
            if (trial % 13 == 0 && g > 2u) { a[0] = ra[0] = 0.0f; a[1] = ra[1] = -0.0f; }

            /* Reference: exactly what ships - the exported transform, then one
             * independent multiply per element. */
            nd_fwht(ra, g); nd_fwht(rb, g);
            for (uint32_t i = 0; i < g; i++) { ra[i] *= scale; rb[i] *= scale; }

            /* third block: same construction, independent contents */
            for (uint32_t i = 0; i < g; i++) { c[i] = rc[i] = rndf(); }
            nd_fwht(rc, g); for (uint32_t i = 0; i < g; i++) rc[i] *= scale;
            nd_fwht3s(a, b, c, g, scale);
            for (uint32_t i = 0; i < g; i++) {
                if (memcmp_(&a[i], &ra[i]) || memcmp_(&b[i], &rb[i]) || memcmp_(&c[i], &rc[i])) bad++;
                cases += 3;
            }
        }
    }
    printf("FWSCALE_EQUIV cases=%d bit_mismatches=%d\n", cases, bad);
    return bad != 0;
}
C
# memcmp for scalars, spelled out so a bit difference cannot be optimised away
sed -i 's/int main(void) {/static int memcmp_(const void *x, const void *y){ unsigned char p[4],q[4]; __builtin_memcpy(p,x,4); __builtin_memcpy(q,y,4); return p[0]!=q[0]||p[1]!=q[1]||p[2]!=q[2]||p[3]!=q[3]; }\n\nint main(void) {/' "$S/test.c"
cc -O2 -Iengine/include "$S/test.c" -o "$S/test" -L host/build -lneedle_engine -lm 2>&1 | head -8
"$S/test"; echo "equiv_rc=$?"
