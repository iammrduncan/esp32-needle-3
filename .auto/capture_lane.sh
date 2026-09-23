#!/usr/bin/env bash
# Behavioural capture on the accepted image, on board 1, inside one board lock.
#
# Why a script: `make capture` needs the API (demo/capture.py talks to
# 127.0.0.1:8081 and fails with a bare "Connection refused" without it), the API
# needs the console, and the console needs the board lock - so sync, build, flash,
# API, capture and API teardown must all happen inside a single
# `needle-board run 1 --` invocation, in that order, with the API killed by pid
# (pkill does not stop needle-api: it spawns tools/serial_api.py as a child that
# keeps the console, which made the next board-1 run fail with "Board 1 is busy").
#
# The image being captured must be the ACCEPTED image, so the worker is reset from
# the local branch and the engine files are taken from the main checkout by path -
# `git fetch origin` is not used because this container has no credentials and a
# stale-origin fetch reverts the worker (that is how a control reads low).
set -uo pipefail

MAIN=/workspace/esp32-needle-3
W=/root/board-pool/board1
BRANCH=autoresearch/decode-tps-2026-09-18
LOG=/root/board-pool/batches/capture-$(date -u +%Y%m%dT%H%M%SZ).log
exec > >(tee "$LOG") 2>&1

echo "start=$(date -u +%FT%TZ) board=1 purpose=make_capture"

cd "$MAIN" || { echo "SYNC_FAIL main"; exit 1; }
git -C "$W" fetch -q "$MAIN" "$BRANCH" && git -C "$W" reset --hard -q FETCH_HEAD \
    || { echo "SYNC_FAIL reset"; exit 1; }
for f in engine/src/nd_model.c engine/src/nd_quant.c engine/src/nd_sample.c \
         engine/include/nd_quant.h engine/include/nd_model.h \
         esp32/main/main.c esp32/main/CMakeLists.txt \
         esp32/components/needle/CMakeLists.txt esp32/sdkconfig.defaults; do
    cp "$MAIN/$f" "$W/$f" || echo "copy failed: $f"
done
rm -f "$W/esp32/sdkconfig"          # gitignored; regenerate from the accepted defaults
echo "engine_md5=$(md5sum < $W/engine/src/nd_quant.c | cut -c1-12) $(md5sum < $W/engine/src/nd_model.c | cut -c1-12)"
echo "main_tree_engine=$(md5sum < $MAIN/engine/src/nd_quant.c | cut -c1-12) $(md5sum < $MAIN/engine/src/nd_model.c | cut -c1-12)"

cd "$W" || exit 1
make flash-app FLASH_PORT="${FLASH_PORT:-/dev/needle-pi/board1-flash}" > /tmp/cap_flash.log 2>&1
echo "flash_rc=$? tail=$(tail -2 /tmp/cap_flash.log | tr '\n' ' ' | cut -c1-120)"
grep -q "Hash of data verified" /tmp/cap_flash.log || echo "WARN flash hash line missing"

# Boot and warm the two model caches; the firmware needs ~5 min before EVT READY.
#
# Do NOT open and close the console in a poll loop. pyserial asserts DTR and pulses
# RTS on open, and a RTS transition resets this board - so a 15 s poll was rebooting
# the chip every 15 s and it could never finish its two model caches. Open ONCE, with
# the flags configured before open() and DTR asserted only after, then read
# continuously (AGENTS.md's own warning about DTR/RTS on open).
echo "waiting for EVT READY on ${SERIAL_PORT:-/dev/needle-pi/console}"
ready_flag=$(SERIAL_PORT="${SERIAL_PORT:-/dev/needle-pi/console}" .venv/bin/python - <<'PY'
import os, time, serial
sp = os.environ.get("SERIAL_PORT", "/dev/needle-pi/console")
s = serial.Serial()
s.port = sp
s.baudrate = 115200
s.timeout = 1
s.dsrdtr = False           # no hardware handshake toggles on open
s.rtscts = False
s.dtr = True               # the USB CDC bridge drops output without DTR
s.rts = False              # and RTS is the reset line: never assert it here
try:
    s.open()
except Exception as e:
    print("open_failed", e); raise SystemExit(1)
t0, buf, seen = time.time(), b"", 0
while time.time() - t0 < 600:
    buf += s.read(4096)
    if b"EVT READY" in buf:
        seen = 1
        break
print(seen)
s.close()
PY
)
echo "ready=$ready_flag"
[ "$ready_flag" = "1" ] || { echo "BOARD_NOT_READY"; exit 1; }

# API in the background, then the repo's own behavioural suite.
.venv/bin/python tools/serial_api.py --serial "${SERIAL_PORT:-/dev/needle-pi/board1-console}" > /tmp/cap_api.log 2>&1 &
API=$!
echo "api_pid=$API"
for _ in $(seq 1 20); do sleep 3; curl -s -m 3 http://127.0.0.1:8081/health >/dev/null 2>&1 && break; done
curl -s -m 5 http://127.0.0.1:8081/health | head -c 200; echo

make capture > /tmp/cap_capture.log 2>&1
CAP=$?
echo "capture_rc=$CAP"
grep -aoE '"[a-z_]*(match|succeeded|passes|progressed|applied|expired|external[a-z_]*)":[^,}]*' /tmp/cap_capture.log | head -12
tail -4 /tmp/cap_capture.log

kill "$API" 2>/dev/null; sleep 2
kill -9 "$API" 2>/dev/null
pkill -9 -f '[s]erial_api.py --serial' 2>/dev/null
sleep 1
echo "api_still_running=$(pgrep -cf '[s]erial_api.py')"
echo "CAPTURE_DONE rc=$CAP end=$(date -u +%FT%TZ)"
exit "$CAP"
