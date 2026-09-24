#!/usr/bin/env bash
# Boot-only harvest of the corrected split screen on board 1 (run #449).
# Board 1's USB console leg is dead (measured twice in #440), but the Espressif
# USB-JTAG/serial leg on $FLASH_PORT does show boot output, so the screen's lines
# are captured there. The screen runs right after worker_start(), i.e. before
# model open and prefix priming, so a boot-only window is enough.
# Three fixes over the first version, each of which invalidated a run:
#   idf.py needs the project directory (-C esp32) - from the repo root it fails;
#   esptool `run` only RESETS the installed image, so the new firmware has to be
#     flashed explicitly or the capture describes the OLD boot log;
#   cycles -> us is /240.0 at 240 MHz, not /240000.
set -uo pipefail
T=/root/board-pool/board1
cd "$T" || exit 9
cp /workspace/esp32-needle-3/.auto/exp79/main_screen2.c esp32/main/main.c || exit 9
echo "main_md5=$(md5sum esp32/main/main.c | cut -c1-12) engine_md5=$(cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c 2>/dev/null | md5sum | cut -c1-12)"
. /opt/esp/idf/export.sh >/dev/null 2>&1
idf.py -C esp32 -B esp32/build build > /tmp/b1_screen_build.log 2>&1 || { echo BUILD_FAILED; tail -15 /tmp/b1_screen_build.log; exit 8; }
echo "bin_md5=$(md5sum esp32/build/needle_demo.bin 2>/dev/null | cut -c1-12)"
echo "FLASH_PORT=$FLASH_PORT"
idf.py -C esp32 -B esp32/build -p "$FLASH_PORT" flash > /tmp/b1_screen_flash.log 2>&1 || { echo FLASH_FAILED; tail -15 /tmp/b1_screen_flash.log; exit 7; }
echo "flash_rc=0"
python3 - "$FLASH_PORT" <<'PY'
import sys, time, serial
port = sys.argv[1]
buf = b''
with serial.Serial(port, 115200, timeout=0.2) as s:
    s.dtr = True; s.rts = False
    time.sleep(1); s.reset_input_buffer()
    t0 = time.time()
    while time.time() - t0 < 150:
        b = s.read(4096)
        if b: buf += b
lines = buf.decode('utf-8', 'replace').splitlines()
print('cap_bytes=%d lines=%d' % (len(buf), len(lines)))
hits = [l for l in lines if l.startswith('SPLIT') or 'EVT ready' in l or 'panic' in l.lower()]
for l in hits: print(l)
print('SPLIT_LINES=%d' % len([l for l in hits if l.startswith('SPLIT')]))
PY
echo SCREEN_DONE
