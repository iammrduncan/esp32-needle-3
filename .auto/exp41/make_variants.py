#!/usr/bin/env python3
"""Experiment 41 candidate generators: interleave INDEPENDENT dependent chains.

fw2 (#378, +0.461 %) won because a butterfly is a serial add/sub chain with nothing
else in flight, not because "elementwise loops" respond to unrolling - silu4 (+0.03 %)
and the five elementwise unrolls (#365-#370) are nulls. So this family's generator is
"find a long reduction, run several of them at once". Unrolling ONE reduction is
forbidden (reassociation; #366/#367 correctly refused it), while interleaving
independent reductions is bit-exact because each accumulator's own sequence of
operands never moves.

Two families are generated here, both from the SHIPPED text so there is no hand-typed
transcription of an FP loop (the class that made run #363 diverge 4/17 on device while
its benchmark reported diff=0, and that produced my own first cond2):

  cond{2,4}   - the Hadamard MLP's conditioning projection: 8 channels x dm-term
                serial sums per layer. Each channel keeps its own accumulator from
                0.0f to its own store.
  fw{N}pair   - N FWHT groups walked together. Each group's stages still complete in
                its own order, so every cell ends with the value nd_fwht produces.
                `inner` selects the within-stage butterfly grouping (2 = the shipped
                fw2 form, 1 = no within-stage unroll) so the attribution question
                "is the win cross-group or is it the same unroll again?" is answerable.
  fw2single   - that attribution variant: paired groups, one butterfly per stage.

Every variant is guarded off-device before a board:
  .auto/exp41/test_cond_equiv.c   generated cond loop vs shipped form, bit-for-bit
  .auto/exp41/test_odd_split.c    nd_cq_prepare under 3+3 / 1+5 / 5+1 / 2+4 / one-group
                                  splits, which is what the device does and the host's
                                  rows_serial never does.

Usage: python3 .auto/exp41/make_variants.py cond2 cond4 fw4pair fw2single
"""
import pathlib
import sys

MODEL = pathlib.Path('engine/src/nd_model.c')
QUANT = pathlib.Path('engine/src/nd_quant.c')

# ---------------------------------------------------------------- cond family

COND = """static ND_HOT void cond_rows(void *vc, uint32_t c0, uint32_t c1)
{
    const cond_ctx *c = (const cond_ctx *)vc;
    uint32_t        ch, i;
    for (ch = c0; ch < c1; ch++) {
        float acc = 0.0f;
        for (i = 0; i < c->dm; i++)
            acc += c->x[i] * c->cv[(size_t)i * 8 + ch];
        c->dst[ch] = acc;
    }
}"""


def cond_variant(k):
    accs = ", ".join("a%d = 0.0f" % u for u in range(k))
    # The channel offset MUST stay relative to ch. The first version of this line
    # emitted a literal `+ 0u` / `+ 1u`, so channels 2..7 silently computed
    # channels 0 and 1's sums; the host golden caught it (exact=0/19) and
    # test_cond_equiv.c localised it in one run.
    body = "\n".join("            a%d += xv * cv[(size_t)i * 8u + ch + %du];" % (u, u)
                     for u in range(k))
    stores = "\n".join("        dst[ch + %du] = a%d;" % (u, u) for u in range(k))
    return f"""static ND_HOT void cond_rows(void *vc, uint32_t c0, uint32_t c1)
{{
    const cond_ctx *c = (const cond_ctx *)vc;
    const float   *x   = c->x;
    const float   *cv  = c->cv;
    float         *dst = c->dst;
    const uint32_t  dm = c->dm;
    uint32_t        ch, i;

    /* {k} channels' reductions in flight. Each channel keeps its OWN accumulator
     * from 0.0f to its own store, so its dm additions happen in exactly the
     * shipped order: bit-exact by construction. cond_v is stride 8 floats, so the
     * {k} channels also share each loaded line instead of re-filling it. */
    for (ch = c0; ch + {k}u <= c1; ch += {k}u) {{
        float {accs};
        for (i = 0; i < dm; i++) {{
            float xv = x[i];
{body}
        }}
{stores}
    }}
    for (; ch < c1; ch++) {{
        float acc = 0.0f;
        for (i = 0; i < dm; i++)
            acc += x[i] * cv[(size_t)i * 8 + ch];
        dst[ch] = acc;
    }}
}}
"""


