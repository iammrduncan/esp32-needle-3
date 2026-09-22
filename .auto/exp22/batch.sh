#!/usr/bin/env bash
# Experiment 22 integration batch: board 1 = branch head (the accepted runtime,
# the control), boards 2 and 3 = head + the handwritten CQ 4-bit row walker.
# Workers are synced by LOCAL path only: origin has no credentials here and
# fetching it would reset a worker to a stale baseline, the recorded way a control
# reads low and invalidates a whole batch.
#
# AUTO_LOG_DIR is per board AND per batch on purpose: the anti-repeat signature
# history lives in it, so a fresh directory both lets the control run (a control
# inside a concurrent candidate batch is what the guard is for) and keeps this
# batch's evidence out of the canonical history.
set -uo pipefail
MAIN=/workspace/esp32-needle-3
POOL=/root/board-pool
BRANCH=autoresearch/decode-tps-2026-09-18
BATCH=${BATCH:-$(date -u +%Y%m%dT%H%M%S.000000Z)}
mkdir -p "$POOL/batches/$BATCH"

if [ "${SKIP_SYNC:-0}" != 1 ]; then
  for n in 1 2 3; do
    W=$POOL/board$n
    git -C "$W" fetch "$MAIN" "$BRANCH" >/dev/null 2>&1 || { echo "fetch failed board $n"; exit 1; }
    git -C "$W" reset --hard FETCH_HEAD >/dev/null 2>&1
    rm -f "$W/esp32/sdkconfig"          # regenerate from this tree's defaults
    cp "$MAIN/.auto/"{measure.sh,checks.sh,bench.py,test_bench_guards.py,board_config_check.sh,prompts.json} "$W/.auto/" 2>/dev/null
    cp "$MAIN"/.auto/golden/* "$W/.auto/golden/" 2>/dev/null
  done
  # Candidate = boards 2 and 3. Everything else stays at the accepted head.
  for n in 2 3; do
    W=$POOL/board$n
    git -C "$W" apply "$MAIN/.auto/exp22/integration.patch" || { echo "apply failed $n"; exit 1; }
    cp "$MAIN/engine/src/gemv4_tie728.S" "$W/engine/src/"
  done
fi

for n in 1 2 3; do
  W=$POOL/board$n
  mkdir -p "$W/.auto/runs/$BATCH"
  echo "board$n HEAD=$(git -C "$W" rev-parse --short HEAD) dirty=$(git -C "$W" status --porcelain -- engine esp32 host tools | wc -l)"
  setsid nohup env AUTO_LOG_DIR="$W/.auto/runs/$BATCH" \
      /root/bin/needle-board run "$n" -- bash .auto/measure.sh \
      > "$POOL/batches/$BATCH/board$n.log" 2>&1 < /dev/null &
  echo "board$n pid=$! log=$POOL/batches/$BATCH/board$n.log"
done
echo "batch=$BATCH control=board1 candidates=boards2,3(gemv4_tie728)"
