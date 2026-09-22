#!/usr/bin/env bash
# Three independent board lanes (owner directive 2026-09-22): each worker gets ONE
# distinct candidate and is compared against the pinned accepted baseline, not
# against a live control. Started under tmux so a lane survives the command shell
# that launched it - the earlier "dead launcher" was actually measure.sh exiting
# silently under `set -euo pipefail` (see the comment in measure.sh), but a
# persistent session is still the right host for a 15-minute board run.
#
# The wrapper must be INSIDE the tmux window: it is what sets NEEDLE_BOARD,
# FLASH_PORT/SERIAL_PORT and takes the board lock. A window that runs measure.sh
# directly falls back to board 1's ports and board 1's lock (measured: lane 2 died
# with 'Board 1 is busy' while lane 1 was legitimately holding it). AUTO_LOG_DIR is
# therefore the wrapper's per-worker runs/current, and the lane log carries the
# batch id.
# usage: .auto/lanes.sh <batch-id> "1=asm" "2=inline" ["3=none"]
set -uo pipefail
MAIN=/workspace/esp32-needle-3
POOL=/root/board-pool
BRANCH=autoresearch/decode-tps-2026-09-18
BATCH=${1:?batch id}; shift

for spec in "$@"; do
  n=${spec%%=*}; cand=${spec##*=}
  W=$POOL/board$n
  git -C "$W" fetch "$MAIN" "$BRANCH" >/dev/null 2>&1 || { echo "board$n fetch failed"; exit 1; }
  git -C "$W" reset --hard FETCH_HEAD >/dev/null 2>&1
  rm -f "$W/esp32/sdkconfig"                      # regenerate from this tree's defaults
  cp "$MAIN/.auto/"{measure.sh,checks.sh,bench.py,test_bench_guards.py,board_config_check.sh,prompts.json} "$W/.auto/" 2>/dev/null
  cp "$MAIN"/.auto/golden/* "$W/.auto/golden/" 2>/dev/null
  case $cand in
    asm)    git -C "$W" apply "$MAIN/.auto/exp22/integration.patch" &&
            cp "$MAIN/engine/src/gemv4_tie728.S" "$W/engine/src/" ;;
    inline) git -C "$W" apply "$MAIN/.auto/exp22/variantB_inline.patch" ;;
    none)   : ;;
    *)      echo "unknown candidate $cand for board $n"; exit 1 ;;
  esac
  # The candidate must be visible in the tree before a board is spent on it.
  case $cand in
    asm)    grep -q nd_gemv4_rows_tie1 "$W/engine/include/nd_quant.h" &&
            [ -f "$W/engine/src/gemv4_tie728.S" ] || { echo "board$n asm NOT applied"; exit 1; } ;;
    inline) grep -q always_inline "$W/engine/src/nd_quant.c" ||
            { echo "board$n inline NOT applied"; exit 1; } ;;
  esac
  LOG=$POOL/batches/$BATCH/board$n.log; mkdir -p "$POOL/batches/$BATCH" "$W/.auto/runs/$BATCH"
  : > "$LOG"
  tmux new-session -d -s "lane$n" -c "$W" \
    "echo LANE$n candidate=$cand batch=$BATCH head=\$(git rev-parse --short HEAD) start=\$(date -u +%FT%TZ) | tee -a '$LOG'; \
     tail -n +1 -F '$W/.auto/runs/current/auto_build.log' >> '$LOG' 2>/dev/null & \
     /root/bin/needle-board run $n -- bash .auto/measure.sh >> '$LOG' 2>&1; \
     echo LANE$n rc=\$? end=\$(date -u +%FT%TZ) >> '$LOG'; sleep 3600"
  echo "board$n candidate=$cand dirty=$(git -C "$W" status --porcelain -- engine esp32 host tools | wc -l) log=$LOG"
done