def make_cond(k, out):
    s = MODEL.read_text()
    assert s.count(COND) == 1, "cond_rows shipped body not found verbatim"
    out.write_text(s.replace(COND, cond_variant(k), 1))
    print("wrote %s (cond%d)" % (out, k))


# ---------------------------------------------------------------- fw pair family

RESCALE = """        {
            uint32_t j = 0u;
            float   *restrict b4 = blk;
            for (; j + 3u < g; j += 4u) {
                float v0 = b4[j], v1 = b4[j + 1u], v2 = b4[j + 2u], v3 = b4[j + 3u];
                b4[j] = v0 * scale; b4[j + 1u] = v1 * scale;
                b4[j + 2u] = v2 * scale; b4[j + 3u] = v3 * scale;
            }
            for (; j < g; j++) b4[j] *= scale;
        }"""

HEADER = """    for (gi = g0; gi < g1; gi++) {
        float   *blk = xh + (size_t)gi * g;
        nd_fwht(blk, g);"""

FW_SCALE = """/* Shared by both paths, so the rescale text here is the shipped one verbatim
 * (run #362's unroll-4, +0.332 %). */
static ND_HOT void fw_scale(float *restrict b4, uint32_t g, float scale)
{
    uint32_t j = 0u;

    for (; j + 3u < g; j += 4u) {
        float v0 = b4[j], v1 = b4[j + 1u], v2 = b4[j + 2u], v3 = b4[j + 3u];
        b4[j] = v0 * scale; b4[j + 1u] = v1 * scale;
        b4[j + 2u] = v2 * scale; b4[j + 3u] = v3 * scale;
    }
    for (; j < g; j++) b4[j] *= scale;
}

"""


def paired_def(ng, inner):
    """Build `nd_fwhtN(float *x0, ..., uint32_t n)`: one stage walk that performs
    `inner` butterflies per stage in each of `ng` groups. All indices are derived
    from the shipped loop's own (base, j, len) geometry, and each group's body is
    the shipped butterfly text applied to that group's pointer."""
    params = ", ".join("float *x%d" % u for u in range(ng))
    decls, stores = [], []
    for u in range(ng):
        for v in range(inner):
            decls.append("                float a%d%d = x%d[j + %du], b%d%d = x%d[j + %du + len];"
                         % (u, v, u, v, u, v, u, v))
            stores.append("                x%d[j + %du] = a%d%d + b%d%d;"
                          "             x%d[j + %du + len] = a%d%d - b%d%d;"
                          % (u, v, u, v, u, v, u, v, u, v, u, v))
    tdecl, tstore = [], []
    for u in range(ng):
        tdecl.append("                float p%d = x%d[j], q%d = x%d[j + len];" % (u, u, u, u))
        tstore.append("                x%d[j] = p%d + q%d; x%d[j + len] = p%d - q%d;"
                      % (u, u, u, u, u, u))
    # Butterflies per stage inside one group. The first version used 2*inner,
    # which with inner=1 stepped by 2 and skipped every other butterfly - a
    # fast-wrong kernel, caught by reading the emitted text before any build.
    stride = inner
    return """/* %d groups' transforms interleaved, %d butterflies per stage in each. Each
 * group's stages still complete in its own order (stage k before stage k+1 of the
 * same group), so every cell ends with the value the shipped nd_fwht would have
 * written: bit-exact by construction, the same disjointness argument run #378's
 * within-stage unroll used, applied across groups. */
static ND_HOT void nd_fwht%d(%s, uint32_t n)
{
    uint32_t len, step, base, j;

    for (len = 1u; len < n; len <<= 1) {
        step = len << 1;
        for (base = 0u; base + step <= n; base += step) {
            const uint32_t half = base + len;
            for (j = base; j + %du < half; j += %du) {
%s

%s
            }
            if (j < half) {
%s

%s
            }
        }
    }
}

""" % (ng, inner, ng, params, stride - 1, stride, "\n".join(decls), "\n".join(stores),
       "\n".join(tdecl), "\n".join(tstore))


