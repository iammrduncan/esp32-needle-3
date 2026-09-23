#!/usr/bin/env python3
"""HTTP bridge: inference and tool execution happen on the ESP32-S3."""

import argparse
import json
import os
import re
import threading
import time
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import serial

CATALOG = json.loads((Path(__file__).with_name('model-catalog.json')).read_text())

# Printed when the startup log turns out to be interleaved with echoed console
# probes: past this point it must not be parsed as boot output.
_NOISY_LOG = "WARN boot_log_interleaved"


def metric(line, key):
    match = re.search(rf"\b{key}=([0-9.]+)", line)
    return float(match.group(1)) if match else None


def validate_prompt(prompt):
    if (not isinstance(prompt, str) or not prompt.strip() or
            prompt.lstrip().startswith("!") or
            any(ord(c) < 32 or ord(c) == 127 for c in prompt) or
            len(prompt.encode("utf-8")) > 255):
        raise ValueError("input must be a nonempty line, at most 255 UTF-8 bytes, without control characters or a leading !")


class Device:
    # Declared at class level only so an instance built without __init__ (the
    # unit tests) has the attributes _line and state() touch.
    last_line = ""
    status_credit = 0
    _log_clean = True
    # Echo the startup log as it is read; turned off for probe replies.
    _log_open = True
    _warned_noisy_log = False

    def __init__(self, port, baud, boot_timeout, request_timeout, think):
        self.port = port
        self.baud = baud
        self.lock = threading.RLock()
        self.request_timeout = request_timeout
        self.ready_line = None
        self.available = False
        self.think = think
        self.status_credit = 0
        self._log_open = True
        self._warned_noisy_log = False
        self._open(port, baud)
        self._handshake(boot_timeout)

    def _open(self, port, baud):
        # Both of this board's ports are USB-Serial/JTAG, and a DTR/RTS edge
        # around the open resets the chip. That restarts the ~2 min model-cache
        # priming and makes the first "!think" land in the ESP-ROM loader, whose
        # banner then breaks sync. pyserial raises dtr/rts before opening and
        # drops them again afterwards, so they have to be pinned down on either
        # side of the open. Measured on the connected board: an otherwise
        # identical open reset it (rst:0x15 USB_UART_CHIP_RESET); with this, no.
        self.serial = serial.Serial()
        self.serial.port = port
        self.serial.baudrate = baud
        self.serial.timeout = 1
        self.serial.write_timeout = 5
        self.serial.exclusive = True
        self.serial.dtr = False
        self.serial.rts = False
        self.serial.open()
        self.serial.dtr = False
        self.serial.rts = False
        self._start_drain()

    def _start_drain(self):
        """Consume the port constantly, even when nobody is reading a line.

        Both console directions are USB-Serial/JTAG, and the firmware's printf
        blocks once its TX ring has no host willing to take the bytes. A task
        blocked inside printf never returns to getchar(), so the board stops
        answering EVERYTHING - measured on board 1 after the suite's largest
        output burst (the 67-token three-call case): a non-mutating "!status"
        with DTR/RTS pinned got 0 bytes back, and raising the per-request budget
        to 1200 s did not move the stall by one case (run #393). So this is a
        flow-control deadlock created by the reader, not firmware slowness. A
        pump that never stops makes it impossible; lines are taken from the
        buffer below instead of from the port.
        """
        self._rx = bytearray()
        self._rx_lock = threading.Lock()
        self._rx_stop = threading.Event()

        def pump():
            while not self._rx_stop.is_set():
                try:
                    chunk = self.serial.read(4096)
                except Exception:
                    return          # port closed under us: this attach is over
                if chunk:
                    with self._rx_lock:
                        # Bound it: an unread board must not grow host memory
                        # without limit. Oldest bytes go; nothing waits on them.
                        if len(self._rx) > 8 << 20:
                            del self._rx[:6 << 20]
                        self._rx.extend(chunk)

        self._rx_thread = threading.Thread(target=pump, daemon=True)
        self._rx_thread.start()

    def _stop_drain(self):
        stop = getattr(self, "_rx_stop", None)
        if stop is not None:
            stop.set()
            try:
                self._rx_thread.join(timeout=2.0)
            except Exception:
                pass
            self._rx_stop = None

    def _handshake(self, boot_timeout):
        """Attach, then re-attach quietly if the board was busy on arrival.

        The console echoes whatever is written to it while the firmware is
        working, so a probe sent during the ~2 min post-reset priming comes back
        as a request line and is answered "ERR unknown_command" or
        "ERR incomplete_call". Those replies belong to the handshake and not to
        any request: failing on them, or leaving them queued for the first real
        request, is what aborted the first three-board baseline batch.

        So: read until the firmware answers a status we asked for. A board that
        was busy answers only behind those echoes, so reconnect without
        resetting - by then it is idle - and take one clean status off it. That
        costs the buffered startup log, which on such an attach is interleaved
        anyway; requests go out only after this returns, so the measurement is
        unaffected.
        """
        self._set_think()
        state = self._await_attach(boot_timeout)
        if state is None:
            self._reconnect()
            state = self._request_state(min(boot_timeout, 30.0))
        if state is None:
            raise TimeoutError("ESP32 did not become ready; check serial port and firmware boot logs")
        if state.get("schema") != "agent-watch-v2":
            raise RuntimeError("flash the agent watch firmware before starting this bridge")
        self.available = True

    def _await_attach(self, timeout):
        """Watch the startup log until the board answers a status of ours.

        Read the way the original handshake read it - echoing, and stopping at
        the think ack - so bench.py still gets the boot bench and the per-phase
        profile. Two differences, both learned the hard way: an "ERR" is no
        longer fatal (it is usually the answer to an echoed probe), and a
        firmware "READY" banner is welcome rather than required, because a
        reconnect can only attach after that banner has already scrolled past.
        """
        self._log_open = True
        self._log_clean = True
        self._warned_noisy_log = False
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            line = self._line()
            if not line:
                continue
            if self._is_echo_reply(line):
                self._log_clean = False
            elif line.startswith("EVT  READY"):
                self.ready_line = line
                self._set_think()
            elif line == f"EVT think={int(self.think)}":
                # The ack is ours: bench.py starts reading at the next line, so
                # leaving it here would hand it to the first case.
                self.ready_line = self.ready_line or "attached to running firmware"
                self.available = True
                if not self._log_clean:
                    return None
                return self._request_state(max(timeout - (time.monotonic() - (until - timeout)), 5.0))
        return None

    def _reconnect(self):
        """Re-attach to an idle board, keeping its state but not its backlog.

        Everything before the first clean "!status" belongs to the handshake, so
        it must never reach bench.py: the board prints its startup log once, at
        boot, and on a reconnect that log is already in the queue behind a wall
        of echoed probe replies. reset_input_buffer() is not enough - pyserial
        only clears bytes nobody has read yet, the driver has already delivered
        them to this process. So the backlog is drained with reads instead.
        """
        self._stop_drain()
        try:
            self.serial.close()
        except Exception:
            pass
        self._open(self.port, self.baud)
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline and self._line(timeout=0.5):
            pass
        self.available = False
        self.ready_line = None
        self.status_credit = 0
        # Nothing more to learn from this board's startup log, and provoking an
        # ack here would queue a reply that the next reader mistakes for the end
        # of a request.
        self._log_open = False

    def _request_state(self, timeout):
        """Ask for the state and return only the frame that answers this ask.

        The firmware has no request/response framing: it answers whatever it
        reads from the console, so a probe written while it was busy is echoed
        and answered inside the next reply. A "STATE" frame in the stream is
        therefore not necessarily an answer to us. Crediting each "!status" and
        accepting only frames that pay a credit is what stops an echoed STATE
        being taken for the reply to a later, unrelated request.
        """
        self.status_credit += 1
        self.serial.write(b"!status\n")
        self.serial.flush()
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            line = self._line()
            if line.startswith("STATE ") and self.status_credit > 0:
                self.status_credit -= 1
                return json.loads(line[6:])
        self.status_credit -= 1
        return None

    def _is_echo_reply(self, line):
        """True if `line` can only be the answer to a probe we wrote.

        The firmware answers whatever it reads from the console, so anything
        written while it is working is echoed and answered as if it were a
        prompt: "ERR" for a command it cannot parse, "STATE" for a "!status" we
        never asked for. If that happens the startup log is interleaved, and
        bench.py must not read it as boot output.
        """
        if line.startswith("ERR ") or (line.startswith("STATE ") and
                                       self.status_credit <= 0):
            if self._log_open and not self._warned_noisy_log:
                print(_NOISY_LOG, flush=True)
                self._warned_noisy_log = True
                self._log_open = False     # nothing after this is boot output
            return True
        return False

    def _skip_until(self, wanted, timeout):
        """Drain up to and including `wanted`, or give up after `timeout`."""
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            if self._line_quiet() == wanted:
                return True
        return False

    def _line(self, timeout=1.0):
        # Preserve spaces in tokens. strip() corrupts the generated text.
        # Taken from the pump's buffer (see _start_drain), with the same return
        # semantics readline had: an empty string when nothing arrives in time.
        raw = b""
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            with self._rx_lock:
                nl = self._rx.find(b"\n")
                if nl >= 0:
                    raw = bytes(self._rx[:nl + 1])
                    del self._rx[:nl + 1]
                    break
            time.sleep(0.005)
        line = raw.decode("utf-8", "replace").rstrip("\r\n")
        self.last_line = line
        if line and self._log_open:
            print(line, flush=True)
        return line

    def drain_stale(self, window=1.0):
        """Discard console bytes that arrived before we asked anything.

        The firmware has no request/response framing (see _request_state), so any
        line still sitting in the queue when a request or toggle is written will be
        read back as the START of this reply - or, for a toggle, echoed as a torn
        line the REPL never answers. bench.py runs its whole suite on one attach, so
        one extra/missing `END` shifts EVERY later case by one: measured in run #345
        a route prompt came back answered with tools-schema calls its own grammar
        forbids, and with this drain the same three prompts answered their own schema
        (run #346). Draining with reads - not reset_input_buffer, which only clears
        bytes nobody has read yet - before the write cannot touch our own answer,
        because it has not been asked for yet. Returns the lines it removed from the
        queue, and echoes them through the log: excluded from THIS reply's parsing,
        never silently thrown away. The caller warns when nonzero so a desync cannot
        pass as a clean run.

        A fixed window, deliberately: settle-to-quiet (quiet=1.5 s, budget=8 s) was
        tried on the same suite in run #346 and regressed it - 17 cases and a request
        timeout instead of 20/20 - so keep this short and let the toggle pay it.
        """
        dropped, deadline = [], time.monotonic() + window
        while time.monotonic() < deadline:
            # Read through _line(), not the raw port: these are real firmware
            # output (the boot bench and the `EVT prof` block live here, and
            # bench.py parses them), so DROPPING them would be a second bug -
            # the idle-board startup-log test exists precisely because of that.
            # Excluded from reply parsing, surfaced to the log: both properties,
            # not one at the cost of the other.
            line = self._line()
            if not line:
                break
            # Same classifier the attach path uses: if a drained line can only be
            # the answer to a probe, the startup log is interleaved and bench.py
            # must stop reading it as boot output - whether the line was read by
            # the attach reader or by this drain.
            self._is_echo_reply(line)
            dropped.append(line)
        return dropped

    def _line_quiet(self):
        """A read whose reply is ours, not the caller's boot log."""
        log_open, self._log_open = self._log_open, False
        try:
            return self._line()
        finally:
            self._log_open = log_open

    def _set_think(self):
        # Same reason complete() drains: the toggle's ack is read by a DIFFERENT
        # reader (bench.py's switch_think waits for `EVT think=`), and an undrained
        # tail from the previous answer is what made that ack "missing" (run #345's
        # enlarged suite) - so the mode never got set and the think number silently
        # measured the constrained path instead. One guard here covers every caller.
        stale = self.drain_stale()
        if stale:
            print(f"WARN drained {len(stale)} stale line(s) before the think toggle")
        self.serial.write(f"!think {int(self.think)}\n".encode())
        self.serial.flush()

    def _require_ready(self):
        if not self.available:
            raise ConnectionError("serial stream lost synchronization; wait for the board to finish, then restart the bridge")

    def state(self):
        with self.lock:
            self._require_ready()
            state = self._request_state(5)
            if state is None:
                self.available = False
                raise TimeoutError("ESP32 status request timed out")
            return state

    def _hard_reset(self):
        """Pulse the chip's EN line through the USB-JTAG (flash) port.

        The soft rescue is not enough when the board stops answering a fresh
        attach at all - which is what run #390's canonical run died on ("board did
        not answer after one reconnect"), and what had already cost four gated
        verdicts on 2026-09-23. A reconnect only replaces the host's file
        descriptor; a wedge behind the CDC bridge needs the chip restarted.

        On the S3's USB-Serial-JTAG the modem lines ARE the boot straps: RTS is EN
        and DTR is IO0, so asserting RTS with DTR low and releasing it restarts the
        app into normal boot - the same two transitions esptool uses, without
        needing esptool importable inside the bench's own venv. The console is the
        separate CDC port, which cannot reset anything, and the ledger's fact holds:
        reset with the console CLOSED, or the strap is sampled wrong and the chip
        sits in DOWNLOAD mode. Cost is the ~5 min prefix re-priming, which is why
        this is the second stage, never the first.

        Refuses to reset unless the flash node is identifiable as the same board as
        the console: under the pool both paths carry `boardN-`, and a mismatch would
        mean rebooting somebody else's measurement.
        """
        flash = os.environ.get("FLASH_PORT") or "/dev/ttyACM0"
        con, fl = self.port, flash
        if "/needle-pi/" in con and "board" in con:
            cb = con.split("board")[-1][:1]
            fb = fl.split("board")[-1][:1] if "board" in fl else ""
            if cb != fb:
                raise TimeoutError(f"console {con} and flash {fl} are different boards; "
                                   "refusing to reset one of them")
        try:
            self.serial.close()
        except Exception:
            pass
        rst = serial.Serial()
        rst.port = fl
        rst.baudrate = 115200
        rst.timeout = 0.5
        rst.dtr = False                 # IO0 high  -> normal boot, not download
        rst.rts = True                  # EN low    -> hold in reset
        rst.open()
        time.sleep(0.10)
        rst.rts = False                 # EN high   -> release into boot
        time.sleep(0.10)
        rst.close()
        self.available = False
        self._open(self.port, self.baud)
        self._handshake(max(self.request_timeout, 600.0))   # two model caches re-warm

    def complete(self, prompt, phase="tools"):
        """Run one request, rescuing a stalled console by re-sending it once.

        Run #345's open defect, reproduced three times on 2026-09-23: late in a long
        attached session a request goes unanswered - the same console fatigue that
        makes `!think` stop acking - and `bench.py` loses the rest of the suite, which
        loses the *device* byte-exact gate for whatever candidate was being measured
        (the host gate cannot cover a two-core defect, #298). The rescue is the one
        #346 proved for the think ack: reconnect without resetting the chip (DTR/RTS
        are pinned through the open) and re-establish readiness.

        The request is re-sent, never skipped, and a second failure propagates: a run
        that quietly drops a case measures nothing while reporting as a pass. The
        `retried` marker plus the WARN line keep it visible in the lane log.
        """
        try:
            return self._complete_once(prompt, phase)
        except TimeoutError:
            print("WARN request timed out; reconnecting and re-sending this case "
                  "(not skipping it)")
            self.available = False
            self._reconnect()
            if self._request_state(min(self.request_timeout, 30.0)) is None:
                if not os.environ.get("AUTO_HARD_RESET"):
                    raise TimeoutError(
                        "board did not answer after one reconnect; case was not skipped, "
                        "and no chip reset was attempted (AUTO_HARD_RESET unset)")
                # A mid-suite reset restarts the firmware's demo timer and sampling
                # counters, and two held-out cases (`heldout_interval_one`,
                # `heldout_long_tools_note_only`) depend on that state: measured on
                # run #391's canonical session, exactly the two cases after a reset
                # diverged. So a gated run must not mix pre- and post-reset context -
                # opt in only for speed-only or diagnostic runs.
                print("WARN reconnect got no answer; resetting the chip and re-sending "
                      "this case (not skipping it)")
                self._hard_reset()          # re-establishes readiness or raises
                result = self._complete_once(prompt, phase)
                result["retried"] = 1
                result["hard_reset"] = 1
                return result
            self.available = True
            result = self._complete_once(prompt, phase)
            result["retried"] = 1
            return result

    def _complete_once(self, prompt, phase="tools"):
        validate_prompt(prompt)
        if phase not in ("tools", "route"):
            raise ValueError("unknown inference phase")
        with self.lock:
            self._require_ready()
            stale = self.drain_stale()
            if stale:
                # Loud, not silent: a nonzero count means the last response was not
                # drained, which is exactly how run #345's cases answered the wrong
                # schema. Surfaces in bench.py's log instead of shifting the suite.
                print(f"WARN drained {len(stale)} stale line(s) before this request")
            start = time.monotonic()
            self.serial.write((b"!route " if phase == "route" else b"") + prompt.encode() + b"\n")
            self.serial.flush()
            output, calls, results = [], None, None
            prefill_ms = decode_ms = prefill_tps = decode_tps = None
            tokens = confidence = error = None
            done = False
            try:
                while time.monotonic() - start < self.request_timeout:
                    line = self._line_quiet()
                    if line.startswith("TOK "):
                        output.append(line[4:].replace("\\n", "\n"))
                    elif line.startswith("JSON "):
                        calls = json.loads(line[5:])
                    elif line.startswith("RESULT "):
                        results = json.loads(line[7:])
                    elif line.startswith("CONF "):
                        confidence = float(line[5:])
                    elif line.startswith("EVT prefill "):
                        prefill_ms = metric(line, "ms")
                        prefill_tps = metric(line, "tps")
                    elif line.startswith("EVT done "):
                        decode_ms = metric(line, "ms")
                        decode_tps = metric(line, "tps")
                        tokens = metric(line, "tokens")
                        done = True
                    elif line.startswith("ERR "):
                        error = line[4:]
                    elif line == "END":
                        break
                else:
                    raise TimeoutError("ESP32 inference exceeded request timeout")
            except (TimeoutError, ValueError, serial.SerialException):
                self.available = False
                raise
            if error is None and (not done or calls is None or results is None):
                error = "incomplete_firmware_response"
            if error is None and not all(item.get("success") for item in results):
                error = "tool_execution_failed"
            return {
                "type": "call", "success": error is None, "error": error,
                "function_calls": calls or [], "results": results or [],
                "raw": "".join(output), "confidence": confidence,
                "prefill_ms": prefill_ms, "prefill_tps": prefill_tps,
                "decode_ms": decode_ms, "decode_tps": decode_tps,
                "decode_tokens": int(tokens) if tokens is not None else None,
                "latency_ms": round((time.monotonic() - start) * 1000, 1),
                "device": "esp32-s3", "reasoning": self.think,
                "phase": phase,
            }

    def agent(self, prompt):
        """A real model decision, then either stop or make a second model call."""
        validate_prompt(prompt)
        # Keep the two model passes and their outcomes together on this device.
        with self.lock:
            start = time.monotonic()
            route = self.complete(prompt, phase="route")
            result = {"input": prompt, "success": False, "routing": route,
                      "execution": None, "selected_model": None,
                      "remote_called": False, "inference_passes": 1}
            calls = route["function_calls"]
            if not route["success"] or len(calls) != 1:
                result.update(outcome="route_failed", error=route.get("error") or "invalid_model_choice")
            else:
                choice = next((key for key, model in CATALOG.items() if calls[0].get("name") in model["route_tools"]), None)
                model = CATALOG.get(choice)
                if model is None:
                    result.update(outcome="route_failed", error="unknown_model")
                else:
                    result["selected_model"] = {"key": choice, **model}
                    if model["execution"] == "local":
                        # Original request is passed unchanged under a new schema.
                        execution = self.complete(prompt, phase="tools")
                        result.update(execution=execution, inference_passes=2,
                                      success=execution["success"], error=execution["error"],
                                      outcome="local_executed" if execution["success"] else "local_failed")
                    else:
                        result.update(success=True, error=None, outcome="external_selected")
            result["latency_ms"] = round((time.monotonic() - start) * 1000, 1)
            return result


