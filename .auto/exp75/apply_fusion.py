#!/usr/bin/env python3
"""Candidate `fusion-r5`: radix-4 stage fusion + cold-path IRAM refund, REBASED onto bundle5.

Composed from the real source, not a memory: `git show 787fea9:engine/src/nd_quant.c`, the file run
#404 measured at 5.1467 with `internal_free` back to the accepted figure. It is installed as a whole
file because the diff from the accepted `nd_quant.c` is 149 lines and every hunk is the transform:
three groups walked with fused radix-4 stage pairs, a peeled (branch-free) rescaled final stage, and
`ND_HOT` dropped plus `noinline` added on the field-dead fallbacks so they stop occupying internal
heap twice. Nothing else in the file moves, which is why head4, taphoist, tap2col, tapfwd, condT and
the accepted header are untouched by this candidate.

WHY IT IS COMPOSED AND NOT RESTORED (the operator caught the first version doing exactly this):
`git show 787fea9:engine/src/nd_quant.c` predates head4, whose dispatch lives in THIS file
(`nd_cq_gemv_prepared`'s `#if ND_GEMV4_ASM` block, `ctx.base = 0` + `gemv4_asm_usable`). Installing
that file wholesale deletes 10 lines of head4 and restores generic full-head dispatch - a silent
-0.35 % inside a candidate that would have been blamed on the transform. The candidate is therefore
the accepted file plus exactly five hunks of that diff - the `fw_scale`/`nd_fwht2`/`nd_fwht3s`
attribute change, the `nd_fwht4s` body, and the guarded transform dispatch - with the sixth hunk
(`@@ -537,18 +621,8 @@`, the head4 block) excluded, and the script refuses to apply unless head4's
markers are still present afterwards.

Why it deserves a re-measurement rather than retirement: it was measured +0.58 % on the true 5.1167
base with nine agreeing readings on three boards and zero RAM cost, and it was never shipped only
because the 20-case single-session device gate was unreachable behind the console wedge. Full
sessions now complete (run #431 accepted on exactly that gate). Its interaction with bundle5 is
unknown - that is the premise being tested, and #431 established that individually sub-bar levers on
a new base do not compose additively.
"""
import hashlib, os, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_quant.c"
BASE_MD5 = "a485999e5c91"        # accepted (run #431 bundle5) engine/src/nd_quant.c
CONTENT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "nd_quant.fusion.c")
WANT_MD5 = "afdb953abb85"         # accepted + ONLY the five transform hunks of 787fea9


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    new = open(CONTENT, "rb").read()
    text = new.decode()
    if hashlib.md5(new).hexdigest()[:12] != WANT_MD5 \
            or text.count("nd_fwht4s") < 2 or text.count("noinline") != 3 \
            or "nd_fwht3s" not in text \
            or text.count("gemv4_asm_usable") != 3 or "ctx.base = 0" not in text:
        print("CONTENT_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new)
    print("APPLIED target=" + PATH + " md5=" + hashlib.md5(new).hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main())
