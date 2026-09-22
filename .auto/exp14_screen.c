/* Experiment 14 - CQ2 integer feasibility screen (host, no device).
 *
 * Fixture: real layer-0 q_proj records captured through the shipping float
 * pair-table kernel (nd_cq2_capture, capture build only). Each record carries
 * the prepared activation xh, the pair table the kernel used, the kernel's
 * real output vector, and a spread of rows' packed 2-bit bytes + fp16 norms.
 *
 * Variants, all scored against the captured float output:
 *   self  float reference replayed from the captured table - must be bit-exact,
 *         which proves the index decode and norm model are the kernel's.
 *   A     int8 activation (one tensor-wide scale) x int8 codebook -> int32.
 *   Apg   same, one activation scale per FWHT group.
 *   Axc   int8 activation, codebook kept exact (isolates codebook error).
 *   A16   int16 activation x int8 codebook (ceiling of the integer form).
 *   B     boot-time int8 expansion of the rows (one byte per weight = the int8
 *         codebook level, per-group scale |norm|*max|cb|). Equal to A by
 *         construction; checked, so the difference is bytes, not error.
 *
 * Usage: exp14_screen <fixture.bin> [more...]
 * Exit 0 only if the self replay is bit-exact.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nd_quant.h"

#define MAGIC  0x43513246u
#define NVAR   6
#define NGRP_MAX 16

typedef struct {
    uint32_t magic, seq, out, in_pad, group, ngroup, rowbytes, nsel;
    float    cb[4];
} rec;

typedef struct { double maxabs, sumsq, maxrel, n, reln; } err;

static void eadd(err *e, double got, double ref)
{
    double d = fabs(got - ref);

    e->n++;
    e->sumsq += d * d;
    if (d > e->maxabs) e->maxabs = d;
    if (fabs(ref) > 1e-6) {
        double r = d / fabs(ref);
        if (r > e->maxrel) e->maxrel = r;
        e->reln++;
    }
}

/* The shipping row walker (nd_lut2_rows_c) replayed on the captured table. */
static float replay(const rec *r, const float *lut, const uint8_t *pk,
                    const uint16_t *nm)
{
    uint32_t gi;
    float    acc = 0.0f;

    for (gi = 0; gi < r->ngroup; gi++) {
        const uint8_t *q  = pk + (size_t)gi * (r->rowbytes / r->ngroup);
        const float   *T  = lut + ((size_t)gi * (r->group / 2)) * 16;
        float          s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        uint32_t        j;

        for (j = 0; j < r->group; j += 16) {
            uint32_t w = ((const uint32_t *)(const void *)q)[0];
            q += 4;
            s0 += T[         w         & 15u];
            s1 += T[16 + ((w >>  4)  & 15u)];
            s2 += T[32 + ((w >>  8)  & 15u)];
            s3 += T[48 + ((w >> 12)  & 15u)];
            T += 64;
            s0 += T[         (w >> 16) & 15u];
            s1 += T[16 + ((w >> 20)  & 15u)];
            s2 += T[32 + ((w >> 24)  & 15u)];
            s3 += T[48 +  (w >> 28)];
            T += 64;
        }
        acc += nd_f16(nm[gi]) * ((s0 + s1) + (s2 + s3));
    }
    return acc;
}

/* One group's int32 partial: sum_j codebook[idx_j] * xq[j].
 * Packing: weight k is 2-bit field k, so pair p (weights 2p,2p+1) is nibble p,
 * i.e. byte p/2 half p%2 -> the two elements of byte b half h are 4b+2h and
 * 4b+2h+1. Same decode the shipping kernel does. */
static int32_t igroup(const uint8_t *q, uint32_t bytes, const void *xq,
                      int wide, const int8_t *cbq)
{
    int32_t   s = 0;
    uint32_t   b;

    for (b = 0; b < bytes; b++) {
        uint8_t v = q[b];
        uint32_t h;

        for (h = 0; h < 2; h++) {
            uint8_t  nib = (uint8_t)((v >> (4 * h)) & 15u);
            size_t   e   = (size_t)(4 * b + 2 * h);

            if (wide) {
                const int16_t *x = (const int16_t *)xq;
                s += (int32_t)cbq[nib & 3] * x[e] + (int32_t)cbq[nib >> 2] * x[e + 1];
            } else {
                const int8_t *x = (const int8_t *)xq;
                s += (int32_t)cbq[nib & 3] * x[e] + (int32_t)cbq[nib >> 2] * x[e + 1];
            }
        }
    }
    return s;
}

