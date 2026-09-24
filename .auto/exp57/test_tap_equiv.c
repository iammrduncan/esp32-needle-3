/* Bit-equivalence guard for taphoist (#exp57): the shipped per-element remainder form versus
 * the per-call row-pointer form, over every startup position and tap count the archive can
 * produce (and one extra). Each output must be BIT-identical, so any mismatch is a transcription
 * bug - the class that cost runs #363 and #380 a board each. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { float *proj, *hist; const float *w;
                 uint32_t taps, pos, dim; } tap_ctx;

static void shipped(void *vc, uint32_t b0, uint32_t b1)
{
    const tap_ctx *c = (const tap_ctx *)vc;
    uint32_t i, j, dim = c->dim;
    uint32_t lo = b0 * 256, hi = (b1 * 256 < dim) ? b1 * 256 : dim;
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
    uint32_t lo = b0 * 256, hi = (b1 * 256 < dim) ? b1 * 256 : dim;
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

int main(void)
{
    const uint32_t dims[] = { 96, 128, 576, 768, 33 };
    uint32_t bad = 0, cases = 0;
    float *w = malloc(sizeof(float) * 4 * 768), *h = malloc(sizeof(float) * 4 * 768);
    float *pa = malloc(sizeof(float) * 768), *pb = malloc(sizeof(float) * 768);

    for (uint32_t s = 0; s < 4u * 768u; s++) {           /* deterministic mixed-sign operands */
        w[s] = (float)((s * 37 + 11) % 997) * 1.3e-3f - 0.64f;
        h[s] = (float)((s * 91 + 7) % 1013) * 1.7e-3f - 0.86f;
    }
    for (unsigned di = 0; di < 5u; di++) {
        uint32_t dim = dims[di];
        for (uint32_t taps = 1; taps <= 4; taps++)
            for (uint32_t pos = 0; pos < 2 * taps + 2; pos++)
                for (uint32_t nb = 1; nb <= (dim + 255) / 256; nb++) {
                    tap_ctx c = { pa, h, w, taps, pos, dim };
                    /* Only [0,hi) is written by a nb-block range - comparing the untouched
                     * tail would just compare the two fill patterns (the first version of this
                     * guard did, and reported 32,256 false mismatches at i=256). */
                    uint32_t hi = (nb * 256 < dim) ? nb * 256 : dim;
                    memset(pa, 0xA5, sizeof(float) * dim);
                    shipped(&c, 0, nb);
                    memcpy(pb, pa, sizeof(float) * dim);
                    memset(pa, 0x5A, sizeof(float) * dim);
                    c.proj = pa;
                    hoisted(&c, 0, nb);
                    for (uint32_t i = 0; i < hi; i++)
                        if (memcmp(&pa[i], &pb[i], sizeof(float))) {
                            if (bad < 3)
                                printf("MISMATCH dim=%u taps=%u pos=%u i=%u %.9g vs %.9g\n",
                                       dim, taps, pos, i, pa[i], pb[i]);
                            bad++;
                        }
                    cases += hi;
                }
    }
    printf("TAP_EQUIV comparisons=%u mismatches=%u %s\n", cases, bad, bad ? "FAIL" : "OK");
    return bad != 0;
}
