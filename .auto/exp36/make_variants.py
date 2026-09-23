#!/usr/bin/env python3
"""Experiment 36 - unroll the ZCRMS *emit* loops (the shape that actually paid).

Run #362 shipped the only instruction-level win this window found: the per-group
rescale in `fwht_rows` unrolled by 4, +0.332 %, and run #365 showed the *other*
lever in the same class - const/alias hoisting - is worth nothing. So the shape
that matters is specific: a loop of independent per-element arithmetic that the
compiler walks one element at a time.

Two loops in nd_model.c match it exactly, and both are the emit half of a
ZCRMSNorm:

    out[i] = (1.0f + s[i]) * x[i] * inv;

`zcsplit_rows` is the big mass - `zcrms()` is called three times per layer
(nd_model.c:1813/1815/1829) over d_model=768, so ~18,432 elements per decode
token. `zcrms_head_rows` is the per-head version (12 q + 2 kv heads x head_dim,
twice per layer).

Bit-exactness is by construction and it is narrow on purpose: the emit loop has
one independent expression per element, so grouping four of them changes no
value, no operand and no order. The **sum-of-squares loop in the same functions
is deliberately left alone** - `ss += v[i] * v[i]` is an accumulation, and
reassociating it would change the norm and therefore the model. That distinction
is the whole candidate, so it is written in the code comment too.
"""
import pathlib

SRC = pathlib.Path("engine/src/nd_model.c")
body = SRC.read_text()

EMIT_DOC = """    /* Unrolled by 4: the emit is one independent expression per element, so
     * grouping four of them moves no value, operand or order - bit-exact by
     * construction. The sum-of-squares above is NOT unrolled for the same
     * reason it must not be: `ss += v[i] * v[i]` is an accumulation and
     * reassociating it changes the norm. Same lever that shipped +0.332 % on the
     * FWHT rescale in run #362; run #365 showed aliasing/const-hoisting is not
     * the same lever and is worth zero here. */
"""

SPLIT_OLD = """    for (i = lo; i < hi; i++)
        c->out[i] = (1.0f + c->s[i]) * c->x[i] * c->inv;
}"""
SPLIT_NEW = EMIT_DOC + """    float  *restrict out = c->out;
    const float *restrict s = c->s;
    const float *restrict x = c->x;
    const float inv = c->inv;
    uint32_t  j = lo;
    for (; j + 3u < hi; j += 4u) {
        float a0 = s[j], a1 = s[j + 1u], a2 = s[j + 2u], a3 = s[j + 3u];
        float b0 = x[j], b1 = x[j + 1u], b2 = x[j + 2u], b3 = x[j + 3u];
        out[j]           = (1.0f + a0) * b0 * inv;
        out[j + 1u]      = (1.0f + a1) * b1 * inv;
        out[j + 2u]      = (1.0f + a2) * b2 * inv;
        out[j + 3u]      = (1.0f + a3) * b3 * inv;
    }
    for (; j < hi; j++) out[j] = (1.0f + s[j]) * x[j] * inv;
}"""

HEAD_OLD = """            for (i = 0; i < dim; i++)
                v[i] = (1.0f + c->s[i]) * v[i] * inv;
        }"""
HEAD_NEW = EMIT_DOC + """            {
                uint32_t j = 0u;
                for (; j + 3u < dim; j += 4u) {
                    float v0 = v[j], v1 = v[j + 1u], v2 = v[j + 2u], v3 = v[j + 3u];
                    float s0 = c->s[j], s1 = c->s[j + 1u], s2 = c->s[j + 2u], s3 = c->s[j + 3u];
                    v[j]      = (1.0f + s0) * v0 * inv;
                    v[j + 1u] = (1.0f + s1) * v1 * inv;
                    v[j + 2u] = (1.0f + s2) * v2 * inv;
                    v[j + 3u] = (1.0f + s3) * v3 * inv;
                }
                for (; j < dim; j++) v[j] = (1.0f + c->s[j]) * v[j] * inv;
            }
        }"""

# kv_store_int8 is deliberately NOT generated here. Its Markstein fixup is subtle, and a
# hand transcription could be wrong in a value range the frozen prompts never reach -
# exactly run #230's nd_expf class, which every golden waved through. When a board is
# free for it, generate it by substituting the shipped loop body mechanically.

OUT = {"zcsplit": None, "zchead": None}
OUT["zcsplit"] = body.replace(SPLIT_OLD, SPLIT_NEW, 1)
OUT["zchead"] = body.replace(HEAD_OLD, HEAD_NEW, 1)
assert OUT["zcsplit"] != body and OUT["zchead"] != body, "emit loops changed shape"

for name, txt in OUT.items():
    assert txt != body, name
    pathlib.Path(f"/tmp/ndm_{name}.c").write_text(txt)
    print(f"wrote /tmp/ndm_{name}.c")
