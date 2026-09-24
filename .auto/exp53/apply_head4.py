#!/usr/bin/env python3
"""Candidate `head4`: route the full-vocabulary output head through the existing guarded
4-bit assembly row walker.

The terminal stop-token prediction reaches `nd_cq_gemv_prepared`, which hands EVERY row to
`gemv_rows_generic`, while `nd_cq_gemv_rows` - used by the mHC phi projections - picks
`gemv_rows_offset_asm` whenever `nd_gemv4_asm_ok()` accepts the tensor. The head's tensor is
8192x768, bits=4, group=128, six groups, and its 49,152 norm exponents are all ordinary, so the
guard is satisfied and the fast walker is simply never asked for. This is reuse of a proven
kernel on an uncovered call path, once per request.

Two things the patch must not get wrong, both stated in the code it writes:
  * `ctx.base` is currently uninitialized in this function. The generic walker ignores it; the
    assembly walker maps i -> tensor row through it, so it is set to 0 explicitly.
  * the generic walker stays as the fallback for every geometry or norm exponent the guard
    rejects (2/3-bit tensors, a 0 or 31 exponent, g != 128), so nothing outside the eligible
    4-bit case changes at all.

Pins and asserts its BASE (#379).
"""
import hashlib
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_quant.c"
BASE_MD5 = "cc174624959b"   # accepted engine/src/nd_quant.c (tree cc046f59204e)

OLD = """    ctx.rowbytes = rowbytes;

    nd_parallel_rows(gemv_rows_generic, &ctx, out);
}"""

NEW = """    ctx.rowbytes = rowbytes;
#if ND_GEMV4_ASM
    /* The all-rows path (the full-vocabulary head, once per request) satisfies the same
     * guard the phi projections pass, so ask it. `base` is explicit because the generic
     * walker ignores it while the assembly walker resolves i -> tensor row through it;
     * anything the guard rejects keeps the generic walker, unchanged. */
    ctx.base = 0u;
    nd_parallel_rows(gemv4_asm_usable(&ctx, blob, out) ? gemv_rows_offset_asm
                                                      : gemv_rows_generic,
                     &ctx, out);
#else
    nd_parallel_rows(gemv_rows_generic, &ctx, out);
#endif
}"""


def main() -> int:
    args = [a for a in sys.argv[1:] if a != "--pin"]
    path = args[0] if args else PATH
    data = open(path, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if "--pin" in sys.argv:
        print(f"PIN {path} md5={got}")
        return 0
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing to edit")
        return 2
    text = data.decode()
    if text.count(OLD) != 1:
        print(f"ANCHOR count={text.count(OLD)} -> refusing")
        return 2
    new = text.replace(OLD, NEW, 1)
    if new == text or new.count("gemv4_asm_usable(&ctx, blob, out)") != 1 or new.count("ctx.base = 0u;") != 1:
        print("MARKER_ASSERT_FAILED")
        return 3
    open(path, "wb").write(new.encode())
    print(f"APPLIED target={path} md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
