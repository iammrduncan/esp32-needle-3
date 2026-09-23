#!/usr/bin/env python3
"""Candidate `condT`: stage the conditioning projection `cond_v` channel-major.

The diagnosis is already in the ledger, from runs #380/#381: interleaving the eight
conditioning reductions made things WORSE (-0.394 %, -0.427 %, monotonically), and the
conclusion recorded was that `cond_rows` is bound by its strided `cond_v` loads, so the
interleave only bought register pressure. That says what to attack: the loads, not the
number of chains.

`cond_v` is [d_model][8] in the archive and is read one COLUMN at a time - channel ch's
reduction walks `cv[i*8 + ch]` over all d_model rows, a 32 B stride, so each 64 B cache
line delivers two useful floats and is touched again for each of the other seven
channels. Storing this one slot channel-major turns eight strided sweeps of 24 KB into
eight sequential sweeps of 3 KB each, and it costs NOTHING in memory: the fp32 staging
pool is already allocated with the same element count, this is a different fill order.

Bit-exact by construction, and the reason it is admissible where an integer path or a
reassociation would not be: the values are the same floats, the reduction is still
`acc += x[i] * cv[..]` over i = 0..dm-1 in the same order with the same single
accumulator, so every partial sum is the same sequence of roundings. Only the address
arithmetic moved.

Precedent inside the same function: the `cu` blend below `cond_rows` was already
pre-folded for exactly this reason ("walks memory sequentially ... every value is
bit-identical"), and the campaign's largest win of all (run #2, +99.9 %) is a staging
layout change.

Pins its BASE file and asserts it: a stale base silently produces a candidate that is
byte-identical to the accepted code (#379).
"""
import hashlib
import sys

MODEL = "engine/src/nd_model.c"
BASE_MD5 = "5c646468b78f"          # accepted engine/src/nd_model.c

FILL_OLD = """                    m->fp16_slot[li][SLOT[f]] = p;
                    for (k = 0; k < n; k++)
                        p[k] = nd_f16(h[k]);
"""
FILL_NEW = """                    m->fp16_slot[li][SLOT[f]] = p;
                    if (SLOT[f] == 25 && n == m->d_model * 8u) {
                        /* cond_v: [d_model][8] in the archive, read one column at a
                         * time by cond_rows, so store it [8][d_model]. Same pool, same
                         * element count, same values - only the fill order differs - and
                         * the reader's reduction order is untouched, so every sum is
                         * bit-identical. Eight sequential 3 KB sweeps instead of eight
                         * 32 B-stride sweeps over 24 KB. */
                        uint32_t r, cc;
                        for (r = 0; r < m->d_model; r++)
                            for (cc = 0; cc < 8u; cc++)
                                p[cc * m->d_model + r] = nd_f16(h[r * 8u + cc]);
                    } else {
                        for (k = 0; k < n; k++)
                            p[k] = nd_f16(h[k]);
                    }
"""

READ_OLD = """            acc += c->x[i] * c->cv[(size_t)i * 8 + ch];"""
READ_NEW = """            /* cv is staged channel-major (see the fp16 pool fill): channel ch is one
             * contiguous 3 KB row now, so this sweep is sequential. The term order and
             * the accumulator are exactly the shipped ones, so the sum is identical. */
            acc += c->x[i] * c->cv[(size_t)ch * c->dm + i];"""


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else MODEL
    data = open(path, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing to edit")
        return 2
    text = data.decode()
    for name, old in (("FILL", FILL_OLD), ("READ", READ_OLD)):
        if text.count(old) != 1:
            print(f"{name}_ANCHOR count={text.count(old)}")
            return 2
    new = text.replace(FILL_OLD, FILL_NEW, 1).replace(READ_OLD, READ_NEW, 1)
    if new == text:
        print("NO_CHANGE")
        return 2
    open(path, "w").write(new)
    print(f"applied md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
