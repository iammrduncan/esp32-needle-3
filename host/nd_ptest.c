/* Verify that schema switches cannot leak KV/convolution state across passes.
 * Uses a real model archive; no synthetic inference responses. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nd_model.h"

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); long bytes = ftell(f); rewind(f);
    void *blob = malloc(bytes);
    if (!blob || fread(blob, 1, bytes, f) != (size_t)bytes) return 2;
    fclose(f);
    nd_model m;
    if (nd_model_open(&m, blob, bytes)) return 2;
    nd_prefix *prefix[2];
    float expected[2][4];
    const uint32_t rows[] = {0, 17, 100, 201};
    for (int i = 0; i < 2; i++) {
        nd_model_reset(&m);
        nd_model_step_hidden(&m, 13 + i);
        nd_model_step_hidden(&m, 44 + i);
        if (nd_model_snapshot(&m)) return 2;
        prefix[i] = nd_model_prefix_save(&m);
        if (!prefix[i]) return 2;
        const float *h = nd_model_step_hidden(&m, 100);
        nd_model_logits_subset(&m, h, rows, 4, expected[i]);
    }
    for (int n = 0; n < 4; n++) {
        int i = n % 2;
        if (nd_model_prefix_restore(&m, prefix[i])) return 2;
        if (m.pos != 2 || m.n_sink != 2) return 1;
        float actual[4];
        const float *h = nd_model_step_hidden(&m, 100);
        nd_model_logits_subset(&m, h, rows, 4, actual);
        if (memcmp(actual, expected[i], sizeof(actual))) {
            fprintf(stderr, "prefix %d changed logits after switch\n", i); return 1;
        }
    }
    nd_model_prefix_free(prefix[0]); nd_model_prefix_free(prefix[1]);
    nd_model_close(&m); free(blob);
    puts("PASS: alternating prefixes reproduce identical logits");
    return 0;
}
