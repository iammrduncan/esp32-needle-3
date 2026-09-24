                    m->fp16_slot[li][SLOT[f]] = p;
                    if (SLOT[f] == 25 && n == m->d_model * 8u) {
                        /* cond_v is [d_model][8] in the archive and cond_rows reduces one
                         * channel (one column of it) at a time, so convert it straight into
                         * the transposed layout: element (i, ch) lands at ch*dm + i. Same
                         * buffer, same size, same converted values - only the position each
                         * halfword is written to moves, which is what turns eight stride-8
                         * sweeps over 24 KB into eight contiguous 3 KB ones. */
                        for (k = 0; k < n; k++)
                            p[(size_t)(k & 7u) * m->d_model + (k >> 3)] = nd_f16(h[k]);
                    } else {
                        for (k = 0; k < n; k++)
                            p[k] = nd_f16(h[k]);
                    }
