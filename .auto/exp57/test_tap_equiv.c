/* Bit-equivalence guard for the tap family (#exp57 hoist, #exp59 two-column tile, #exp60
 * current-input forwarding) against the SHIPPED per-element-remainder form.
 *
 * Every variant must be BIT-identical to the shipped one, so a mismatch is a transcription bug -
 * the class that cost runs #363 and #380 a board each. The caller's contract is modelled exactly:
 * `proj` is both the current input and the output buffer, and the history row for slot
 * (pos % taps) is a byte copy of it made immediately before the call (which is what makes the
 * forwarding variant exact, and also why it must read column i before storing it).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { float *proj, *hist; const float *w;
                 uint32_t taps, pos, dim; } tap_ctx;

static uint32_t hi_of(uint32_t dim, uint32_t nb) { return nb * 256 < dim ? nb * 256 : dim; }

static void shipped(void *vc, uint32_t b0, uint32_t b1)
{
    const tap_ctx *c = (const tap_ctx *)vc;
    uint32_t i, j, dim = c->dim;
    uint32_t lo = b0 * 256, hi = hi_of(dim, b1);
    for (i = lo; i < hi; i++) {
        float value = 0.0f;
        for (j = 0; j < c->taps && j <= c->pos; j++) {
            uint32_t prior = (c->pos - j) % c->taps;
            value += c->w[(size_t)j * dim + i] * c->hist[(size_t)prior * dim + i];
        }
        c->proj[i] = value;
    }
}

static void hoisted(void *vc, uint32_t b0, uint32_t b1)
{
    const tap_ctx *c = (const tap_ctx *)vc;
    uint32_t i, j, dim = c->dim;
    uint32_t lo = b0 * 256, hi = hi_of(dim, b1);
    uint32_t nt = c->taps <= c->pos + 1u ? c->taps : c->pos + 1u;
    if (nt == 3u) {
        const float *h0 = c->hist + (size_t)( c->pos        % c->taps) * dim;
        const float *h1 = c->hist + (size_t)((c->pos - 1u)  % c->taps) * dim;
        const float *h2 = c->hist + (size_t)((c->pos - 2u)  % c->taps) * dim;
        const float *w0 = c->w, *w1 = c->w + dim, *w2 = c->w + 2u * dim;
        for (i = lo; i < hi; i++) {
            float value = 0.0f;
            value += w0[i] * h0[i];
            value += w1[i] * h1[i];
            value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;
    }
    for (i = lo; i < hi; i++) {
        float value = 0.0f;
        for (j = 0; j < nt; j++) {
            uint32_t prior = (c->pos - j) % c->taps;
            value += c->w[(size_t)j * dim + i] * c->hist[(size_t)prior * dim + i];
        }
        c->proj[i] = value;
    }
}

static void tile2(void *vc, uint32_t b0, uint32_t b1)
{
    const tap_ctx *c = (const tap_ctx *)vc;
    uint32_t i, j, dim = c->dim;
    uint32_t lo = b0 * 256, hi = hi_of(dim, b1);
    uint32_t nt = c->taps <= c->pos + 1u ? c->taps : c->pos + 1u;
    if (nt == 3u) {
        const float *h0 = c->hist + (size_t)( c->pos        % c->taps) * dim;
        const float *h1 = c->hist + (size_t)((c->pos - 1u)  % c->taps) * dim;
        const float *h2 = c->hist + (size_t)((c->pos - 2u)  % c->taps) * dim;
        const float *w0 = c->w, *w1 = c->w + dim, *w2 = c->w + 2u * dim;
        for (i = lo; i + 1u < hi; i += 2u) {
            float a = 0.0f, b = 0.0f;
            a += w0[i] * h0[i]; a += w1[i] * h1[i]; a += w2[i] * h2[i];
            b += w0[i + 1u] * h0[i + 1u]; b += w1[i + 1u] * h1[i + 1u];
            b += w2[i + 1u] * h2[i + 1u];
            c->proj[i] = a; c->proj[i + 1u] = b;
        }
        if (i < hi) {
            float value = 0.0f;
            value += w0[i] * h0[i]; value += w1[i] * h1[i]; value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;
    }
    for (i = lo; i < hi; i++) {
        float value = 0.0f;
        for (j = 0; j < nt; j++) {
            uint32_t prior = (c->pos - j) % c->taps;
            value += c->w[(size_t)j * dim + i] * c->hist[(size_t)prior * dim + i];
        }
        c->proj[i] = value;
    }
}

/* The forwarding candidate, and its mirror-image control: `ctl_fwd` writes the current input into
 * the slot row first (exactly what nd_model.c's memcpy does) and then reads that row, while
 * `fwd` reads the source directly. Any difference would be an aliasing bug, not a rounding one. */
