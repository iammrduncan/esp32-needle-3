#!/bin/bash
# Primary metric: mean device-reported decode tps over the frozen primary
# prompt set, measured through the real request path on the attached board.
#
# Env knobs (never change what the metric means):
#   AUTO_GROUPS=primary,...   restrict groups (diagnostics only; default all)
#   AUTO_SAVE=1               rewrite the device golden (only for a documented
#                             re-baseline, never to make a run look good)
#   AUTO_PROFILE=1            build with ND_PROFILE=ON for the phase breakdown
#                             (adds timers: do not use for keep/discard)
#   AUTO_NOFLASH=1            re-measure the already-flashed app (noise checks)
#   AUTO_MODEL=1              also rewrite the model partition
set -euo pipefail
cd "$(dirname "$0")/.."
AUTO_LOG_DIR=${AUTO_LOG_DIR:-$PWD/.auto/runs/local}
mkdir -p "$AUTO_LOG_DIR"
ROOT=$PWD
# The coordinator's normal confirmation run shares board 1 with its worker.
# Pool workers already hold this lock in needle-board.
if [ -z "${NEEDLE_BOARD:-}" ] && [ -d /root/board-pool ]; then
    exec 9>/root/board-pool/locks/board1.lock
    flock -n 9 || { echo "Board 1 is busy"; exit 1; }
    FLASH_PORT=/dev/needle-pi/flash
    SERIAL_PORT=/dev/needle-pi/console
fi
# IDF_PATH can be exported while the tools are not on PATH (the experiment
# harness does this); sourcing is then what makes idf.py/esptool findable.
if ! command -v idf.py >/dev/null 2>&1; then
    . /opt/esp/idf/export.sh >/dev/null 2>&1 || true
fi
FLASH_PORT=${FLASH_PORT:-/dev/ttyACM0}
SERIAL_PORT=${SERIAL_PORT:-/dev/ttyACM1}

# Fast pre-check: the engine has to compile at all (seconds, no hardware).
cmake -S host -B host/build > "$AUTO_LOG_DIR/auto_host.log" 2>&1
cmake --build host/build -j8 >> "$AUTO_LOG_DIR/auto_host.log" 2>&1 || {
    echo HOST_BUILD_FAILED; grep -E 'error' "$AUTO_LOG_DIR/auto_host.log" | head -20; exit 1; }
.venv/bin/python tools/download_model.py --verify-only > /dev/null

CFG=(-DNEEDLE_PROFILE=OFF)
[ "${AUTO_PROFILE:-0}" = 1 ] && CFG=(-DNEEDLE_PROFILE=ON)
t0=$(date +%s)
idf.py -C esp32 ${CFG[@]+"${CFG[@]}"} build > "$AUTO_LOG_DIR/auto_build.log" 2>&1 || {
    echo FIRMWARE_BUILD_FAILED
    grep -E 'error:|ERROR' "$AUTO_LOG_DIR/auto_build.log" | head -25
    exit 1; }
echo "build_s=$(( $(date +%s) - t0 ))"
# A rebuild that silently no-ops is the one failure mode that makes every number
# below meaningless: it measures the previous candidate. Record what went in.
{ echo "engine_md5=$(md5sum engine/src/*.c engine/include/*.h | md5sum | cut -c1-12)"
  echo "app_md5=$(md5sum esp32/build/needle_demo.bin | cut -c1-12)"; } > "$AUTO_LOG_DIR/auto_build_id.txt"
cat "$AUTO_LOG_DIR/auto_build_id.txt"

if [ "${AUTO_NOFLASH:-0}" != 1 ]; then
    t0=$(date +%s)
    if [ "${AUTO_MODEL:-0}" = 1 ]; then
        python3 -m esptool --chip esp32s3 --port "$FLASH_PORT" \
            --baud 921600 write_flash 0x210000 model/needle3.cact > "$AUTO_LOG_DIR/auto_model.log" 2>&1
    fi
    idf.py -C esp32 -p "$FLASH_PORT" flash > "$AUTO_LOG_DIR/auto_flash.log" 2>&1 || {
        echo FLASH_FAILED; tail -20 "$AUTO_LOG_DIR/auto_flash.log"; exit 1; }
    echo "flash_s=$(( $(date +%s) - t0 ))"
fi

ARGS=(device --port "$SERIAL_PORT")
[ -n "${AUTO_GROUPS:-}" ] && ARGS+=(--groups "$AUTO_GROUPS")
# The baseline run (iteration 1) freezes the device goldens; later runs compare
# against them and must stay byte-exact.
SAVE=$( [ "$(wc -l < .auto/log.jsonl 2>/dev/null || echo 0)" -le 1 ] && echo 1 || echo 0 )
cd "$ROOT"
if [ "$SAVE" = 1 ]; then
    .venv/bin/python .auto/bench.py "${ARGS[@]}" --save-golden
else
    .venv/bin/python .auto/bench.py "${ARGS[@]}"
fi
