/* Inspect a Needle 3 archive with the same zero-copy reader intended for ESP32. */
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nd_cact.h"

int main(int argc, char **argv)
{
    struct stat st;
    nd_cact c;
    void *data;
    int fd, rc;
    uint32_t i;

    if (argc != 2) {
        fprintf(stderr, "usage: %s needle3.cact\n", argv[0]);
        return 2;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd < 0 || fstat(fd, &st) != 0 || st.st_size <= 0) {
        perror(argv[1]);
        return 1;
    }
    data = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    rc = nd_cact_open(&c, data, (size_t)st.st_size);
    if (rc != 0) {
        fprintf(stderr, "nd_cact_open failed: %d\n", rc);
        return 1;
    }
    printf("Needle 3: %u layers, %u tensors, %u bytes, %u engram sites\n",
           c.h.num_layers, c.n, (unsigned)st.st_size, c.h.num_sites);
    printf("width=%u qk=%u v=%u context=%u kv_window=%u kv_bits=%u\n",
           c.h.d_model, c.h.qk_head_dim, c.h.v_head_dim,
           c.h.max_seq_len, c.h.kv_window, c.h.kv_bits);
    for (i = 0; i < c.n; i++) {
        nd_tensor t;
        if (nd_cact_tensor(&c, i, &t) != 0 || !nd_cact_data(&c, &t)) {
            fprintf(stderr, "invalid tensor %u\n", i);
            return 1;
        }
    }
    munmap(data, (size_t)st.st_size);
    close(fd);
    return 0;
}
