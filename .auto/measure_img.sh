#!/bin/bash
# Measure a PREBUILT image on a board: flash it, run the frozen 12-case device
# suite, print METRIC lines. Needed for the profiled candidate (the queue's
# Experiment 1) because measure.sh always rebuilds from the checkout's sources.
# The measurement is identical to measure.sh's device phase: same bench.py path,
# same goldens, same boot bench. Only the build source differs.
set -euo pipefail
BOARD=$1; IMG=$2
[ -n "$BOARD" ] && [ -n "$IMG" ] || { echo "usage: $0 <1|2|3> <image.bin>"; exit 2; }
cd "$(dirname "$0")/.."
. /opt/esp/idf/export.sh >/dev/null 2>&1
case $BOARD in
  1) FLASH=/dev/needle-pi/flash;   SERIAL=/dev/needle-pi/console ;;
  2) FLASH=/dev/needle-pi/board2-flash; SERIAL=/dev/needle-pi/board2-console ;;
  3) FLASH=/dev/needle-pi/board3-flash; SERIAL=/dev/needle-pi/board3-console ;;
  *) echo "bad board"; exit 2 ;;
esac
python3 -m esptool --chip esp32s3 --port "$FLASH" --baud 921600 \
    write_flash 0x10000 "$IMG" > /tmp/measure_img_flash.log 2>&1 || {
    echo FLASH_FAILED; tail -20 /tmp/measure_img_flash.log; exit 1; }
echo "img_md5=$(md5sum "$IMG" | cut -c1-12)"
AUTO_LOG_DIR=$PWD/.auto/runs/img
mkdir -p "$AUTO_LOG_DIR"
.venv/bin/python .auto/bench.py device --port "$SERIAL" | tee "$AUTO_LOG_DIR/device.log"