int main(int argc, char **argv)
{
    err   E[NVAR];
    char *names[NVAR] = { "self", "A", "Apg", "Axc", "A16", "B" };
    double refmax = 0, refmean = 0, nref = 0, bytes2 = 0, bytesb = 0, qcost = 0, selfn = 0;
    uint32_t out = 0, in_pad = 0, nrec = 0;
    int      ai, v, bad = 0;

    if (argc < 2) { fprintf(stderr, "usage: %s fixture.bin [...]\n", argv[0]); return 2; }
    memset(E, 0, sizeof E);

    for (ai = 1; ai < argc; ai++) {
        FILE *f = fopen(argv[ai], "rb");
        rec   r;

        if (!f) { perror(argv[ai]); return 1; }
        while (fread(&r, sizeof r, 1, f) == 1) {
            float    *xh, *lut, *y;
            uint8_t  *sel;                 /* nsel * stride: row bytes then norms */
            size_t    stride;
            int8_t   *xq8, *xq8pg, cbq[4];
            int16_t  *xq16;
            double    maxa = 0, maxcb = 0, sxa, scb;
            double    scale_g[NGRP_MAX];
            uint32_t   gi, ri, j, qb;

            if (r.magic != MAGIC)  { fprintf(stderr, "bad magic\n");  return 1; }
            if (r.ngroup > NGRP_MAX) { fprintf(stderr, "ngroup too big\n"); return 1; }
            stride = r.rowbytes + (size_t)r.ngroup * 2;
            xh    = malloc(sizeof(float) * r.in_pad);
            lut   = malloc(sizeof(float) * (size_t)r.in_pad / 2 * 16);
            y     = malloc(sizeof(float) * r.out);
            sel   = malloc(stride * r.nsel);
            xq8   = malloc(r.in_pad);
            xq8pg = malloc(r.in_pad);
            xq16  = malloc(sizeof(int16_t) * r.in_pad);
            if (fread(xh, sizeof(float), r.in_pad, f) != r.in_pad ||
                fread(lut, sizeof(float), (size_t)r.in_pad / 2 * 16, f) != (size_t)r.in_pad / 2 * 16 ||
                fread(y, sizeof(float), r.out, f) != r.out) {
                fprintf(stderr, "short record\n"); return 1;
            }
            /* the fixture interleaves each row's packed bytes with its norms */
            for (ri = 0; ri < r.nsel; ri++) {
                uint8_t *p = sel + (size_t)ri * stride;
                if (fread(p, r.rowbytes, 1, f) != 1 ||
                    fread(p + r.rowbytes, sizeof(uint16_t), r.ngroup, f) != r.ngroup) {
                    fprintf(stderr, "short record\n"); return 1;
                }
            }
            nrec++;
            out = r.out; in_pad = r.in_pad;
            qb  = r.rowbytes / r.ngroup;            /* packed bytes per group */

            /* ---- quantisation the integer path would pay per token ---- */
            for (j = 0; j < r.in_pad; j++)
                if (fabs(xh[j]) > maxa) maxa = fabs(xh[j]);
            for (j = 0; j < 4; j++)
                if (fabs(r.cb[j]) > maxcb) maxcb = fabs(r.cb[j]);
            sxa = maxa / 127.0;
            scb = maxcb / 127.0;
            for (j = 0; j < r.in_pad; j++) {
                int a = (int)lrint(xh[j] / sxa);
                int b = (int)lrint(xh[j] / (maxa / 32767.0));
                xq8[j]  = (int8_t)(a > 127 ? 127 : a < -127 ? -127 : a);
                xq16[j] = (int16_t)(b > 32767 ? 32767 : b < -32767 ? -32767 : b);
            }
            for (j = 0; j < 4; j++) {
                int a = (int)lrint(r.cb[j] / scb);
                cbq[j] = (int8_t)(a > 127 ? 127 : a < -127 ? -127 : a);
            }
            for (gi = 0; gi < r.ngroup; gi++) {     /* per-group act scale */
                double mg = 0;
                for (j = 0; j < r.group; j++) {
                    double a = fabs(xh[(size_t)gi * r.group + j]);
                    if (a > mg) mg = a;
                }
                scale_g[gi] = mg / 127.0;
                for (j = 0; j < r.group; j++) {     /* quantised on that scale */
                    int v = (int)lrint(xh[(size_t)gi * r.group + j] / scale_g[gi]);
                    xq8pg[(size_t)gi * r.group + j] = (int8_t)(v > 127 ? 127 : v < -127 ? -127 : v);
                }
            }
            qcost   += r.in_pad;                                       /* 768 div+lrint + 4 */
            bytes2  += (double)r.out * r.rowbytes + (double)r.out * r.ngroup * 2;
            bytesb  += (double)r.out * r.in_pad;                       /* int8 rows */

            for (ri = 0; ri < r.nsel; ri++) {
                const uint8_t  *pk = sel + (size_t)ri * stride;
                const uint16_t *nm = (const uint16_t *)(const void *)(pk + r.rowbytes);
                uint32_t        rowid = (uint32_t)((uint64_t)ri * r.out / r.nsel);
                double          got[NVAR];
                float           ref = replay(&r, lut, pk, nm);

                selfn++;
                if (ref != y[rowid]) { if (bad < 4) printf("SELF MISMATCH rec %u row %u: %.9g vs %.9g\n",
                                              r.seq, rowid, ref, y[rowid]); bad++; }
                else E[0].n++;
                for (v = 0; v < NVAR; v++) got[v] = 0;
                got[0] = (double)ref;
                for (gi = 0; gi < r.ngroup; gi++) {
                    const uint8_t *q  = pk + (size_t)gi * qb;
                    double         nf = nd_f16(nm[gi]);
                    double         axc = 0;
                    /* element indices inside igroup are group-local */
                    int32_t        S   = igroup(q, qb, xq8  + (size_t)gi * r.group,      0, cbq);
                    int32_t        S16 = igroup(q, qb, xq16 + (size_t)gi * r.group,      1, cbq);
                    int32_t        Spg = igroup(q, qb, xq8pg + (size_t)gi * r.group,      0, cbq);
                    uint32_t        k;

                    for (k = 0; k < qb; k++) {      /* exact cb, int8 activation */
                        for (uint32_t h = 0; h < 2; h++) {
                            uint8_t nib = (uint8_t)((q[k] >> (4 * h)) & 15u);
                            axc += r.cb[nib & 3] * (double)xq8[(size_t)gi * r.group + 4 * k + 2 * h]
                                 + r.cb[nib >> 2] * (double)xq8[(size_t)gi * r.group + 4 * k + 2 * h + 1];
                        }
                    }

                    got[1] += nf * sxa * scb * S;                    /* A      */
                    got[2] += nf * scale_g[gi] * scb * Spg;          /* Apg    */
                    got[3] += nf * sxa * axc;                        /* Axc    */
                    got[4] += nf * (maxa / 32767.0) * scb * S16;     /* A16    */
                    /* B: stored byte = round(cb*norm/(|norm|*scb)) = sign(nf)*cbq,
                     * dequant scale |norm|*scb*sxa -> algebraically identical to A. */
                    got[5] += (nf < 0 ? -nf : nf) * scb * sxa * (double)S * (nf < 0 ? -1.0 : 1.0);
                }
                for (v = 1; v < NVAR; v++) eadd(&E[v], got[v], (double)y[rowid]);
                refmax += fabs(y[rowid]);
                if (fabs(y[rowid]) > 0) refmean += fabs(y[rowid]), nref++;
            }
            free(xh); free(lut); free(y); free(sel); free(xq8); free(xq8pg); free(xq16);
        }
        fclose(f);
    }

    printf("fixture: real layer-0 q_proj, %u records, shape %ux%u\n", nrec, out, in_pad);
    printf("output scale over the scored rows: mean|y|=%.6g  (all records)\n",
           nref ? refmean / nref : 0);
    for (v = 0; v < NVAR; v++) {
        err *e = &E[v];
        if (v == 0) { printf("  %-5s bit-exact rows=%d/%d\n", names[v], (int)e->n, (int)selfn); continue; }
    printf("  %-5s max_abs=%.4e  rms=%.4e  max_rel=%.4e  (n=%d)\n",
               names[v], e->maxabs, sqrt(e->sumsq / (e->n ? e->n : 1)), e->maxrel, (int)e->n);
    }
    printf("bytes read per q_proj token: 2-bit packed+norms=%.0f   int8 rows=%.0f (%.2fx)\n",
           bytes2 / nrec, bytesb / nrec, bytesb / bytes2);
    printf("activation quantisation per token: %.0f elements (one prepared activation)\n",
           qcost / nrec);
    printf("A and B identical (byte-quantised codebook is the same arithmetic): %s\n",
           E[1].maxabs == E[5].maxabs ? "yes" : "NO");
    printf("float replay bit-exact: %s\n", bad ? "NO" : "yes");
    return bad;
}
