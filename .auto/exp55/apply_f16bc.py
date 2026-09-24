#!/usr/bin/env python3
"""Candidate `f16bc`: the same bitcast mechanism as expbc, applied to `nd_f16` itself.

Run #424 measured the mechanism on the device: this target compiles -fno-builtin-memcpy, so the
4-byte copy that moves the assembled exponent into a float is a ROM library call, and an explicit
builtin is exempt. Screen (board 1, kbench, all 65,536 half encodings, mismatch=0): 77.15 -> 50.22
cycles per conversion on a real PSRAM operand, 76.22 -> 47.33 over the whole encoding space.

`nd_f16` is the engine's hottest converter - once per group norm, per MLP diagonal and per
confidence-probe element, >100K calls per token - so this is the same one-line lever on the
largest call count. The subnormal/inf/nan fallback (`nd_f16_slow`) and the arithmetic are
untouched; only the bit transfer changes. Base pinned (a stale base silently yields a candidate
identical to accepted, #379).
"""
import hashlib
import re
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/include/nd_quant.h"
BASE_MD5 = "45a6b2e9836e"          # accepted + expbc (the tree main carries)

OLD = "    memcpy(&f, &bits, 4);"
NEW = "    __builtin_memcpy(&f, &bits, 4);"


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
    if text.count(OLD) != 1:
        print(f"ANCHOR count={text.count(OLD)} -> refusing")
        return 2
    new = text.replace(OLD, NEW, 1)
    code = strip_comments(new)
    n_built = len(re.findall(r"(?<![\w$])__builtin_memcpy\s*\(", code))
    n_plain = len(re.findall(r"(?<![\w$.])memcpy\s*\(", code))
    if n_built != 4 or n_plain != 0:
        print(f"CALLSITE_ASSERT_FAILED builtin_calls={n_built} want=4 plain_calls={n_plain} want=0")
        return 3
    open(PATH, "wb").write(new.encode())
    print(f"APPLIED target={PATH} builtin_calls={n_built} plain_calls={n_plain} "
          f"md5={hashlib.md5(new.encode()).hexdigest()[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
