#!/usr/bin/env bash
# Measure ONE generator-produced candidate on one board, from the accepted commit.
#
# The candidate is applied by a pinned generator inside the worker's own tree, never
# in the main checkout: the main tree is what the next candidate starts from, and two
# candidates must never meet in one image. The generator asserts its base md5, the lane
# asserts the file actually changed and that the new symbol/line is present, so a
# generator that silently matched nothing cannot pose as a measurement (#379/#363).
set -uo pipefail
B="${1:?board}"; GEN="${2:?generator script}"; TARGET="${3:-engine/src/nd_model.c}"
MAIN=/workspace/esp32-needle-3
W=/root/board-pool/board$B
BRANCH=autoresearch/decode-tps-2026-09-18
[ -f "$GEN" ] || { echo "GENERATOR_MISSING $GEN"; exit 1; }
cd "$MAIN" || { echo "SYNC_FAIL main"; exit 1; }
git -C "$W" fetch -q "$MAIN" "$BRANCH" && git -C "$W" reset --hard -q FETCH_HEAD || { echo "SYNC_FAIL reset"; exit 1; }
rm -f "$W/esp32/sdkconfig"          # gitignored; regenerate from tracked defaults (#288/#300)
REL="$TARGET"; TARGET="$W/$REL"
BEFORE=$(md5sum < "$TARGET" | cut -c1-12)
echo "accepted_engine_md5=$BEFORE target=$REL"
python3 "$GEN" "$TARGET" || { echo "APPLY_FAILED"; exit 1; }
AFTER=$(md5sum < "$TARGET" | cut -c1-12)
echo "candidate_engine_md5=$AFTER target=$REL"
[ "$AFTER" != "$BEFORE" ] || { echo "CANDIDATE_IS_NOOP"; exit 1; }
cd "$W" || exit 1
bash .auto/measure.sh
