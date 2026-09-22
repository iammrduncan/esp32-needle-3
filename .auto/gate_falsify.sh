#!/bin/bash
# Falsify the campaign's quality/metric gates: deliberately break each guarded
# input and require checks.sh to go RED. A gate that cannot fail is worthless -
# run #296 proved one silently passed 3 of 17 cases - and "all gates green" is
# only evidence if the gates can turn red.
#
# Every mutation is reverted with `git checkout` on the way out, and the final
# control run must be green or this script itself fails.
#
#   bash .auto/gate_falsify.sh
set -uo pipefail
cd "$(dirname "$0")/.."
D=${AUTO_LOG_DIR:-$PWD/.auto/runs/local}; mkdir -p "$D"
# Key on the tool, not on IDF_PATH: this environment sets IDF_PATH with the tools
# off PATH, which is the exact trap checks.sh documents for cmake. Keying on
# IDF_PATH here made the M5 probe see no ctest output and report "zero tests".
command -v ctest >/dev/null 2>&1 || . /opt/esp/idf/export.sh >/dev/null 2>&1 || true

fails=0
restore() { git checkout -- "$@" 2>/dev/null; }

# run_gate <name> <file to restore> <expected marker...>
# Runs checks.sh, and PASSES only if it exits non-zero and names one of the
# expected markers. An unmutated tree must NOT be run through run_gate.
run_gate() {
    local name=$1 file=$2; shift 2
    local out="$D/falsify_$name.log" rc=0 marker found
    if bash .auto/checks.sh > "$out" 2>&1; then rc=0; else rc=$?; fi
    found=""
    for marker in "$@"; do grep -q "$marker" "$out" && found=$marker; done
    restore "$file"
    if [ "$rc" != 0 ] && [ -n "$found" ]; then
        echo "CAUGHT  $name: rc=$rc gate=$found"
    else
        echo "MISSED  $name: rc=$rc expected one of [$*] -> $(grep -oE '^[A-Z_]{5,}' "$out" | head -3 | tr '\n' ' ')"
        fails=$((fails + 1))
    fi
}

# M1 - the byte-exact reference itself. Change one stored generation's text by
#      one character; the host gate must refuse the tree.
python3 - <<'PY'
import json
p='.auto/golden/host.json'; d=json.load(open(p))
c=d['cases']['sampling5']; c['raw']=c['raw'][:-1]+('X' if c['raw'][-1]!='X' else 'Y')
json.dump(d, open(p,'w'), indent=1)
PY
run_gate golden_text .auto/golden/host.json HOST_OUTPUT_DIVERGED

# M2 - the numeric fidelity reference. Perturb one logit row; the drift gate or
#      the C-side nd_ftest must refuse it.
python3 - <<'PY'
import re
p='.auto/golden/logits.txt'; s=open(p).read().split('\n')
for i, ln in enumerate(s):
    m = re.match(r'^(\s*-?\d+\.\d+)', ln)
    if m:
        s[i] = ln.replace(m.group(1), '%.6f' % (float(m.group(1)) + 0.5), 1)
        break
open(p, 'w').write('\n'.join(s))
PY
run_gate logits_reference .auto/golden/logits.txt LOGIT_DRIFT_TOO_LARGE FIDELITY_CRASHED FIDELITY

# M3 - the metric's own definition. Edit one primary prompt's text. checks.sh's
#      frozen-input git diff does NOT cover .auto/prompts.json, so this is caught
#      only by the pinned hash in test_bench_guards.py.
python3 - <<'PY'
import json
p='.auto/prompts.json'; d=json.load(open(p))
d['primary'][0]['input'] = d['primary'][0]['input'].replace('5 seconds', '6 seconds')
json.dump(d, open(p,'w'), indent=1, ensure_ascii=False); open(p,'a').write('\n')
PY
run_gate primary_prompt .auto/prompts.json BENCH_GUARDS_FAILED

# M4 - a REAL quality trade, not a reference edit: Sinkhorn 20 -> 10 iterations.
#      Historically (run #134-era) 20 -> 6 cost max_delta 7.8 and top1 6/10, so
#      10 must move logits well past the 2e-3 gate. This is the gate's whole
#      purpose: refuse speed bought with model behaviour.
sed -i 's/#define ND_SINKHORN 20/#define ND_SINKHORN 10/' engine/src/nd_model.c
grep -q 'ND_SINKHORN 10' engine/src/nd_model.c || { echo "MUTATION_FAILED sinkhorn"; fails=$((fails+1)); }
run_gate sinkhorn_budget engine/src/nd_model.c HOST_OUTPUT_DIVERGED LOGIT_DRIFT_TOO_LARGE BENCH_GUARDS_FAILED

# M5 - repo test suite must actually be able to fail (guards against a suite that
#      silently collects zero tests).
if ctest --test-dir host/build -N 2>/dev/null | grep -qE "Total Tests: [1-9]"; then
    echo "CAUGHT  test_count: ctest reports a non-zero test count"
else
    echo "MISSED  test_count: ctest reports zero tests"; fails=$((fails + 1))
fi

# M6 (opt-in, needs the board): the DEVICE golden is the only reference that can
# see a two-core defect - on the host nd_parallel_rows is rows_serial, which is
# exactly why run #159's shared-table race looked byte-exact - and it is gated in
# measure.sh, not checks.sh, so run_gate() cannot reach it. Breaks one device
# golden entry by one character and requires measure.sh to refuse the run.
# Measured on run #298: DEVICE_OUTPUT_DIVERGED 16/17 with device_token_delta=0,
# i.e. a text-only change that any token-count check would pass.
if [ -n "${AUTO_DEVICE_FALSIFY:-}" ]; then
    python3 - <<'PY'
import json
p = '.auto/golden/device.json'
d = json.load(open(p))
c = d['cases']['timer60']
c['raw'] = c['raw'][:-1] + ('Z' if c['raw'][-1] != 'Z' else 'W')
json.dump(d, open(p, 'w'), indent=1)
PY
    AUTO_NOFLASH=1 bash .auto/measure.sh > "$D/falsify_device_golden.log" 2>&1
    drc=$?
    restore .auto/golden/device.json
    if [ "$drc" != 0 ] && grep -q DEVICE_OUTPUT_DIVERGED "$D/falsify_device_golden.log"; then
        echo "CAUGHT  device_golden: measure.sh refused a text-only device divergence (rc=$drc)"
    else
        echo "MISSED  device_golden: rc=$drc $(grep -oE '^[A-Z_]{5,}' "$D/falsify_device_golden.log" | tail -2 | tr '\n' ' ')"
        fails=$((fails + 1))
    fi
fi

# Control - the unmutated tree must be green, or nothing above means anything.
if bash .auto/checks.sh > "$D/falsify_control.log" 2>&1; then
    echo "CONTROL green: unmutated tree passes every gate"
else
    echo "CONTROL RED: $(tail -5 "$D/falsify_control.log" | tr '\n' ' ')"; fails=$((fails + 1))
fi

echo "FALSIFY_RESULT gates_missed=$fails"
exit $((fails > 0))
