#!/bin/bash
# Build the CURRENT checkout's app image without flashing (host pre-check +
# idf build), leaving esp32/build/needle_demo.bin for a later image flash.
# Lets one checkout produce several candidate images (e.g. sync and profiled
# builds of the same source) that a batch can then flash on different boards.
set -euo pipefail
TAG=${1:-cand}
cd "$(dirname "$0")/.."
. /opt/esp/idf/export.sh >/dev/null 2>&1
cmake -S host -B host/build > /tmp/buildimg_host.log 2>&1
cmake --build host/build -j8 >> /tmp/buildimg_host.log 2>&1
CFG=()
[ "${AUTO_PROFILE:-0}" = 1 ] && CFG=(-DNEEDLE_PROFILE=ON)
idf.py -C esp32 ${CFG[@]+"${CFG[@]}"} build > "/tmp/buildimg_${TAG}.log" 2>&1 || {
    echo BUILD_FAILED; grep -E 'error:|ERROR' "/tmp/buildimg_${TAG}.log" | head -20; exit 1; }
OUT=${2:-/tmp/img_${TAG}.bin}
cp esp32/build/needle_demo.bin "$OUT"
echo "img_md5=$(md5sum "$OUT" | cut -c1-12) out=$OUT"
