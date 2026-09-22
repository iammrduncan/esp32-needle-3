#!/usr/bin/env python3
"""Experiment 21 heat curve: reset, let it warm, then load and log temperature.

  /root/bin/needle-board run N -- env EXP21_BOARD=N EXP21_TAG=boardN \
      .venv/bin/python .auto/exp21/heat.py 12

Order matters, and both orders were learned by failing:
  * holding the console open while esptool resets the chip leaves the strap
    sampled wrong - the chip comes back in `boot:0x0 DOWNLOAD(USB/UART0)` and
    sits there forever, so the console is reset with the port CLOSED;
  * a board that booted minutes earlier delivers NOTHING to a console attached
    afterwards unless DTR is asserted, so the read side asserts DTR.

Between the two, the model caches warm (the firmware primes for minutes after a
reset), which is also the cold-boot-then-warm case the octal-120 MHz caveat is
about, so the wait is part of the experiment rather than overhead.
"""
import os
import subprocess
import sys
import time

import serial

MINUTES = float(sys.argv[1]) if len(sys.argv) > 1 else 12.0
PROMPTS = ["List tools", "What is the heap right now", "sampling rate please",
           "!route Translate good morning into German"]

PORT = os.environ.get("SERIAL_PORT", "")
FLASH = os.environ.get("FLASH_PORT", "")
B = os.environ.get("EXP21_BOARD", "")
_WC = {1: "/dev/needle-pi/console"}.get(int(B or 0), "/dev/needle-pi/board%s-console" % B)
_WF = {1: "/dev/needle-pi/flash"}.get(int(B or 0), "/dev/needle-pi/board%s-flash" % B)
if PORT != _WC or FLASH != _WF:
    raise SystemExit("port mismatch: board %r got %r %r (want %r %r)" % (B, PORT, FLASH, _WC, _WF))

LOG = open("/tmp/exp21-heat-%s.log" % os.environ.get("EXP21_TAG", "x"), "w")


def note(s):
    print(s, flush=True)
    LOG.write(s + "\n")
    LOG.flush()


subprocess.run(["python3", "-m", "esptool", "--chip", "esp32s3", "-p", FLASH,
                "--after", "hard-reset", "chip_id"],
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
note("RESET_DONE t=0; waiting for the caches to prime")
time.sleep(360)

ser = serial.Serial()
ser.port = PORT
ser.baudrate = 115200
ser.timeout = 1
ser.exclusive = True
ser.dtr = False
ser.rts = False
ser.open()
ser.dtr = True
time.sleep(0.5)

t0 = time.time()
end = t0 + MINUTES * 60
i = done = faults = 0
note("ATTACH; soaking requests for %.0f min" % MINUTES)
while time.time() < end:
    p = PROMPTS[i % len(PROMPTS)]
    i += 1
    ser.write((p + "\n").encode())
    ser.flush()
    deadline = time.time() + 600
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        txt = raw.decode(errors="replace").rstrip()
        if not txt:
            continue
        LOG.write(txt + "\n")
        if txt.startswith("TDIAG"):
            note("%.0fs %s" % (time.time() - t0, txt))
        elif txt.startswith("EVT done"):
            done += 1
            LOG.flush()
            break
        elif ("Guru" in txt or txt.startswith("EVT ERR") or "abort" in txt.lower()):
            note("%.0fs FAULT %s" % (time.time() - t0, txt))
            faults += 1
note("HEAT_DONE requests=%d completed=%d faults=%d span=%.0fs" %
     (i, done, faults, time.time() - t0))
LOG.close()
