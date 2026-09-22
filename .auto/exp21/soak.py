#!/usr/bin/env python3
"""Experiment 21 thermal/integrity soak.

Run INSIDE the board lock:
  /root/bin/needle-board run N -- .venv/bin/python .auto/exp21/soak.py 20

It attaches to an ALREADY-WARM board (DTR/RTS pinned False so opening the
console never resets the chip: a reset costs the five-minute cache warm-up the
campaign keeps warning about), confirms the app is alive with the non-mutating
`!status`, then drives real requests back to back and logs EVERY console line.

Logging every line is the point: the firmware's TDIAG records are unsolicited,
and the higher-level reader in tools/serial_api.py discards anything that is not
an EVT marker, which would throw away the temperature and PSRAM-CRC evidence
this soak exists to collect. (An earlier version did use that reader and it
never issued a single request: its handshake waits for `EVT READY`, which a
board that booted six minutes ago is never going to print again.)

The load is what moves the die temperature, and temperature is the axis the
accepted 120 MHz result is only valid across.
"""
import os
import sys
import time

sys.path.insert(0, "/workspace/esp32-needle-3/tools")
import serial_api  # noqa: E402

MINUTES = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0
PROMPTS = [
    ("List tools", "tools"),
    ("What is the heap right now", "tools"),
    ("sampling rate please", "tools"),
    ("Translate good morning into German", "route"),
]


def main() -> int:
    port = os.environ.get("SERIAL_PORT", "")
    b = os.environ.get("EXP21_BOARD", "")
    want = {1: "/dev/needle-pi/console"}.get(int(b or 0),
                                             "/dev/needle-pi/board%s-console" % b)
    if port != want:
        raise SystemExit("port mismatch: asked for board %r, SERIAL_PORT is %r "
                         "(expected %r)" % (b, port, want))

    log = open("/tmp/exp21-soak-%s.log" % os.environ.get("EXP21_TAG", "x"), "a")

    def note(s):
        print(s, flush=True)
        log.write(s + "\n")
        log.flush()

    # The campaign's own console driver: it knows the attach protocol, keeps
    # DTR/RTS low so it cannot drop the chip into the ROM loader, and tolerates
    # the unsolicited lines. A hand-rolled reader was tried first (twice) and
    # never got a request answered, because `!status` on an already-warm board
    # is not the attach this console expects.
    dev = serial_api.Device(port, 115200, 900, 600, False)

    # Wrap the line reader so the firmware's unsolicited TDIAG records survive:
    # the harness discards anything that is not an EVT marker, and those records
    # ARE the measurement.
    real_line = dev._line
    faults = [0]

    def logged_line():
        ln = real_line()
        if ln:
            log.write(ln + "\n")
            if ln.startswith("TDIAG"):
                note("TDIAG " + ln[6:])
            elif ("Guru" in ln or ln.startswith("EVT ERR")
                    or "abort()" in ln or "panic" in ln.lower()):
                note("FAULT " + ln)
                faults[0] += 1
            if "psram_crc_bad=" in ln:
                bad = ln.split("psram_crc_bad=")[1].split()[0]
                if bad != "0":
                    note("PSRAM_CRC_MISMATCH " + ln)
                    faults[0] += 1
            log.flush()
        return ln

    dev._line = logged_line
    end = time.time() + MINUTES * 60
    t0 = time.time()
    i = done = 0
    while time.time() < end:
        p, phase = PROMPTS[i % len(PROMPTS)]
        i += 1
        try:
            res = dev.complete(p, phase)
            done += 1
            note("%.0fs REQ %d %s -> rc=%s tps=%s" %
                 (time.time() - t0, i, p, res.get("rc"), res.get("tps")))
        except Exception as exc:
            faults[0] += 1
            note("%.0fs REQ %d %s -> EXC %s" %
                 (time.time() - t0, i, p, type(exc).__name__))
    note("SOAK_DONE requests=%d completed=%d faults=%d span=%.0fs" %
         (i, done, faults[0], time.time() - t0))
    log.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
