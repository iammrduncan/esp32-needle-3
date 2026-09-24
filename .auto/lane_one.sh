#!/bin/bash
# Apply exactly ONE file to a worker, verify it, and launch measure.sh from that tree.
#   $1 = source file (absolute)  $2 = repo-relative target  $3 = board number
# Deliberately tiny: the multi-file variant's argument convention cost a lane when a single file
# was passed, and a lane that silently measures the wrong tree is the most expensive bug here.
set -uo pipefail
SRC="$1"; TGT="$2"; B="${3:-2}"
[ -f "$SRC" ] || { echo "NO_SOURCE $SRC"; exit 1; }
W="/root/board-pool/board$B"   # the wrapper keeps the checkout at this path; .board.json has no checkout key
MAIN=/workspace/esp32-needle-3
case "$TGT" in
    engine/*) INC="$MAIN/engine/include" ;;
    esp32/main/*) INC="$MAIN/engine/include $MAIN/engine/src" ;;
    *) echo "BAD_TARGET $TGT"; exit 1 ;;
esac
git -C "$W" checkout -- engine esp32/main || { echo "SYNC_FAIL"; exit 1; }
[ -L "$W/.venv" ] || ln -s /workspace/esp32-needle-3/.venv "$W/.venv"
cp -f "$SRC" "$W/$TGT" || { echo "COPY_FAIL $TGT"; exit 1; }
# Run the probe unconditionally. The earlier form gated it on a marker file that does not exist,
# so the && chain short-circuited and reported TYPECHECK_FAIL without ever invoking the compiler -
# a lane-killing false negative. Absent cc is now an explicit skip, never a failure and never a
# silent pass.
if ! command -v cc >/dev/null 2>&1; then
    echo TYPECHECK_SKIPPED_no_cc
elif cc -fsyntax-only -Werror=incompatible-pointer-types -I$INC "$W/$TGT" >/tmp/tc1_$B.log 2>&1; then
    echo TYPECHECK_OK
else
    echo TYPECHECK_FAIL; head -8 "/tmp/tc1_$B.log"; exit 5
fi
git -C "$MAIN" show a6e7454:esp32/main/kbench.c > "$W/esp32/main/kbench.c"
rm -f "$W/.auto/diag_build_cfg" "$W/.auto/diag_groups" "$W/.auto/diag_case_gap_s" "$W/.auto/diag_repeat_ok" "$W/.auto/diag_reattach_each" "$W/.auto/diag_request_timeout_s"
EXP=$( cd "$W" && { cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c; } | md5sum | cut -c1-12 )
printf '%s\n' "$EXP" > "$W/.auto/expect_engine_md5"
echo "APPLIED board=$B target=$TGT src_md5=$(md5sum "$SRC" | cut -c1-12) tree_md5=$(md5sum "$W/$TGT" | cut -c1-12) expect=$EXP"
cd "$W" && exec bash .auto/measure.sh