def make_pair(out, ng, inner, tag):
    s = QUANT.read_text()

    # 1. the paired transform, derived from the shipped nd_fwht's own geometry
    a = s.index("\nND_HOT void nd_fwht(")
    end = s.index("\n}\n", s.index("for (len", a)) + 3
    assert "len <<= 1" in s[a:end], "nd_fwht text does not look like the shipped transform"
    s = s[:a] + "\n" + paired_def(ng, inner) + s[a + 1:]

    # 2. fwht_rows: paired loop, then the shipped single-group path for a remainder
    assert s.count(RESCALE) == 1, "accepted rescale block not found verbatim"
    assert s.count(HEADER) == 1, "accepted group loop not found verbatim"
    ha = s.index(HEADER)
    rb = s.index(RESCALE, ha)
    close = "\n    }\n}"
    tail_at = s.index(close, rb + len(RESCALE))
    assert s[ha:tail_at].endswith(RESCALE), "unexpected fwht_rows body shape"

    ptrs = "\n".join("        float *blk%d = blk + %uu * g;" % (u, u) for u in range(1, ng))
    calls = "\n".join("        fw_scale(blk%d, g, scale);" % u for u in range(1, ng))
    paired = ("""    /* %d groups per walk, then the shipped single-group path for the remainder.
     * The guard is `gi + %d < g1` (ng-1), NOT `gi + %d < g1`: with ngroup=6 split
     * 3+3 each nd_parallel_rows call owns exactly THREE groups, so a loop that
     * demanded four in hand would never execute on device and the candidate would
     * silently re-measure the accepted path - run #333's garbage-collected-kernel
     * null in a new guise. The same geometry is why ng=4 is not a curve point at
     * all here and ng=3 is: 3 groups per core means exactly one triple and no
     * tail, while 2 is one pair plus one single. .auto/exp41/test_odd_split.c
     * checks the remainder under the 3+3 split, which the host's rows_serial
     (even count) never exercises (#333's class). */
    for (gi = g0; gi + %du < g1; gi += %du) {
        float *blk = xh + (size_t)gi * g;
%s

        nd_fwht%d(%s, g);
        fw_scale(blk, g, scale);
%s
    }
    for (; gi < g1; gi++) {
        float   *blk = xh + (size_t)gi * g;
        nd_fwht(blk, g);
        fw_scale(blk, g, scale);
    }
}""" % (ng, ng - 1, ng, ng - 1, ng, ptrs, ng,
        ", ".join(["blk"] + ["blk%d" % u for u in range(1, ng)]), calls))

    s = s[:ha] + paired + s[tail_at + len(close):]
    s = s.replace("static ND_HOT void fwht_rows", FW_SCALE + "static ND_HOT void fwht_rows", 1)

    # The shipped single-group call legitimately appears TWICE in this file - the
    # fwht_rows remainder and the dequant/prepare walk - so the guard counts 2 and
    # separately requires the paired definition and call to exist.
    assert s.count("nd_fwht%d(" % ng) == 2, "paired transform def/call missing for ng=%d" % ng
    assert s.count("nd_fwht(blk, g);") == 2, "unexpected number of single-group calls"
    assert "fw_scale(blk, g, scale);" in s, "remainder does not rescale"
    out.write_text(s)
    pathlib.Path('/tmp/ndm_%s.c' % tag).write_text(MODEL.read_text())
    print("wrote %s (%s: %d groups x %d butterflies/stage + single-group tail)"
          % (out, tag, ng, inner))


VARIANTS = {
    "cond2":   lambda: make_cond(2, pathlib.Path('/tmp/ndm_cond2.c')),
    "cond4":   lambda: make_cond(4, pathlib.Path('/tmp/ndm_cond4.c')),
    "fw2pair": lambda: make_pair(pathlib.Path('/tmp/ndq_fw2pair.c'), 2, 2, "fw2pair"),
    "fw3pair": lambda: make_pair(pathlib.Path('/tmp/ndq_fw3pair.c'), 3, 2, "fw3pair"),
    "fw2single": lambda: make_pair(pathlib.Path('/tmp/ndq_fw2single.c'), 2, 1, "fw2single"),
}

if __name__ == "__main__":
    for which in sys.argv[1:]:
        if which not in VARIANTS:
            raise SystemExit("unknown variant %r (have: %s)" % (which, ", ".join(sorted(VARIANTS))))
        VARIANTS[which]()
