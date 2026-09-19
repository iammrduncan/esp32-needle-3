#include "nd_model.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "nd_quant.h"

uint64_t nd_prof[ND_P_COUNT];

#ifdef ND_PROFILE
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#define ND_NOW_US() ((uint64_t)esp_timer_get_time())
#else
#include <time.h>
#define ND_NOW_US() ((uint64_t)(clock() * (1000000.0 / CLOCKS_PER_SEC)))
#endif
#define ND_T0(v) uint64_t v = ND_NOW_US()
#define ND_T1(v, slot) nd_prof[slot] += ND_NOW_US() - (v)
#else
#define ND_T0(v) ((void)0)
#define ND_T1(v, slot) ((void)0)
#endif

#define ND_EPS      1e-6f
#define ND_EG_HIST  16      /* >= (taps-1)*dilation + 1 = 10, power of two */
#define ND_SINKHORN 20

/* Engram hash constants (architecture.py). */
#define ND_EG_SEED  0x9E3779B9u
#define ND_EG_PRIME 0x01000193u

/* ------------------------------------------------------------------ utils */

static float sigmoidf_(float x)
{
    if (x >= 0.0f) {
        float e = nd_expf(-x);
        return 1.0f / (1.0f + e);
    }
    {
        float e = nd_expf(x);
        return e / (1.0f + e);
    }
}

/* FP16 tensors are read element-wise; they are small (norm scales, gates). */
static float fp16_get(const nd_model *m, const nd_tensor *t, size_t i)
{
    const uint16_t *p = (const uint16_t *)nd_cact_data(&m->c, t);
    return nd_f16(p[i]);
}

typedef struct { const float *s, *x; float *out; float inv; } zcsplit_ctx;

/* zcrms emit pass over a column range. Elementwise once inv is known; the
 * sum-of-squares reduction stays on one core (splitting it re-associates). */
static ND_HOT void zcsplit_rows(void *vc, uint32_t b0, uint32_t b1)
{
    const zcsplit_ctx *c = (const zcsplit_ctx *)vc;
    uint32_t i, lo = b0 * 128, hi = b1 * 128;
    for (i = lo; i < hi; i++)
        c->out[i] = (1.0f + c->s[i]) * c->x[i] * c->inv;
}

typedef struct { float *dst; const float *lane, *hpre;
                 uint32_t n, dm; } lanepre_ctx;

/* u = sum_j hpre[j] * lane[j] over a column range: columns are independent and
 * each keeps its ascending j order, so values are unchanged. */
static ND_HOT void lanepre_rows(void *vc, uint32_t b0, uint32_t b1)
{
    const lanepre_ctx *c = (const lanepre_ctx *)vc;
    uint32_t i, j, lo = b0 * 128, hi = b1 * 128;
    for (i = lo; i < hi; i++) {
        float acc = 0.0f;
        for (j = 0; j < c->n; j++)
            acc += c->hpre[j] * c->lane[j * c->dm + i];
        c->dst[i] = acc;
    }
}

/* x * rsqrt(mean(x^2) + eps) */
/* restrict on all three: the hot callers pass disjoint scratch (n1/n2/nx are
 * separate allocations), and without it the compiler must assume out may alias
 * x and cannot keep the sum-of-squares load stream independent of the store. */
static void rms_unit(const float *restrict x, uint32_t n,
                     float *restrict out)
{
    float    ss = 0.0f;
    uint32_t i;
    for (i = 0; i < n; i++)
        ss += x[i] * x[i];
    {
        float inv = 1.0f / sqrtf(ss / (float)n + ND_EPS);
        for (i = 0; i < n; i++)
            out[i] = x[i] * inv;
    }
}

/* ZCRMSNorm: (1 + scale) * x / sqrt(mean(x^2) + eps) */
/* fp16 -> fp32 into a caller buffer. Only called at open: the hot loops below
 * must never convert per element. */
static void fp16_row(const uint16_t *h, float *dst, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        dst[i] = nd_f16(h[i]);
}

static void zcrms(const nd_model *m, const float *restrict s,
                  const float *restrict x, uint32_t n, float *restrict out)
{
    float           ss = 0.0f;
    uint32_t        i;

    /* The scale arrives already float32: it is one of the staged per-layer
     * tensors, so the multiply loop never unpacks a half per element. */
    for (i = 0; i < n; i++)
        ss += x[i] * x[i];
    {
        float inv = 1.0f / sqrtf(ss / (float)n + ND_EPS);
        if (n >= 512) {
            zcsplit_ctx zc = { s, x, out, inv };
            nd_parallel_rows(zcsplit_rows, &zc, n / 128);
        } else {
            for (i = 0; i < n; i++)
                out[i] = (1.0f + s[i]) * x[i] * inv;
        }
    }
}

/* In-place per-head ZCRMSNorm over head_dim, shared scale across heads. */
static void zcrms_heads(const nd_model *m, const float *s, float *x,
                        uint32_t nheads, uint32_t dim)
{
    uint32_t        h, i;

    for (h = 0; h < nheads; h++) {
        float *v  = x + (size_t)h * dim;
        float  ss = 0.0f;
        for (i = 0; i < dim; i++)
            ss += v[i] * v[i];
        {
            float inv = 1.0f / sqrtf(ss / (float)dim + ND_EPS);
            for (i = 0; i < dim; i++)
                v[i] = (1.0f + s[i]) * v[i] * inv;
        }
    }
}

/* GPT-NeoX style half-split rotary, against cos/sin precomputed once per
 * token (the position is the same for all 27 layers). */
static void apply_rope(const nd_model *m, float *x, uint32_t nheads, uint32_t dim)
{
    uint32_t half = dim / 2;
    uint32_t h, i;

    for (h = 0; h < nheads; h++) {
        float *v = x + (size_t)h * dim;
        for (i = 0; i < half; i++) {
            float c  = m->rope_cos[i];
            float s  = m->rope_sin[i];
            float x1 = v[i];
            float x2 = v[i + half];
            v[i]        = x1 * c - x2 * s;
            v[i + half] = x2 * c + x1 * s;
        }
    }
}

typedef struct { nd_model *m; const float *hres, *hpost;
                 uint32_t n, dm; } lanemix_ctx;

static ND_HOT void lanemix_rows(void *vc, uint32_t k0, uint32_t k1)
{
    const lanemix_ctx *c = (const lanemix_ctx *)vc;
    uint32_t k, j, i;
    for (k = k0; k < k1; k++) {
        float    *dst = c->m->lane_next + (size_t)k * c->dm;
        for (i = 0; i < c->dm; i++)
            dst[i] = c->hpost[k] * c->m->u[i];
        for (j = 0; j < c->n; j++) {
            const float *src = c->m->lane + (size_t)j * c->dm;
            float        w   = c->hres[k * c->n + j];
            for (i = 0; i < c->dm; i++)
                dst[i] += w * src[i];
        }
    }
}

/* Doubly-stochastic normalisation of a lanes x lanes matrix, in log space. */
static void sinkhorn(float *a, uint32_t n)
{
    uint32_t it, i, j;

    for (it = 0; it < ND_SINKHORN; it++) {
        for (i = 0; i < n; i++) {           /* rows */
            float mx = a[i * n];
            float sum = 0.0f;
            for (j = 1; j < n; j++)
                if (a[i * n + j] > mx) mx = a[i * n + j];
            for (j = 0; j < n; j++)
                sum += nd_expf(a[i * n + j] - mx);
            {
                float lse = mx + logf(sum);
                for (j = 0; j < n; j++)
                    a[i * n + j] -= lse;
            }
        }
        for (j = 0; j < n; j++) {           /* columns */
            float mx = a[j];
            float sum = 0.0f;
            for (i = 1; i < n; i++)
                if (a[i * n + j] > mx) mx = a[i * n + j];
            for (i = 0; i < n; i++)
                sum += nd_expf(a[i * n + j] - mx);
            {
                float lse = mx + logf(sum);
                for (i = 0; i < n; i++)
                    a[i * n + j] -= lse;
            }
        }
    }
    for (i = 0; i < n * n; i++)
        a[i] = nd_expf(a[i]);
}

/* ------------------------------------------------------------------- open */

static void bind_layer(nd_model *m, uint32_t i)
{
    uint32_t   b = 1 + i * (m->c.h.qkv_conv_taps ? 27 : 24);
    nd_layer  *L = &m->layer[i];
    nd_tensor *slots[27] = {
        &L->norm_in, &L->q_proj, &L->k_proj, &L->v_proj,
        &L->q_taps, &L->k_taps, &L->v_taps,
        &L->q_norm, &L->k_norm, &L->gate_proj, &L->out_proj,
        &L->post_norm, &L->attn_gate, &L->pre_hada,
        &L->d1, &L->d2, &L->b2, &L->d3, &L->d4,
        &L->w1a, &L->w1b, &L->w2a, &L->w2b, &L->w3a, &L->w3b,
        &L->cond_v, &L->cond_u
    };
    uint32_t k;
    for (k = 0; k < 27; k++)
        nd_cact_tensor(&m->c, b + k, slots[k]);
}

