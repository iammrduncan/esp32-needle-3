#!/usr/bin/env python3
"""Candidate `f16wfr`: `nd_f16`'s own normal-path bitcast uses the accepted `nd_f32_from_bits` helper.

Why this is the last member of the conversion family and not a repeat of #425's `f16bc`: `nd_f16`
still ends with a plain 4-byte copy, and under this target's `-fno-builtin-memcpy` every expansion
GCC does not fully fold is a ROM library call - #424 measured that call at 26.9 cycles per
conversion. `f16bc` fixed it with `__builtin_memcpy` and read +0.190 %, then measured nothing on top
of `head4` because head4 removed nd_f16's biggest consumer; the helper form additionally removes the
stack store and FP reload (#exp61 measured that mechanism at +0.062 % on the exponent scales, and the
kbench screen put the store/reload at most of the ~50 cycles per conversion).

Two things are unchanged by construction: the `e == 0` and `e == 0x1F` cases still call
`nd_f16_slow`, so subnormals, signed zeros, infinities and NaN payloads cannot move; and the value
built above them is the same 32-bit pattern, only delivered by register transfer. The host build -
the correctness oracle - takes the helper's portable `__builtin_memcpy` branch, so the host goldens
verify the arithmetic while the device verifies the Xtensa path.

The helper is also MOVED above `nd_f16`, which is required (use before definition is a hard error,
as a failed lane proved) and changes this file's hash without changing any generated code at the
sites that already used it.
"""
import hashlib, os, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/include/nd_quant.h"
BASE_MD5 = "57a0fa8af900"                       # the accepted header (wfr for the exponent scales)
CONTENT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "nd_quant.f16wfr.h")
WANT_MD5 = "1f88292f22c4"


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    new = open(CONTENT, "rb").read()
    text = new.decode()
    md5 = hashlib.md5(new).hexdigest()[:12]
    if (WANT_MD5 and md5 != WANT_MD5) \
            or text.count("static inline float nd_f32_from_bits(uint32_t bits)") != 1 \
            or text.count("nd_f32_from_bits(") != 5 \
            or text.index("static inline float nd_f32_from_bits") > text.index("static inline float nd_f16"):
        print(f"CONTENT_ASSERT_FAILED md5={md5}"); return 3
    open(PATH, "wb").write(new)
    print("APPLIED target=" + PATH + " md5=" + md5)
    return 0


if __name__ == "__main__":
    sys.exit(main())
