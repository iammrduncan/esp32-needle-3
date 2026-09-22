#!/usr/bin/env python3
"""Capture one cold boot of a diag image: ECC/tier/READY evidence + first TDIAGs.

  /root/bin/needle-board run N -- .venv/bin/python .auto/exp21/boot_capture.py 330

DTR is asserted BEFORE the reset because the ESP32-S3 USB-Serial-JTAG CDC bridge
drops output while DTR is low (measured: an idle console stream stalls at 4 KiB
with DTR deasserted), so every byte the boot actually emits is visible. The
reset itself goes through esptool on the *flash* port, which is a separate
device node and can be open at the same time.
"""
import subprocess
import os
import sys
import time

import serial

SECONDS = float(sys.argv[1]) if len(sys.argv) > 1 else 330.0
OUT = "/tmp/exp21-boot-%s.log" % os.environ.get("EXP21_TAG", "x")

# The pool gives each board its OWN nodes through the wrapper's FLASH_PORT and
# SERIAL_PORT. /dev/ttyACM0//dev/ttyACM1 are the old single-board aliases and
# point at ONE physical board: measured the hard way, three concurrent "board"
# runs all contended for the same chip and two reported the port busy. Refuse
# to run outside the wrapper rather than silently driving the wrong board.
CONSOLE = os.environ.get("SERIAL_PORT", "")
FLASH = os.environ.get("FLASH_PORT", "")
_B = os.environ.get("EXP21_BOARD", "")
# Board 1 keeps the legacy /dev/needle-pi/{flash,console} names; boards 2 and 3
# get boardN- prefixed ones. Check the node belongs to the board we were asked
# to drive, not merely to some board.
_WANT = {1: ("/dev/needle-pi/flash", "/dev/needle-pi/console")}.get(
    int(_B or 0), ("/dev/needle-pi/board%s-flash" % _B,
                   "/dev/needle-pi/board%s-console" % _B))
if (FLASH, CONSOLE) != _WANT:
    raise SystemExit("port mismatch: asked for board %r, wrapper gave %r %r "
                     "(expected %r)" % (_B, FLASH, CONSOLE, _WANT))

ser = serial.Serial(CONSOLE, 115200, timeout=1)
ser.dtr = True
ser.rts = False
time.sleep(0.5)
subprocess.run(["python3", "-m", "esptool", "--chip", "esp32s3",
                "-p", FLASH, "run"],
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
end = time.time() + SECONDS
ready = False
with open(OUT, "w") as f:
    while time.time() < end:
        raw = ser.readline()
        if not raw:
            continue
        txt = raw.decode(errors="replace").rstrip()
        if not txt:
            continue
        f.write(txt + "\n")
        f.flush()
        if txt.startswith(("I (", "G (", "ets ", "abort", "Guru_md",
                           "TDIAG", "THERDIAG")) or "PSRAM" in txt or \
                "SPIRAM" in txt or "ECC" in txt or txt.startswith("EVT") or \
                "tier" in txt.lower() or "panic" in txt.lower():
            print(txt, flush=True)
        if txt.startswith("EVT READY"):
            ready = True
        if ready and time.time() - (end - SECONDS) > 120:
            break
print("BOOT_CAPTURE_DONE ready=%s bytes_logged=%s" %
      (ready, sum(1 for _ in open(OUT))), flush=True)
