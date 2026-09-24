#!/bin/bash
# Build + flash + capture the exp52 f16-bitcast screen on the board this wrapper locked.
# Same shape as .auto/exp29/lane.sh (fresh build dir, verify the image, flash over
# $FLASH_PORT, capture the console), minus the lane macros: this screen is a self-contained
# kbench body that ends the capture with its own KBENCH_DONE.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BD=build-e52
OUT=${LANE_OUT:-/tmp/e52-board${NEEDLE_BOARD:-?}.log}
WIN=${KB_WINDOW:-420}
. /opt/esp/idf/export.sh >/dev/null 2>&1

{
    echo "lane=exp52 board=${NEEDLE_BOARD:-?} root=$ROOT flash=$FLASH_PORT console=$SERIAL_PORT start=$(date -u +%FT%TZ)"
    grep -c "kb_f16_bc" "$ROOT/esp32/main/kbench.c" | sed 's/^/screen_markers=/'
    cd "$ROOT/esp32" || exit 1
    rm -rf "$BD"
    idf.py -B "$BD" -DNEEDLE_KBENCH=ON -DNEEDLE_KBENCH_ASM=OFF -DNEEDLE_PROFILE=OFF build \
        > "$OUT.build.log" 2>&1
    rc=$?
    echo "build_rc=$rc"
    [ $rc -ne 0 ] && { grep -E "error:|Error" "$OUT.build.log" | head -15; exit 1; }
    NL=$(grep -o "DND_KBENCH=1" "$BD/compile_commands.json" | wc -l)
    echo "kbench_on_compile_line=$NL"
    [ "$NL" -lt 1 ] && { echo "KBENCH_NOT_ON_COMPILE_LINE"; exit 1; }
    # Mechanism proof, host side: the shipped sweep must contain the ROM memcpy call and the
    # builtin sweep must not. If the compiler already folded both, the screen is measuring
    # nothing and must say so instead of printing a null delta.
    OBJ=$(grep -o '[^ "]*kbench.c.obj' "$BD/compile_commands.json" | head -1)
    xtensa-esp32s3-elf-objdump -d "$BD/esp32/CMakeFiles/__idf_esp32.dir/main/kbench.c.obj" \
        > "$OUT.objdump" 2>&1 || echo "objdump_rc=$?"
    echo "memcpy_calls_total=$(grep -ac '<memcpy>' "$OUT.objdump" 2>/dev/null)"
    img="$BD/needle_demo.bin"
    ls -l "$img" | awk '{print "bin_bytes="$5}'
    md5sum "$img"
    python3 -m esptool --chip esp32s3 --port "$FLASH_PORT" --baud 921600 \
        write_flash 0x10000 "$img" > "$OUT.flash.log" 2>&1
    echo "flash_rc=$?"
    cd "$ROOT"
    timeout --foreground -k 10 "$WIN" .venv/bin/python .auto/kb_capture.py \
        "$SERIAL_PORT" "$WIN" "$OUT.raw"
    echo "capture_rc=$?"
    grep -a "^KB F16BC\|^EVT KBENCH_DONE\|^KB CFG blob" "$OUT.raw" 2>/dev/null
    echo "lane_done=$(date -u +%FT%TZ)"
} 2>&1 | tee "$OUT"
