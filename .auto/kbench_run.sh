#!/bin/bash
# Flash one prebuilt kernel-bench image per board and capture its console,
# concurrently, each under its own board lock.
#
#   .auto/kbench_run.sh <board>=<img> [<board>=<img> ...]
#
# Logs land under /root/board-pool/batches/<stamp>-kb/. This is kernel
# screening only: the frozen 12-case quality suite is what accepts a change, and
# this harness does not run it.
set -uo pipefail

STAMP=$(date -u +%Y%m%dT%H%M%S.%fZ)
DIR=/root/board-pool/batches/$STAMP-kb
WINDOW=${KB_WINDOW:-180}
mkdir -p "$DIR"
echo "batch=$DIR window=$WINDOW"

pids=()
for spec in "$@"; do
    b=${spec%%=*}; img=${spec#*=}
    /root/bin/needle-board run "$b" -- bash -c \
        "python3 -m esptool --chip esp32s3 --port \$FLASH_PORT --baud 921600 write_flash 0x10000 $img && md5sum $img && timeout --foreground -k 10 $WINDOW .venv/bin/python .auto/kb_capture.py \$SERIAL_PORT $WINDOW $DIR/board$b.raw" \
        > "$DIR/board$b.log" 2>&1 &
    pids+=("$!:$b")
done

for p in "${pids[@]}"; do
    pid=${p%%:*}; b=${p##*:}
    wait "$pid"; rc=$?
    echo "board$b exit=$rc"
    [ "$rc" -ne 0 ] && echo "board$b: flash or capture failed (see board$b.log)"
done

for p in "${pids[@]}"; do
    b=${p##*:}
    echo "===== board$b $(grep -a -m1 'md5' "$DIR/board$b.log" 2>/dev/null) ====="
    grep -a '^KB CFG\|^KB NUM\|^KSUM\|^KB DELTA\|^KB PROBE\|^KB MEM\|^KB FAIL\|^KB SKIP\|^EVT KBENCH_DONE\|ets \|Guru\|abort' "$DIR/board$b.raw" 2>/dev/null | tail -60
done
