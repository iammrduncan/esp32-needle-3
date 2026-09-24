#!/usr/bin/env python3
"""Candidate `expbc`: the three exponent-scale bitcasts become explicit builtin calls.

Mechanism (measured, not borrowed): this target is compiled with -fno-builtin-memcpy, so a
plain 4-byte `memcpy` of a local is emitted as a ROM library call, with argument setup and FP
spills around it. The ELF proves it: the pair-sigmoid and the slow f16 path call ROM memcpy
just to build their scales. An explicit builtin call is exempt from that flag (GCC expands it),
so the call, its argument setup and the spills around it should disappear.

Scope is deliberately one family: the scale bitcast in the scalar exp and the two in the paired
exp - THREE call sites - which is the highest call-count consumer (~14K exp calls/token). The
half-to-float site stays untouched: it is a separate priced experiment. Compiler flags, the
polynomial, range reduction, clamps and the scalar fallback are all unchanged, so the values
are identical unless codegen improves, in which case rounding points must be re-checked on device.
"""
import hashlib
import re
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/include/nd_quant.h"
BASE_MD5 = "a4431e70a440"

SITES = [
    ("    memcpy(&scale, &bits, 4);",
     "    __builtin_memcpy(&scale, &bits, 4);"),
    ("    memcpy(&sc0, &b0, 4);\n    memcpy(&sc1, &b1, 4);",
     "    __builtin_memcpy(&sc0, &b0, 4);\n    __builtin_memcpy(&sc1, &b1, 4);"),
]


def strip_comments(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    return re.sub(r"//[^\n]*", " ", t)


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing to edit")
        return 2
    text = data.decode()
    for i, (old, _) in enumerate(SITES):
        n = text.count(old)
        if n != 1:
            print(f"ANCHOR[{i}] count={n} -> refusing")
            return 2
    new = text
    for old, repl in SITES:
        new = new.replace(old, repl, 1)
    if new == text:
        print("NO_CHANGE")
        return 2
    code = strip_comments(new)
    n_built = len(re.findall(r"(?<![\w$])__builtin_memcpy\s*\(", code))
    n_plain = len(re.findall(r"(?<![\w$.])memcpy\s*\(", code))
    if n_built != 3 or n_plain != 1:
        print(f"CALLSITE_ASSERT_FAILED builtin_calls={n_built} want=3 "
              f"plain_calls={n_plain} want=1 (f16 site only)")
        return 3
    open(PATH, "wb").write(new.encode())
    print(f"APPLIED target={PATH} builtin_calls={n_built} plain_calls={n_plain} "
          f"md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
