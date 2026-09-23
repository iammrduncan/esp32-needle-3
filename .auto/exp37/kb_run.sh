#!/usr/bin/env bash
# Experiment 37 kbench lane: build a NEEDLE_KBENCH image with ONLY the E37 body,
# flash it on the board this script is running under (the caller must be inside
# `needle-board run N --`, which is what sets FLASH_PORT/SERIAL_PORT and holds the
# board lock), then harvest the KB lines.
#
# Two rules the campaign already paid for are encoded here:
#  * a macro-gated candidate must be proven present on the compile line (#359 - two
#    full batches were worthless because a -D flag never reached the compiler);
#  * the console must be open with DTR asserted BEFORE the reset, and the reset must
#    be done with the console open by esptool `run`, or a warm board prints nothing
#    at all (#338's harvest note).
set -uo pipefail

MAIN=/workspace/esp32-needle-3
B="${NEEDLE_BOARD:-3}"
FP="${FLASH_PORT:-/dev/needle-pi/board$B-flash}"
SP="${SERIAL_PORT:-/dev/needle-pi/board$B-console}"
EXP="${KB_EXP:-37}"
LOG=/root/board-pool/batches/e${KB_EXP:-37}-b${NEEDLE_BOARD:-3}-$(date -u +%Y%m%dT%H%M%SZ).log
exec > >(tee "$LOG") 2>&1
echo "start=$(date -u +%FT%TZ) flash=$FP serial=$SP"

# NOT dirname-based: this script lives in .auto/exp37/, so "../" would be
# .auto. The worker root is the board checkout the caller locked.
cd "/root/board-pool/board${NEEDLE_BOARD:-3}" || exit 1
# The bench image must be the ACCEPTED engine plus the bench, not whatever candidate
# this worker last ran.
cp "$MAIN/engine/src/nd_quant.c" engine/src/nd_quant.c
cp "$MAIN/engine/src/nd_model.c"  engine/src/nd_model.c
cp "$MAIN/esp32/main/kbench.c"    esp32/main/kbench.c
echo "engine md5: $(md5sum < engine/src/nd_quant.c | cut -c1-12) $(md5sum < engine/src/nd_model.c | cut -c1-12)"

. /opt/esp/idf/export.sh >/dev/null 2>&1
cd esp32 || exit 1
rm -rf build-e$EXP
idf.py -B "build-e$EXP" -DNEEDLE_KBENCH=ON "-DND_KB_EXP$EXP=1" build > /tmp/kb-e$EXP-b$B-build.log 2>&1
echo "build_rc=$? $(tail -2 /tmp/kb-e$EXP-b$B-build.log | tr '\n' ' ' | cut -c1-110)"
grep -o "ND_KB_EXP$EXP=[0-9]" "build-e$EXP/compile_commands.json" | head -1 | sed 's/^/macro_on_compile_line=/'
grep -c "ND_KBENCH=1\|NEEDLE_KBENCH=ON" build-e$EXP/compile_commands.json | sed 's/^/kbench_flag_lines=/'
python3 - <<PY
import re
d = open('build-e$EXP/compile_commands.json').read()
print("exp37_value=", re.findall(r'-DND_KB_EXP37=(\S+)', d)[:1])
PY
idf.py -B build-e$EXP -p "$FP" flash > /tmp/kb-e$EXP-b$B-flash.log 2>&1
echo "flash_rc=$?"

cd ..
python3 - "$FP" "$SP" "$EXP" <<'PY'
import subprocess, sys, time, serial
fp, sp, exp = sys.argv[1], sys.argv[2], sys.argv[3]
s = serial.Serial(sp, 115200, timeout=0.2)
s.dtr = True                      # the USB CDC bridge needs DTR or a warm board is silent
time.sleep(0.5)
s.reset_input_buffer()
subprocess.run(["python3", "-m", "esptool", "--chip", "esp32s3", "-p", fp, "run"],
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
buf, t0, done = b"", time.time(), False
while time.time() - t0 < 90:
    buf += s.read(4096)
    if b"KBENCH_DONE" in buf or (b"KB E" + exp.encode() in buf and time.time() - t0 > 25):
        done = True
        break
s.close()
lines = [l for l in buf.decode(errors="replace").splitlines()
         if ("KB E" + exp) in l or "KBENCH" in l or "panic" in l.lower() or "abort" in l.lower()]
print("harvest_lines=%d done=%s" % (len(lines), done))
for l in lines[:24]:
    print(l[:220])
PY
echo "E37_DONE end=$(date -u +%FT%TZ)"