int nd_model_open(nd_model *m, const void *blob, size_t size)
{
    uint32_t base, s, i;
    int      rc;

    memset(m, 0, sizeof(*m));
    rc = nd_cact_open(&m->c, blob, size);
    if (rc != 0)
        return rc;

    m->d_model    = m->c.h.d_model;
    m->n_layers   = m->c.h.num_layers;
    m->n_heads    = m->c.h.num_heads;
    m->n_kv_heads = m->c.h.num_kv_heads;
    m->qk_head_dim = m->c.h.qk_head_dim;
    m->v_head_dim = m->c.h.v_head_dim;
    m->attn_dim   = m->n_heads * m->v_head_dim;
    m->k_dim      = m->n_kv_heads * m->qk_head_dim;
    m->v_dim      = m->n_kv_heads * m->v_head_dim;
    m->lanes      = m->c.h.mhc_lanes;
    m->vocab      = m->c.h.vocab_size;
    m->window     = m->c.h.max_seq_len;
    m->n_sites    = m->c.h.num_sites;

    if (m->lanes > ND_MAX_LANES || m->n_sites > ND_MAX_SITES)
        return -10;

    /* Canonical positional layout, verified against the shipped blob. */
    nd_cact_tensor(&m->c, 0, &m->embedding);

    m->layer = (nd_layer *)ND_ALLOC(sizeof(nd_layer) * m->n_layers);
    if (!m->layer)
        return -11;
    for (i = 0; i < m->n_layers; i++)
        bind_layer(m, i);

    /* Convert the multiply-read fp16 weights once. Layout is [slot][layer], so
     * a lookup is a name, never an offset into someone else's buffer. */
    memset(m->fp16_slot, 0, sizeof(m->fp16_slot));
    {
        static const int SLOT[] = { 19, 20, 21, 22, 23, 24,   /* w1a..w3b */
                                    15, 16, 17, 18,           /* d2 b2 d3 d4 */
                                    26,                       /* cond_u */
                                    4, 5, 6,                  /* q/k/v taps */
                                    14, 25,                   /* d1, cond_v */
                                    0, 11, 13,                /* norm_in, post_norm, pre_hada */
                                    7, 8 };                   /* q_norm, k_norm */
        const uint32_t taps = m->c.h.qkv_conv_taps;
        uint32_t li;
        size_t   total = 0;
        int      f;

        for (f = 0; f < (int)(sizeof(SLOT) / sizeof(SLOT[0])); f++)
            for (li = 0; li < m->n_layers; li++) {
                nd_tensor t;
                nd_cact_tensor(&m->c, 1 + li * (taps ? 27 : 24) + SLOT[f], &t);
                total += t.nbytes / 2;
            }
        m->fp16_pool = (float *)ND_ALLOC(sizeof(float) * total);
        if (!m->fp16_pool)
            return -11;
        {
            float *p = m->fp16_pool;
            for (f = 0; f < (int)(sizeof(SLOT) / sizeof(SLOT[0])); f++)
                for (li = 0; li < m->n_layers; li++) {
                    nd_tensor        t;
                    const uint16_t  *h;
                    uint32_t         k, n;
                    nd_cact_tensor(&m->c, 1 + li * (taps ? 27 : 24) + SLOT[f], &t);
                    h = (const uint16_t *)nd_cact_data(&m->c, &t);
                    n = t.nbytes / 2;
                    m->fp16_slot[li][SLOT[f]] = p;
                    for (k = 0; k < n; k++)
                        p[k] = nd_f16(h[k]);
                    p += n;
                }
        }
    }

    base = 1 + m->n_layers * (m->c.h.qkv_conv_taps ? 27 : 24);
    nd_cact_tensor(&m->c, base + 0, &m->mhc_a_pre);
    nd_cact_tensor(&m->c, base + 1, &m->mhc_a_post);
    nd_cact_tensor(&m->c, base + 2, &m->mhc_a_res);
    nd_cact_tensor(&m->c, base + 3, &m->mhc_b_pre);
    nd_cact_tensor(&m->c, base + 4, &m->mhc_b_post);
    nd_cact_tensor(&m->c, base + 5, &m->mhc_b_res);
    nd_cact_tensor(&m->c, base + 6, &m->mhc_phi_pre);
    nd_cact_tensor(&m->c, base + 7, &m->mhc_phi_post);
    nd_cact_tensor(&m->c, base + 8, &m->mhc_phi_res);
    nd_cact_tensor(&m->c, base + 9, &m->hada_p1);
    nd_cact_tensor(&m->c, base + 10, &m->hada_p2);
    base += 11;

    for (s = 0; s < m->n_sites; s++) {
        nd_cact_tensor(&m->c, base + s * 4 + 0, &m->engram[s].tables);
        nd_cact_tensor(&m->c, base + s * 4 + 1, &m->engram[s].key_proj);
        nd_cact_tensor(&m->c, base + s * 4 + 2, &m->engram[s].value_proj);
        nd_cact_tensor(&m->c, base + s * 4 + 3, &m->engram[s].taps);
    }
    base += m->n_sites * 4;
    nd_cact_tensor(&m->c, base, &m->final_norm);

    /* The engram tap matrices are fp16 and read `taps` times per site per
     * token, so they join the staged-float set. */
    {
        uint32_t ss2;
        for (ss2 = 0; ss2 < m->n_sites; ss2++) {
            nd_tensor  t;
            uint32_t   k, nn;
            nd_cact_tensor(&m->c, base - m->n_sites * 4 + ss2 * 4 + 3, &t);
            nn = t.nbytes / 2;
            m->eg_taps_f[ss2] = (float *)ND_ALLOC(sizeof(float) * nn);
            if (!m->eg_taps_f[ss2])
                return -11;
            for (k = 0; k < nn; k++)
                m->eg_taps_f[ss2][k] =
                    nd_f16(((const uint16_t *)nd_cact_data(&m->c, &t))[k]);
        }
    }

    /* Probe heads, if this blob carries them. Layout after final_norm:
     * a manifest of H head codes (1 contrastive, 2 confidence), then H fixed
     * triples [probes, proj, bias]; the tokenizer is the final tensor. */
    {
        uint32_t after = base + 1;
        uint32_t extra = (m->c.n > after + 1) ? m->c.n - after - 1 : 0;

        if (extra >= 4) {
            nd_tensor manifest;
            uint32_t  heads, k;

            nd_cact_tensor(&m->c, after, &manifest);
            heads = (extra - 1) / 3;
            for (k = 0; k < heads && k < manifest.shape[0]; k++) {
                float code = fp16_get(m, &manifest, k);
                if (code > 1.5f && code < 2.5f) {   /* confidence */
                    nd_cact_tensor(&m->c, after + 1 + k * 3 + 0, &m->conf_probes);
                    nd_cact_tensor(&m->c, after + 1 + k * 3 + 1, &m->conf_proj);
                    nd_cact_tensor(&m->c, after + 1 + k * 3 + 2, &m->conf_bias);
                    /* proj is (1, n_probes*d_model); probes is (P, d_model) */
                    if (m->conf_probes.shape[1] == m->d_model &&
                        m->conf_proj.shape[1] ==
                            m->conf_probes.shape[0] * m->d_model) {
                        m->n_probes = m->conf_probes.shape[0];
                        m->has_conf = 1;
                    }
                }
            }
        }
    }

    /* The tokenizer is the single RAW tensor, last in canon order. */
    for (i = m->c.n; i-- > 0;) {
        nd_tensor t;
        nd_cact_tensor(&m->c, i, &t);
        if (t.dtype == ND_DT_RAW) {
            if (nd_tok_init(&m->tok, nd_cact_data(&m->c, &t), (size_t)t.nbytes) == 0)
                m->tok_ready = 1;
            break;
        }
    }

    /* ---- buffers ---- */
    {
        uint32_t dm  = m->d_model;
        uint32_t nl  = m->lanes * dm;
        size_t   kn = (size_t)m->n_layers * m->window * m->k_dim;
        size_t   vn = (size_t)m->n_layers * m->window * m->v_dim;
        size_t   scn = (size_t)m->n_layers * m->window * m->n_kv_heads;
        uint32_t taps = m->c.h.qkv_conv_taps;
        uint32_t hn = m->c.h.hada_n;

        m->lane      = (float *)ND_ALLOC_FAST(sizeof(float) * nl);
        m->lane_next = (float *)ND_ALLOC_FAST(sizeof(float) * nl);
        m->nx        = (float *)ND_ALLOC_FAST(sizeof(float) * nl);
        m->xh        = (float *)ND_ALLOC_FAST(sizeof(float) * (nl > dm ? nl : dm));
        m->lut       = (float *)ND_ALLOC_FAST(sizeof(float) * nd_cq_lut_floats(dm));

        m->u         = (float *)ND_ALLOC_FAST(sizeof(float) * dm);
        m->ublk      = (float *)ND_ALLOC_FAST(sizeof(float) * dm);
        uint32_t dpow = 1;
        while (dpow < dm)
            dpow <<= 1;

        m->n1        = (float *)ND_ALLOC_FAST(sizeof(float) * dm);
        m->n2        = (float *)ND_ALLOC_FAST(sizeof(float) * dpow);
        m->y         = (float *)ND_ALLOC_FAST(sizeof(float) * dm);
        m->tmp       = (float *)ND_ALLOC_FAST(sizeof(float) * dm);
        m->tmp2      = (float *)ND_ALLOC_FAST(sizeof(float) * dm);
        m->q         = (float *)ND_ALLOC_FAST(sizeof(float) * m->n_heads * m->qk_head_dim);
        m->kbuf      = (float *)ND_ALLOC_FAST(sizeof(float) * m->k_dim);
        m->vbuf      = (float *)ND_ALLOC_FAST(sizeof(float) * m->v_dim);
        m->gate      = (float *)ND_ALLOC_FAST(sizeof(float) * m->attn_dim);
        m->attn      = (float *)ND_ALLOC_FAST(sizeof(float) * m->attn_dim);
        m->aout      = (float *)ND_ALLOC_FAST(sizeof(float) * dm);
        m->rope_inv  = (float *)ND_ALLOC_FAST(sizeof(float) * (m->qk_head_dim / 2));
        m->rope_cos  = (float *)ND_ALLOC_FAST(sizeof(float) * (m->qk_head_dim / 2));
        m->rope_sin  = (float *)ND_ALLOC_FAST(sizeof(float) * (m->qk_head_dim / 2));
        m->q_hist    = (float *)ND_ALLOC(sizeof(float) * m->n_layers * taps * m->n_heads * m->qk_head_dim);
        m->k_hist    = (float *)ND_ALLOC(sizeof(float) * m->n_layers * taps * m->k_dim);
        m->v_hist    = (float *)ND_ALLOC(sizeof(float) * m->n_layers * taps * m->v_dim);
        m->hada_a    = (float *)ND_ALLOC_FAST(sizeof(float) * hn);
        m->hada_b    = (float *)ND_ALLOC_FAST(sizeof(float) * hn);
        m->hada_c    = (float *)ND_ALLOC_FAST(sizeof(float) * hn);
        m->scale_row = (float *)ND_ALLOC_FAST(sizeof(float) * hn);
        m->eg_k      = (float *)ND_ALLOC(sizeof(float) * m->n_sites * dm);
        m->eg_v      = (float *)ND_ALLOC(sizeof(float) * m->n_sites * dm);
        m->eg_hist   = (float *)ND_ALLOC(sizeof(float) * m->n_sites * ND_EG_HIST * dm);
        m->logits    = (float *)ND_ALLOC(sizeof(float) * m->vocab);
        m->row       = (float *)ND_ALLOC_FAST(sizeof(float) * (dm > 128 ? dm : 128));
        m->scale_f   = (float *)ND_ALLOC_FAST(sizeof(float) * dm);
        m->k_cache   = (int8_t *)ND_ALLOC(kn);
        m->v_cache   = (int8_t *)ND_ALLOC(vn);
        m->k_scale   = (float *)ND_ALLOC(sizeof(float) * scn);
        m->v_scale   = (float *)ND_ALLOC(sizeof(float) * scn);

        if (m->has_conf) {
            m->probes_f = (float *)ND_ALLOC_FAST(sizeof(float) * m->n_probes * dm);
            m->pool_acc = (float *)ND_ALLOC_FAST(sizeof(float) * m->n_probes * dm);
            m->pool_max = (float *)ND_ALLOC_FAST(sizeof(float) * m->n_probes);
            m->pool_sum = (float *)ND_ALLOC_FAST(sizeof(float) * m->n_probes);
            if (!m->probes_f || !m->pool_acc || !m->pool_max || !m->pool_sum) {
                nd_model_close(m);
                return -13;
            }
        }

        if (!m->lane || !m->lane_next || !m->nx || !m->xh || !m->lut || !m->u || !m->ublk ||
            !m->n1 || !m->n2 || !m->y || !m->tmp || !m->tmp2 || !m->q ||
            !m->kbuf || !m->vbuf || !m->gate || !m->attn || !m->aout ||
            !m->rope_inv || !m->rope_cos || !m->rope_sin ||
            !m->q_hist || !m->k_hist || !m->v_hist ||
            !m->hada_a || !m->hada_b || !m->hada_c || !m->scale_row ||
            !m->eg_k || !m->eg_v || !m->eg_hist || !m->logits ||
            !m->row || !m->scale_f || !m->k_cache || !m->v_cache || !m->k_scale || !m->v_scale) {
            nd_model_close(m);
            return -12;
        }
    }

    /* Expand the confidence probes to float32 once; pool_cell would otherwise
     * convert 8*512 halves per hidden cell, ~114K conversions per token. */
    if (m->has_conf) {
        const uint16_t *pr = (const uint16_t *)nd_cact_data(&m->c, &m->conf_probes);
        uint32_t        k;
        for (k = 0; k < m->n_probes * m->d_model; k++)
            m->probes_f[k] = nd_f16(pr[k]);
    }

    /* Rotary inverse frequencies: 1/theta^(2i/head_dim). */
    for (i = 0; i < m->qk_head_dim / 2; i++)
        m->rope_inv[i] = 1.0f / powf(m->c.h.rope_theta,
                                     (float)(2 * i) / (float)m->qk_head_dim);

    nd_model_reset(m);
    return 0;
}

