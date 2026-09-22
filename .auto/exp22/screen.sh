#!/bin/bash
# Experiment 22 screen: flash the kbench phi image on THIS board and harvest its
# KB lines. Run inside the wrapper so FLASH_PORT/SERIAL_PORT name this board:
#   /root/bin/needle-board run 2 -- bash .auto/exp22/screen.sh 2
set -u
: "${FLASH_PORT:?run inside needle-board run N}"
: "${SERIAL_PORT:?run inside needle-board run N}"
B=${1:?board number}
export EXP21_BOARD=$B
export EXP21_TAG=p22-board$B
ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT/esp32" || exit 1
. /opt/esp/idf/export.sh >/dev/null 2>&1
timeout 600 idf.py -B build_kb -p "$FLASH_PORT" flash > /tmp/p22-flash.log 2>&1
echo "flash_rc=$? board=$B flash=$FLASH_PORT console=$SERIAL_PORT"
# boot_capture.py asserts DTR before resetting through the flash port (the two
# nodes are separate devices) and logs every line to /tmp/exp21-boot-<tag>.log.
"$ROOT/.venv/bin/python" "$ROOT/.auto/exp21/boot_capture.py" 100 >/dev/null 2>&1
echo "--- KB lines (/tmp/exp21-boot-p22-board$B.log)"
grep -aE "^(KB |EVT KBENCH_DONE|Guru|abort|Backtrace)" "/tmp/exp21-boot-p22-board$B.log" | head -40
echo "--- lines_total=$(wc -l < /tmp/exp21-boot-p22-board$B.log)"
