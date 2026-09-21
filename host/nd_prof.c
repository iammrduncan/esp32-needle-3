/* nd_prof - host-side per-phase timing of the forward pass.
 *
 *   nd_prof <model.cact> [steps]
 *
 * Built with -DNEEDLE_PROFILE. Prints the same phase names as the firmware so
 * the two breakdowns can be read side by side. On x86 this is arithmetic-bound;
 * it says what fraction of the forward pass a phase is, not what it costs on a
 * board that streams its weights out of flash.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nd_model.h"

int main(int argc, char **argv)
{
    static const char *PN[ND_P_COUNT] = {
        "proj2bit", "attention", "hadamard", "mhc_phi4",
        "engram", "logits4", "prep+lut", "confpool" };
    FILE *f;
    nd_model m;
    void *blob;
    long  bytes;
    int   steps = argc > 2 ? atoi(argv[2]) : 10, i, p;
    double total = 0.0;
    struct timespec t0, t1;

    if (argc < 2) return 2;
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); bytes = ftell(f); rewind(f);
    blob = malloc((size_t)bytes);
    if (!blob || fread(blob, 1, (size_t)bytes, f) != (size_t)bytes) return 2;
    fclose(f);
    if (nd_model_open(&m, blob, (size_t)bytes) != 0) {
        fprintf(stderr, "nd_model_open FAILED\n");
        return 1;
    }

    printf("MODEL layers=%u d_model=%u vocab=%u\n",
           (unsigned)m.n_layers, (unsigned)m.d_model, (unsigned)m.vocab);
    memset(nd_prof, 0, sizeof(nd_prof));
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < steps; i++)
        nd_model_step_hidden(&m, (uint32_t)(100 + i));
    clock_gettime(CLOCK_MONOTONIC, &t1);
    total = (t1.tv_sec - t0.tv_sec) + 1e-9 * (t1.tv_nsec - t0.tv_nsec);

    printf("HOST_PROF steps=%d total=%.3f ms/tok\n", steps, total * 1000.0 / steps);
    for (p = 0; p < ND_P_COUNT; p++)
        printf("HOST_PROF %-10s %8.3f ms/tok %5.1f%%\n", PN[p],
               nd_prof[p] / 1000.0 / steps,
               100.0 * (nd_prof[p] / 1000.0 / steps) / (total * 1000.0 / steps));
    nd_model_close(&m);
    free(blob);
    return 0;
}
