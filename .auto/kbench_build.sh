#!/bin/bash
# Build one Experiment 2 kernel-bench image variant into a *fresh* build dir.
#
#   .auto/kbench_build.sh <ctrl|asm>
#
#   ctrl - the microbenchmark with the C row walker only  (the control image)
#   asm  - the microbenchmark with the C walker plus nd_lut2_rows_tie1/tie2
#
# Every compile-time knob in this project needs its own build directory: the
# flag is already in CMakeCache from an earlier configure otherwise, ninja sees
# no change, and the "candidate" that gets flashed is the control wearing a
# different hat. Verifies the flag really reached the compile line and prints
# the image hash so two variants can be told apart.
set -euo pipefail

V=${1:?usage: kbench_build.sh ctrl|asm}
case "$V" in
    ctrl) ASM=OFF ;;
    asm)  ASM=ON  ;;
    *)    echo "variant must be ctrl|asm" >&2; exit 2 ;;
esac

BD=build-kb-$V
LOG=/tmp/kb-$V-${NEEDLE_BOARD:-local}.log
. /opt/esp/idf/export.sh >/dev/null 2>&1

cd "$(dirname "$0")/.."           # board checkout root
cd esp32
rm -rf "$BD"
idf.py -B "$BD" \
     -DNEEDLE_KBENCH=ON -DNEEDLE_KBENCH_ASM="$ASM" -DNEEDLE_PROFILE=OFF \
     build > "$LOG" 2>&1 || { tail -40 "$LOG"; exit 1; }

NCC=$(grep -o 'DND_KBENCH=1' "$BD/compile_commands.json" | wc -l)
NSRC=$(grep -o 'lut2_tie728\.S' "$BD/compile_commands.json" | wc -l)
BIN="$BD/needle_demo.bin"

echo "variant=$V build_dir=$BD log=$LOG"
echo "kbench_compile_lines=$NCC asm_sources=$NSRC"
if [ "$NCC" -lt 1 ]; then echo "FAIL: ND_KBENCH never reached the compile line"; exit 1; fi
if [ "$ASM" = ON ] && [ "$NSRC" -lt 1 ]; then echo "FAIL: lut2_tie728.S not compiled"; exit 1; fi
if [ "$ASM" = OFF ] && [ "$NSRC" -ne 0 ]; then echo "FAIL: asm compiled into the control"; exit 1; fi

xtensa-esp32s3-elf-nm "$BD/needle_demo.elf" | grep -E 'nd_lut2_rows_(c|tie1|tie2)|nd_lut2_probe_add_thru' | sort
md5sum "$BIN"
ls -l "$BIN"
