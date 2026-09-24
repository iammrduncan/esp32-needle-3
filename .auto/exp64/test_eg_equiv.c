/* Bit-equivalence guard for the engram tap hoist (#exp64): the shipped tap-OUTER read-modify-write
 * sweep versus the per-call row resolution with a register accumulator. `out` must come out
 * BIT-identical, including which taps are skipped (`back > pos`) and the all-skipped case, so any
 * mismatch is a transcription bug. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ND_EG_HIST 16
typedef struct { float *out; const float *tp, *hist;
                 uint32_t dm, dil, taps, pos, eg_pos, site; } egtap_ctx;

static uint32_t hi_of(uint32_t dm, uint32_t nb) { return nb * 256 < dm ? nb * 256 : dm; }

static void shipped(void *vc, uint32_t b0, uint32_t b1)
{
    const egtap_ctx *c = (const egtap_ctx *)vc;
    uint32_t dm = c->dm, lo = b0 * 256, hi = hi_of(dm, b1), j, d;
    for (d = lo; d < hi; d++) c->out[d] = 0.0f;
    for (j = 0; j < c->taps; j++) {
        uint32_t back = j * c->dil;
        const float *src;
        if (back > c->pos) continue;
        src = c->hist + ((size_t)c->site * ND_EG_HIST + ((c->eg_pos - back) % ND_EG_HIST)) * dm;
        for (d = lo; d < hi; d++) c->out[d] += c->tp[j * dm + d] * src[d];
    }
}

static void hoisted(void *vc, uint32_t b0, uint32_t b1)
{
    const egtap_ctx *c = (const egtap_ctx *)vc;
    uint32_t dm = c->dm, lo = b0 * 256, hi = hi_of(dm, b1), j, d, nt = 0;
    const float *hp[ND_EG_HIST], *wp[ND_EG_HIST];
    if (c->taps > ND_EG_HIST) { shipped(vc, b0, b1); return; }     /* the shipped-fallback path */
    for (j = 0; j < c->taps; j++) {
        uint32_t back = j * c->dil;
        if (back > c->pos) continue;
        wp[nt] = c->tp + (size_t)j * dm;
        hp[nt] = c->hist + ((size_t)c->site * ND_EG_HIST + ((c->eg_pos - back) % ND_EG_HIST)) * dm;
        nt++;
    }
    if (nt == 0u) { for (d = lo; d < hi; d++) c->out[d] = 0.0f; return; }
    for (d = lo; d < hi; d++) {
        float value = 0.0f;
        for (j = 0; j < nt; j++) value += wp[j][d] * hp[j][d];
        c->out[d] = value;
    }
}

int main(void)
{
    const uint32_t dims[] = { 96, 576, 768, 33 };
    static float hist[4 * ND_EG_HIST * 768], tp[8 * 768];
    float *out = malloc(sizeof(float) * 768), *ref = malloc(sizeof(float) * 768);
    uint32_t bad = 0, cmp = 0;
    for (size_t s = 0; s < sizeof(hist)/sizeof(*hist); s++)
        hist[s] = (float)((s * 37 + 5) % 991) * 1.9e-3f - 0.94f;
    for (size_t s = 0; s < sizeof(tp)/sizeof(*tp); s++)
        tp[s] = (float)((s * 61 + 13) % 1009) * 1.1e-3f - 0.55f;

    for (unsigned di = 0; di < 4u; di++) {
        uint32_t dm = dims[di];
        for (uint32_t taps = 1; taps <= 8; taps++)
            for (uint32_t dil = 1; dil <= 4; dil++)
                for (uint32_t pos = 0; pos < 3 * dil + 1; pos++)
                    for (uint32_t eg_pos = 0; eg_pos < ND_EG_HIST + 2; eg_pos++)
                        for (uint32_t site = 0; site < 3; site++)
                            for (uint32_t nb = 1; nb <= (dm + 255) / 256; nb++) {
                                uint32_t hi = hi_of(dm, nb);
                                egtap_ctx c = { out, tp, hist, dm, dil, taps, pos, eg_pos, site };
                                memset(out, 0xA5, sizeof(float) * dm);
                                shipped(&c, 0, nb);
                                memcpy(ref, out, sizeof(float) * dm);
                                memset(out, 0x5A, sizeof(float) * dm);
                                c.out = out;
                                hoisted(&c, 0, nb);
                                for (uint32_t d = 0; d < hi; d++)
                                    if (memcmp(&out[d], &ref[d], sizeof(float))) {
                                        if (bad < 3)
                                            printf("MISMATCH dm=%u taps=%u dil=%u pos=%u eg_pos=%u site=%u d=%u %.9g vs %.9g\n",
                                                   dm, taps, dil, pos, eg_pos, site, d, out[d], ref[d]);
                                        bad++;
                                    }
                                cmp += hi;
                            }
    }
    printf("EGTAP_EQUIV comparisons=%u mismatches=%u %s\n", cmp, bad, bad ? "FAIL" : "OK");
    return bad != 0;
}
