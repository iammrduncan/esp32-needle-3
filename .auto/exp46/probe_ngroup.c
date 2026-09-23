/* Which transform paths actually execute in the field?
 *
 * fwht_rows walks three groups per stage, then a two-group path, then a
 * single-group remainder. Runs #379/#395 shipped the triple path; fw2foldnb
 * (folding the pair path's rescale) measured +0.033 %, i.e. nothing. Hypothesis:
 * the pair and single paths are DEAD CODE in the field, because every tensor the
 * engine prepares has ngroup divisible by 3, and the device's half-split keeps
 * each core's count divisible by 3. If true, the remainder-fold retry (#388's
 * peeled version) is field-dead too and must not be measured on a board.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nd_cact.h"
#include "nd_model.h"

static int paths_for(uint32_t n)
{
    /* nd_parallel_rows on the device: core0 takes the first ceil(n/2). */
    int trip = 0, pair = 0, single = 0;
    for (int half = 0; half < 2; half++) {
        uint32_t c0 = (n / 2) + (n % 2), cnt = half ? n - c0 : c0, gi = 0;
        for (; gi + 2 < cnt; gi += 3) trip++;
        for (; gi + 1 < cnt; gi += 2) pair++;
        for (; gi < cnt; gi++) single++;
    }
    return (trip << 16) | (pair << 8) | single;
}

int main(int argc, char **argv)
{
    const char *blob_path = argc > 1 ? argv[1] : "model/needle3.cact";
    FILE *f = fopen(blob_path, "rb");
    if (!f) { fprintf(stderr, "no archive\n"); return 2; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    void *blob = malloc((size_t)size);
    if (fread(blob, 1, (size_t)size, f) != (size_t)size) return 2;
    fclose(f);
    nd_model m;
    if (nd_model_open(&m, blob, (size_t)size) != 0) { fprintf(stderr, "open failed\n"); return 3; }

    nd_layer *L = &m.layer[0];
    struct { const char *name; const nd_tensor *t; } sites[] = {
        { "attn q/k/v/gate (q_proj,L0)",  &L->q_proj },
        { "attn out_proj (L0)",            &L->out_proj },
        { "attn gate_proj (L0)",           &L->gate_proj },
        { "logits embedding",              &m.embedding },
        { "mHC phi_pre",                   &m.mhc_phi_pre },
        { "engram site0 key_proj",         &m.engram[0].key_proj },
        { "engram site0 value_proj",       &m.engram[0].value_proj },
    };
    printf("%-34s %8s %7s %7s %6s  %s\n", "prepare site", "in_pad", "group", "ngroup", "%3",
           "triple/pair/single calls per token (2-core split)");
    int all_div3 = 1, pair_calls = 0, single_calls = 0;
    for (unsigned i = 0; i < sizeof(sites) / sizeof(sites[0]); i++) {
        const nd_tensor *t = sites[i].t;
        uint32_t pad = nd_cq_in_pad(t), g = t->group, ng = nd_cq_groups(t);
        int p = paths_for(ng);
        int trip = p >> 16, pair = (p >> 8) & 0xff, single = p & 0xff;
        printf("%-34s %8u %7u %7u %6u  %d/%d/%d\n", sites[i].name, pad, g, ng, ng % 3u, trip, pair, single);
        if (ng % 3u) all_div3 = 0;
        pair_calls += pair; single_calls += single;
    }
    printf("all_ngroup_divisible_by_3=%d pair_path_calls=%d single_path_calls=%d\n",
           all_div3, pair_calls, single_calls);
    printf("VERDICT: %s\n", (pair_calls == 0 && single_calls == 0)
           ? "pair and single paths NEVER run in the field -> folding them is field-dead"
           : "remainder paths DO run -> a folded remainder is worth a board");
    nd_model_close(&m);
    free(blob);
    return 0;
}
