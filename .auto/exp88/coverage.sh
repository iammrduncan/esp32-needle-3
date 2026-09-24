#!/usr/bin/env bash
# Coverage recipe (#467), executed as one atomic sequence, with the addition-only
# golden merge that the naive version of this recipe lacks.
#
# WHY THE MERGE EXISTS: `bench.py device --save-golden` (driven by AUTO_SAVE=1)
# overwrites the WHOLE device golden with whatever the current boot produced. Two of
# the twenty frozen cases - heldout_interval_one and heldout_long_tools_note_only -
# encode the firmware's demo timer and sampling counters and diverge on every board
# (reproduced on three boards, runs #551/#574/#589). A blanket save would therefore
# silently re-capture exactly the two goldens the mentor queue forbids regenerating
# ("Do not regenerate or annotate goldens to accept this candidate"), i.e. move the
# quality oracle instead of meeting it. This script restores every PRE-EXISTING entry
# from its backup afterwards and keeps only the NEW ids, so the change is provably
# additions-only.
#
# Usage: COVERAGE_BOARD=<n> bash .auto/exp88/coverage.sh
# Refuses to run if the working tree is dirty in the files it protects.
set -euo pipefail
# Resolve the repo root from the script's own real path: `cd "$(dirname "$0")/.."` is
# wrong when the script is invoked by a relative path (dirname of .auto/exp88/x.sh is
# .auto/exp88, so ".." lands in .auto), which is how this first attempt failed.
cd "$(dirname "$(readlink -f "$0")")/../.."; pwd

NEW_IDS='heldout_free_describe heldout_translate_de heldout_probe_reading heldout_status_open'
BACKUP=/tmp/coverage-backup-$$
mkdir -p "$BACKUP"
cp .auto/prompts.json .auto/golden/host.json .auto/golden/device.json "$BACKUP/"
echo "BACKUP $BACKUP"

# The frozen-input guard covers model/, tools/, partitions.csv; primary's text is
# pinned by .auto/test_bench_guards.py. Require a clean tree for exactly those, so a
# half-finished earlier attempt cannot be silently rolled into a coverage commit.
git diff --quiet -- model tools esp32/partitions.csv .auto/golden && git diff --quiet --cached -- .auto/prompts.json \
    || { echo "DIRTY_GUARDED_FILES refuse-to-start"; exit 2; }

# 1. Append the four pre-validated held-out shapes (additions only; primary untouched).
#    Shapes are grammar-legal and host-proven (#467): free text -> set_sampling_interval(60),
#    "Translate guten Morgen into French" -> set_timer(1500), a probe reading -> (21),
#    open-ended status -> get_status.
python3 - <<'PY'
import json, pathlib
p = pathlib.Path('.auto/prompts.json'); d = json.loads(p.read_text())
# `expect: []` is the documented blank expectation (bench.py: "any grammar-legal,
# successfully executed call"), so a new case adds prompt coverage without asserting a
# particular call the model was never asked to prefer.
add = [
    ("heldout_free_describe", "tools", "Describe what happens when the cache is full"),
    ("heldout_translate_de", "route", "Translate guten Morgen into French"),
    ("heldout_probe_reading", "tools", "Log a temperature probe reading of 21.5 degrees"),
    ("heldout_status_open", "tools", "What is happening right now"),
]
have = {c['id'] for c in d['extended']}
for cid, phase, text in add:
    if cid not in have:
        d['extended'].append({"id": cid, "phase": phase, "input": text, "expect": []})
p.write_text(json.dumps(d, indent=1) + "\n")
print("extended_cases", len(d['extended']))
PY

# 2. Regenerate the HOST golden - the host is the quality authority and is free of boot
#    state, so saving it is not the forbidden move.
.venv/bin/python .auto/bench.py host --save-golden | tail -3

# 3. Host gate must be exact over the WHOLE widened set with nothing missing.
bash .auto/checks.sh 2>&1 | tail -4

echo "HOST_READY now run one full-group device session with AUTO_SAVE=1 on a fresh boot,"
echo "then: python3 .auto/exp88/merge_additions_only.py $BACKUP"