void nd_model_close(nd_model *m)
{
    if (!m)
        return;
    if (m->tok_ready)
        nd_tok_free(&m->tok);
    ND_FREE(m->layer);
    ND_FREE(m->fp16_pool);
    ND_FREE(m->scale_f);
    { uint32_t z; for (z = 0; z < ND_MAX_SITES; z++) ND_FREE(m->eg_taps_f[z]); }
    ND_FREE(m->lane); ND_FREE(m->lane_next); ND_FREE(m->nx); ND_FREE(m->xh);
    ND_FREE(m->u); ND_FREE(m->ublk); ND_FREE(m->n1); ND_FREE(m->n2);
    ND_FREE(m->y); ND_FREE(m->tmp); ND_FREE(m->tmp2);
    ND_FREE(m->q); ND_FREE(m->kbuf); ND_FREE(m->vbuf); ND_FREE(m->gate);
    ND_FREE(m->attn); ND_FREE(m->aout);
    ND_FREE(m->q_hist); ND_FREE(m->k_hist); ND_FREE(m->v_hist);
    ND_FREE(m->hada_a); ND_FREE(m->hada_b); ND_FREE(m->hada_c);
    ND_FREE(m->scale_row);
    ND_FREE(m->rope_inv); ND_FREE(m->rope_cos); ND_FREE(m->rope_sin);
    ND_FREE(m->eg_k); ND_FREE(m->eg_v); ND_FREE(m->eg_hist);
    ND_FREE(m->logits); ND_FREE(m->row);
    ND_FREE(m->k_cache); ND_FREE(m->v_cache);
    ND_FREE(m->k_scale); ND_FREE(m->v_scale);
    ND_FREE(m->snap_eg_hist);
    ND_FREE(m->snap_q_hist); ND_FREE(m->snap_k_hist); ND_FREE(m->snap_v_hist);
    ND_FREE(m->snap_pool_acc); ND_FREE(m->snap_pool_max); ND_FREE(m->snap_pool_sum);
    ND_FREE(m->probes_f);
    ND_FREE(m->pool_acc); ND_FREE(m->pool_max); ND_FREE(m->pool_sum);
    memset(m, 0, sizeof(*m));
}

/* Cache slot for an absolute position. Sinks occupy the first n_sink slots
 * permanently; everything after rings through the remainder. */
static uint32_t kv_slot(const nd_model *m, uint32_t p)
{
    if (p < m->n_sink)
        return p;
    return m->n_sink + (p - m->n_sink) % (m->window - m->n_sink);
}

/* Recent-context slots kept free no matter how long the pinned prefix is. */
#define ND_MIN_RECENT 64

void nd_model_set_sink(nd_model *m, uint32_t n)
{
    /* The prefix must be pinned *entirely*: a partially pinned prefix leaves
     * its tail in the ring, where a long generation wraps around and silently
     * overwrites part of the tool schema. */
    uint32_t cap = (m->window > ND_MIN_RECENT) ? m->window - ND_MIN_RECENT : 0;
    m->n_sink = (n > cap) ? cap : n;
}

/* Fold one hidden cell into the running softmax pool.
 *
 * probe_pool() softmaxes probe·cell/sqrt(d) over every cell of every token,
 * then takes the weighted mean. Streaming it with a running max keeps the
 * result identical to the batch computation while holding only the
 * accumulator, which is what makes this affordable on the ESP32. */
static void pool_cell(nd_model *m, const float *cell)
{
    const float    *probes = m->probes_f;
    uint32_t        dm     = m->d_model;
    float           inv_s  = 1.0f / sqrtf((float)dm);
    uint32_t        k, i;

    for (k = 0; k < m->n_probes; k++) {
        const float    *pr = probes + (size_t)k * dm;
        float          *ac = m->pool_acc + (size_t)k * dm;
        float           z = 0.0f, w;

        for (i = 0; i < dm; i++)
            z += pr[i] * cell[i];
        z *= inv_s;

        if (z > m->pool_max[k]) {
            float rescale = expf(m->pool_max[k] - z);
            for (i = 0; i < dm; i++)
                ac[i] *= rescale;
            m->pool_sum[k] *= rescale;
            m->pool_max[k] = z;
        }
        w = expf(z - m->pool_max[k]);
        m->pool_sum[k] += w;
        for (i = 0; i < dm; i++)
            ac[i] += w * cell[i];
    }
}

