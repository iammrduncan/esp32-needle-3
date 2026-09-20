#!/usr/bin/env python3
"""Capture one ESP32 console for a bounded time.

  kb_capture.py <port> <seconds> <outfile>

Opens the UART the way the firmware expects (DTR/RTS set before opening resets
the board, which is what starts a kernel-bench run) and copies everything it
sees. Stops early on the bench's own completion marker so a batch is not pinned
to the full window.
"""
import sys
import time

import serial

port, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]

done = False
nbytes = 0
with open(out, "wb") as f, serial.Serial() as s:
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.5
    s.dtr = True
    s.rts = True
    s.open()
    t0 = time.time()
    tail = b""
    while time.time() - t0 < secs:
        d = s.read(4096)
        if d:
            f.write(d)
            f.flush()
            nbytes += len(d)
            tail = (tail + d)[-64:]
            if b"KBENCH_DONE" in tail or b"Guru" in tail or b"abort() was" in tail:
                done = True
                break
        else:
            idle = time.time() - t0
            if idle > 25 and nbytes > 0:
                break
print(f"capture bytes={nbytes} done={int(done)} secs={time.time() - t0:.1f}")
