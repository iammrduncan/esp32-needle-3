#!/bin/bash
# Experiment 36 lanes: the ZCRMS emit-loop unroll, one variant per board.
#
# Same shape discipline as run #362 (independent per-element arithmetic, unrolled
# by 4, accumulation loops untouched). Each variant is host-proven byte-exact
# before a board is touched - the host compiles this same C, so `checks.sh` is a
# real correctness verdict and not a proxy (that is how run #363's class gets
# caught without spending 13 minutes of flash).
#
# Unlike runs #333-#359 the candidate lives in engine/src/nd_model.c, so the
# per-lane sync carries that file too, verified by hash.
set -uo pipefail
REPO=/workspace/esp32-needle-3
BATCH=${1:?usage: launch.sh <batch-dir>}

pane() { # n variant [noflash]
  local n="$1" variant="$2" repeat_env=""
  if [ "${3:-}" = noflash ]; then repeat_env="AUTO_NOFLASH=1"; fi
  local R=/root/board-pool/board$n src=$REPO/../nonexistent
  # Any /tmp/ndm_<variant>.c the generators produced; `accepted` is the shipping
  # engine file, used when a lane needs the baseline.
  case "$variant" in
    accepted) src=/tmp/ndq_rescale_only.c ;;
    *)        src=/tmp/ndm_$variant.c ;;
  esac
  local target=$R/engine/src/nd_model.c
  [ "$variant" = accepted ] && target=$R/engine/src/nd_quant.c
  # Boards 2 and 3 were last flashed with the REJECTED run #365 variants of
  # nd_quant.c. Leaving that file in place would measure two changes at once, so
  # every lane starts from the accepted engine file and changes exactly one lever.
  cp /tmp/ndq_rescale_only.c "$R/engine/src/nd_quant.c"
  [ "$(md5sum < /tmp/ndq_rescale_only.c)" = "$(md5sum < "$R/engine/src/nd_quant.c")" ] ||
    { echo "BASE_SYNC_FAILED board=$n"; return 1; }
  if [ ! -f "$src" ]; then echo "MISSING_VARIANT board=$n $src"; return 1; fi
  cp "$src" "$target"
  [ "$(md5sum < "$src")" = "$(md5sum < "$target")" ] ||
    { echo "SYNC_VERIFY_FAILED board=$n"; return 1; }
  # Some candidates need a second engine file (run #368's bundle changes
  # nd_model.c AND nd_quant.c). If a variant ships /tmp/ndq_<variant>.c it is
  # synced too, with the same hash proof, so a lane can never silently measure
  # half a candidate.
  if [ -f "/tmp/ndq_$variant.c" ]; then
    cp "/tmp/ndq_$variant.c" "$R/engine/src/nd_quant.c"
    [ "$(md5sum < "/tmp/ndq_$variant.c")" = "$(md5sum < "$R/engine/src/nd_quant.c")" ] ||
      { echo "QSYNC_FAILED board=$n"; return 1; }
  fi
  rm -f "$R/esp32/sdkconfig"
  tmux kill-session -t "e36$n" 2>/dev/null
  cat > "/tmp/e36lane$n.sh" <<EOF
#!/bin/bash
{
  echo "start=\$(date -u +%FT%TZ) variant=$variant"
  md5sum engine/src/nd_model.c | cut -c1-12 | sed 's/^/nd_model_md5=/'
  grep -c 'Unrolled by 4' engine/src/nd_model.c | sed 's/^/emit_unroll_hunks=/'
  grep -c 'ss += ' engine/src/nd_model.c | sed 's/^/accum_loops_untouched=/'
  ${repeat_env:-} /root/bin/needle-board run $n -- bash .auto/measure.sh
  echo "MEASURE rc=\$?"
  sleep 45
} >> "$BATCH/lane$n.log" 2>&1
EOF
  chmod +x "/tmp/e36lane$n.sh"
  tmux new-session -d -s "e36$n" -c "$R" "/tmp/e36lane$n.sh"
  grep -c 'Unrolled by 4' "$R/engine/src/nd_quant.c" | sed "s/^/q_rescale=/" >> "$BATCH/lane$n.log" 2>/dev/null
  echo "launched board $n variant=$src -> $target"
}

mkdir -p "$BATCH"
# Boards 2 and 3 were already flashed with exactly these two candidate images
# (app_md5 8e0263737dc7 and 46ac9263ab84) before the sessions were destroyed, so
# they re-measure without paying a redundant flash of identical bytes.
# LANES lets a later batch pick boards/variants/modes without editing the file:
#   LANES="2:hadascale 3:hadagather"  /  LANES="2:zcsplit:noflash ..."
if [ -n "${LANES:-}" ]; then
  for spec in $LANES; do
    n=${spec%%:*}; rest=${spec#*:}; v=${rest%%:*}; m=$( [ "$rest" = "$v" ] && echo "" || echo "${rest#*:}" )
    pane "$n" "$v" "$m"
  done
  echo "panes=${LANES} batch=$BATCH"
  exit 0
fi
pane 2 zcsplit noflash
pane 3 zchead noflash
echo "panes=3 batch=$BATCH mode=${MODE:-full}"
pane 1 kvstore
