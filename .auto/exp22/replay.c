/* Experiment 22 harness: replay the shipping mHC phi (4-bit generic path) GEMV
 * on the REAL captured fixture and require bit-exactness.
 *
 *   cc -O2 -ffp-contract=off -Iengine/include .auto/exp22/replay.c -lm -o /tmp/replay22
 *   /tmp/replay22 .auto/exp22/phi4-decode.bin
 *
 * The row expression is copied verbatim from engine/src/nd_quant.c
 * (dot_group's bits==4 path and gemv_rows_offset's accumulation), so a PASS
 * here proves the fixture is complete AND gives the exact bit-level target any
 * candidate kernel must match. Exit 0 only if every captured row matches on
 * every bit.
 *
 * `-ffp-contract=off` is mandatory: the engine is built that way and an FMA
 * here would disagree with the shipped kernel by construction.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAGIC 0x34465048u   /* 'HPF4' */

/* Use the ENGINE's own fp16 conversion rather than a copy: an fp16 routine of
 * my own could disagree with the kernel for a reason that has nothing to do
 * with the candidate under test, and that mistake would be invisible until a
 * board disagreed. nd_f16 is a static inline in the shipping header. */
#include "nd_quant.h"

/* Verbatim from nd_quant.c (bits == 4, aligned group start). */
static float dot_group4(const uint8_t *q, uint32_t g, const float *cb, const float *xh)
{
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
    uint32_t j;

    for (j = 0; j < g; j += 8) {
        uint32_t w = ((const uint32_t *)(const void *)q)[0];
        q += 4;
        s0 += cb[ w        & 15u] * xh[j];
        s1 += cb[(w >>  4) & 15u] * xh[j + 1];
        s2 += cb[(w >>  8) & 15u] * xh[j + 2];
        s3 += cb[(w >> 12) & 15u] * xh[j + 3];
        s0 += cb[(w >> 16) & 15u] * xh[j + 4];
        s1 += cb[(w >> 20) & 15u] * xh[j + 5];
        s2 += cb[(w >> 24) & 15u] * xh[j + 6];
        s3 += cb[ w >> 28]        * xh[j + 7];
    }
    return (s0 + s1) + (s2 + s3);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : ".auto/exp22/phi4-decode.bin";
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long sz;
    size_t off = 0, rows_checked = 0, bad = 0;
    unsigned recs = 0;

    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)sz);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "short read\n"); return 2; }
    fclose(f);

    while (off + 44 <= (size_t)sz) {
        uint32_t magic = *(uint32_t *)(buf + off);
        uint32_t out, in_pad, group, bits, ngroup, rowbytes, nsel, row0, nrows;
        const float *cb, *xh, *y;
        const uint8_t *rows;
        uint32_t i;

        if (magic != MAGIC) { fprintf(stderr, "bad magic at %zu\n", off); return 2; }
        memcpy(&out,      buf + off +  8, 4);
        memcpy(&in_pad,   buf + off + 12, 4);
        memcpy(&group,    buf + off + 16, 4);
        memcpy(&bits,     buf + off + 20, 4);
        memcpy(&ngroup,   buf + off + 24, 4);
        memcpy(&rowbytes, buf + off + 28, 4);
        memcpy(&nsel,     buf + off + 32, 4);
        memcpy(&row0,     buf + off + 36, 4);
        memcpy(&nrows,    buf + off + 40, 4);
        off += 44;
        cb  = (const float *)(buf + off);              off += (size_t)(1u << bits) * 4;
        xh  = (const float *)(buf + off);              off += (size_t)in_pad * 4;
        y   = (const float *)(buf + off);              off += (size_t)nrows * 4;
        rows = buf + off;

        for (i = 0; i < nsel; i++) {
            uint32_t row;
            const uint8_t *rp, *np;
            const uint16_t *nrm;
            float acc = 0.0f, got, want;
            uint32_t gi;
            size_t p = off + (size_t)i * (4 + rowbytes + (size_t)ngroup * 2);

            memcpy(&row, buf + p, 4);
            rp  = buf + p + 4;
            np  = buf + p + 4 + rowbytes;
            nrm = (const uint16_t *)(const void *)np;
            for (gi = 0; gi < ngroup; gi++)
                acc += nd_f16(nrm[gi]) *
                       dot_group4(rp + (size_t)gi * group * bits / 8, group, cb,
                                  xh + (size_t)gi * group);
            /* gemv_rows_offset writes y[i] with i relative to the slice, and the
             * capture handed the kernel row0 as the tensor row base. */
            want = y[row - row0];
            got  = acc;
            rows_checked++;
            if (memcmp(&want, &got, sizeof want) != 0) {
                if (bad < 6)
                    printf("MISMATCH rec=%u row=%u want=%08x got=%08x diff=%.6g\n",
                           recs, row, *(uint32_t *)&want, *(uint32_t *)&got,
                           (double)fabsf(want - got));
                bad++;
            }
        }
        off += (size_t)nsel * (4 + rowbytes + (size_t)ngroup * 2);
        (void)out;
        recs++;
    }
    printf("PHI4_REPLAY records=%u rows=%zu mismatches=%zu %s\n",
           recs, rows_checked, bad, bad ? "FAILED" : "BITEXACT");
    free(buf);
    return bad ? 1 : 0;
}
