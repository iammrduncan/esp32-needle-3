/* Experiment 41 guard: does fwht_rows handle an ODD number of groups per call?
 *
 * The device splits ngroup=6 as 3+3 across the two cores, so each nd_parallel_rows
 * invocation gets an odd count. The host's nd_parallel_rows is rows_serial, so it
 * always sees 6 - an even count - and a paired-group loop without a tail is
 * therefore invisible to the host goldens while it corrupts every device run.
 * That is run #333's class (a defect in the range form no single-row test can
 * reach), so the split is exercised here through the public prepare entry, on the
 * real static fwht_rows, with no transcription of the loop anywhere (#363's class).
 *
 * Build against the host engine with the candidate applied:
 *   cc -O1 -Iengine/include .auto/exp41/test_odd_split.c \
 *      host/build/libneedle_engine.a -lm -o /tmp/odd_split
 * Exit 0 only if every split reproduces the serial activation bit for bit.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nd_cact.h"
#include "nd_model.h"
#include "nd_quant.h"

static void serial_split(nd_row_fn fn, void *ctx, uint32_t n) { fn(ctx, 0, n); }
static void split_3_3(nd_row_fn fn, void *ctx, uint32_t n) { fn(ctx, 0, n / 2 + n % 2); fn(ctx, n / 2 + n % 2, n); }
static void split_1_5(nd_row_fn fn, void *ctx, uint32_t n) { fn(ctx, 0, 1); fn(ctx, 1, n); }
static void split_5_1(nd_row_fn fn, void *ctx, uint32_t n) { fn(ctx, 0, n - 1); fn(ctx, n - 1, n); }
static void split_2_4(nd_row_fn fn, void *ctx, uint32_t n) { fn(ctx, 0, 2); fn(ctx, 2, n); }
static void split_each(nd_row_fn fn, void *ctx, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) fn(ctx, i, i + 1);
}

int main(int argc, char **argv)
{
    static struct { const char *name; void (*fn)(nd_row_fn, void *, uint32_t); } modes[] = {
        { "serial",     serial_split },
        { "3_3",        split_3_3    },
        { "1_5",        split_1_5    },
        { "5_1",        split_5_1    },
        { "2_4",        split_2_4    },
        { "one_group",  split_each   },
    };
    nd_cact c;
    const nd_tensor *t;
    float *x, *ref, *got;
    uint32_t ti, mi, in_pad, li;
    int bad = 0, tensors = 0;

    size_t blob_size;
    unsigned char *blob;
    FILE *fp = fopen(argc > 1 ? argv[1] : "model/needle3.cact", "rb");

    if (!fp) { perror("archive"); return 2; }
    fseek(fp, 0, SEEK_END); blob_size = (size_t)ftell(fp); fseek(fp, 0, SEEK_SET);
    blob = malloc(blob_size);
    if (fread(blob, 1, blob_size, fp) != blob_size) { fprintf(stderr, "short read\n"); return 2; }
    fclose(fp);
    if (nd_cact_open(&c, blob, blob_size) != 0) { fprintf(stderr, "cannot open archive\n"); return 2; }
    x   = malloc(sizeof(float) * 4096);
    ref = malloc(sizeof(float) * 4096);
    got = malloc(sizeof(float) * 4096);

    /* A few spread-out CQ tensors with a group count of 6 (the geometry every
     * attention projection uses) plus whatever else the archive holds: the point
     * is to run the real fwht_rows over real prepared activations. */
    for (ti = 0; ti < c.n && tensors < 6; ti++) {
        nd_tensor tt;
        if (nd_cact_tensor(&c, ti, &tt) != 0) continue;
        t = &tt;
        if (t->bits != 2 || t->group != 128 || t->shape[1] > 4000)
            continue;
        in_pad = nd_cq_in_pad(t);
        if (in_pad / t->group < 2)
            continue;
        for (li = 0; li < t->shape[1]; li++)
            x[li] = (float)((int)(li % 17) - 8) * 0.25f + 0.0625f;
        nd_parallel_rows = modes[0].fn;
        nd_cq_prepare(t, x, ref);
        for (mi = 1; mi < sizeof(modes) / sizeof(modes[0]); mi++) {
            nd_parallel_rows = modes[mi].fn;
            nd_cq_prepare(t, x, got);
            nd_parallel_rows = serial_split;
            if (memcmp(ref, got, sizeof(float) * in_pad) != 0) {
                uint32_t i, first = in_pad;
                for (i = 0; i < in_pad; i++)
                    if (memcmp(&ref[i], &got[i], sizeof(float))) { first = i; break; }
                printf("MISMATCH tensor=%u split=%s first_cell=%u ref=%.9g got=%.9g\n",
                       ti, modes[mi].name, first, ref[first], got[first]);
                bad++;
            }
        }
        nd_parallel_rows = serial_split;
        tensors++;
    }
    printf("ODD_SPLIT tensors=%u splits=5 mismatches=%d\n", tensors, bad);
    free(blob);
    free(x); free(ref); free(got);
    return bad ? 1 : 0;
}