static void fwd(void *vc, uint32_t b0, uint32_t b1)
{
    const tap_ctx *c = (const tap_ctx *)vc;
    uint32_t i, j, dim = c->dim;
    uint32_t lo = b0 * 256, hi = hi_of(dim, b1);
    uint32_t nt = c->taps <= c->pos + 1u ? c->taps : c->pos + 1u;
    if (nt == 3u) {
        const float *h0 = c->proj;                                     /* the current input */
        const float *h1 = c->hist + (size_t)((c->pos - 1u) % c->taps) * dim;
        const float *h2 = c->hist + (size_t)((c->pos - 2u) % c->taps) * dim;
        const float *w0 = c->w, *w1 = c->w + dim, *w2 = c->w + 2u * dim;
        for (i = lo; i < hi; i++) {
            float value = 0.0f;
            value += w0[i] * h0[i];                                   /* read before store */
            value += w1[i] * h1[i];
            value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;
    }
    for (i = lo; i < hi; i++) {
        float value = 0.0f;
        for (j = 0; j < nt; j++) {
            uint32_t prior = (c->pos - j) % c->taps;
            value += c->w[(size_t)j * dim + i] * c->hist[(size_t)prior * dim + i];
        }
        c->proj[i] = value;
    }
}

/* The bundled variant (#exp62): forwarding AND the two-column tile together. */
static void fwd_tile(void *vc, uint32_t b0, uint32_t b1)
{
    const tap_ctx *c = (const tap_ctx *)vc;
    uint32_t i, j, dim = c->dim;
    uint32_t lo = b0 * 256, hi = hi_of(dim, b1);
    uint32_t nt = c->taps <= c->pos + 1u ? c->taps : c->pos + 1u;
    if (nt == 3u) {
        const float *h0 = c->proj;
        const float *h1 = c->hist + (size_t)((c->pos - 1u) % c->taps) * dim;
        const float *h2 = c->hist + (size_t)((c->pos - 2u) % c->taps) * dim;
        const float *w0 = c->w, *w1 = c->w + dim, *w2 = c->w + 2u * dim;
        for (i = lo; i + 1u < hi; i += 2u) {
            float a = 0.0f, b = 0.0f;
            a += w0[i] * h0[i]; a += w1[i] * h1[i]; a += w2[i] * h2[i];
            b += w0[i + 1u] * h0[i + 1u]; b += w1[i + 1u] * h1[i + 1u];
            b += w2[i + 1u] * h2[i + 1u];
            c->proj[i] = a; c->proj[i + 1u] = b;
        }
        if (i < hi) {
            float value = 0.0f;
            value += w0[i] * h0[i]; value += w1[i] * h1[i]; value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;
    }
    for (i = lo; i < hi; i++) {
        float value = 0.0f;
        for (j = 0; j < nt; j++) {
            uint32_t prior = (c->pos - j) % c->taps;
            value += c->w[(size_t)j * dim + i] * c->hist[(size_t)prior * dim + i];
        }
        c->proj[i] = value;
    }
}

int main(void)
{
    const uint32_t dims[] = { 96, 128, 576, 768, 33 };
    const char *NM[] = { "hoist", "tile2", "fwd", "bund" };
    void (*FN[])(void *, uint32_t, uint32_t) = { hoisted, tile2, fwd, fwd_tile };
    uint32_t bad[4] = { 0, 0, 0, 0 }, cmp = 0;
    float *w = malloc(sizeof(float) * 4 * 768), *h = malloc(sizeof(float) * 4 * 768);
    float *src = malloc(sizeof(float) * 768), *pa = malloc(sizeof(float) * 768);
    float *ref = malloc(sizeof(float) * 768);

    for (uint32_t s = 0; s < 4u * 768u; s++) {      /* deterministic mixed-sign operands */
        w[s] = (float)((s * 37 + 11) % 997) * 1.3e-3f - 0.64f;
        h[s] = (float)((s * 91 + 7) % 1013) * 1.7e-3f - 0.86f;
    }
    for (unsigned di = 0; di < 5u; di++) {
        uint32_t dim = dims[di];
        for (uint32_t taps = 1; taps <= 4; taps++)
            for (uint32_t pos = 0; pos < 2 * taps + 2; pos++)
                for (uint32_t nb = 1; nb <= (dim + 255) / 256; nb++) {
                    uint32_t hi = hi_of(dim, nb), slot = pos % taps, v;
                    for (uint32_t i = 0; i < dim; i++)       /* current input, freshly filled */
                        src[i] = (float)((i * 53 + pos * 17 + taps * 7 + dim) % 977) * 2.1e-3f - 1.02f;
                    /* the caller's memcpy, then the shipped reference */
                    memcpy(pa, src, sizeof(float) * dim);
                    memcpy(h + (size_t)slot * dim, src, sizeof(float) * dim);
                    { tap_ctx c = { pa, h, w, taps, pos, dim }; shipped(&c, 0, nb); }
                    memcpy(ref, pa, sizeof(float) * dim);
                    for (v = 0; v < 4; v++) {
                        memcpy(pa, src, sizeof(float) * dim);
                        memcpy(h + (size_t)slot * dim, src, sizeof(float) * dim);
                        { tap_ctx c = { pa, h, w, taps, pos, dim }; FN[v](&c, 0, nb); }
                        for (uint32_t i = 0; i < hi; i++)
                            if (memcmp(&pa[i], &ref[i], sizeof(float))) {
                                if (bad[v] < 3)
                                    printf("%s MISMATCH dim=%u taps=%u pos=%u i=%u %.9g vs %.9g\n",
                                           NM[v], dim, taps, pos, i, pa[i], ref[i]);
                                bad[v]++;
                            }
                    }
                    cmp += hi;
                }
    }
    printf("TAP_EQUIV comparisons=%u mismatches hoist=%u tile2=%u fwd=%u bund=%u %s\n",
           cmp, bad[0], bad[1], bad[2], bad[3],
           (bad[0] | bad[1] | bad[2] | bad[3]) ? "FAIL" : "OK");
    return (bad[0] | bad[1] | bad[2] | bad[3]) != 0;
}
