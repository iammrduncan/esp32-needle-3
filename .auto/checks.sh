#!/bin/bash
# Backpressure: everything that must be true before a speedup may be kept.
# Quiet on success; failures print the useful tail.
set -euo pipefail
cd "$(dirname "$0")/.."
AUTO_LOG_DIR=${AUTO_LOG_DIR:-$PWD/.auto/runs/local}
mkdir -p "$AUTO_LOG_DIR"

# Same guard as measure.sh: IDF_PATH can be set with the tools off PATH, which
# turns every cmake/esptool call into an instant exit 127.
if ! command -v cmake >/dev/null 2>&1; then
    . /opt/esp/idf/export.sh >/dev/null 2>&1 || true
fi

# Host build of the engine must compile, and the repo's own tests must pass.
cmake -S host -B host/build > "$AUTO_LOG_DIR/auto_chost.log" 2>&1
cmake --build host/build -j8 >> "$AUTO_LOG_DIR/auto_chost.log" 2>&1 || {
    echo HOST_BUILD_FAILED; grep -E 'error' "$AUTO_LOG_DIR/auto_chost.log" | head -20; exit 1; }
.venv/bin/python -m unittest discover -s tests > "$AUTO_LOG_DIR/auto_chtest.log" 2>&1 || {
    echo PYTHON_TESTS_FAILED; tail -30 "$AUTO_LOG_DIR/auto_chost.log"; exit 1; }
ctest --test-dir host/build > "$AUTO_LOG_DIR/auto_ctest.log" 2>&1 || {
    echo CTEST_FAILED; tail -40 "$AUTO_LOG_DIR/auto_ctest.log"; exit 1; }

# The harness's own guards. This is where the *metric definition* is protected:
# the frozen-input git diff below covers model/, tools/ and partitions.csv but not
# .auto/prompts.json, so without this an edit to `primary` - the six prompts the
# metric is the mean of - would pass every gate silently.
.venv/bin/python .auto/test_bench_guards.py > "$AUTO_LOG_DIR/auto_cguards.log" 2>&1 || {
    echo BENCH_GUARDS_FAILED; tail -20 "$AUTO_LOG_DIR/auto_cguards.log"; exit 1; }

# The seed-win equivalence guard, wired here because the host goldens CANNOT see the
# assembly path (the host compiles the C fallback), so this is the only host-side
# evidence that nd_lut2_rows_tie1n's add-from-zero seeding is bit-exact. It sweeps the
# fp32 space and fails if the difference set is anything other than {-0.0, sNaN}, and
# fails outright if the compiler is missing - a check that passes when its tool is
# absent is the failure mode this campaign has been bitten by four times (#300, #530).
if ! command -v cc >/dev/null 2>&1; then echo C_GUARD_NO_COMPILER; exit 1; fi
cc -O2 -ffp-contract=off .auto/exp87/test_seed_equiv.c -lm -o "$AUTO_LOG_DIR/seed_equiv" 2>&1 || {
    echo SEED_EQUIV_BUILD_FAILED; exit 1; }
"$AUTO_LOG_DIR/seed_equiv" > "$AUTO_LOG_DIR/auto_seed_equiv.log" 2>&1 || {
    echo SEED_EQUIV_FAILED; tail -10 "$AUTO_LOG_DIR/auto_seed_equiv.log"; exit 1; }

# The console/session guards for the same reason: they are the only thing standing
# between "the suite completed" and "the suite measured every case". Neither was
# wired here, and .auto/exp28/test_pairing.py had been dying on its first assertion
# since run #346 (drain_stale() changed twice - to _line() and to returning lines -
# with the fake never updated), i.e. a guard that had stopped checking anything for
# ~40 runs. A check nobody runs is a check that rots.
for guard in .auto/exp28/test_pairing.py .auto/exp28/test_retry.py; do
    .venv/bin/python "$guard" > "$AUTO_LOG_DIR/auto_serial_guard.log" 2>&1 || {
        echo SERIAL_SESSION_GUARD_FAILED "$guard"
        tail -20 "$AUTO_LOG_DIR/auto_serial_guard.log"; exit 1; }
done

# No buying speed with the model: the archive, its rung, the schemas and the
# partition layout are frozen for the whole session.
.venv/bin/python tools/download_model.py --verify-only > /dev/null
./host/build/nd_dump model/needle3.cact header | grep -q '^num_layers 8$'
git diff --quiet -- model/manifest.json tools/demo-tools.json tools/model-routes.json \
    tools/model-catalog.json esp32/partitions.csv model/needle3.cact || {
    echo FROZEN_INPUTS_MODIFIED; git status --short; exit 1; }

# Quality gate 1: byte-identical generation on every prompt in the frozen set.
.venv/bin/python .auto/bench.py host > "$AUTO_LOG_DIR/auto_cq.log" 2>&1 || {
    echo HOST_QUALITY_CRASHED; tail -30 "$AUTO_LOG_DIR/auto_cq.log"; exit 1; }
grep -E '^(METRIC|DIVERGE|FIDELITY)' "$AUTO_LOG_DIR/auto_cq.log"
exact=$(grep -o 'host_output_exact=[0-9]*' "$AUTO_LOG_DIR/auto_cq.log" | cut -d= -f2)
cases=$(grep -o 'host_cases=[0-9]*' "$AUTO_LOG_DIR/auto_cq.log" | cut -d= -f2)
[ "${exact:-0}" = "${cases:-1}" ] || { echo "HOST_OUTPUT_DIVERGED ${exact}/${cases}"; exit 1; }
# exact == cases is not enough on its own: a case with no golden entry counts as
# exact (that is what keeps adding a held-out case non-breaking), so the count can
# match while comparing against nothing. Run #296 measured this at missing=3.
missing=$(grep -o 'host_golden_missing=[0-9]*' "$AUTO_LOG_DIR/auto_cq.log" | cut -d= -f2)
[ "${missing:-1}" = "0" ] || { echo "HOST_GOLDEN_INCOMPLETE missing=${missing:-unknown}"; exit 1; }

# Quality gate 2: numeric fidelity of the forward pass on a fixed probe.
.venv/bin/python .auto/bench.py fidelity >> "$AUTO_LOG_DIR/auto_cq.log" 2>&1 || {
    echo FIDELITY_CRASHED; tail -30 "$AUTO_LOG_DIR/auto_cq.log"; exit 1; }
grep -E '^(METRIC|FIDELITY)' "$AUTO_LOG_DIR/auto_cq.log" | tail -3
delta=$(grep -o 'logit_max_delta=[0-9.eE+-]*' "$AUTO_LOG_DIR/auto_cq.log" | tail -1 | cut -d= -f2)
awk -v d="${delta:-999}" 'BEGIN { exit !(d <= 0.002) }' || {
    echo "LOGIT_DRIFT_TOO_LARGE ${delta}"; exit 1; }