float nd_model_confidence(nd_model *m)
{
    const uint16_t *proj;
    float           logit;
    uint32_t        k, i;

    if (!m->has_conf)
        return -1.0f;

    proj  = (const uint16_t *)nd_cact_data(&m->c, &m->conf_proj);
    logit = nd_f16(*(const uint16_t *)nd_cact_data(&m->c, &m->conf_bias));

    for (k = 0; k < m->n_probes; k++) {
        const float *ac  = m->pool_acc + (size_t)k * m->d_model;
        float        inv = (m->pool_sum[k] > 0.0f) ? 1.0f / m->pool_sum[k] : 0.0f;
        for (i = 0; i < m->d_model; i++)
            logit += nd_f16(proj[(size_t)k * m->d_model + i]) * ac[i] * inv;
    }
    return sigmoidf_(logit);
}

int nd_model_snapshot(nd_model *m)
{
    size_t egn = (size_t)m->n_sites * ND_EG_HIST * m->d_model;
    size_t qn = (size_t)m->n_layers * m->c.h.qkv_conv_taps * m->n_heads * m->qk_head_dim;
    size_t kn = (size_t)m->n_layers * m->c.h.qkv_conv_taps * m->k_dim;
    size_t vn = (size_t)m->n_layers * m->c.h.qkv_conv_taps * m->v_dim;

    if (!m->snap_eg_hist) {
        m->snap_eg_hist = (float *)ND_ALLOC(sizeof(float) * egn);
        m->snap_q_hist = (float *)ND_ALLOC(sizeof(float) * qn);
        m->snap_k_hist = (float *)ND_ALLOC(sizeof(float) * kn);
        m->snap_v_hist = (float *)ND_ALLOC(sizeof(float) * vn);
        if (!m->snap_eg_hist || !m->snap_q_hist || !m->snap_k_hist || !m->snap_v_hist)
            return -1;
        if (m->has_conf) {
            m->snap_pool_acc = (float *)ND_ALLOC_FAST(sizeof(float) * m->n_probes * m->d_model);
            m->snap_pool_max = (float *)ND_ALLOC_FAST(sizeof(float) * m->n_probes);
            m->snap_pool_sum = (float *)ND_ALLOC_FAST(sizeof(float) * m->n_probes);
            if (!m->snap_pool_acc || !m->snap_pool_max || !m->snap_pool_sum)
                return -1;
        }
    }

    nd_model_set_sink(m, m->pos);
    m->snap_pos    = m->pos;
    m->snap_eg_pos = m->eg_pos;
    memcpy(m->snap_hist, m->hist, sizeof(m->hist));
    memcpy(m->snap_eg_hist, m->eg_hist, sizeof(float) * egn);
    memcpy(m->snap_q_hist, m->q_hist, sizeof(float) * qn);
    memcpy(m->snap_k_hist, m->k_hist, sizeof(float) * kn);
    memcpy(m->snap_v_hist, m->v_hist, sizeof(float) * vn);
    if (m->has_conf) {
        memcpy(m->snap_pool_acc, m->pool_acc,
               sizeof(float) * m->n_probes * m->d_model);
        memcpy(m->snap_pool_max, m->pool_max, sizeof(float) * m->n_probes);
        memcpy(m->snap_pool_sum, m->pool_sum, sizeof(float) * m->n_probes);
    }
    m->has_snap = 1;
    return 0;
}

void nd_model_rewind(nd_model *m)
{
    size_t egn = (size_t)m->n_sites * ND_EG_HIST * m->d_model;
    size_t qn = (size_t)m->n_layers * m->c.h.qkv_conv_taps * m->n_heads * m->qk_head_dim;
    size_t kn = (size_t)m->n_layers * m->c.h.qkv_conv_taps * m->k_dim;
    size_t vn = (size_t)m->n_layers * m->c.h.qkv_conv_taps * m->v_dim;

    if (!m->has_snap) {
        nd_model_reset(m);
        return;
    }
    m->pos    = m->snap_pos;
    m->eg_pos = m->snap_eg_pos;
    memcpy(m->hist, m->snap_hist, sizeof(m->hist));
    memcpy(m->eg_hist, m->snap_eg_hist, sizeof(float) * egn);
    memcpy(m->q_hist, m->snap_q_hist, sizeof(float) * qn);
    memcpy(m->k_hist, m->snap_k_hist, sizeof(float) * kn);
    memcpy(m->v_hist, m->snap_v_hist, sizeof(float) * vn);
    if (m->has_conf) {
        memcpy(m->pool_acc, m->snap_pool_acc,
               sizeof(float) * m->n_probes * m->d_model);
        memcpy(m->pool_max, m->snap_pool_max, sizeof(float) * m->n_probes);
        memcpy(m->pool_sum, m->snap_pool_sum, sizeof(float) * m->n_probes);
    }
}

struct nd_prefix {
    const nd_model *owner;
    uint32_t pos, sink, eg_pos, hist[8];
    unsigned char data[];
};

/* Copy all persistent prefix state, including KV slots that can be overwritten
 * by a different schema. Existing snapshot buffers are reused on restore. */
static size_t prefix_copy(nd_model *m, unsigned char *data, int restore)
{
    size_t offset = 0;
    size_t sc = (size_t)m->n_layers * m->window * m->n_kv_heads;
#define PREFIX_BUFFER(ptr, bytes) do { \
    size_t count = (bytes); \
    if (data && count) { \
        if (restore) memcpy((ptr), data + offset, count); \
        else memcpy(data + offset, (ptr), count); \
    } \
    offset += count; \
} while (0)
    PREFIX_BUFFER(m->k_cache, (size_t)m->n_layers * m->window * m->k_dim);
    PREFIX_BUFFER(m->v_cache, (size_t)m->n_layers * m->window * m->v_dim);
    PREFIX_BUFFER(m->k_scale, sc * sizeof(float));
    PREFIX_BUFFER(m->v_scale, sc * sizeof(float));
    PREFIX_BUFFER(m->snap_eg_hist, (size_t)m->n_sites * ND_EG_HIST * m->d_model * sizeof(float));
    PREFIX_BUFFER(m->snap_q_hist, (size_t)m->n_layers * m->c.h.qkv_conv_taps * m->n_heads * m->qk_head_dim * sizeof(float));
    PREFIX_BUFFER(m->snap_k_hist, (size_t)m->n_layers * m->c.h.qkv_conv_taps * m->k_dim * sizeof(float));
    PREFIX_BUFFER(m->snap_v_hist, (size_t)m->n_layers * m->c.h.qkv_conv_taps * m->v_dim * sizeof(float));
    if (m->has_conf) {
        PREFIX_BUFFER(m->snap_pool_acc, (size_t)m->n_probes * m->d_model * sizeof(float));
        PREFIX_BUFFER(m->snap_pool_max, m->n_probes * sizeof(float));
        PREFIX_BUFFER(m->snap_pool_sum, m->n_probes * sizeof(float));
    }
#undef PREFIX_BUFFER
    return offset;
}

nd_prefix *nd_model_prefix_save(nd_model *m)
{
    if (!m->has_snap || m->pos != m->snap_pos) return NULL;
    nd_prefix *p = (nd_prefix *)ND_ALLOC(sizeof(*p) + prefix_copy(m, NULL, 0));
    if (!p) return NULL;
    p->owner = m; p->pos = m->snap_pos; p->sink = m->n_sink;
    p->eg_pos = m->snap_eg_pos;
    memcpy(p->hist, m->snap_hist, sizeof(p->hist));
    prefix_copy(m, p->data, 0);
    return p;
}

int nd_model_prefix_restore(nd_model *m, const nd_prefix *p)
{
    if (!p || p->owner != m) return -1;
    prefix_copy(m, (unsigned char *)p->data, 1);
    m->snap_pos = p->pos; m->snap_eg_pos = p->eg_pos; m->n_sink = p->sink;
    memcpy(m->snap_hist, p->hist, sizeof(p->hist));
    m->has_snap = 1;
    nd_model_rewind(m);
    return 0;
}

void nd_model_prefix_free(nd_prefix *p) { ND_FREE(p); }

