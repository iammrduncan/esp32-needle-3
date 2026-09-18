/* nd_ftest - forward-pass fidelity against a frozen logits dump.
 *
 *   nd_ftest <model.cact> <golden.txt> [ids...]
 *
 * Runs the same fixed probe sequence as the dump, then reports the worst
 * absolute logits difference and how many steps kept the same argmax. This is
 * the fast quality gate: a speedup that changes the numbers is a quality
 * change, and it has to say so here long before it costs a device run.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nd_model.h"

#define MAX_V 16384

int main(int argc, char **argv)
{
    static float probe[MAX_V * 16];
    const char  *golden_path;
    FILE        *f;
    long         n = 0, cap = 0, step, v, ids_n;
    float       *ref, *got;
    double       worst = 0.0;
    int          agree = 0, rc = 0;
    uint32_t     i;

    if (argc < 3) {
        fprintf(stderr, "usage: nd_ftest <model.cact> <golden.txt> [token_ids...]\n");
        return 2;
    }
    golden_path = argv[2];
    ids_n       = argc - 3;

    f = fopen(golden_path, "r");
    if (!f) { perror(golden_path); return 2; }
    cap = 64;
    ref = (float *)malloc(sizeof(float) * (size_t)cap);
    {
        double d;
        while (fscanf(f, "%lf", &d) == 1) {
            if (n == cap) {
                cap *= 2;
                ref = (float *)realloc(ref, sizeof(float) * (size_t)cap);
            }
            ref[n++] = (float)d;
        }
    }
    fclose(f);
    if (n == 0 || ids_n <= 0 || n % ids_n) {
        fprintf(stderr, "golden shape mismatch: %ld values, %ld steps\n", n, ids_n);
        free(ref);
        return 2;
    }
    v = n / ids_n;
    if (v > MAX_V) { free(ref); return 2; }

    {
        FILE *mf = fopen(argv[1], "rb");
        void *blob;
        long  bytes;
        if (!mf) { perror(argv[1]); free(ref); return 2; }
        fseek(mf, 0, SEEK_END); bytes = ftell(mf); rewind(mf);
        blob = malloc((size_t)bytes);
        if (!blob || fread(blob, 1, (size_t)bytes, mf) != (size_t)bytes) {
            fprintf(stderr, "read failed\n"); free(ref); return 2;
        }
        fclose(mf);
        {
            nd_model m;
            if (nd_model_open(&m, blob, (size_t)bytes) != 0) {
                fprintf(stderr, "model open failed\n"); free(ref); return 2;
            }
            got = probe;
            for (step = 0; step < ids_n; step++) {
                const float *lg;
                i = (uint32_t)atoi(argv[3 + step]);
                lg = nd_model_step(&m, i);
                memcpy(got + step * v, lg, sizeof(float) * (size_t)v);
            }
            nd_model_close(&m);
        }
        free(blob);
    }

    for (step = 0; step < ids_n; step++) {
        const float *a = ref + step * v, *b = got + step * v;
        uint32_t     ja = 0, jb = 0, k;
        double       d;
        for (k = 0; k < v; k++) {
            d = (double)(a[k] > b[k] ? a[k] - b[k] : b[k] - a[k]);
            if (d > worst) worst = d;
            if (a[k] > a[ja]) ja = k;
            if (b[k] > b[jb]) jb = k;
        }
        agree += (ja == jb);
    }
    printf("FIDELITY steps=%ld vocab=%ld max_delta=%.3e top1=%ld/%ld\n",
           ids_n, v, worst, (long)agree, ids_n);
    if (worst > 0.002 || agree != ids_n)
        rc = 1;
    free(ref);
    return rc;
}
