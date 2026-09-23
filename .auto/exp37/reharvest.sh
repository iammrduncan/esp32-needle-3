#!/usr/bin/env bash
# Re-read a kbench image that is ALREADY flashed (reset only, no rebuild, no flash):
# the bench prints its whole report within seconds of boot, so a lane that lost its
# output to a too-narrow harvest filter can be recovered for the price of a reset.
set -uo pipefail
B="${NEEDLE_BOARD:?}"; EXP="${KB_EXP:?}"
FP="${FLASH_PORT}"; SP="${SERIAL_PORT}"
cd "/root/board-pool/board$B" || exit 1
exec > >(tee "/root/board-pool/batches/reharvest-e$EXP-b$B-$(date -u +%Y%m%dT%H%M%SZ).log") 2>&1
echo "reharvest board=$B exp=$EXP start=$(date -u +%FT%TZ)"
.venv/bin/python - "$FP" "$SP" "$EXP" <<'PY'
import subprocess, sys, time, serial
fp, sp, exp = sys.argv[1], sys.argv[2], sys.argv[3]
s = serial.Serial(); s.port = sp; s.baudrate = 115200; s.timeout = 0.2
s.dsrdtr = False; s.rtscts = False; s.dtr = True; s.rts = False   # RTS is reset; DTR feeds CDC
s.open(); time.sleep(0.4); s.reset_input_buffer()
subprocess.run(["python3", "-m", "esptool", "--chip", "esp32s3", "-p", fp, "run"],
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
buf, t0 = b"", time.time()
while time.time() - t0 < 90:
    buf += s.read(4096)
    if b"KBENCH_DONE" in buf: break
s.close()
tag = "KB E%s" % exp
lines = [l for l in buf.decode(errors="replace").splitlines()
         if tag in l or "KBENCH" in l or "panic" in l.lower()]
print("harvest_lines=%d" % len(lines))
for l in lines[:20]: print(l[:230])
PY
echo "REHARVEST_DONE end=$(date -u +%FT%TZ)"
