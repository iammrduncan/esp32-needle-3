#!/usr/bin/env python3
"""Compose the `asmemo + fusion-r5` BUNDLE from the two measured candidates, and emit its installer.

Both were measured separately on the accepted base and both are above the bar alone: `asmemo`
+2.200 % (5.4200 on two boards, identical to the digit) and `fusion-r5` +0.660 % (5.3383). Their
mechanisms do not overlap - one memoises a per-call eligibility predicate in the 2-bit projection
wrapper, the other fuses butterfly stages inside the transform walk - so they are composable, and run
#431 established on this campaign's own numbers that sub-bar halves can deliver more than their sum.
What this bundle tests is whether two ABOVE-bar levers compose additively or compete (both touch
`nd_quant.c`, and the fusion costs IRAM text while the memo costs .bss).

Composition is by applying the two verified candidate scripts in sequence to the accepted tree, which
is what keeps this from being a third implementation nobody tested: fusion first (it asserts the
accepted `nd_quant.c` md5), then asmemo with its `nd_quant.c` base expectation moved to the fusion
result - so the memo patch is still anchored against real text, and asmemo's own `nd_model.c` and
`nd_quant.h` edits apply to the untouched accepted files. The installer this writes then refuses any
tree that is not exactly the accepted one.
"""
import hashlib
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FILES = ["engine/src/nd_quant.c", "engine/src/nd_model.c", "engine/include/nd_quant.h"]
ACC = {"engine/src/nd_quant.c": "a485999e5c91",
       "engine/src/nd_model.c": "0d639424f636",
       "engine/include/nd_quant.h": "57a0fa8af900"}
FUSION_OUT = "afdb953abb85"          # accepted + the five fusion hunks, head4 intact


def md5(b):
    return hashlib.md5(b).hexdigest()[:12]


def run(script, *args, cwd=None):
    r = subprocess.run([sys.executable, script, *args], capture_output=True, text=True, cwd=cwd)
    sys.stdout.write(r.stdout)
    if r.returncode:
        sys.stdout.write(r.stderr)
    return r


def main() -> int:
    tree = "/tmp/bundle78"
    shutil.rmtree(tree, ignore_errors=True)
    os.makedirs(tree + "/engine/src")
    os.makedirs(tree + "/engine/include")
    for rel in FILES:
        src = os.path.join(ROOT, rel)
        if md5(open(src, "rb").read()) != ACC[rel]:
            print(f"MAIN_NOT_ACCEPTED {rel}")
            return 2
        shutil.copy(src, os.path.join(tree, rel))

    r = run(os.path.join(HERE, "..", "exp75", "apply_fusion.py"), tree + "/engine/src/nd_quant.c")
    if r.returncode:
        print("FUSION_STEP_FAILED")
        return r.returncode
    q = open(tree + "/engine/src/nd_quant.c", "rb").read()
    if md5(q) != FUSION_OUT:
        print(f"FUSION_MD5 {md5(q)} want {FUSION_OUT}")
        return 3

    asm = open(os.path.join(HERE, "..", "exp77", "apply_asmemo.py")).read()
    asm = asm.replace('"engine/src/nd_quant.c": "a485999e5c91"',
                      '"engine/src/nd_quant.c": "' + FUSION_OUT + '"', 1)
    open("/tmp/asmemo_on_fusion.py", "w").write(asm)
    r = run("/tmp/asmemo_on_fusion.py", tree + "/engine/src/nd_quant.c")
    if r.returncode:
        print("ASMEMO_STEP_FAILED")
        return r.returncode

    out = {}
    for rel in FILES:
        out[rel] = open(os.path.join(tree, rel), "rb").read()
    quant = out["engine/src/nd_quant.c"].decode()
    checks = {
        "nd_fwht4s": quant.count("nd_fwht4s") >= 2,
        "noinline_x3": quant.count("noinline") == 3,
        "memo": quant.count("s_asm_memo") == 3,
        "no_single_entry_cache": "s_asm_blob" not in quant,
        "head4_gemv4_asm_usable_x3": quant.count("gemv4_asm_usable") == 3,
        "head4_ctx_base_zero": "ctx.base = 0" in quant,
        "reset_declared": out["engine/include/nd_quant.h"].decode().count("nd_cq_asmemo_reset") == 1,
        "reset_called": out["engine/src/nd_model.c"].decode().count("nd_cq_asmemo_reset();") == 1,
    }
    bad = [k for k, v in checks.items() if not v]
    if bad:
        print("BUNDLE_ASSERT_FAILED " + ",".join(bad))
        return 4

    for rel, data in out.items():
        dst = os.path.join(HERE, os.path.basename(rel)) + ".bundle"
        open(dst, "wb").write(data)
        print(f"STORED {dst} md5={md5(data)}")
    print("BUNDLE_COMPOSED ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
