/* Rule #1 test for the attention softmax exact-zero skip. The shipped expression is
 * reproduced here as it stands at engine/src/nd_model.c:1352 - one nd_expf_pair() for
 * the two positions of a pair - and compared BIT FOR BIT against the guarded form, over
 * the 49,152 argument pairs Experiment 12 captured from the six frozen primary
 * generations, plus dense sweeps and the pathological edges. The goldens cannot see a
 * numeric bug in an argument range the frozen prompts never reach (runs #230-231). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nd_quant.h"

static void ship(float d0, float d1, float *w0, float *w1)
{
    nd_expf_pair(d0, d1, w0, w1);
}

static void cand(float d0, float d1, float *w0, float *w1)
{
    if (d0 == 0.0f) {
        *w0 = 1.0f;
        *w1 = (d1 == 0.0f) ? 1.0f : nd_expf(d1);
    } else if (d1 == 0.0f) {
        *w1 = 1.0f;
        *w0 = nd_expf(d0);
    } else {
        nd_expf_pair(d0, d1, w0, w1);
    }
}

static int bits_eq(float a, float b){ return memcmp(&a,&b,4)==0; }

int main(void)
{
    long bad = 0, n = 0, fzero = 0;
    FILE *f;

    /* Precondition: the literal really is what the shipped kernel returns. */
    if (!bits_eq(nd_expf(0.0f), 1.0f) || !bits_eq(nd_expf(-0.0f), 1.0f)) {
        printf("PRECONDITION FAILED: nd_expf(+/-0) != 1.0f\n"); return 1;
    }
    printf("precondition ok: nd_expf(+0.0f) and nd_expf(-0.0f) are exactly 1.0f\n");

    /* 1. the real captured fixture */
    f = fopen("pairs.bin", "rb");
    if (f) {
        float buf[4096]; size_t got;
        while ((got = fread(buf, sizeof(float), 4096, f)) > 0) {
            for (size_t i = 0; i + 1 < got; i += 2) {
                float a0,a1,b0,b1;
                ship(buf[i], buf[i+1], &a0, &a1);
                cand(buf[i], buf[i+1], &b0, &b1);
                n += 2;
                if (buf[i] == 0.0f || buf[i+1] == 0.0f) fzero++;
                if (!bits_eq(a0,b0) || !bits_eq(a1,b1)) {
                    bad++;
                    if (bad < 4) printf("MISMATCH pair %g %g -> %g/%g want %g/%g\n",
                                        buf[i],buf[i+1],b0,b1,a0,a1);
                }
            }
        }
        fclose(f);
        printf("captured fixture: %ld elements, %ld pairs had an exact zero, %ld mismatches\n",
               n, fzero, bad);
    } else {
        printf("pairs.bin not found - running sweeps only\n");
    }

    /* 2. dense sweeps: one-zero, both-zero, near-zero, clamp edges, signed zeros */
    for (float x = -90.0f; x <= 90.0f; x += 0.05f) {
        for (int k = 0; k < 6; k++) {
            float p[6][2];
            p[0][0]=0.0f;                 p[0][1]=x;
            p[1][0]=x;                    p[1][1]=0.0f;
            p[2][0]=-0.0f;                p[2][1]=x;
            p[3][0]=x;                    p[3][1]=-0.0f;
            p[4][0]=x;                    p[4][1]=x;
            p[5][0]=x;                    p[5][1]=x-1e-8f;
            for (int q = 0; q < 6; q++) {
                float a0,a1,b0,b1;
                ship(p[q][0], p[q][1], &a0,&a1);
                cand(p[q][0], p[q][1], &b0,&b1);
                n += 2;
                if (!bits_eq(a0,b0) || !bits_eq(a1,b1)) {
                    bad++;
                    if (bad < 8) printf("SWEEP MISMATCH %g %g -> %.9g/%.9g want %.9g/%.9g\n",
                                        p[q][0],p[q][1],b0,b1,a0,a1);
                }
            }
        }
    }
    /* 3. non-finite and huge arguments must fall through untouched */
    {
        float edge[]={0.0f/0.0f, 0.0f/0.0f, 1.0f/0.0f, -1.0f/0.0f, 1e38f, -1e38f,
                      88.0f, -88.0f, 87.99f, -88.01f, 0.0f, -0.0f};
        unsigned N = sizeof(edge)/sizeof(edge[0]);
        for (unsigned i = 0; i < N; i++) for (unsigned j = 0; j < N; j++) {
            float a0,a1,b0,b1;
            ship(edge[i], edge[j], &a0,&a1);
            cand(edge[i], edge[j], &b0,&b1);
            n += 2;
            /* NaN payloads may differ legitimately; compare equality-as-the-model-sees-it */
            int nan_a = (a0!=a0), nan_b = (b0!=b0), nan_a1 = (a1!=a1), nan_b1 = (b1!=b1);
            if (nan_a != nan_b || nan_a1 != nan_b1) { bad++; printf("NaN CLASS MISMATCH %g %g\n",edge[i],edge[j]); }
            else if (!nan_a && !bits_eq(a0,b0)) { bad++; printf("EDGE MISMATCH0 %g %g -> %g want %g\n",edge[i],edge[j],b0,a0); }
            else if (!nan_a1 && !bits_eq(a1,b1)) { bad++; printf("EDGE MISMATCH1 %g %g -> %g want %g\n",edge[i],edge[j],b1,a1); }
        }
    }
    printf("TOTAL elements=%ld mismatches=%ld\n", n, bad);
    return bad != 0;
}
