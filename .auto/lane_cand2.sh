#!/bin/bash
# Like lane_cand.sh but for a candidate that changes nd_quant.c (or any engine file) as well:
# it copies every file found in the candidate directory into the worker's matching engine path.
# Absolute paths throughout - the wrapper's cwd is not the checkout (#409).
set -uo pipefail
BOARD="$1"; SRC="$2"                       # SRC = candidate directory
W="/root/board-pool/board$BOARD"
[ -d "$SRC" ] || { echo "SRC_DIR_MISSING=$SRC"; exit 1; }
applied=0
for f in "$SRC"/*; do
    base=$(basename "$f")
    case "$base" in
        nd_model.c|nd_quant.c) cp "$f" "$W/engine/src/$base" || { echo "COPY_FAIL $base"; exit 1; } ;;
        *.h)                   cp "$f" "$W/engine/include/$base" || { echo "COPY_FAIL $base"; exit 1; } ;;
        main.c|kbench.c)       cp "$f" "$W/esp32/main/$base" || { echo "COPY_FAIL $base"; exit 1; } ;;
        *.S)                   cp "$f" "$W/engine/src/$base" || { echo "COPY_FAIL $base"; exit 1; } ;;
        *) echo "UNPLACED $base (refusing to guess a destination)"; exit 1 ;;
    esac
    applied=$((applied + 1))
done
[ "$applied" -gt 0 ] || { echo "NOTHING_TO_APPLY"; exit 1; }
rm -f "$W/.auto/diag_build_cfg" "$W/.auto/diag_groups" "$W/.auto/diag_case_gap_s" \
      "$W/.auto/diag_repeat_ok" "$W/.auto/diag_reattach_each" "$W/.auto/diag_request_timeout_s"
for t in "$SRC"/*.c "$SRC"/*.S; do
    [ -e "$t" ] || continue
    if ! cc -fsyntax-only -Werror=incompatible-pointer-types -Iengine/include -Iengine/src "$t" \
          > /tmp/typecheck2_${BOARD}.log 2>&1; then
        echo "TYPECHECK_FAILED board=$BOARD"; grep -a "error:" /tmp/typecheck2_${BOARD}.log | head -3; exit 1
    fi
done
echo "TYPECHECK_OK board=$BOARD files=$applied"
EXP=$( cd "$W" && { cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c; } | md5sum | cut -c1-12 )
printf '%s\n' "$EXP" > "$W/.auto/expect_engine_md5"
echo "APPLIED board=$BOARD expect=$EXP"
cd "$W" && exec bash .auto/measure.sh
