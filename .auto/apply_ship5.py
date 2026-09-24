#!/usr/bin/env python3
"""Build the SHIPPING tree from the accepted base: the two files that carry the acceptance candidate.

  engine/include/nd_quant.h  <- #exp61 wfr exponent-scale bitcasts
  engine/src/nd_model.c      <- #exp67 bundle5 = sigpair + tap2col + tapfwd + condT (channel-major)

Together with expbc (#424), head4 (#425) and taphoist (#427), already committed, this is the tree
that read 5.3033 on the 5.2717 base (+0.599 %) = +3.65 % over the accepted 5.1167. Every step is
base-pinned and marker-asserted, so a stale file cannot be shipped silently; run `.auto/checks.sh`
after this and then a device gate session before treating it as accepted.
"""
import subprocess, sys, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STEPS = [("engine/include/nd_quant.h", ".auto/exp61/apply_wfr.py"),
         ("engine/src/nd_model.c", ".auto/exp67/apply_bundle5.py")]
for target, gen in STEPS:
    r = subprocess.run([sys.executable, os.path.join(ROOT, gen), os.path.join(ROOT, target)])
    if r.returncode:
        print(f"SHIP_STEP_FAILED step={gen} rc={r.returncode}")
        sys.exit(r.returncode)
print("SHIP_TREE_READY steps=2")
