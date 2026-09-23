#!/usr/bin/env bash
# Lane body for the request-path profile harvest. Run inside:
#   needle-board run N -- bash .auto/lane_prof.sh
# Both ND_PROFILE (phase timers) and ND_SAMPLE_NSTAT (candidate-count counter) are
# diagnostics behind defines no shipping configure sets.
set -uo pipefail
if ! command -v idf.py >/dev/null 2>&1; then
    . /opt/esp/idf/export.sh >/dev/null 2>&1 || true
fi
export PROF_FLAGS=${PROF_FLAGS--DND_PROFILE=1\ -DND_SAMPLE_NSTAT=1}
bash .auto/prof_harvest.sh "${OUT:-/tmp/prof_harvest.log}"
