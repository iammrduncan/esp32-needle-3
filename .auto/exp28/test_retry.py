#!/usr/bin/env python3
"""Off-device falsification for the stalled-request rescue in Device.complete().

The property that matters is not "it retries" - it is that it NEVER drops a case.
A bench run that skips a case still prints a full-looking METRIC block, and the
device byte-exact gate (#298) is the thing that would be silently gone, which is
this campaign's most expensive failure class (four checks that reported success
while verifying nothing). So every path is asserted, including the ones that must
still fail.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import serial_api  # noqa: E402


class Fake(serial_api.Device):
    """A Device with the serial layer removed: only the retry logic is under test."""

    def __init__(self, fails, answers_after_reconnect=True):
        self.request_timeout = 1
        self.available = True
        self.fails = fails
        self.answers = answers_after_reconnect
        self.attempts = 0
        self.reconnects = 0

    def _complete_once(self, prompt, phase="tools"):
        self.attempts += 1
        if self.attempts <= self.fails:
            raise TimeoutError("stalled console")
        return {"text": "ok", "calls": [], "results": []}

    def _reconnect(self):
        self.reconnects += 1

    def _request_state(self, timeout):
        return {"schema": "agent-watch-v2"} if self.answers else None


def main():
    # A: one stall -> rescued, and the SAME case is re-sent.
    d = Fake(fails=1)
    r = d.complete("hello")
    assert r.get("retried") == 1, r
    assert d.attempts == 2, d.attempts
    assert d.reconnects == 1, d.reconnects

    # B: board does not answer the reconnect -> propagate, and do NOT attempt a
    # third time or return a result. This is the "no silent skip" case.
    d = Fake(fails=1, answers_after_reconnect=False)
    try:
        d.complete("hello")
        raise AssertionError("must not return when the reconnect is unanswered")
    except TimeoutError:
        pass
    assert d.attempts == 1, f"unanswered reconnect must not re-send: {d.attempts}"

    # C: still stalled after the rescue -> propagate after exactly one re-send.
    d = Fake(fails=2)
    try:
        d.complete("hello")
        raise AssertionError("a persistently stalled board must fail the run")
    except TimeoutError:
        pass
    assert d.attempts == 2 and d.reconnects == 1, (d.attempts, d.reconnects)

    # D: the happy path is untouched - no reconnect, no retried marker.
    d = Fake(fails=0)
    assert "retried" not in d.complete("hello")
    assert d.reconnects == 0 and d.attempts == 1

    print("RETRY_GUARD_OK cases=4 attempts_are_re_sends_not_skips=1")


if __name__ == "__main__":
    main()
