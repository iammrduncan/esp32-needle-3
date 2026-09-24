#!/bin/bash
# Launch a nd_model.c-only candidate on one board: apply by ABSOLUTE path, pin provenance from
# the applied tree, clear leftover diagnostic switches, then run the repo's own measure.sh.
#
# Why a file: three lanes died today from nested quotes inside `needle-board run N -- bash -c`.
# Why absolute paths: the wrapper's cwd is not the checkout, so a relative `cp` patches some
# other tree while a relative `md5sum` agrees with itself (#409).
# Why the expect value is computed AFTER the copy: hand-assembling the hash has twice produced a
# wrong expectation that would have aborted a correct tree (a missed header, a stale file).
set -uo pipefail
BOARD="$1"; SRC="$2"   # output is redirected by the caller
W="/root/board-pool/board$BOARD"
[ -f "$SRC" ] || { echo "SRC_MISSING=$SRC"; exit 1; }
cp "$SRC" "$W/engine/src/nd_model.c" || { echo "COPY_FAIL"; exit 1; }
rm -f "$W/.auto/diag_build_cfg" "$W/.auto/diag_groups"          # never measure a diagnostic build
rm -f "$W/.auto/diag_case_gap_s" "$W/.auto/diag_repeat_ok" "$W/.auto/diag_reattach_each"
git -C /workspace/esp32-needle-3 show a6e7454:esp32/main/kbench.c > "$W/esp32/main/kbench.c"  # bench edits must not ride along
EXP=$( cd "$W" && { cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c; } | md5sum | cut -c1-12 )
printf '%s\n' "$EXP" > "$W/.auto/expect_engine_md5"
# A host build treats an incompatible pointer initialisation as a WARNING and keeps going; the
# xtensa build refuses it. That difference cost a board slot on run #414's condv16 lane, so the
# type error is now refused here, before any flash, with the host compiler alone.
if ! cc -fsyntax-only -Werror=incompatible-pointer-types -Iengine/include -Iengine/src "$SRC" \
      > /tmp/typecheck_${BOARD}.log 2>&1; then
    echo "TYPECHECK_FAILED board=$BOARD (see /tmp/typecheck_${BOARD}.log)"
    grep -a "error:" /tmp/typecheck_${BOARD}.log | head -3
    exit 1
fi
echo "TYPECHECK_OK board=$BOARD"
echo "APPLIED board=$BOARD expect=$EXP src_md5=$(md5sum "$SRC" | cut -c1-12)"
cd "$W" && exec bash .auto/measure.sh
