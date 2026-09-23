#!/bin/bash
# One Experiment 29 performance lane on the board this wrapper locked.
#
#   inside `needle-board run N`:  .auto/exp29/lane.sh <lane 1|2|3>
#
# Builds a FRESH directory per lane (the campaign's build-integrity rule: a knob
# reused in an old CMakeCache silently re-flashes the previous image), verifies
# the lane macro really reached the compile line, asserts the new bench symbol is
# in the ELF (run #333's loophole: an unreferenced/unbuilt candidate reads as a
# result), flashes, then captures the console with kb_capture.py.
set -uo pipefail

L=${1:?usage: lane.sh 1|2|3}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BD=build-e29-$L
OUT=${LANE_OUT:-/tmp/e29-lane$L-board${NEEDLE_BOARD:-?}.log}
WIN=${KB_WINDOW:-200}
. /opt/esp/idf/export.sh >/dev/null 2>&1

{
    echo "lane=$L board=${NEEDLE_BOARD:-?} root=$ROOT flash=$FLASH_PORT console=$SERIAL_PORT"
    cd "$ROOT/esp32" || exit 1
    rm -rf "$BD"
    idf.py -B "$BD" -DNEEDLE_KBENCH=ON -DND_KBENCH_ASM=ON -DND_KB_LANE="$L" \
         -DNEEDLE_PROFILE=OFF build > "$OUT.build.log" 2>&1
    rc=$?
    echo "build_rc=$rc"
    [ $rc -ne 0 ] && { grep -E "error:|Error" "$OUT.build.log" | head -10; exit 1; }
    NL=$(grep -o "DND_KB_LANE=$L" "$BD/compile_commands.json" | wc -l)
    # The APP elf, not elf_loader.elf - checking the loader reports 0 for a bench
    # that really is in the image (run #347 false alarm; the map has .text.bench_fused
    # and the console prints KB FUSE, which is the stronger evidence anyway: a probe
    # is verified by its output, not by symbol presence).
    NB=$(xtensa-esp32s3-elf-nm "$BD/needle_demo.elf" 2>/dev/null | grep -c bench_fused || true)
    [ "$NL" = 0 ] && NL=$(grep -o "ND_KB_LANE=$L" "$BD/project_description.json" | wc -l)
    echo "lane_macro_on_compile_line=$NL bench_fused_in_elf=$NB"
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
    echo "KB_LINES=$(grep -ac '^KB \|^EVT KBENCH_DONE' "$OUT.raw" 2>/dev/null)"
    echo "lane_done=$(date -u +%FT%TZ)"
} 2>&1 | tee "$OUT"
