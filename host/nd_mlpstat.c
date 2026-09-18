/* nd_mlpstat - where does the Monarch MLP's fp16 weight traffic actually go?
 *
 *   nd_mlpstat <model.cact>
 *
 * Counts, per layer, the loads the current kron_apply performs and the smaller
 * number it could perform, and prints the element ranges it addresses inside
 * each factor. If a factor is addressed past na*na, the two factors are packed
 * contiguously in the blob and a float copy has to keep that layout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nd_model.h"
#include "nd_cact.h"

int main(int argc, char **argv)
{
    FILE   *f;
    void   *blob;
    long    bytes;
    nd_model m;
    uint32_t li, na, nb, loads = 0, ideal = 0;

    if (argc != 2) return 2;
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); bytes = ftell(f); rewind(f);
    blob = malloc((size_t)bytes);
    if (!blob || fread(blob, 1, (size_t)bytes, f) != (size_t)bytes) return 2;
    fclose(f);
    if (nd_model_open(&m, blob, (size_t)bytes) != 0) return 2;

    na = 32; nb = 32;
    (void)na; (void)nb;

    for (li = 0; li < 1; li++) {
        const nd_layer *L = &m.layer[li];
        const nd_tensor *fac[6] = { &L->w1a, &L->w1b, &L->w2a, &L->w2b, &L->w3a, &L->w3b };
        int f2;
        printf("layer %u: hada_n=%u\n", li, m.c.h.hada_n);
        for (f2 = 0; f2 < 6; f2++) {
            uint32_t a_rows = fac[f2]->shape[0], a_cols = fac[f2]->shape[1];
            uint32_t span = a_rows * (a_cols ? a_cols : 1);
            /* The kernel walks a[(i*na + k)] for i,k in [0,na) and
             * b[(j*nb + l)] for j,l in [0,nb). */
            printf("  factor %d: shape %u x %u  elements=%u  nbytes=%u  "
                   "kernel_a_max_index=%u  kernel_b_max_index=%u\n",
                   f2, a_rows, a_cols, span, fac[f2]->nbytes,
                   (a_rows - 1) * a_rows + (a_rows - 1),
                   (a_rows - 1) * a_rows + (a_rows - 1));
        }
        /* per layer per kron: A loads = na*nb*na, B loads = na*nb*nb */
        loads  = 3 * (32 * 32 * 32 * 2);
        ideal  = 3 * (32 * 32) * 2;
        printf("  fp16 element loads per token (this layer, 3 kroneckers): %u\n", loads);
        printf("  distinct elements (a float copy would convert):          %u\n", ideal);
    }
    printf("whole model: %u loads/token over %u layers\n",
           loads * m.n_layers, m.n_layers);
    nd_model_close(&m);
    free(blob);
    return 0;
}
