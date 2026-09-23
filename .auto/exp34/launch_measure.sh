#!/bin/bash
# Experiment 34: measure the integrated prepare patch on three boards at once.
#
# Board 1 runs BOTH changes (the candidate as it would ship), board 2 runs the
# transform only, board 3 the rescale only. That is deliberately an ablation
# batch rather than three unrelated screens: the combined +0.70 % prediction is
# the sum of two independent measured kernel deltas (transform +36.75 % of 80.6 %
# of the call, rescale +263 % of 18.3 %), and a single combined reading cannot
# say which half carries the win - or whether the transform's 8-byte-alignment
# gate silently never fires on the real scratch buffer, which would make the
# combined delta look like the rescale's own +0.24 %. Reason recorded before
# taking the boards, as the pool rules require.
set -uo pipefail
BATCH="$1"; REPO=/workspace/esp32-needle-3
pane() {
  local n="$1" variant="$2"
  local R=/root/board-pool/board$n
  # One explicit file, verified by hash, instead of a bulk rsync whose errors were
  # swallowed by 2>/dev/null: run #359's first attempt at this batch "launched" a
  # lane that was still building the accepted image. The candidate is a single
  # engine file, so copy that file and prove the copy landed.
  if [ "$variant" = both ]; then src="$REPO/engine/src/nd_quant.c"; else src="/tmp/ndq_$variant.c"; fi
  cp "$src" "$R/engine/src/nd_quant.c"
  [ "$(md5sum < "$src")" = "$(md5sum < "$R/engine/src/nd_quant.c")" ] || { echo "SYNC_VERIFY_FAILED board=$n"; return 1; }
  rm -f "$R/esp32/sdkconfig"
  : > "$BATCH/lane$n.out"
  tmux kill-session -t "e34$n" 2>/dev/null
  cat > "/tmp/e34lane$n.sh" <<EOF
#!/bin/bash
{
  echo "start=\$(date -u +%FT%TZ) variant=$variant"
  grep -c 'fwht_stages_pairs' engine/src/nd_quant.c | sed 's/^/transform_uses=/'
  grep -c 'Unrolled by 4' engine/src/nd_quant.c | sed 's/^/rescale_hunk=/'
  md5sum engine/src/nd_quant.c | cut -c1-12 | sed 's/^/nd_quant_md5=/'
  /root/bin/needle-board run $n -- bash .auto/measure.sh
  echo "MEASURE rc=\$?"
  sleep 45
} >> "$BATCH/lane$n.log" 2>&1
EOF
  chmod +x "/tmp/e34lane$n.sh"
  tmux new-session -d -s "e34$n" -c "$R" "/tmp/e34lane$n.sh"
  sleep 2
}
mkdir -p "$BATCH"
pane 1 both
pane 2 transform_only
pane 3 rescale_only
echo "panes=3 batch=$BATCH"
grep -c . "$BATCH"/lane*.out 2>/dev/null
