#!/usr/bin/env bash
# Measure the `fwscale` candidate on one board, inside that board's lock.
#
# Unlike .auto/cand_lane.sh (which copies the main tree's working files) this lane
# starts from the ACCEPTED commit and applies one pinned generator, because the main
# checkout is concurrently carrying a different candidate (fw3res on board 1). Two
# candidates must never meet in one image, and "which file did the worker actually
# build" is answered here by an md5 assertion rather than by trust.
set -uo pipefail
B="${1:?board}"
MAIN=/workspace/esp32-needle-3
W=/root/board-pool/board$B
BRANCH=autoresearch/decode-tps-2026-09-18
cd "$MAIN" || { echo "SYNC_FAIL main"; exit 1; }
git -C "$W" fetch -q "$MAIN" "$BRANCH" && git -C "$W" reset --hard -q FETCH_HEAD || { echo "SYNC_FAIL reset"; exit 1; }
rm -f "$W/esp32/sdkconfig"          # gitignored; must regenerate from tracked defaults
ACC=$(md5sum < "$W/engine/src/nd_quant.c" | cut -c1-12)
echo "accepted_engine_md5=$ACC"
python3 "$MAIN/.auto/exp44/apply_fwscale.py" "$W/engine/src/nd_quant.c" || { echo "APPLY_FAILED"; exit 1; }
CAND=$(md5sum < "$W/engine/src/nd_quant.c" | cut -c1-12)
echo "candidate_engine_md5=$CAND"
[ "$CAND" != "$ACC" ] || { echo "CANDIDATE_IS_NOOP"; exit 1; }
grep -q "nd_fwht2s(blk, blk2, g, scale)" "$W/engine/src/nd_quant.c" || { echo "CANDIDATE_NOT_WIRED"; exit 1; }
cd "$W" || exit 1
bash .auto/measure.sh
