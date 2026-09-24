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
# Diagnostic knobs are tracked files, so a reset restores them into the worker. Their mere
# presence changes harness behaviour (repeat override, per-request timeout), which contaminates a
# clean measurement; every lane that printed a trustworthy number deleted them first (#423).
rm -f "$W/.auto/diag_repeat_ok" "$W/.auto/diag_request_timeout_s" "$W/.auto/diag_case_gap_s" \
      "$W/.auto/diag_groups" "$W/.auto/diag_reattach_each" "$W/.auto/diag_build_cfg"
REL="$TARGET"; TARGET="$W/$REL"
# Assert the BASE, not just the change: after the #421 base error a worker HEAD could carry a
# gate-blocked candidate, so "accepted + candidate" was really "fusion + candidate".
TREE_BEFORE=$( cd "$W" && { cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c; } | md5sum | cut -c1-12 )
ACCEPTED_ENGINE_MD5="${ACCEPTED_ENGINE_MD5:-cc046f59204e}"
if [ "$TREE_BEFORE" != "$ACCEPTED_ENGINE_MD5" ]; then
    echo "BASE_NOT_ACCEPTED tree_md5=$TREE_BEFORE expected=$ACCEPTED_ENGINE_MD5"; exit 6
fi
echo "BASE_ACCEPTED tree_md5=$TREE_BEFORE"
BEFORE=$(md5sum < "$TARGET" | cut -c1-12)
echo "accepted_engine_md5=$BEFORE target=$REL"
python3 "$GEN" "$TARGET" || { echo "APPLY_FAILED"; exit 1; }
AFTER=$(md5sum < "$TARGET" | cut -c1-12)
echo "candidate_engine_md5=$AFTER target=$REL"
[ "$AFTER" != "$BEFORE" ] || { echo "CANDIDATE_IS_NOOP"; exit 1; }
TREE_AFTER=$( cd "$W" && { cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c; } | md5sum | cut -c1-12 )
[ "$TREE_AFTER" != "$TREE_BEFORE" ] || { echo "CANDIDATE_NOT_IN_TREE"; exit 1; }
printf '%s\n' "$TREE_AFTER" > "$W/.auto/expect_engine_md5"   # now machine-enforced by measure.sh
echo "APPLIED board=$B target=$REL expect=$TREE_AFTER"
cd "$W" || exit 1
bash .auto/measure.sh