class Handler(BaseHTTPRequestHandler):
    device = None

    def setup(self):
        super().setup()
        self.connection.settimeout(10)

    def do_GET(self):
        if self.path == "/models":
            self._json(200, CATALOG)
            return
        if self.path not in ("/health", "/state"):
            self._json(404, {"error": "not found"})
            return
        try:
            state = self.device.state()
            self._json(200, {"ready": True, **state} if self.path == "/health" else state)
        except (TimeoutError, ConnectionError, serial.SerialException, OSError) as exc:
            self._json(503, {"ready": False, "error": str(exc)})

    def do_POST(self):
        if self.path not in ("/complete", "/agent", "/route"):
            self._json(404, {"error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 4096:
                raise ValueError("request body must be 1..4096 bytes")
            body = json.loads(self.rfile.read(length))
            if not isinstance(body, dict) or "input" not in body:
                raise ValueError('body must be an object with an "input" string')
            if self.path == "/agent":
                result = self.device.agent(body["input"])
            else:
                result = self.device.complete(body["input"], phase="route" if self.path == "/route" else "tools")
            self._json(200 if result["success"] else 502, result)
        except (ValueError, KeyError, UnicodeError) as exc:
            self._json(400, {"error": str(exc)})
        except TimeoutError as exc:
            self._json(504, {"error": str(exc)})
        except (ConnectionError, serial.SerialException, OSError) as exc:
            self._json(503, {"error": str(exc)})

    def _json(self, status, data):
        payload = json.dumps(data, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", default="/dev/ttyACM1")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8081)
    parser.add_argument("--boot-timeout", type=float, default=600)
    parser.add_argument("--request-timeout", type=float, default=300)
    parser.add_argument("--think", action="store_true", help="include model reasoning (slower; does not guarantee correctness)")
    args = parser.parse_args()
    Handler.device = Device(args.serial, args.baud, args.boot_timeout, args.request_timeout, args.think)
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"Needle 3 ESP32 API at http://{args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        Handler.device.serial.close()


if __name__ == "__main__":
    main()
