#!/usr/bin/env bash
# Boot-only harvest of the corrected split screen on board 1.
# Board 1's USB console leg is dead (measured twice in #440: no EVT READY after a
# successful flash), but the Espressif USB-JTAG/serial leg on $FLASH_PORT does show
# boot output, so this captures the screen's lines there and never touches the suite.
set -uo pipefail
cd /root/board-pool/board1
cp /workspace/esp32-needle-3/.auto/exp79/main_screen2.c esp32/main/main.c || exit 9
md5sum esp32/main/main.c | cut -c1-12
. /opt/esp/idf/export.sh >/dev/null 2>&1
idf.py -B esp32/build build > /tmp/b1_screen_build.log 2>&1 || { echo BUILD_FAILED; tail -20 /tmp/b1_screen_build.log; exit 8; }
echo "FLASH_PORT=$FLASH_PORT"
python3 - "$FLASH_PORT" <<'PY'
import sys, time, serial, esptool
port = sys.argv[1]
def rd(sec):
    out = b''
    with serial.Serial(port, 115200, timeout=0.2) as s:
        s.dtr = True; s.rts = False
        t0 = time.time()
        while time.time() - t0 < sec:
            b = s.read(4096)
            if b: out += b
    return out
print('cap_pre_bytes=%d' % len(rd(2)))
esptool.main(['--chip','esp32s3','-p',port,'--before','default','--after','hard-reset','run'])
time.sleep(2)
buf = rd(120)
sys.stdout.write('cap_bytes=%d\n' % len(buf))
for ln in buf.decode('utf-8','replace').splitlines():
    if ln.startswith('SPLIT') or 'EVT ready' in ln or 'panic' in ln.lower():
        print(ln)
PY
echo SCREEN_DONE
