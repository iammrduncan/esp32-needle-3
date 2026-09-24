#!/usr/bin/env python3
"""Candidate `sinkpair`: Sinkhorn's log-sum-exp computes two adjacent exponentials at a time with the
shipped interleaved `nd_expf_pair`, and adds the two partials to the SAME accumulator in the SAME
order.

`nd_expf` is a degree-5 Horner chain, so its latency - not its work - is what is exposed when the
kernel asks for one exponential at a time; pairing two of them is the single most productive mechanism
this campaign has (Experiment 12 measured +19.96 % on the attention softmax, and it is already applied
to the attention softmax, the attention gate and SiLU). Sinkhorn is what is left: 5,120 exponential
terms per decode token, ~1,334 of them removed by the exact-zero literal (#291), and the row/column
sweeps are otherwise empty loops - which is precisely where removing or overlapping exponential work
has measured well, in contrast to the attention softmax where the same trick cost -0.43 % (#292)
because that loop is the scheduler's best block.

Bit-exact by construction, and the construction is what is delicate: the ADDS stay sequential on one
accumulator (a second accumulator would reassociate the sum and is refused by the byte-exact gate),
the exact-zero literal keeps its own branch, and a pair only uses `nd_expf_pair` when NEITHER operand
is the literal. `.auto/exp70/test_sinkpair_equiv.c` compares the resulting sum bit-for-bit against the
shipped loop over 480,000 cases at n = 2 and 4, row and column strides, with the exact-zero term
forced on every seventh trial: mismatches=0.
"""
import hashlib, sys
PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "0d639424f636"           # accepted+expbc+head4+taphoist+bundle5 engine/src/nd_model.c

ROWS_OLD = """                float d = a[i * n + j] - mx;
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);"""
ROWS_NEW = """                /* Two adjacent exponentials go through the interleaved pair, which is
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
                }"""

COLS_OLD = """                float d = a[i * n + j] - mx;   /* same exact-zero shortcut */
                sum += (d == 0.0f) ? 1.0f : nd_expf(d);"""
COLS_NEW = """                float d0 = a[i * n + j] - mx, d1 = a[i * n + j + n] - mx;
                if (d0 != 0.0f && d1 != 0.0f) {
                    float e0, e1;
                    nd_expf_pair(d0, d1, &e0, &e1);
                    sum += e0;
                    sum += e1;
                    i++;
                } else {
                    sum += (d0 == 0.0f) ? 1.0f : nd_expf(d0);
                }"""


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    if text.count("for (j = 0; j < n; j++) {\n                float d = a[i * n + j] - mx;") != 1 or \
       text.count("for (i = 0; i < n; i++) {\n                float d = a[i * n + j] - mx;") != 1:
        print("ANCHOR_NOT_UNIQUE -> refusing (row or column sweep changed)"); return 2
    new = text.replace(ROWS_OLD, ROWS_NEW, 1).replace(COLS_OLD, COLS_NEW, 1)
    if new == text or new.count("nd_expf_pair(d0, d1, &e0, &e1);") != 2:
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print("APPLIED target=" + PATH + " md5=" + hashlib.md5(new.encode()).hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main())
