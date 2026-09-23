#!/bin/bash
# Experiment 38 lane: build + flash + harvest the cold/warm x backing-store
# calibration of the shipped nd_fwht, on one board, under that board's lock.
#
# This is a kbench image, so it never runs the request path and produces no
# decode_tps - by design. The number it produces is a calibration factor for
# every isolated screen this campaign has ever run, which is why it is worth a
# board even though it cannot be kept or rejected as a speed candidate.
set -uo pipefail

R=${REPO:-$PWD}
BD=build-e38
BATCH=${BATCH:-/tmp}
LOGF=$BATCH/e38.${NEEDLE_BOARD:-local}.txt
. /opt/esp/idf/export.sh > /dev/null 2>&1

# idf.py wants the project directory, not the repo root - running it from the
# root fails with "CMakeLists.txt not found" (the same trap that cost the first
# make capture attempt). All paths below are relative to esp32/.
# The bench body and the CMake forwarding list live in the MAIN checkout, so this
# lane syncs both itself. The first two refusals were this script's fault, not the
# candidate's: the guard correctly found no E38 body in images built from workers
# whose kbench.c/CMakeLists had never been updated. A lane that depends on files
# in another tree has to prove it copied them, or it measures the old code.
MAIN=/workspace/esp32-needle-3
cp "$MAIN/esp32/main/kbench.c" "$R/esp32/main/kbench.c" || exit 1
cp "$MAIN/esp32/main/CMakeLists.txt" "$R/esp32/main/CMakeLists.txt" || exit 1
echo "[e38] synced kbench_md5=$(md5sum < "$R/esp32/main/kbench.c" | cut -c1-12) cmake_md5=$(md5sum < "$R/esp32/main/CMakeLists.txt" | cut -c1-12)"
cd "$R/esp32" || exit 1
echo "[e38] board=${NEEDLE_BOARD:-?} repo=$R start=$(date -u +%FT%TZ)"

# The candidate is a bench body, so it must be visible on the compile line:
# a macro that never reaches the compiler produces no symbol and no link error
# and the run silently measures the old default body (run #359, cost two full
# three-board batches).
rm -rf "$BD"
idf.py -B "$BD" -DNEEDLE_KBENCH=ON -DND_KB_EXP38=1 build > "$BATCH/e38.build.${NEEDLE_BOARD:-local}.log" 2>&1
rc=$?
echo "[e38] build_rc=$rc"
[ $rc -ne 0 ] && { tail -20 "$BATCH/e38.build.${NEEDLE_BOARD:-local}.log"; exit 1; }
python3 -c "
import json
db = json.load(open('$BD/compile_commands.json'))
hit = [e for e in db if e['file'].endswith('main/kbench.c')]
print('kbench_own_compile_lines=%d with_flag=%d' % (
    len(hit), sum(1 for e in hit if '-DND_KB_EXP38=1' in e.get('command',''))))
" | sed 's/^/[e38] exp38_on_compile_line=/'
# The ELF lives under <build>/esp32/ when idf.py is invoked from the project
# directory. Probing the wrong path makes the guard fail CLOSED (it refuses a
# good image), which is the safe direction, but it still costs a board slot -
# print what was actually looked at so the next failure is one step to find.
ELF=$(ls "$BD"/esp32/*.elf "$BD"/*.elf 2>/dev/null | head -1)
echo "[e38] elf=$ELF"
# bench_e38 is static with exactly one call site, so -O2 INLINES it and nm has no
# symbol to find - the first guard therefore refused a perfectly good image. The
# right proof is the string only the E38 body prints, which lands in .rodata, and
# for a macro candidate that is the whole point of the rule (run #359): prove the
# body is in the image, do not assume it from a green build.
nm=$(strings "$ELF" 2>/dev/null | grep -c 'KB E38 ENTER')
echo "[e38] e38_body_string_in_elf=$nm"
[ "${nm:-0}" -ge 1 ] || { echo "[e38] REFUSING to flash: no E38 body string in $ELF (macro did not reach kbench.c?)"; exit 1; }

# idf.py has no --before/--after (those are esptool flags); it manages reset itself.
idf.py -B "$BD" -p "${FLASH_PORT:-/dev/ttyACM0}" flash \
  > "$BATCH/e38.flash.${NEEDLE_BOARD:-local}.log" 2>&1
echo "[e38] flash_rc=$?"

# kbench prints KBENCH_DONE within seconds of boot, before the model caches warm,
# so the harvest is: console closed, reset the chip, then read. Holding the
# console open across the reset leaves the strap sampled wrong (#331).
python3 - "$SERIAL_PORT" "$FLASH_PORT" "$LOGF" <<'PY'
import sys, time, subprocess
console, flash, out = sys.argv[1], sys.argv[2], sys.argv[3]
subprocess.run(["python3", "-m", "esptool", "--chip", "esp32s3", "-p", flash, "run"],
               capture_output=True, timeout=90)
import serial
s = serial.Serial(console, 115200, timeout=0.5)
s.dtr = True
buf = b""
t0 = time.time()
while time.time() - t0 < 150:
    d = s.read(4096)
    if d:
        buf += d
        if b"KBENCH_DONE" in buf:
            break
s.close()
open(out, "wb").write(buf)
lines = [l for l in buf.decode("utf8", "replace").splitlines() if "KB E38" in l]
print("\n".join(lines) if lines else "[e38] NO KB E38 OUTPUT (%d bytes captured)" % len(buf))
done = b"KBENCH_DONE" in buf
print("[e38] kbench_done=%s bytes=%d" % (done, len(buf)))
PY
echo "[e38] done=$(date -u +%FT%TZ)"