void nd_model_reset(nd_model *m)
{
    m->pos    = 0;
    m->n_sink = 0;
    m->has_snap = 0;
    m->eg_pos = 0;
    if (m->has_conf) {
        uint32_t k;
        memset(m->pool_acc, 0, sizeof(float) * m->n_probes * m->d_model);
        for (k = 0; k < m->n_probes; k++) {
            m->pool_max[k] = -INFINITY;
            m->pool_sum[k] = 0.0f;
        }
    }
    memset(m->hist, 0, sizeof(m->hist));
    if (m->eg_hist)
        memset(m->eg_hist, 0,
               sizeof(float) * m->n_sites * ND_EG_HIST * m->d_model);
    if (m->q_hist)
        memset(m->q_hist, 0, sizeof(float) * m->n_layers * m->c.h.qkv_conv_taps * m->n_heads * m->qk_head_dim);
    if (m->k_hist)
        memset(m->k_hist, 0, sizeof(float) * m->n_layers * m->c.h.qkv_conv_taps * m->k_dim);
    if (m->v_hist)
        memset(m->v_hist, 0, sizeof(float) * m->n_layers * m->c.h.qkv_conv_taps * m->v_dim);
}

/* ----------------------------------------------------------------- engram */

/* k/v for the current token at every engram site. */
static void engram_step(nd_model *m, uint32_t token)
{
    uint32_t orders = m->c.h.num_orders;
    uint32_t heads  = m->c.h.engram_tables / (orders ? orders : 1);
    uint32_t slots  = m->c.h.engram_slots;
    uint32_t sub    = m->c.h.engram_sub_dim;
    uint32_t dil    = m->c.h.engram_dilation;
    uint32_t taps   = m->c.h.engram_conv_taps;
    uint32_t dm     = m->d_model;
    uint32_t s, oi, h, j;

    /* Shift the token history: hist[0] is the current token. */
    for (j = 7; j > 0; j--)
        m->hist[j] = m->hist[j - 1];
    m->hist[0] = token;

    for (s = 0; s < m->n_sites; s++) {
        float   *e = m->xh; /* reused: engram e is d_model long, xh is >= that */
        uint32_t table = 0;

        for (oi = 0; oi < orders; oi++) {
            uint32_t order = m->c.h.orders[oi];
            for (h = 0; h < heads; h++, table++) {
                uint32_t seed = ND_EG_SEED * (oi * heads + h + 1);
                uint32_t acc  = seed;
                uint32_t idx;
                int      ok   = (m->pos + 1 >= order); /* enough history */

                for (j = 0; j < order; j++) {
                    uint32_t tk = (j <= m->pos) ? m->hist[j] : 0u;
                    acc = (acc ^ tk) * ND_EG_PRIME;
                }
                acc ^= acc >> 15;
                idx = acc % slots;

                if (ok) {
                    nd_tensor *tt = &m->engram[s].tables;
                    nd_cq_dequant_row(&m->c, tt, nd_cact_data(&m->c, tt),
                                      table * slots + idx, m->row,
                                      e + (size_t)table * sub);
                } else {
                    memset(e + (size_t)table * sub, 0, sizeof(float) * sub);
                }
            }
        }

        /* k = key_proj @ e, raw v = value_proj @ e */
        {
            nd_tensor *kp = &m->engram[s].key_proj;
            nd_tensor *vp = &m->engram[s].value_proj;
            float     *vraw = m->eg_hist + ((size_t)s * ND_EG_HIST +
                                            (m->eg_pos % ND_EG_HIST)) * dm;
            /* e currently lives in xh; prepare needs its own output, so use
             * tmp2 as the transformed activation buffer. */
            nd_cq_prepare(kp, e, m->tmp2);
            nd_cq_lut_build(&m->c, m->tmp2, nd_cq_in_pad(kp), m->lut);
            nd_cq_gemv_lut2(kp, nd_cact_data(&m->c, kp), m->lut,
                            m->eg_k + (size_t)s * dm);
            nd_cq_gemv_lut2(vp, nd_cact_data(&m->c, vp), m->lut, vraw);
        }

        /* Dilated causal tap convolution over the raw v history. */
        {
            const float *tp = m->eg_taps_f[s];
            float *out = m->eg_v + (size_t)s * dm;
            uint32_t d;

            memset(out, 0, sizeof(float) * dm);
            for (j = 0; j < taps; j++) {
                uint32_t back = j * dil;
                const float *src;
                if (back > m->pos)
                    continue;  /* tap_ok */
                src = m->eg_hist + ((size_t)s * ND_EG_HIST +
                                    ((m->eg_pos - back) % ND_EG_HIST)) * dm;
                for (d = 0; d < dm; d++)
                    out[d] += tp[j * dm + d] * src[d];
            }
        }
    }
    m->eg_pos++;
}

/* ------------------------------------------------------------- attention */

/* Online softmax over one range of heads.
 *
 * A two-pass max-then-accumulate computes every q.k twice, and with a pinned
 * 158-token prefix those dot products dominate the whole forward pass. Keeping
 * a running max and rescaling the accumulator gives the identical result from
 * a single pass over K and V. Heads are independent, so the range also splits
 * across cores exactly like GEMV rows. */
typedef struct {
    nd_model *m;
    uint32_t  li, nkv, rep, qk_hd, v_hd, sinks, rfirst, rcount;
    float     scale;
} attn_ctx;

static ND_HOT void attn_heads(void *vc, uint32_t h0, uint32_t h1)
{
    const attn_ctx *c   = (const attn_ctx *)vc;
    nd_model       *m   = c->m;
    uint32_t        qk_hd = c->qk_hd;
    uint32_t        v_hd = c->v_hd;
    uint32_t        nkv = c->nkv;
    uint32_t        li  = c->li;
    uint32_t        h, i;

    for (h = h0; h < h1; h++) {
        const float *qh  = m->q + (size_t)h * qk_hd;
        float       *oh  = m->attn + (size_t)h * v_hd;
        uint32_t     kvh = h / c->rep;
        float        mx = -INFINITY, denom = 0.0f;
        uint32_t     run, p;

        memset(oh, 0, sizeof(float) * v_hd);

        for (run = 0; run < 2; run++) {
            uint32_t base  = run ? c->rfirst : 0;
            uint32_t count = run ? c->rcount : c->sinks;

            /* Two positions per iteration. The same two scores are compared
             * against the running max in slot order, so "a is the new max" and
             * "then b is" is the same pair of rescales in the same order;
             * "a is, b is not" folds to exp(b-mx_new), which is what the
             * one-at-a-time loop computed; "neither" rescales by
             * exp(m_old-m_new) once instead of not at all, which is the
             * normalisation this loop does at the end anyway. */
            for (p = 0; p + 1 < count; p += 2) {
                uint32_t      sl0 = kv_slot(m, base + p);
                uint32_t      sl1 = kv_slot(m, base + p + 1);
                size_t        o0  = (size_t)li * m->window + sl0;
                size_t        o1  = (size_t)li * m->window + sl1;
                const int8_t *kp0 = m->k_cache + o0 * m->k_dim + (size_t)kvh * qk_hd;
                const int8_t *kp1 = m->k_cache + o1 * m->k_dim + (size_t)kvh * qk_hd;
                const int8_t *vp0 = m->v_cache + o0 * m->v_dim + (size_t)kvh * v_hd;
                const int8_t *vp1 = m->v_cache + o1 * m->v_dim + (size_t)kvh * v_hd;
                float         sc0 = m->k_scale[o0 * nkv + kvh] * c->scale;
                float         sc1 = m->k_scale[o1 * nkv + kvh] * c->scale;
                float         s0 = 0.0f, s1 = 0.0f, mnew, rescale, w0, w1;

                /* One 32-bit load per four K bytes. qk_head_dim (48) and the
                 * k_cache row pitch are multiples of 4, so the word loads are
                 * aligned. Products are added to each score in the same pairs,
                 * in the same index order, as the byte-wise loop did. */
                for (i = 0; i < qk_hd; i += 4) {
                    uint32_t a = ((const uint32_t *)(const void *)kp0)[i >> 2];
                    uint32_t b = ((const uint32_t *)(const void *)kp1)[i >> 2];
                    s0 += qh[i + 0] * (float)(int8_t)(a & 0xff)
                        + qh[i + 1] * (float)(int8_t)((a >> 8) & 0xff);
                    s0 += qh[i + 2] * (float)(int8_t)((a >> 16) & 0xff)
                        + qh[i + 3] * (float)(int8_t)(a >> 24);
                    s1 += qh[i + 0] * (float)(int8_t)(b & 0xff)
                        + qh[i + 1] * (float)(int8_t)((b >> 8) & 0xff);
                    s1 += qh[i + 2] * (float)(int8_t)((b >> 16) & 0xff)
                        + qh[i + 3] * (float)(int8_t)(b >> 24);
                }
                s0 *= sc0;
                s1 *= sc1;

                mnew = (s0 > s1) ? s0 : s1;
                if (mnew > mx) {
                    if (denom > 0.0f) {
                        rescale = nd_expf(mx - mnew);
                        for (i = 0; i < v_hd; i++)
                            oh[i] *= rescale;
                        denom *= rescale;
                    }
                    mx = mnew;
                }
                w0 = nd_expf(s0 - mx);
                w1 = nd_expf(s1 - mx);
                denom += w0 + w1;
                {
                    float wv0 = w0 * m->v_scale[o0 * nkv + kvh];
                    float wv1 = w1 * m->v_scale[o1 * nkv + kvh];
                    /* One 32-bit load per four V bytes. v_head_dim (64) and the
                     * v_cache row pitch are multiples of 4, so vp0/vp1 are
                     * 4-aligned and the loop covers v_hd exactly. The scalar
                     * loop's per-element order is preserved: each oh[] entry
                     * still receives its wv0 term and then its wv1 term, and no
                     * accumulator is re-associated. Byte-identical goldens, same
                     * probe value. */
                    for (i = 0; i < v_hd; i += 4) {
                        uint32_t a = ((const uint32_t *)(const void *)vp0)[i >> 2];
                        uint32_t b = ((const uint32_t *)(const void *)vp1)[i >> 2];
                        oh[i + 0] += wv0 * (float)(int8_t)(a & 0xff);
                        oh[i + 0] += wv1 * (float)(int8_t)(b & 0xff);
                        oh[i + 1] += wv0 * (float)(int8_t)((a >> 8) & 0xff);
                        oh[i + 1] += wv1 * (float)(int8_t)((b >> 8) & 0xff);
                        oh[i + 2] += wv0 * (float)(int8_t)((a >> 16) & 0xff);
                        oh[i + 2] += wv1 * (float)(int8_t)((b >> 16) & 0xff);
                        oh[i + 3] += wv0 * (float)(int8_t)(a >> 24);
                        oh[i + 3] += wv1 * (float)(int8_t)(b >> 24);
                    }
                }
            }
            if (p < count) {                    /* odd tail, unchanged path */
                uint32_t      sl = kv_slot(m, base + p);
                size_t        o  = (size_t)li * m->window + sl;
                const int8_t *kp = m->k_cache + o * m->k_dim + (size_t)kvh * qk_hd;
                const int8_t *vp = m->v_cache + o * m->v_dim + (size_t)kvh * v_hd;
                float         dot = 0.0f, w;

                for (i = 0; i < qk_hd; i++)
                    dot += qh[i] * (float)kp[i];
                dot *= m->k_scale[o * nkv + kvh] * c->scale;

                if (dot > mx) {
                    if (denom > 0.0f) {
                        float r = nd_expf(mx - dot);
                        for (i = 0; i < v_hd; i++)
                            oh[i] *= r;
                        denom *= r;
                    }
                    mx = dot;
                }
                w = nd_expf(dot - mx);
                denom += w;
                {
                    float wv = w * m->v_scale[o * nkv + kvh];
                    for (i = 0; i < v_hd; i++)
                        oh[i] += wv * (float)vp[i];
                }
            }
        }
        {
            float inv = 1.0f / denom;
            for (i = 0; i < v_hd; i++)
                oh[i] *= inv;
        }
    }
}

