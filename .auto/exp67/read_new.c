        /* cv is staged channel-major (see the fp16 pool fill), so channel ch is one
         * contiguous row: the sweep is sequential instead of a stride-8 walk over 24 KB.
         * The term order and the accumulator are exactly the shipped ones, so the sum
         * is bit-identical - only where each term is fetched from moves. */
        const float *cvr = c->cv + (size_t)ch * c->dm;
        for (i = 0; i < c->dm; i++)
            acc += c->x[i] * cvr[i];
