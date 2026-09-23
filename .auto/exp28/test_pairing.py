#!/usr/bin/env python3
"""Falsification for the two console-pairing guards (run #346).

`drain_stale` and the think-mode ack are the kind of check this campaign has
repeatedly found to be unable to fail, so both are exercised here with fakes -
no board, no serial. Each assertion is "the guard must report the bad thing".
"""
import importlib.util, os, sys, types

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def load(name, rel):
    spec = importlib.util.spec_from_file_location(name, os.path.join(ROOT, rel))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


sa = load('serial_api_probe', 'tools/serial_api.py')
bench = load('bench_probe', '.auto/bench.py')


class FakeSerial:
    """Yields canned lines then reports 'nothing more', like a drained port."""

    def __init__(self, lines):
        self.out = [l.encode() for l in lines]
        self.written = []

    def readline(self):
        return self.out.pop(0) if self.out else b''

    def write(self, b):
        self.written.append(b)

    def flush(self):
        pass


class FakeDev:
    def __init__(self, lines):
        self.serial = FakeSerial(lines)
        self.think = False
        # _is_echo_reply() inspects the attach bookkeeping; a canned line set that
        # contains no ERR/STATE never reads it, but the attribute must exist.
        self.status_credit = 1
        self._log_open = False
        self._warned_noisy_log = False

    # `drain_stale()` reads through `_line()` since run #346's rewrite (it has to
    # drain with reads, not reset_input_buffer). The fake only ever provided the
    # serial layer, so the guard had been dying on its FIRST assertion for many
    # runs - an AttributeError is loud, but nothing ran it. Same semantics as the
    # real Device._line: a stripped line, or '' to mean pyserial's read timeout.
    def _line(self):
        raw = self.serial.readline()
        return raw.decode().rstrip("\n") if raw else ''

    drain_stale = sa.Device.drain_stale
    _is_echo_reply = sa.Device._is_echo_reply


# 1. drain_stale counts stale lines, and reports zero on a clean port.
d = FakeDev(['TOK x', 'CONF 0.5', 'END', ''])
n = d.drain_stale(window=1.0)
# drain_stale() returns the stale LINES (complete() logs len(stale)) - the count
# contract this assertion was written against is gone. The canned '' stands for
# pyserial's read timeout (b'').
assert len(n) == 3, n
assert d.drain_stale(window=1.0) == [], 'clean port must drop nothing'

# 2. _set_think drains BEFORE writing, so a spill cannot be read as its ack.
d = FakeDev(['TOK spill', 'END', ''])
sa.Device._set_think(d)
assert d.serial.written == [b'!think 0\n'], d.serial.written

# 3. switch_think: right ack passes, wrong ack and no ack both fail loudly.
class AckDev(FakeDev):
    def __init__(self, lines):
        FakeDev.__init__(self, [])
        self.lines = list(lines)

    def _line(self):
        return self.lines.pop(0) if self.lines else ''

    def _set_think(self):
        pass   # the write itself is asserted on FakeDev above

    def _reconnect(self):
        self.reconnected = True   # the retry path must be exercised, not hung on

    def _handshake(self, timeout):
        self.rehandshaked = True


def clock(step):
    """Advance a fake monotonic so the no-ack path does not really wait 30 s."""
    class T:
        t = [0.0]

        def monotonic(self):
            v = self.t[0]
            self.t[0] += step
            return v
    return T()


bench.time = clock(11.0)
AckDev.think = False
d = AckDev(['EVT think=1'])
d.think = True
bench.switch_think(d, True)                     # right mode -> returns
# a wrong ack, then a correct one after the reconnect -> the retry is what saved it
d = AckDev(['EVT think=0', 'EVT think=1'])
d.request_timeout = 600
bench.switch_think(d, True)
assert getattr(d, 'reconnected', False), 'retry path not taken'

# a board that never acks must fail loudly, after exactly one reconnect
d = AckDev([])
d.request_timeout = 600
try:
    bench.switch_think(d, True)
except SystemExit as e:
    assert str(e).startswith('THINK_ACK_MISSING'), str(e)
    assert getattr(d, 'reconnected', False), 'must have tried the reconnect'
else:
    raise AssertionError('THINK_ACK_MISSING did not fire')
print('PAIRING_GUARDS_OK drain=3/0 think_ack=right/wrong/missing all enforced')
