#!/usr/bin/env python3
"""Read the per-REQUEST ND_PROFILE table off a warm board.

measure.sh's log only carries the boot-bench table: bench.py drives the board through
serial_api.Device, whose `complete()` reads with `_line_quiet()`, so the request-path
`EVT prof` block that prof_dump() prints AFTER `EVT done` is swallowed. This attaches
to a board that is already booted and primed (a profiled image - no reflash needed),
makes the reader echo lines instead of swallowing them, and runs one tools request and
one route request through the real API call path.

Run #158's caveat still applies: prof_dump() does not zero nd_prof, so every dump is
cumulative over the boot bench plus all requests so far in this boot. Read the dump for
`sample`/`logits4` (which the bench cannot produce at all) and take the difference
between two dumps for a per-request increment.

Usage, inside `needle-board run N` so $SERIAL_PORT is this board's console:
    python3 .auto/prof_request.py "tools prompt" "route prompt"
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import serial_api  # noqa: E402

port = os.environ.get("SERIAL_PORT")
if not port:
    sys.exit("SERIAL_PORT not set - run inside: needle-board run N -- ...")

prompts = sys.argv[1:] or ["What is the heap high water mark?"]

# Echo everything the firmware says instead of swallowing it in the parser.
serial_api.Device._line_quiet = lambda self: self._line()

dev = serial_api.Device(port, 115200, 600, 600, False)
for i, p in enumerate(prompts):
    phase = "route" if p.startswith("!route ") else ("tools" if i == 0 else "route")
    print("### request phase=%s: %s" % (phase, p), flush=True)
    r = dev.complete(p.replace("!route ", ""), phase=phase)
    print("### done tps=%s tokens=%s decode_ms=%s" %
          (r["decode_tps"], r["decode_tokens"], r["decode_ms"]), flush=True)
