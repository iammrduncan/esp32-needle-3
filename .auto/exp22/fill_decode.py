#!/usr/bin/env python3
"""Decode the wide-load fill-order probe (.auto/exp22/kbench_phi22.c kb_fill_probe).

Probe A: nibble k of each index word = k, xh[j] = 2^(j%8), cb[i] = i.
         C pairs nibble k with xh[k], so the exact answer is sum(k*2^k, k=0..7) * 16
         = 1538 * 16 = 24608. Any other value means nibble k was multiplied by
         xh[pi(k)] for some permutation pi (or a lane was dropped).
Probe B: every nibble = 1, cb[i] = 1, xh[j] = 2^(j%8): sum 2^j * 16 = 4080, which is
         permutation-invariant, so a shortfall names a DROPPED lane directly.

usage: fill_decode.py <A> <B>
"""
import itertools
import sys

WA = sum(k * (1 << k) for k in range(8)) * 16      # 24608
WB = sum(1 << j for j in range(8)) * 16            # 4080


def decode(a, b):
    print(f"expected A={WA} B={WB}  observed A={a} B={b}  dA={WA - a} dB={WB - b}")
    if b != WB:
        d = WB - b
        for k in range(8):
            if abs(d - 16 * (1 << k)) < 0.5:
                print(f"  B: lane {k} contributes nothing (16*2^{k} missing) -> "
                      f"that register is not filled with xh[{k}]: a real drop")
        else:
            print(f"  B: shortfall {d} is not a single 16*2^k -> more than one lane "
                  f"missing, or the load address/post-increment is wrong")
        return
    print("  B: all eight lanes present, so the wide loads do fill every register")
    if a == WA:
        print("  A: pairing is nibble k -> xh[k]. The wide form is CORRECT.")
        return
    hits = [p for p in itertools.permutations(range(8))
            if abs(sum(k * (1 << p[k]) for k in range(8)) * 16 - a) < 0.5]
    print(f"  A: {len(hits)} permutation(s) of the 8 float slots reproduce A={a}:")
    for p in hits[:12]:
        print("     nibble k -> xh[", ", ".join(str(x) for x in p), "]", sep="")
    if not hits:
        print("     none: the deviation is not a pure lane permutation (post-increment "
              "stride or a partial-word reload), so measure it per word next")


if __name__ == "__main__":
    # self-check: identity reproduces A, and one swap is identified unambiguously
    assert WA == sum(k * (1 << k) for k in range(8)) * 16 == 24608
    assert WB == 4080
    sw = (1, 0, 2, 3, 4, 5, 6, 7)
    v = sum(k * (1 << sw[k]) for k in range(8)) * 16
    assert v != WA and any(abs(sum(k * (1 << p[k]) for k in range(8)) * 16 - v) < 0.5
                           for p in itertools.permutations(range(8)))
    if len(sys.argv) == 3:
        decode(float(sys.argv[1]), float(sys.argv[2]))
    else:
        print("SELFTEST OK (identity reproduces A; a swap is detectable)")
