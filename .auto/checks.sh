#!/bin/bash
# Backpressure: everything that must be true before a speedup may be kept.
# Quiet on success; failures print the useful tail.
set -euo pipefail
cd "$(dirname "$0")/.."

# Host build of the engine must compile, and the repo's own tests must pass.
cmake -S host -B host/build > /tmp/auto_chost.log 2>&1
cmake --build host/build -j8 >> /tmp/auto_chost.log 2>&1 || {
    echo HOST_BUILD_FAILED; grep -E 'error' /tmp/auto_chost.log | head -20; exit 1; }
.venv/bin/python -m unittest discover -s tests > /tmp/auto_chtest.log 2>&1 || {
    echo PYTHON_TESTS_FAILED; tail -30 /tmp/auto_chost.log; exit 1; }
ctest --test-dir host/build > /tmp/auto_ctest.log 2>&1 || {
    echo CTEST_FAILED; tail -40 /tmp/auto_ctest.log; exit 1; }

# No buying speed with the model: the archive, its rung, the schemas and the
# partition layout are frozen for the whole session.
.venv/bin/python tools/download_model.py --verify-only > /dev/null
./host/build/nd_dump model/needle3.cact header | grep -q '^num_layers 8$'
git diff --quiet -- model/manifest.json tools/demo-tools.json tools/model-routes.json \
    tools/model-catalog.json esp32/partitions.csv model/needle3.cact || {
    echo FROZEN_INPUTS_MODIFIED; git status --short; exit 1; }

# Quality gate 1: byte-identical generation on every prompt in the frozen set.
.venv/bin/python .auto/bench.py host > /tmp/auto_cq.log 2>&1 || {
    echo HOST_QUALITY_CRASHED; tail -30 /tmp/auto_cq.log; exit 1; }
grep -E '^(METRIC|DIVERGE|FIDELITY)' /tmp/auto_cq.log
exact=$(grep -o 'host_output_exact=[0-9]*' /tmp/auto_cq.log | cut -d= -f2)
cases=$(grep -o 'host_cases=[0-9]*' /tmp/auto_cq.log | cut -d= -f2)
[ "${exact:-0}" = "${cases:-1}" ] || { echo "HOST_OUTPUT_DIVERGED ${exact}/${cases}"; exit 1; }

# Quality gate 2: numeric fidelity of the forward pass on a fixed probe.
.venv/bin/python .auto/bench.py fidelity >> /tmp/auto_cq.log 2>&1 || {
    echo FIDELITY_CRASHED; tail -30 /tmp/auto_cq.log; exit 1; }
grep -E '^(METRIC|FIDELITY)' /tmp/auto_cq.log | tail -3
delta=$(grep -o 'logit_max_delta=[0-9.eE+-]*' /tmp/auto_cq.log | tail -1 | cut -d= -f2)
awk -v d="${delta:-999}" 'BEGIN { exit !(d <= 0.002) }' || {
    echo "LOGIT_DRIFT_TOO_LARGE ${delta}"; exit 1; }
