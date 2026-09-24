#!/usr/bin/env bash
# Behavioural capture on the board this session owns, against the tree this worker holds.
#
# Why a script instead of an inline lane command: `needle-board run N -- bash -c "...$SERIAL_PORT..."`
# expands the variable in the PANE's shell, which does not have it yet - the wrapper exports it for
# the command it execs - so the API got `--serial ''` and capture.py reported "connection refused",
# which looks like a dead board and is actually an empty argument. Reading the variable here, at
# runtime, is the fix; the script refuses to run at all if it is missing.
set -uo pipefail
PORT="${SERIAL_PORT:-}"
BOARD="${NEEDLE_BOARD:-?}"
[ -n "$PORT" ] || { echo "NO_SERIAL_PORT board=$BOARD"; exit 2; }
API_PORT="${CAPTURE_API_PORT:-8093}"
cd "$(dirname "$0")/.." || exit 2
echo "CAPTURE board=$BOARD tree=$(md5sum engine/src/nd_model.c | cut -c1-12) header=$(md5sum engine/include/nd_quant.h | cut -c1-12) serial=$PORT"
.venv/bin/python tools/serial_api.py --serial "$PORT" --port "$API_PORT" \
    > /tmp/capture-api-board$BOARD.log 2>&1 &
API=$!
for _ in $(seq 1 60); do
    sleep 5
    curl -s -m 3 "http://127.0.0.1:$API_PORT/health" >/dev/null && break
done
if ! curl -s -m 3 "http://127.0.0.1:$API_PORT/health" >/dev/null; then
    echo "API_NOT_UP board=$BOARD port=$API_PORT - see /tmp/capture-api-board$BOARD.log"
    tail -5 "/tmp/capture-api-board$BOARD.log"
    kill "$API" 2>/dev/null; exit 3
fi
echo "API_UP board=$BOARD port=$API_PORT"
.venv/bin/python demo/capture.py --url "http://127.0.0.1:$API_PORT"
RC=$?
kill "$API" 2>/dev/null
echo "CAPTURE_RC=$RC board=$BOARD"
exit $RC