static void tap_projection(nd_model *m, uint32_t tap_slot,
                           float *projection, float *history,
                           uint32_t li, uint32_t dim)
{
    uint32_t taps = m->c.h.qkv_conv_taps;
    uint32_t slot = m->pos % taps;
    uint32_t i, j;
    /* Staged float32 (see the slot table in nd_model_open): the tap weights are
     * read once per element per tap, so converting inside the loop cost
     * taps*dim conversions per projection. */
    const float *weights = m->fp16_slot[li][tap_slot];
    float *layer_history = history + (size_t)li * taps * dim;
    memcpy(layer_history + (size_t)slot * dim, projection, dim * sizeof(float));
    for (i = 0; i < dim; i++) {
        float value = 0.0f;
        for (j = 0; j < taps && j <= m->pos; j++) {
            uint32_t prior = (m->pos - j) % taps;
            value += weights[(size_t)j * dim + i] *
                     layer_history[(size_t)prior * dim + i];
        }
        projection[i] = value;
    }
}

static void attention(nd_model *m, uint32_t li, const float *xin, float *out)
{
    const nd_layer *L    = &m->layer[li];
    uint32_t        qk_hd = m->qk_head_dim;
    uint32_t        v_hd = m->v_head_dim;
    uint32_t        nh   = m->n_heads;
    uint32_t        nkv  = m->n_kv_heads;
    uint32_t        slot = kv_slot(m, m->pos);
    size_t          kbase = ((size_t)li * m->window + slot) * m->k_dim;
    size_t          vbase = ((size_t)li * m->window + slot) * m->v_dim;
    size_t          scbase = ((size_t)li * m->window + slot) * nkv;
    uint32_t        i, kh;

    /* q, k, v and the gate all reduce over the same activation, so one
     * prepare and one pair table serve all four. */
    /* Pair table, not the quad table: the quad kernel needs group-outer
     * ordering, which reads 32 bytes out of every 64-byte cache line and
     * measured 38% SLOWER on device despite issuing fewer instructions. */
    { ND_T0(tp);
      nd_cq_prepare(&L->q_proj, xin, m->xh);
      nd_cq_lut_build(&m->c, m->xh, nd_cq_in_pad(&L->q_proj), m->lut);
      ND_T1(tp, ND_P_PREP); }
    { ND_T0(tg);
      nd_cq_gemv_lut2(&L->q_proj,    nd_cact_data(&m->c, &L->q_proj),    m->lut, m->q);
      nd_cq_gemv_lut2(&L->k_proj,    nd_cact_data(&m->c, &L->k_proj),    m->lut, m->kbuf);
      nd_cq_gemv_lut2(&L->v_proj,    nd_cact_data(&m->c, &L->v_proj),    m->lut, m->vbuf);
      nd_cq_gemv_lut2(&L->gate_proj, nd_cact_data(&m->c, &L->gate_proj), m->lut, m->gate);
      ND_T1(tg, ND_P_PROJ); }

    tap_projection(m, 4, m->q, m->q_hist, li, nh * qk_hd);
    tap_projection(m, 5, m->kbuf, m->k_hist, li, m->k_dim);
    tap_projection(m, 6, m->vbuf, m->v_hist, li, m->v_dim);

    zcrms_heads(m, m->fp16_slot[li][7], m->q, nh, qk_hd);
    zcrms_heads(m, m->fp16_slot[li][8], m->kbuf, nkv, qk_hd);

    apply_rope(m, m->q, nh, qk_hd);
    apply_rope(m, m->kbuf, nkv, qk_hd);

    /* Store this position's k/v as symmetric int8, one scale per head. */
    for (kh = 0; kh < nkv; kh++) {
        float mk = 0.0f, mv = 0.0f;
        for (i = 0; i < qk_hd; i++) {
            float a = fabsf(m->kbuf[kh * qk_hd + i]);
            if (a > mk) mk = a;
        }
        for (i = 0; i < v_hd; i++) {
            float b = fabsf(m->vbuf[kh * v_hd + i]);
            if (b > mv) mv = b;
        }
        {
            float ks = (mk > 0.0f) ? mk / 127.0f : 1.0f;
            float vs = (mv > 0.0f) ? mv / 127.0f : 1.0f;
            m->k_scale[scbase + kh] = ks;
            m->v_scale[scbase + kh] = vs;
            for (i = 0; i < qk_hd; i++) {
                float kq = m->kbuf[kh * qk_hd + i] / ks;
                if (kq > 127.0f)  kq = 127.0f;
                if (kq < -127.0f) kq = -127.0f;
                m->k_cache[kbase + kh * qk_hd + i] = (int8_t)lrintf(kq);
            }
            for (i = 0; i < v_hd; i++) {
                float vq = m->vbuf[kh * v_hd + i] / vs;
                if (vq > 127.0f)  vq = 127.0f;
                if (vq < -127.0f) vq = -127.0f;
                m->v_cache[vbase + kh * v_hd + i] = (int8_t)lrintf(vq);
            }
        }
    }

    /* Attend to the pinned sinks [0, n_sink) plus the most recent
     * (window - n_sink) positions. With n_sink == 0 this is a plain sliding
     * window; the two runs are visited without materialising a score buffer. */
    { ND_T0(ta);
    {
        attn_ctx actx;
        uint32_t rcap = m->window - m->n_sink;

        actx.m      = m;
        actx.li     = li;
        actx.nkv    = nkv;
        actx.rep    = nh / nkv;
        actx.qk_hd  = qk_hd;
        actx.v_hd   = v_hd;
        actx.scale  = 1.0f / sqrtf((float)qk_hd);
        actx.sinks  = (m->n_sink < m->pos + 1) ? m->n_sink : m->pos + 1;

        if (m->pos + 1 <= m->n_sink) {
            actx.rfirst = m->pos + 1;
            actx.rcount = 0;
        } else {
            actx.rcount = m->pos + 1 - m->n_sink;
            if (actx.rcount > rcap)
                actx.rcount = rcap;
            actx.rfirst = m->pos + 1 - actx.rcount;
        }
        nd_parallel_rows(attn_heads, &actx, nh);
    }
    ND_T1(ta, ND_P_ATTN); }

    /* Gate, then project back to d_model. */
    for (i = 0; i < m->attn_dim; i++)
        m->attn[i] *= sigmoidf_(m->gate[i]);

    { ND_T0(tp2);
      nd_cq_prepare(&L->out_proj, m->attn, m->xh);
      nd_cq_lut_build(&m->c, m->xh, nd_cq_in_pad(&L->out_proj), m->lut);
      ND_T1(tp2, ND_P_PREP); }
    { ND_T0(tg2);
      nd_cq_gemv_lut2(&L->out_proj, nd_cact_data(&m->c, &L->out_proj), m->lut, out);
      ND_T1(tg2, ND_P_PROJ); }
}

