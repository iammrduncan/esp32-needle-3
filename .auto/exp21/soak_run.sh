#!/usr/bin/env bash
# Experiment 21 soak driver for one board. Run inside that board's lock:
#   /root/bin/needle-board run N -- bash /workspace/esp32-needle-3/.auto/exp21/soak_run.sh N <builddir>
#
# Flash, capture one cold boot (ECC/tier/READY/retune evidence + first TDIAGs),
# then hold the board under continuous real requests for SOAK_MIN minutes so the
# die self-heats. Everything lands in /tmp/exp21-board<N>.log.
set -u
REPO=/workspace/esp32-needle-3
B=${1:?board}
BD=${2:?build dir}
SOAK_MIN=${3:-25}
export EXP21_TAG=board$B
export EXP21_BOARD=$B
LOG=/tmp/exp21-board$B.log
: > "$LOG"

echo "=== BOARD $B flash $BD" | tee -a "$LOG"
cd "/root/board-pool/board$B/esp32" || exit 1
[ -n "$FLASH_PORT" ] && [ -n "$SERIAL_PORT" ] || { echo "NO_PORT_VARS"; exit 1; }
idf.py -B "$BD" -p "$FLASH_PORT" flash >> "$LOG" 2>&1 || { echo "FLASH_FAILED" | tee -a "$LOG"; exit 1; }
echo "=== flash ok; boot capture" | tee -a "$LOG"
"$REPO/.venv/bin/python" "$REPO/.auto/exp21/boot_capture.py" 330 >> "$LOG" 2>&1
echo "=== boot capture done; soak ${SOAK_MIN}m" | tee -a "$LOG"
"$REPO/.venv/bin/python" "$REPO/.auto/exp21/soak.py" "$SOAK_MIN" >> "$LOG" 2>&1
echo "=== BOARD $B soak complete" | tee -a "$LOG"
echo "=== log tails" | tee -a "$LOG"
tail -3 "/tmp/exp21-soak-board$B.log" >> "$LOG" 2>&1
