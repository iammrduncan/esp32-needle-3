        for (i = 0; i < c->dm; i++)
            acc += c->x[i] * c->cv[(size_t)i * 8 + ch];
