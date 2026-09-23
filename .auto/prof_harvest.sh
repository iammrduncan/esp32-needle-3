#!/usr/bin/env bash
# Request-path phase harvest on a profiled image (diagnostic only - the timers cost
# ~0.2 % and this image is never a measurement of decode_tps).
#
# Why this exists: run #341 measured mean_n = 6.8 candidate rows per decode step on
# the HOST, which makes the subset projection ~0.08 ms, while a profiled board reads
# logits4 = 3.8 ms per request token. Those two numbers differ by ~24x and nothing
# else in the ledger explains it, so the sampler phase must be priced on the device
# with n measured alongside it.
#
# ND_PROFILE goes in through CMAKE_C_FLAGS on a FRESH build dir: the component's
# option(NEEDLE_PROFILE) does not take effect in a fresh configure (run #158), and the
# token appears as a bare -DND_PROFILE, so verify that exact string in
# compile_commands.json before believing any table. Pass extra defines verbatim
# (PROF_FLAGS="-DND_PROFILE=1 -DX=1"); they are substituted as-is, because a template
# that prefixes one -D silently loses it on every flag after the first and the compile
# then dies with "ND_X=1: linker input file not found".
set -euo pipefail
BOARD=${NEEDLE_BOARD:?run inside needle-board run N}
LOG=${1:-/tmp/prof_harvest.log}
PROF_FLAGS=${PROF_FLAGS--DND_PROFILE=1}

if ! command -v idf.py >/dev/null 2>&1; then
    . /opt/esp/idf/export.sh >/dev/null 2>&1 || true
fi

rm -rf esp32/build-prof                 # a poisoned cache cannot be reconfigured around
( cd esp32 && idf.py -B build-prof -D CMAKE_C_FLAGS="${PROF_FLAGS}" build ) \
    > "${LOG}.build" 2>&1
grep -q -- '-DND_PROFILE' esp32/build-prof/compile_commands.json || {
    echo PROFILE_FLAG_MISSING; exit 1; }
( cd esp32 && idf.py -B build-prof -p "$FLASH_PORT" flash ) >> "${LOG}.build" 2>&1
echo "PROFILED_FLASH_OK bin=$(md5sum esp32/build-prof/needle3.bin | cut -c1-12)"

# The request-path table is NOT in measure.sh's log: serial_api.Device's complete()
# reads with _line_quiet(), which swallows the EVT prof block printed after EVT done.
# Drive two requests against the warm, primed board instead.
sleep 45                                 # let the priming finish after the flash reset
.venv/bin/python .auto/prof_request.py \
    "What is the heap high water mark and the current sampling interval?" \
    "Translate good morning into German" > "$LOG" 2>&1 || true
grep -E '^###|EVT done|EVT cands|EVT prof (sample|logits4|whole block|attn-stage|proj2bit|prep|tok-)|ERR ' \
    "$LOG" || true
echo "HARVEST_DONE board=$BOARD"
