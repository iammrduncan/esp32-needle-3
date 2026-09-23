#!/usr/bin/env python3
"""Raw console probe for the response-pairing defect (run #345, defect 3).

bench.py pairs a request with the lines up to the next `END`. When the extended
set grew to 22 cases, the last cases came back answered with the *other* schema's
calls - which their own grammar forbids - so either one request produced two
responses, or one response was read past its END. Drive the prompts one at a time
and print EVERY line with a sequence number, plus anything that arrives after
END, so the pairing is visible instead of inferred. Prompts only: no reflash, no
reset. Run inside `needle-board run N` so SERIAL_PORT is this board's console.
"""
import json, os, sys, time

sys.path.insert(0, '/workspace/esp32-needle-3/tools')
import serial_api  # noqa: E402

PROMPTS = json.load(open('/workspace/esp32-needle-3/.auto/prompts.json'))
cases = {c['id']: c for g in ('primary', 'extended') for c in PROMPTS[g]}
WANT = sys.argv[1:] or ['heldout_long_tools', 'heldout_long_route',
                        'heldout_interval_one', 'heldout_status']
PORT = os.environ.get('SERIAL_PORT', '/dev/needle-pi/console')

dev = serial_api.Device(PORT, 115200, 600, 180, False)
print(f'ATTACHED port={PORT} board={os.environ.get("NEEDLE_BOARD")}', flush=True)
for cid in WANT:
    c = cases[cid]
    buf = (b'!route ' if c['phase'] == 'route' else b'') + c['input'].encode() + b'\n'
    print(f'--- SEND {cid} phase={c["phase"]} bytes={len(buf) - 1}', flush=True)
    t0 = time.monotonic()
    dev.serial.write(buf)
    dev.serial.flush()
    seq, ends = 0, 0
    deadline = t0 + 180
    while time.monotonic() < deadline:
        ln = dev._line_quiet()
        if ln is None:
            print(f'  [{seq:03d}] <no line for read timeout> ends={ends}', flush=True)
            break
        seq += 1
        if ln == 'END':
            ends += 1
        print(f'  [{seq:03d}]{" END" if ln == "END" else "     "} {ln[:104]}', flush=True)
        if ends and time.monotonic() - t0 > 0:
            # Stay attached past the first END: a second response for one request
            # is the failure mode, and it can only be seen by not stopping.
            stop = time.monotonic() + 3.0
            while time.monotonic() < stop:
                extra = dev._line_quiet()
                if extra is None:
                    continue
                seq += 1
                ends += (extra == 'END')
                print(f'  [{seq:03d}] POST-END {extra[:104]}', flush=True)
            break
    print(f'--- DONE {cid} lines={seq} ends={ends} elapsed={time.monotonic() - t0:.1f}s', flush=True)
