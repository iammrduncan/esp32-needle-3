#!/usr/bin/env python3
"""Candidate `wfr`: move the assembled exponent into a float register directly instead of through
the stack, at the three exponent-scale bitcasts.

Run #424 removed the ROM `memcpy` CALL, but the transfer itself was still a store/load round trip:
the cross-compiler lowers the 4-byte builtin copy to `s32i.n` + `lsi` (the mentor's addresses on
the taphoist base are 0x4037c6e0 then 0x4037c6e2), and the LX7 store-to-FP-load latency is exactly
the ~50-cycle floor the board-1 kbench screen measured for the conversion. Xtensa has the register
transfer this needs: `wfr f, r` moves an address register into a FP register in one instruction,
verified with this target's own compiler (`wfr f0, a2`, no spill).

This is NOT run #231. That candidate changed the ARITHMETIC - it added k<<23 into the mantissa
field of `p` itself, which is a different value and it lost on speed. Here the arithmetic is
untouched: the same `bits` word is assembled by the same expressions, becomes the same float, and
is still multiplied by the same `p` (`p * scale`). Only the path from the integer register to the
FP register changes, so the result is bit-identical by construction.

Non-Xtensa builds (the host, which is the campaign's correctness oracle) keep the plain builtin
copy, so the host suite still exercises a bit-equivalent implementation.
"""
import hashlib, re, sys
PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/include/nd_quant.h"
BASE_MD5 = "45a6b2e9836e"           # accepted + expbc nd_quant.h

HELPER_OLD = """static inline float nd_expf(float x)"""
HELPER_NEW = """/* Put an assembled 32-bit pattern into a float register.
 *
 * On Xtensa this is a single register transfer. The plain 4-byte copy - even as an explicit
 * builtin, which is what run #424 needs for the -fno-builtin-memcpy flag - is lowered to a stack
 * store plus an FP load (`s32i.n` + `lsi`), and that round trip, not the arithmetic, is what the
 * board-1 kbench screen measured at ~50 cycles per conversion. Same bits in, same float out; the
 * host build (the correctness oracle) keeps the portable copy. */
static inline float nd_f32_from_bits(uint32_t bits)
{
#if defined(__XTENSA__)
    float f;
    __asm__ __volatile__("wfr %0, %1" : "=f"(f) : "r"(bits));
    return f;
#else
    float f;
    __builtin_memcpy(&f, &bits, 4);
    return f;
#endif
}

static inline float nd_expf(float x)"""

SITES = [
    ("    __builtin_memcpy(&scale, &bits, 4);\n", "    scale = nd_f32_from_bits(bits);\n"),
    ("    __builtin_memcpy(&sc0, &b0, 4);\n    __builtin_memcpy(&sc1, &b1, 4);\n",
     "    sc0 = nd_f32_from_bits(b0);\n    sc1 = nd_f32_from_bits(b1);\n"),
]


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    if text.count(HELPER_OLD) != 1:
        print(f"HELPER_ANCHOR count={text.count(HELPER_OLD)}"); return 2
    for i, (old, _) in enumerate(SITES):
        if text.count(old) != 1:
            print(f"ANCHOR[{i}] count={text.count(old)} -> refusing"); return 2
    new = text.replace(HELPER_OLD, HELPER_NEW, 1)
    for old, repl in SITES:
        new = new.replace(old, repl, 1)
    code = re.sub(r"/\*.*?\*/", " ", new, flags=re.S)
    code = re.sub(r"//[^\n]*", " ", code)
    n_wfr = code.count("nd_f32_from_bits(")
    n_built = len(re.findall(r"(?<![\w$])__builtin_memcpy\s*\(", code))
    if n_wfr != 4 or n_built != 1:      # the helper's own portable copy (nd_f16 still uses the plain copy)
        print(f"MARKER_ASSERT_FAILED wfr_uses={n_wfr} want=4 builtin_copies={n_built} want=1")
        return 3
    open(PATH, "wb").write(new.encode())
    print(f"APPLIED target={PATH} wfr_uses={n_wfr} builtin_copies={n_built} "
          f"md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
