#!/usr/bin/env python3
"""Experiment 41: interleave INDEPENDENT dependent chains.

fw2 (+0.461 %, run #378) won by putting more independent add/sub work inside one
iteration of a long dependent butterfly chain. silu4 (+0.03 %) and the four
elementwise unrolls (#365-#370) show the lever is not general: it pays only
where the loop's work is a serial dependency chain with nothing else in flight.
The model's purest such chains are reductions - `acc += x[i]*cv[i*8+ch]` in
cond_rows (8 channels x 768 terms per Hadamard MLP, so ~49k dependent FMAs per
token) and the FWHT's butterflies across groups. A reduction cannot be unrolled
bit-exactly (that reassociates the sum, which is what #366/#367 correctly
avoided), but two or four *different rows'* accumulators are already independent,
so interleaving them changes no value, no operand and no order within a chain.
Variants are generated from the shipped text so there is no transcription to get
wrong (#363's class) and each carries an assert that the shipped form was found.
"""
import pathlib, sys

MODEL = pathlib.Path('engine/src/nd_model.c')
QUANT = pathlib.Path('engine/src/nd_quant.c')

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

def cond_body(k):
    idx = "".join("float a%u = 0.0f;\n        " % u for u in range(k))
    reads = "".join("a%u += xv * cv%u[i];\n            " % (u, u) for u in range(k))
    stor  = "".join("dst[ch + %uu] = a%u;\n            " % (u, u) for u in range(k))
    return f"""static ND_HOT void cond_rows(void *vc, uint32_t c0, uint32_t c1)
{{
    const cond_ctx *c = (const cond_ctx *)vc;
    const float   *restrict x   = c->x;
    const float   *restrict cv  = c->cv;
    float         *restrict dst = c->dst;
    const uint32_t  dm = c->dm;
    uint32_t        ch, i;

    /* {k} channels' reductions in flight. Each channel keeps its OWN accumulator
     * from 0.0f to its store, so its 768 additions happen in exactly the shipped
     * order: interleaving independent chains moves no value, no operand and no
     * order, and is therefore bit-exact by construction (unlike unrolling one
     * channel's loop, which would reassociate the sum - what #366/#367 refused
     * to do). This is the mechanism that won +0.461 % on the FWHT (#378): a
     * serial reduction has nothing else to hide its FMA latency.
     * The cv reads also get cheaper: stride 8 floats means the {k} channels share
     * the same cache lines, so one line fill now serves {k} uses. */
    for (ch = c0; ch + {k}u <= c1; ch += {k}u) {{
        {idx}for (i = 0; i < dm; i++) {{
            float xv = x[i];
            float cv0[i ? 1 : 1]; /* placeholder */
        }}
    }}
    for (; ch < c1; ch++) {{
        float acc = 0.0f;
        for (i = 0; i < dm; i++)
            acc += x[i] * cv[(size_t)i * 8 + ch];
        dst[ch] = acc;
    }}
}}"""

def cond_variant(k):
    """Build the interleaved body explicitly: one load of x[i], k strided cv
    loads, k independent accumulate chains."""
    accs = ", ".join("a%d = 0.0f" % u for u in range(k))
    # The channel offset MUST be (ch + u): the first version of this line emitted
    # a literal `+ 0u`/`+ 1u`, which made channels 2..7 compute channels 0..1's
    # sums. The host golden caught it (exact=0/19); the standalone differential
    # .auto/exp41/test_cond_equiv.c localised it in one run and is kept as the
    # guard for this generator.
    body = "\n".join("            a%d += xv * cv[(size_t)i * 8u + ch + %du];" % (u, u)
                     for u in range(k))
    stores = "\n".join("        dst[ch + %du] = a%d;" % (u, u) for u in range(k))
    return f"""static ND_HOT void cond_rows(void *vc, uint32_t c0, uint32_t c1)
{{
    const cond_ctx *c = (const cond_ctx *)vc;
    const float   *restrict x   = c->x;
    const float   *restrict cv  = c->cv;
    float         *restrict dst = c->dst;
    const uint32_t  dm = c->dm;
    uint32_t        ch, i;

    /* {k} channels' reductions in flight at once. Each channel keeps its OWN
     * accumulator from 0.0f through to its own store, so its dm additions happen
     * in exactly the shipped order - interleaving independent chains moves no
     * value, no operand and no order, which makes this bit-exact by construction.
     * Unrolling ONE channel's loop would reassociate the sum (#366/#367 refused
     * that for the same reason), so this is the only bit-exact way to attack a
     * reduction's latency. Mechanism is the one that won +0.461 % on the FWHT
     * (#378): a serial chain has nothing else in flight to hide its FMA latency.
     * Side effect in the same direction: cond_v is stride 8 floats, so {k}
     * channels now share each loaded cache line instead of re-filling it. */
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
    print(f"wrote {out} (cond{k})")

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

TAIL_LOOP = """    for (; gi < g1; gi++) {
        float   *blk = xh + (size_t)gi * g;
        nd_fwht(blk, g);
        fw_scale(blk, g, scale);
    }
}"""

PAIRED = """    /* Pairs of groups transformed together, then the shipped single-group path
     * for an odd remainder. The remainder matters: the device splits ngroup
     * 6+6-style as 3+3 across the two cores, so a per-core count of 3 reaches
     * this code and a paired loop without a tail would silently drop a group -
     * a defect the host cannot see, because its nd_parallel_rows is serial and
     * therefore always hands one core an even count. That is the #333 class
     * (a bug in the range form that no single-row test can reach), so the tail
     * is written first and the odd split is exercised on the host separately. */
    for (gi = g0; gi + 1u < g1; gi += 2u) {
        float *blk  = xh + (size_t)gi * g;
        float *blk2 = blk + g;

        nd_fwht2(blk, blk2, g);
        fw_scale(blk,  g, scale);
        fw_scale(blk2, g, scale);
    }
