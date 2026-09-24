#!/usr/bin/env python3
"""Candidate `f16wfr`: `nd_f16`'s own bitcast goes through the same `wfr` helper the exponent scales
use, on top of the exponent-scale change (#exp61) it composes with.

Two measured facts make this the last member of the conversion family:
  * the accepted header still ends `nd_f16` with a plain 4-byte copy, and this target is configured
    with `-fno-builtin-memcpy`, so every expansion that GCC does not fully fold is a ROM library
    call - #424 measured that call at 26.9 cycles per conversion and #425 measured the builtin form
    at +0.190 % before head4 subsumed its biggest consumer;
  * the accepted `nd_model.c.obj` already contains 222 `wfr` instructions, so where the bits are in
    a register GCC already transfers them directly; what the helper adds is that this happens at the
    sites where the copy forced a store/reload.

Not run #231 (which replaced the bitcast with exponent-field arithmetic) and not run #230 (float to
integer transfers): the value is formed by moving integer bits into an FP register, arithmetic
untouched. Guarded by the same whole-encoding-space equality #424 used: e == 0 and e == 31 keep the
slow path, so subnormals, zeros, infinities and NaNs are unchanged by construction.
"""
import hashlib, sys
PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/include/nd_quant.h"
BASE_MD5 = "57a0fa8af900"           # the wfr header (#exp61), i.e. accepted+expbc+head4+taphoist+wfr

OLD = """    memcpy(&f, &bits, 4);
    return f;
}"""
NEW = """    /* Same value, one register transfer instead of a store plus a reload. The e == 0 and
     * e == 0x1F cases above still take the out-of-line slow path, so only normal finite
     * halfwords reach here and the returned float is bit-identical by construction. */
    f = nd_f32_from_bits(bits);
    return f;
}"""


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    if text.count(OLD) != 1:
        print(f"ANCHOR count={text.count(OLD)} -> refusing"); return 2
    new = text.replace(OLD, NEW, 1)
    if new == text or new.count("f = nd_f32_from_bits(bits);") != 1:
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print("APPLIED target=" + PATH + " md5=" + hashlib.md5(new.encode()).hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main())
