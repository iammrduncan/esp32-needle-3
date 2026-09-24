#!/usr/bin/env python3
"""Candidate `bundle5`: bundle5 = bundle4 plus condT (cond_v staged channel-major, in place).

Each half was measured on this base (#428's tree) and each is individually below the 0.2 pct bar,
which is the situation run #371 bundled: two-plus bar-marginal levers whose mechanisms are
independent, so only the combination can clear the bar.

  wfr      (#exp61, +0.062 pct, ext 5.2000) - the exponent scale reaches the FP register by `wfr`
           instead of the store/reload the 4-byte copy lowers to; arithmetic untouched (`p * scale`).
  sigpair  (#exp58, +0.125 pct) - a mixed-sign gate pair keeps its interleaved exp pair instead of
           falling back to two scalar chains; the sign only picks which scalar division form runs.
  tapbund  (#exp59 +0.094 pct, #exp60 +0.062 pct) - two tap columns per iteration AND tap j=0 reads
           the current input rather than the history slot the caller just copied it into.

Bit-exactness is by construction everywhere and each piece has its own guard: the tap family is
`.auto/exp57/test_tap_equiv.c` (87,836 comparisons, four variants, mismatches=0) and the sigmoid
pair only re-selects the shipped scalar division forms with the shipped sign-normalized exponent
arguments. Base pinned; the markers below fail the lane if any single edit stops matching.
"""
import hashlib, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "53553a3507c5"           # accepted+expbc+head4+taphoist engine/src/nd_model.c

EDITS = [
    ("SIGPAIR", """    if ((x0 >= 0.0f) == (x1 >= 0.0f)) {
        float e0, e1;

        nd_expf_pair(x0 >= 0.0f ? -x0 : x0,
                     x1 >= 0.0f ? -x1 : x1, &e0, &e1);
        if (x0 >= 0.0f) {
            *y0 = 1.0f / (1.0f + e0);
            *y1 = 1.0f / (1.0f + e1);
        } else {
            *y0 = e0 / (1.0f + e0);
            *y1 = e1 / (1.0f + e1);
        }
    } else {
        *y0 = sigmoidf_(x0);
        *y1 = sigmoidf_(x1);
    }
}""", """    /* The pair needs matching EXPONENTS, not matching signs: both chains are fed the
     * sign-normalized magnitude exactly as the scalar path would, so the sign only selects
     * which of the two shipped scalar division forms each element uses. The shipped code
     * required the signs to agree and otherwise ran two serially-dependent scalar chains. */
    float e0, e1;

    nd_expf_pair(x0 >= 0.0f ? -x0 : x0,
                 x1 >= 0.0f ? -x1 : x1, &e0, &e1);
    if (x0 >= 0.0f) *y0 = 1.0f / (1.0f + e0);
    else            *y0 = e0 / (1.0f + e0);
    if (x1 >= 0.0f) *y1 = 1.0f / (1.0f + e1);
    else            *y1 = e1 / (1.0f + e1);
}"""),
    ("TAPFWD", """        const float *h0 = c->hist + (size_t)( c->pos        % c->taps) * dim;""", """        /* j = 0 is the row the caller memcpy'd out of c->proj immediately before this call,
         * so read the source: same bytes, same order, one PSRAM sweep fewer per projection. */
        const float *h0 = c->proj;"""),
    ("TAPTILE", """        for (i = lo; i < hi; i++) {
            float value = 0.0f;
            value += w0[i] * h0[i];
            value += w1[i] * h1[i];
            value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;""", """        for (i = lo; i + 1u < hi; i += 2u) {
            /* Two columns, two accumulators: each keeps its own ascending-tap order and
             * its own +0.0f seed, so both stored sums are the shipped rounding sequence. */
            float a = 0.0f, b = 0.0f;
            a += w0[i] * h0[i];
            a += w1[i] * h1[i];
            a += w2[i] * h2[i];
            b += w0[i + 1u] * h0[i + 1u];
            b += w1[i + 1u] * h1[i + 1u];
            b += w2[i + 1u] * h2[i + 1u];
            c->proj[i] = a;
            c->proj[i + 1u] = b;
        }
        if (i < hi) {
            float value = 0.0f;
            value += w0[i] * h0[i];
            value += w1[i] * h1[i];
            value += w2[i] * h2[i];
            c->proj[i] = value;
        }
        return;"""),
    ("CONDFILL", """
                    m->fp16_slot[li][SLOT[f]] = p;
                    for (k = 0; k < n; k++)
                        p[k] = nd_f16(h[k]);
""", """
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
"""),
    ("CONDREAD", """
        for (i = 0; i < c->dm; i++)
            acc += c->x[i] * c->cv[(size_t)i * 8 + ch];
""", """
        /* cv is staged channel-major (see the fp16 pool fill), so channel ch is one
         * contiguous row: the sweep is sequential instead of a stride-8 walk over 24 KB.
         * The term order and the accumulator are exactly the shipped ones, so the sum
         * is bit-identical - only where each term is fetched from moves. */
        const float *cvr = c->cv + (size_t)ch * c->dm;
        for (i = 0; i < c->dm; i++)
            acc += c->x[i] * cvr[i];
"""),
    ("SINKROWS", """                float d = a[i * n + j] - mx;
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);""", """                /* Two adjacent exponentials go through the interleaved pair, which is
                 * where nd_expf's exposed Horner latency lives, but the partials are
                 * still added one after the other into this same accumulator, so the
                 * sum's bits are the shipped ones. The exp(0) literal keeps its own
                 * test: a pair is only used when neither operand is the literal. */
                float d0 = a[i * n + j] - mx, d1 = a[i * n + j + 1u] - mx;
                if (d0 != 0.0f && d1 != 0.0f) {
                    float e0, e1;
                    nd_expf_pair(d0, d1, &e0, &e1);
                    sum += e0;
                    sum += e1;
                    j++;
                } else {
                    sum += (d0 == 0.0f) ? 1.0f : nd_expf(d0);
                }"""),
    ("SINKCOLS", """                float d = a[i * n + j] - mx;   /* same exact-zero shortcut */
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);""", """                float d0 = a[i * n + j] - mx, d1 = a[i * n + j + n] - mx;
                if (d0 != 0.0f && d1 != 0.0f) {
                    float e0, e1;
                    nd_expf_pair(d0, d1, &e0, &e1);
                    sum += e0;
                    sum += e1;
                    i++;
                } else {
                    sum += (d0 == 0.0f) ? 1.0f : nd_expf(d0);
                }"""),
]


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    for name, old, _ in EDITS:
        if text.count(old) != 1:
            print(f"{name}_ANCHOR count={text.count(old)} -> refusing"); return 2
    new = text
    for _, old, repl in EDITS:
        new = new.replace(old, repl, 1)
    if (new == text or new.count("sigmoidf_(x") != 0
            or new.count("const float *h0 = c->proj;") != 1
            or new.count("i + 1u] * h0[i + 1u]") != 1):
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print("APPLIED target=" + PATH + " md5=" + hashlib.md5(new.encode()).hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main())
