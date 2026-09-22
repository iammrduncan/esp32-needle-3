#!/usr/bin/env bash
# Flash an already-built image, capture one cold boot, then soak it under load.
#   /root/bin/needle-board run N -- bash .auto/exp21/flash_boot_soak.sh N <builddir> [minutes]
#
# Ports come from the wrapper (FLASH_PORT/SERIAL_PORT). The single-board
# /dev/ttyACM{0,1} aliases are board 1's nodes and the Python steps refuse to
# run without the per-board variables, so a mis-wired batch fails loudly instead
# of driving the wrong chip.
set -u
REPO=/workspace/esp32-needle-3
B=${1:?board}
BD=${2:?build dir}
SOAK_MIN=${3:-20}
export EXP21_TAG=board$B
export EXP21_BOARD=$B
LOG=/tmp/exp21-board$B.log

[ -n "$FLASH_PORT" ] && [ -n "$SERIAL_PORT" ] || { echo "NO_PORT_VARS"; exit 1; }
: > "$LOG"
echo "=== BOARD $B flash $BD ($FLASH_PORT)" | tee -a "$LOG"
cd "/root/board-pool/board$B/esp32" || exit 1
idf.py -B "$BD" -p "$FLASH_PORT" flash >> "$LOG" 2>&1 || { echo "FLASH_FAILED" | tee -a "$LOG"; exit 1; }
echo "=== flash ok; boot capture" | tee -a "$LOG"
"$REPO/.venv/bin/python" "$REPO/.auto/exp21/boot_capture.py" 330 >> "$LOG" 2>&1
echo "=== boot capture done; soak ${SOAK_MIN}m" | tee -a "$LOG"
"$REPO/.venv/bin/python" "$REPO/.auto/exp21/soak.py" "$SOAK_MIN" 2>&1 | tee -a "$LOG"
echo "=== BOARD $B complete" | tee -a "$LOG"