/* -------------------------------------------------------- Monarch Hadamard MLP */

/* JAX reference: einsum("ij,ik,jl->kl", z, a, b). Keep only one 1024-float
 * intermediate rather than materialising a 1024x1024 matrix. */
/* Both halves of (A (x) B) applied to src. The inner products are written as
 * accumulations over the *row* index rather than the output index: the naive
 * form has one loop-carried sum per output element, so every FMA waits for the
 * previous one on the LX7. Unrolling the row loop by two gives two independent
 * chains per output and, in the first pass, lets a[i] and a[i+1] stay in
 * registers across the nb columns they both touch. */
typedef struct { nd_model *m; float *dst; const float *b;
                 uint32_t na, nb; } kron2_ctx;

/* Second Kronecker half over a range of output rows. Each k reads only its own
 * hada_c row and writes only its own dst row, so the split is exact: the per
 * output element still sums nb products in j order. */
static ND_HOT void kron2_rows(void *vc, uint32_t k0, uint32_t k1)
{
    const kron2_ctx *c = (const kron2_ctx *)vc;
    uint32_t k, j, l;

    for (k = k0; k < k1; k++) {
        const float *crow = c->m->hada_c + (size_t)k * c->nb;
        for (l = 0; l + 7 < c->nb; l += 8) {
            float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
            float s4 = 0.0f, s5 = 0.0f, s6 = 0.0f, s7 = 0.0f;
            for (j = 0; j + 1 < c->nb; j += 2) {
                float cj = crow[j];
                float dj = crow[j + 1];
                const float *br = c->b + (size_t)j * c->nb + l;
                const float *cr = c->b + (size_t)(j + 1) * c->nb + l;
                s0 += cj * br[0]; s0 += dj * cr[0];
                s1 += cj * br[1]; s1 += dj * cr[1];
                s2 += cj * br[2]; s2 += dj * cr[2];
                s3 += cj * br[3]; s3 += dj * cr[3];
                s4 += cj * br[4]; s4 += dj * cr[4];
                s5 += cj * br[5]; s5 += dj * cr[5];
                s6 += cj * br[6]; s6 += dj * cr[6];
                s7 += cj * br[7]; s7 += dj * cr[7];
            }
            c->dst[(size_t)k * c->nb + l + 0] = s0;
            c->dst[(size_t)k * c->nb + l + 1] = s1;
            c->dst[(size_t)k * c->nb + l + 2] = s2;
            c->dst[(size_t)k * c->nb + l + 3] = s3;
            c->dst[(size_t)k * c->nb + l + 4] = s4;
            c->dst[(size_t)k * c->nb + l + 5] = s5;
            c->dst[(size_t)k * c->nb + l + 6] = s6;
            c->dst[(size_t)k * c->nb + l + 7] = s7;
        }
        for (; l < c->nb; l++) {
            float sum = 0.0f;
            for (j = 0; j < c->nb; j++)
                sum += crow[j] * c->b[(size_t)j * c->nb + l];
            c->dst[(size_t)k * c->nb + l] = sum;
        }
    }
}

typedef struct { nd_model *m; const float *src, *a;
                 uint32_t na, nb; } kron1_ctx;

/* First Kronecker half over a range of 4-row blocks. A block writes only its
 * own 4 rows of hada_c and reads the whole src, so blocks are independent and
 * each output element still accumulates its na products in i order. */
