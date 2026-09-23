#!/usr/bin/env python3
"""Off-device falsification for the stalled-request rescue in Device.complete().

The property that matters is not "it retries" - it is that it NEVER drops a case.
A bench run that skips a case still prints a full-looking METRIC block, and the
device byte-exact gate (#298) is the thing that would be silently gone, which is
this campaign's most expensive failure class (checks that report success while
verifying nothing). So every path is asserted, including the ones that must still
fail, and the escalation order is asserted too: reconnect before reset, reset only
when the reconnect got no answer.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import serial_api  # noqa: E402


class Fake(serial_api.Device):
    """A Device with the serial layer removed: only the rescue logic is under test."""

    def __init__(self, fails, answers_after_reconnect=True, hard_reset_works=True,
                 port="/dev/needle-pi/board2-console", flash="/dev/needle-pi/board2-flash"):
        self.request_timeout = 1
        self.available = True
        self.fails = fails
        self.answers = answers_after_reconnect
        self.hard_reset_works = hard_reset_works
        self.port = port
        self.attempts = 0
        self.reconnects = 0
        self.hard_resets = 0
        self._flash_env = flash

    def _complete_once(self, prompt, phase="tools"):
        self.attempts += 1
        if self.attempts <= self.fails:
            raise TimeoutError("stalled console")
        return {"text": "ok", "calls": [], "results": []}

    def _reconnect(self):
        self.reconnects += 1

    def _request_state(self, timeout):
        return {"schema": "agent-watch-v2"} if self.answers else None

    def _hard_reset(self):
        # The real method pulses EN through the JTAG port; here assert only the
        # safety rule it enforces (same board on both paths) and the outcome.
        fl = self._flash_env
        cb = self.port.split("board")[-1][:1]
        fb = fl.split("board")[-1][:1] if "board" in fl else ""
        if cb != fb:
            raise TimeoutError("console and flash are different boards; refusing to reset")
        self.hard_resets += 1
        if not self.hard_reset_works:
            raise TimeoutError("chip did not come back")
        self.answers = True


def main():
    # A: one stall -> soft rescue, and the SAME case is re-sent. No reset.
    d = Fake(fails=1)
    r = d.complete("hello")
    assert r.get("retried") == 1 and "hard_reset" not in r, r
    assert (d.attempts, d.reconnects, d.hard_resets) == (2, 1, 0), (d.attempts, d.reconnects)

    # B: reconnect unanswered and NO opt-in -> propagate without touching the chip.
    # Gated-run default: a mid-suite reset restarts the firmware's demo timer, and
    # run #391's canonical session diverged on exactly the two cases that followed a
    # reset (both timer/sampling dependent). Failing the run is honest; mixing two
    # boot contexts inside one gate verdict is not.
    os.environ.pop("AUTO_HARD_RESET", None)
    d = Fake(fails=1, answers_after_reconnect=False)
    try:
        d.complete("hello")
        raise AssertionError("must fail rather than reset a board mid-gate")
    except TimeoutError:
        pass
    assert (d.attempts, d.hard_resets) == (1, 0), (d.attempts, d.hard_resets)

    # B2: opt-in (speed-only or diagnostic runs) -> escalate and STILL re-send.
    os.environ["AUTO_HARD_RESET"] = "1"
    d = Fake(fails=1, answers_after_reconnect=False)
    r = d.complete("hello")
    assert r.get("hard_reset") == 1, r
    assert (d.attempts, d.reconnects, d.hard_resets) == (2, 1, 1), (d.attempts, d.hard_resets)

    # C: the reset does not revive the board -> propagate. Attempts stay at one
    # re-send: no silent skip, no infinite loop.
    d = Fake(fails=3, answers_after_reconnect=False, hard_reset_works=True)
    try:
        d.complete("hello")
        raise AssertionError("a board that never answers must fail the run")
    except TimeoutError:
        pass
    assert (d.attempts, d.hard_resets) == (2, 1), (d.attempts, d.hard_resets)

    # D: the chip reset itself fails -> propagate, nothing returned.
    d = Fake(fails=1, answers_after_reconnect=False, hard_reset_works=False)
    try:
        d.complete("hello")
        raise AssertionError("must not return when the chip does not come back")
    except TimeoutError:
        pass
    assert d.attempts == 1, d.attempts

    # E: SAFETY - console and flash nodes belonging to different boards must never
    # let this process reboot a board that is not the one being measured.
    d = Fake(fails=1, answers_after_reconnect=False,
             port="/dev/needle-pi/board2-console", flash="/dev/needle-pi/board3-flash")
    try:
        d.complete("hello")
        raise AssertionError("cross-board reset must be refused")
    except TimeoutError as exc:
        assert "different boards" in str(exc), exc
    assert d.hard_resets == 0, "refused reset must not have run"

    # F: the happy path is untouched.
    d = Fake(fails=0)
    assert "retried" not in d.complete("hello")
    assert (d.reconnects, d.hard_resets, d.attempts) == (0, 0, 1)

    os.environ.pop("AUTO_HARD_RESET", None)
    print("RETRY_GUARD_OK cases=7 escalation=reconnect_then_reset no_case_is_ever_skipped=1")


if __name__ == "__main__":
    main()
