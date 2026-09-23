#!/bin/bash
# Behavioural capture on the SHIPPING image (the cadence the campaign owes when
# shipping code changes - it changed at run #362, the fwht_rows rescale unroll).
#
# Runs inside `needle-board run 1 --`, which is what provides $FLASH_PORT,
# $SERIAL_PORT and the board lock. Every step prints a marker, because a lane
# whose log stops is indistinguishable from a lane that never started (#359).
set -uo pipefail
R=$(pwd)
LOG=/root/board-pool/batches/20260923T0940L/capture1.log
mkdir -p "$(dirname "$LOG")"
say() { echo "[capture] $*"; }

say "start=$(date -u +%FT%TZ) board=1 root=$R"
# The accepted tree is one file: prove the copy landed before believing anything
# the board says afterwards (run #359's "launched lane that was still the old
# image" is the failure this guard exists for).
cp /tmp/ndq_rescale_only.c "$R/engine/src/nd_quant.c"
[ "$(md5sum < /tmp/ndq_rescale_only.c)" = "$(md5sum < "$R/engine/src/nd_quant.c")" ] ||
  { say "SYNC_VERIFY_FAILED"; exit 1; }
say "nd_quant_md5=$(md5sum < "$R/engine/src/nd_quant.c" | cut -c1-12) rescale=$(grep -c 'Unrolled by 4' "$R/engine/src/nd_quant.c")"
rm -f "$R/esp32/sdkconfig"

# Use the repo's own targets: the IDF project lives in esp32/, and running
# idf.py from the worker root fails with "CMakeLists.txt not found in project
# directory" (measured: build_rc=2, flash_rc=2 - which is also the only reason
# no stale binary got flashed). Abort on failure instead of carrying on and
# capturing whatever image happens to be on the board.
. /opt/esp/idf/export.sh >/dev/null 2>&1
BEFORE=$(md5sum < "$R/esp32/build/needle_demo.bin" 2>/dev/null | cut -c1-12)
say "prev_image_md5=${BEFORE:-none}"
say "building shipping image (make -C $R flash-app)"
make -C "$R" flash-app FLASH_PORT="$FLASH_PORT" > "$LOG.build" 2>&1; RC=$?
say "flash_app_rc=$RC"
tail -4 "$LOG.build"
[ "$RC" = 0 ] || { say "ABORT: could not build/flash the shipping image; refusing to capture a stale one"; exit 1; }
AFTER=$(md5sum < "$R/esp32/build/needle_demo.bin" | cut -c1-12)
say "new_image_md5=$AFTER changed=$([ "$BEFORE" != "$AFTER" ] && echo yes || echo no)"

say "starting api on $SERIAL_PORT (boot + two-prefix priming is ~5 min)"
needle-api --serial "$SERIAL_PORT" > "$LOG.api" 2>&1 &
API=$!
say "api_pid=$API"
for i in $(seq 1 90); do
  sleep 10
  if curl -sf --max-time 5 http://127.0.0.1:8081/health > "$LOG.health" 2>&1; then
    say "health_ok after ${i}0s: $(head -c 160 "$LOG.health")"
    break
  fi
  [ "$((i % 6))" = 0 ] && say "waiting for ready ($((i * 10))s)"
done
curl -sf --max-time 5 http://127.0.0.1:8081/state > "$LOG.state" 2>&1 && say "state: $(head -c 200 "$LOG.state")"

say "running capture"
cd /workspace/esp32-needle-3
.venv/bin/python demo/capture.py > "$LOG.capture" 2>&1; CAP=$?
say "capture_rc=$CAP"
grep -aE "flag|match|verified|FLAG|✓|FAIL|Error|Traceback" "$LOG.capture" | tail -22

# The API spawns tools/serial_api.py as a child which keeps the console open, so
# pkill on the api name is not enough - the leftover child makes the next board-1
# run fail with "Board 1 is busy" (#334's lost slot). Kill the child by pid first.
say "stopping api"
pkill -P "$API" 2>/dev/null
kill "$API" 2>/dev/null
sleep 5
LEFT=$(pgrep -f "[s]erial_api.py" | tr '\n' ' ')
say "serial_api_children_remaining=${LEFT:-none}"
say "done=$(date -u +%FT%TZ) capture_rc=$CAP"
exit "$CAP"