static ND_HOT void kron1_blocks(void *vc, uint32_t b0, uint32_t b1)
{
    const kron1_ctx *c = (const kron1_ctx *)vc;
    uint32_t b, i, j;

    for (b = b0; b < b1; b++) {
        uint32_t k0 = b * 4;
        for (j = 0; j + 1 < c->nb; j += 2) {
            float s[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float u[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (i = 0; i < c->na; i++) {
                float        v0 = c->src[(size_t)i * c->nb + j];
                float        v1 = c->src[(size_t)i * c->nb + j + 1];
                const float *ar = c->a + (size_t)i * c->na + k0;
                s[0] += v0 * ar[0]; u[0] += v1 * ar[0];
                s[1] += v0 * ar[1]; u[1] += v1 * ar[1];
                s[2] += v0 * ar[2]; u[2] += v1 * ar[2];
                s[3] += v0 * ar[3]; u[3] += v1 * ar[3];
            }
            { uint32_t t; for (t = 0; t < 4; t++) {
                    c->m->hada_c[(size_t)(k0 + t) * c->nb + j]     = s[t];
                    c->m->hada_c[(size_t)(k0 + t) * c->nb + j + 1] = u[t]; } }
        }
    }
}

static void kron_apply(nd_model *m, const float *src, float *dst,
                       const float *a, const float *b,
                       uint32_t na, uint32_t nb)
{
    /* Both halves split over cores. The first half's reuse is the column-major
     * a-row, so it runs 4 rows x 2 j columns per pass (its measured optimum) and
     * hands the 8 independent row-blocks to the splitter. The second half's
     * reuse is the loaded hada_c value, so it blocks 8 columns with paired b
     * rows, and its na output rows are independent units of work. In both, the
     * products summed per output element, and their order, are unchanged. */
    {
        kron1_ctx kc;
        kc.m = m; kc.src = src; kc.a = a; kc.na = na; kc.nb = nb;
        nd_parallel_rows(kron1_blocks, &kc, na / 4);
    }
    {
        kron2_ctx kc;
        kc.m = m; kc.dst = dst; kc.b = b; kc.na = na; kc.nb = nb;
        nd_parallel_rows(kron2_rows, &kc, na);
    }
}

typedef struct { float *a; const float *d2, *b2, *sc; } silu_ctx;

/* The gate's SiLU stage: 1024 independent elements, chunked by 128 so the two
 * cores each run four chunks. Elementwise, so every value is identical. */
static ND_HOT void silu_rows(void *vc, uint32_t b0, uint32_t b1)
{
    const silu_ctx *c = (const silu_ctx *)vc;
    uint32_t i, lo = b0 * 128, hi = b1 * 128;
    for (i = lo; i < hi; i++) {
        float z = c->d2[i] * c->sc[i] * c->a[i] + c->b2[i];
        c->a[i] = z * sigmoidf_(z);
    }
}

static void hadamard_mlp_unscaled(nd_model *m, uint32_t li, const float *x)
{
    const nd_layer *L = &m->layer[li];
    uint32_t dm = m->d_model, n = m->c.h.hada_n, i, j;
    float *const *fp = m->fp16_slot[li];
    const float *d1 = fp[14];
        const float *d2 = fp[15], *b2 = fp[16], *d3 = fp[17]; const float *cv = fp[25];
    const float    *cu = fp[26];
    const float *p1 = (const float *)nd_cact_data(&m->c, &m->hada_p1);
    const float *p2 = (const float *)nd_cact_data(&m->c, &m->hada_p2);
    float cond[8] = {0};
    float max_cond, sum_cond;

    /* Softmax(x @ cond_v), with the eight conditioning channels in this blob. */
    for (i = 0; i < dm; i++)
        for (j = 0; j < 8; j++)
            cond[j] += x[i] * cv[(size_t)i * 8 + j];
    max_cond = cond[0];
    for (j = 1; j < 8; j++) if (cond[j] > max_cond) max_cond = cond[j];
    sum_cond = 0.0f;
    for (j = 0; j < 8; j++) {
        cond[j] = nd_expf(cond[j] - max_cond);
        sum_cond += cond[j];
    }
    for (j = 0; j < 8; j++) cond[j] /= sum_cond;

    for (i = 0; i < dm; i++) m->hada_a[i] = d1[i] * x[i];
    for (i = dm; i < n; i++) m->hada_a[i] = 0.0f;
    kron_apply(m, m->hada_a, m->hada_b, fp[19], fp[20],
               L->w1a.shape[0], L->w1b.shape[0]);
    for (i = 0; i < n; i++) m->hada_a[i] = m->hada_b[(uint32_t)p1[i]];

    /* cu is row-major over the 8 conditioning channels, so the blend gathered
     * one column out of eight rows per element. Pre-folding the conditioned
     * rows into one scale row costs the same 8*n FMAs, walks memory
     * sequentially, and leaves one read per element in the hot loop. Fold order
     * j = 0..7 into a running sum is the order the per-element loop already
     * used, so every value is bit-identical. */
    {
        float *sc = m->scale_row;
        for (i = 0; i < n; i++)
            sc[i] = 1.0f + cond[0] * cu[i];
        for (j = 1; j < 8; j++) {
            const float *row = cu + (size_t)j * n;
            float        cj  = cond[j];
            for (i = 0; i < n; i++)
                sc[i] += cj * row[i];
        }
        {
            silu_ctx sg = { m->hada_a, d2, b2, sc };
            nd_parallel_rows(silu_rows, &sg, n / 128);
        }
    }
    kron_apply(m, m->hada_a, m->hada_b, fp[21], fp[22],
               L->w2a.shape[0], L->w2b.shape[0]);
    for (i = 0; i < n; i++) m->hada_a[i] = m->hada_b[(uint32_t)p2[i]] * d3[i];
    kron_apply(m, m->hada_a, m->hada_b, fp[23], fp[24],
               L->w3a.shape[0], L->w3b.shape[0]);
    /* the caller applies d4 and folds the residual add */
}

/* ------------------------------------------------------------------ block */

/* u <- block(u); the engram injection happens first when this layer is a site. */
static void block(nd_model *m, uint32_t li, float *u)
{
    const nd_layer *L  = &m->layer[li];
    uint32_t        dm = m->d_model;
    uint32_t        s, i;

    for (s = 0; s < m->n_sites; s++) {
        if (m->c.h.sites[s] != li)
            continue;
        {
            const float *ek = m->eg_k + (size_t)s * dm;
            const float *ev = m->eg_v + (size_t)s * dm;
            float        dot = 0.0f, alpha;

            rms_unit(u, dm, m->n1);
            rms_unit(ek, dm, m->n2);
            for (i = 0; i < dm; i++)
                dot += m->n1[i] * m->n2[i];
            alpha = sigmoidf_(dot / sqrtf((float)dm));
            for (i = 0; i < dm; i++)
                u[i] += alpha * ev[i];
        }
    }

    /* attention sub-block */
    zcrms(m, m->fp16_slot[li][0], u, dm, m->n1);
    attention(m, li, m->n1, m->aout);
    zcrms(m, m->fp16_slot[li][11], m->aout, dm, m->n2);
    {
        float g = sigmoidf_(fp16_get(m, &L->attn_gate, 0));
        for (i = 0; i < dm; i++)
            u[i] += g * m->n2[i];
    }

    /* Hadamard MLP sub-block. The MLP writes into a padded buffer, so n2 must
     * hold next_pow2(d_model) floats; for d_model=512 that is exact. */
    /* Hadamard MLP sub-block. Its d4 output scale and the residual add are
     * fused into one pass: the kernel leaves the unscaled result in hada_b, so
     * folding d4 into the add avoids writing dm scaled floats and reading them
     * straight back. Same arithmetic per element. */
    { ND_T0(tm);
    zcrms(m, m->fp16_slot[li][13], u, dm, m->n1);
    hadamard_mlp_unscaled(m, li, m->n1);
    { const float *d4 = m->fp16_slot[li][18];
      for (i = 0; i < dm; i++)
          u[i] += m->hada_b[i] * d4[i]; }
    ND_T1(tm, ND_P_MLP); }
}

/* ------------------------------------------------------------------- step */

const float *nd_model_step(nd_model *m, uint32_t token)
{
    const float *hidden = nd_model_step_hidden(m, token);
    return nd_model_logits_all(m, hidden);
}

const float *nd_model_logits_all(nd_model *m, const float *hidden)
{
    ND_T0(tl);
    nd_cq_gemv(&m->c, &m->embedding, nd_cact_data(&m->c, &m->embedding),
               hidden, m->xh, m->logits);
    ND_T1(tl, ND_P_LOGITS);
    return m->logits;
}

void nd_model_logits_subset(nd_model *m, const float *hidden,
                            const uint32_t *ids, uint32_t n, float *out)
{
    ND_T0(tl);
    nd_cq_prepare(&m->embedding, hidden, m->xh);
    nd_cq_gemv_gather(&m->c, &m->embedding, nd_cact_data(&m->c, &m->embedding),
                      m->xh, ids, n, out);
    ND_T1(tl, ND_P_LOGITS);
}

const float *nd_model_step_hidden(nd_model *m, uint32_t token)
{
    uint32_t dm    = m->d_model;
    uint32_t n     = m->lanes;
    uint32_t nl    = n * dm;
    float    escale = sqrtf((float)dm);
    uint32_t i, j, li;

    /* Rotary tables for this position, shared by every layer. */
    for (i = 0; i < m->qk_head_dim / 2; i++) {
        float angle = (float)m->pos * m->rope_inv[i];
        m->rope_cos[i] = cosf(angle);
        m->rope_sin[i] = sinf(angle);
    }

    /* Embedding (tied), scaled. */
    nd_cq_dequant_row(&m->c, &m->embedding, nd_cact_data(&m->c, &m->embedding),
                      token, m->xh, m->tmp);
    for (i = 0; i < dm; i++)
        m->tmp[i] *= escale;
    if (m->has_conf)
        pool_cell(m, m->tmp);       /* cells[0] = x0 */
    for (j = 0; j < n; j++)
        for (i = 0; i < dm; i++)
            m->lane[j * dm + i] = m->tmp[i];

    { ND_T0(te); engram_step(m, token); ND_T1(te, ND_P_ENGRAM); }

    for (li = 0; li < m->n_layers; li++) {
        float hpre[ND_MAX_LANES], hpost[ND_MAX_LANES];
        float hres[ND_MAX_LANES * ND_MAX_LANES];
        float a_pre  = fp16_get(m, &m->mhc_a_pre, li);
        float a_post = fp16_get(m, &m->mhc_a_post, li);
        float a_res  = fp16_get(m, &m->mhc_a_res, li);
        uint32_t lane_id = li % n;

        rms_unit(m->lane, nl, m->nx);

        /* The phi tensors stack all layers; this layer owns a row slice. */
        ND_T0(tphi);
        nd_cq_prepare(&m->mhc_phi_pre, m->nx, m->xh);
        nd_cq_gemv_rows(&m->c, &m->mhc_phi_pre,
                        nd_cact_data(&m->c, &m->mhc_phi_pre), m->xh,
                        li * n, n, hpre);
        nd_cq_gemv_rows(&m->c, &m->mhc_phi_post,
                        nd_cact_data(&m->c, &m->mhc_phi_post), m->xh,
                        li * n, n, hpost);
        nd_cq_gemv_rows(&m->c, &m->mhc_phi_res,
                        nd_cact_data(&m->c, &m->mhc_phi_res), m->xh,
                        li * n * n, n * n, hres);
        ND_T1(tphi, ND_P_PHI);

        for (j = 0; j < n; j++) {
            float pre_off  = 8.0f * (j == lane_id ? 1.0f : 0.0f) - 4.0f;
            float post_off = -4.0f * (j == lane_id ? 0.0f : 1.0f);
            hpre[j]  = sigmoidf_(a_pre * hpre[j] +
                                 fp16_get(m, &m->mhc_b_pre, li * n + j) + pre_off);
            hpost[j] = 2.0f * sigmoidf_(a_post * hpost[j] +
                                 fp16_get(m, &m->mhc_b_post, li * n + j) + post_off);
        }
        for (i = 0; i < n * n; i++)
            hres[i] = a_res * hres[i] + fp16_get(m, &m->mhc_b_res, li * n * n + i);
        sinkhorn(hres, n);

        /* u = sum_j hpre[j] * lane[j] */
        {
            lanepre_ctx lp = { m->u, m->lane, hpre, n, dm };
            nd_parallel_rows(lanepre_rows, &lp, dm / 128);
        }

        /* y = block(u) - u */
        memcpy(m->ublk, m->u, sizeof(float) * dm);
        block(m, li, m->u);
        for (i = 0; i < dm; i++)
            m->u[i] -= m->ublk[i];

        /* lane' = hres @ lane + hpost * y */
        /* Loop order swapped: the n lanes are the short dimension, so the
         * column-major form re-read n strided lane rows for every d_model
         * column. Row-major accumulation keeps both streams sequential; hpost
         * folds into the initialiser, which re-associates one add - the change
         * shows up on the fidelity probe (7.2e-05 -> 5.3e-05, i.e. smaller) and
         * the goldens stay byte-identical. */
        {
            lanemix_ctx lm = { m, hres, hpost, n, dm };
            /* Output lanes are independent (each reads all lanes but writes
             * only its own row), so the 4 lanes are 4 units of work. */
            nd_parallel_rows(lanemix_rows, &lm, n);
        }
        {
            float *swap = m->lane;
            m->lane      = m->lane_next;
            m->lane_next = swap;
        }

        if (m->has_conf) {
            /* collect_hidden yields the mean over lanes for each layer. */
            for (i = 0; i < dm; i++) {
                float acc = 0.0f;
                for (j = 0; j < n; j++)
                    acc += m->lane[j * dm + i];
                m->n1[i] = acc / (float)n;
            }
            { ND_T0(tc); pool_cell(m, m->n1); ND_T1(tc, ND_P_CONF); }
        }
    }

    /* Mean over lanes, final norm, tied-embedding logits. */
    for (i = 0; i < dm; i++) {
        float acc = 0.0f;
        for (j = 0; j < n; j++)
            acc += m->lane[j * dm + i];
        m->tmp[i] = acc / (float)n;
    }
        /* final_norm is not per-layer, so it is not in the staged set: convert the
     * one row into the scratch the staged vectors already have. */
    fp16_row((const uint16_t *)nd_cact_data(&m->c, &m->final_norm), m->scale_f,
             dm);
    zcrms(m, m->scale_f, m->tmp, dm, m->y);

    m->pos++;
    return m->y;
}
