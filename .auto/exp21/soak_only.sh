#!/usr/bin/env bash
# Soak only: the board already runs the image under test.
#   /root/bin/needle-board run N -- bash .auto/exp21/soak_only.sh N <minutes>
set -u
REPO=/workspace/esp32-needle-3
B=${1:?board}
SOAK_MIN=${2:-20}
export EXP21_TAG=board$B
export EXP21_BOARD=$B
LOG=/tmp/exp21-soak-board$B.log
echo "=== BOARD $B soak-only ${SOAK_MIN}m (console $SERIAL_PORT)" | tee -a "$LOG"
"$REPO/.venv/bin/python" "$REPO/.auto/exp21/soak.py" "$SOAK_MIN" 2>&1 | tee -a "$LOG"
