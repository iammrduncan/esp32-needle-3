#!/usr/bin/env python3
"""Candidate `tapfwd`: read the current input for tap j=0 instead of re-reading the history slot
the same call just wrote it into.

The caller does `memcpy(layer_history + slot*dim, projection, dim*sizeof(float))` and then runs
`tap_rows`, whose j=0 term reads `hist[(pos % taps)*dim + i]` - i.e. a byte-for-byte copy of
`projection[i]` that is already resident where the loop is standing, in a PSRAM row that a decode
token touches once. Reading the source instead of the copy removes one full dim-float PSRAM sweep
per projection (3 projections x 8 layers per token) with no arithmetic change.

Bit-exact by identity, not by tolerance: the value read IS the value written by that memcpy, in
the same ascending j order, into the same accumulator. The overwrite is safe per column: column i
is read as j=0's operand before `c->proj[i]` is stored, and no other column reads it.
"""
import hashlib, sys
PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "53553a3507c5"

OLD = """        const float *h0 = c->hist + (size_t)( c->pos        % c->taps) * dim;"""
NEW = """        /* j = 0 is the row the caller memcpy'd out of c->proj immediately before this call,
         * so read the source: same bytes, same order, one PSRAM sweep fewer per projection. */
        const float *h0 = c->proj;"""

def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    if text.count(OLD) != 1:
        print(f"ANCHOR count={text.count(OLD)} -> refusing"); return 2
    new = text.replace(OLD, NEW, 1)
    if new == text or new.count("const float *h0 = c->proj;") != 1:
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print(f"APPLIED target={PATH} md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