"""

def make_fw2pair(out):
    """Two FWHT groups transformed together: two groups' butterfly chains are
    fully independent, so one stage walk doing a butterfly in each gives four
    independent add/sub per iteration on top of the shipped 2-way within-stage
    unroll. The rescale is shared verbatim through fw_scale so neither path's
    rescale text differs from what ships."""
    s = QUANT.read_text()
    assert s.count(RESCALE) == 1, "accepted rescale block not found verbatim"
    assert s.count(HEADER) == 1, "accepted group loop not found verbatim"

    helper = """/* Shared by both paths, so the rescale text here is the shipped one verbatim. */
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

/* Two groups' transforms interleaved. nd_fwht's butterflies are a long serial
 * chain of add/sub across stages and fwht_rows calls it once per group with
 * nothing else in flight. Walking two groups together keeps each group's stages
 * in its own order - stage k of a group still completes before stage k+1 of that
 * same group - so every cell ends with the value nd_fwht would have produced:
 * bit-exact by construction, the same disjointness argument run #378's
 * within-stage unroll used, applied across groups instead of within one. */
static ND_HOT void nd_fwht2(float *x0, float *x1, uint32_t n)
{
    uint32_t len, step, base, j;

    for (len = 1u; len < n; len <<= 1) {
        step = len << 1;
        for (base = 0u; base + step <= n; base += step) {
            const uint32_t half = base + len;
            for (j = base; j + 1u < half; j += 2u) {
                float a0 = x0[j], b0 = x0[j + len], a1 = x0[j + 1u], b1 = x0[j + 1u + len];
                float c0 = x1[j], d0 = x1[j + len], c1 = x1[j + 1u], d1 = x1[j + 1u + len];

                x0[j] = a0 + b0;          x0[j + len] = a0 - b0;
                x0[j + 1u] = a1 + b1;     x0[j + 1u + len] = a1 - b1;
                x1[j] = c0 + d0;          x1[j + len] = c0 - d0;
                x1[j + 1u] = c1 + d1;     x1[j + 1u + len] = c1 - d1;
            }
            if (j < half) {
                float a = x0[j], b = x0[j + len];
                float c = x1[j], d = x1[j + len];

                x0[j] = a + b; x0[j + len] = a - b;
                x1[j] = c + d; x1[j + len] = c - d;
            }
        }
    }
}

"""
    # replace the accepted loop (header + rescale block) with paired + tail
    # The loop body is header + a measured-result comment + the rescale block, so
    # locate it by span rather than by one composed literal.
    a = s.index(HEADER)
    b = s.index(RESCALE, a)
    end = s.index("\n    }\n}", b) + len("\n    }\n}")
    accepted_body = s[a:end]
    assert accepted_body.startswith(HEADER) and accepted_body.endswith(RESCALE + "\n    }\n}"), \
        "unexpected fwht_rows body shape: " + repr(accepted_body[-30:])
    new_body = PAIRED + TAIL_LOOP
    s2 = s.replace("static ND_HOT void fwht_rows", helper + "static ND_HOT void fwht_rows", 1)
    out.write_text(s2.replace(accepted_body, new_body, 1))
    print(f"wrote {out} (fw2pair, paired loop + odd tail + shared fw_scale)")

for which in sys.argv[1:]:
    if which == "cond2":
        make_cond(2, pathlib.Path('/tmp/ndm_cond2.c'))
    elif which == "cond4":
        make_cond(4, pathlib.Path('/tmp/ndm_cond4.c'))
    elif which == "fw2pair":
        make_fw2pair(pathlib.Path('/tmp/ndq_fw2pair.c'))
        # this lane's model file must be the accepted one
        pathlib.Path('/tmp/ndm_fw2pair.c').write_text(MODEL.read_text())
    else:
        raise SystemExit("unknown variant " + which)
