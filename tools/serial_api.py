#!/usr/bin/env python3
"""HTTP bridge: inference and tool execution happen on the ESP32-S3."""

import argparse
import json
import os
import re
import threading
import time
import uuid
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import serial

CATALOG = json.loads((Path(__file__).with_name('model-catalog.json')).read_text())
# The firmware compiles these two schemas in (esp32/main/CMakeLists.txt); a
# request cannot change them at run time.
SCHEMAS = {
    "tools": json.loads((Path(__file__).with_name('demo-tools.json')).read_text()),
    "route": json.loads((Path(__file__).with_name('model-routes.json')).read_text()),
}
# OpenAI-compatible model ids, each bound to one firmware phase.
OPENAI_MODELS = {"needle3": "tools", "needle3-router": "route"}

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
        # The baseline identity, so a later WARN line is a comparison and not a guess.
        try:
            print("ATTACH_DEVICE %s" % self._dev_identity())
        except Exception:                        # pragma: no cover - diagnostic only
            pass

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

    def _dev_identity(self):
        """Identify the kernel node behind the path we were handed.

        Every 'the board stopped answering' investigation here has assumed firmware, and the
        failures are engine-independent (the accepted image reproduces them at the same case).
        The console is a USB-UART bridge on UART0, so it can drop off USB by itself while the
        ESP32 keeps running - which is what the thermal instrument showed: logs still streaming
        on the other leg, model open, heap and PSRAM CRC clean. Resolving the alias chain makes a
        stall readable as 'the console leg re-enumerated at case N'. Never raises: a diagnostic
        that breaks a rescue is worse than the blind spot it closes.
        """
        try:
            import os
            real = os.path.realpath(self.port)
            st = os.stat(real)
            return "path=%s real=%s rdev=%d.%d" % (
                self.port, real, os.major(st.st_rdev), os.minor(st.st_rdev))
        except Exception as exc:                       # pragma: no cover - diagnostic only
            return "path=%s identity_unavailable=%s" % (
                getattr(self, "port", "?"), type(exc).__name__)

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
        try:
            self.serial.close()
        except Exception:
            pass
        self._open(self.port, self.baud)
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline and self.serial.readline():
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

    def _line(self):
        # Preserve spaces in tokens. strip() corrupts the generated text.
        line = self.serial.readline().decode("utf-8", "replace").rstrip("\r\n")
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
            return METRICS.observe(self._complete_once(prompt, phase))
        except TimeoutError:
            if getattr(self, "port", None) is None:
                # Nothing to reconnect to: this Device was never attached (the
                # framing tests build a bare one to test response pairing). Without
                # this the rescue raised AttributeError out of _reconnect and hid
                # the TimeoutError the caller needed - measured, make test has been
                # red on exactly this since the rescue landed in run #389.
                raise
            # Which tty is actually behind the path? The console leg is a USB-UART bridge on
            # UART0, so it can leave USB on its own while the ESP32 keeps computing; printing the
            # resolved node turns 'the board hung at case 17' into 'the console leg re-enumerated
            # at case 17', which is a rig fault rather than a firmware one. Diagnostic only, and
            # it must never be able to break the rescue.
            try:
                print("WARN stalled_device_identity %s" % self._dev_identity())
            except Exception as _exc:                  # pragma: no cover
                print("WARN stalled_device_identity_unavailable %s" % type(_exc).__name__)
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
                result = METRICS.observe(self._complete_once(prompt, phase))
                result["retried"] = 1
                result["hard_reset"] = 1
                return result
            self.available = True
            result = METRICS.observe(self._complete_once(prompt, phase))
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
            prefill_ms = decode_ms = prefill_tps = decode_tps = prefill_tokens = None
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
                        prefill_tokens = metric(line, "tokens")
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
                "prefill_tokens": int(prefill_tokens) if prefill_tokens is not None else None,
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


class Metrics:
    """Firmware timings from `EVT prefill` / `EVT done`, as Prometheus text."""

    def __init__(self):
        self.lock = threading.Lock()
        self.counters = {"requests": 0, "failures": 0, "http_errors": 0,
                         "prefill_tokens": 0, "prefill_ms": 0.0,
                         "decode_tokens": 0, "decode_ms": 0.0}
        self.last_decode_tps = None
        self.last_prefill_tps = None

    def observe(self, result):
        with self.lock:
            c = self.counters
            c["requests"] += 1
            if not result.get("success"):
                c["failures"] += 1
            for key in ("prefill_tokens", "prefill_ms", "decode_tokens", "decode_ms"):
                if result.get(key) is not None:
                    c[key] += result[key]
            if result.get("decode_tps") is not None:
                self.last_decode_tps = result["decode_tps"]
            if result.get("prefill_tps") is not None:
                self.last_prefill_tps = result["prefill_tps"]
        return result

    def error(self):
        with self.lock:
            self.counters["http_errors"] += 1

    def render(self):
        with self.lock:
            c = dict(self.counters)
            last = (self.last_prefill_tps, self.last_decode_tps)
        rows = [
            ("needle_requests_total", "counter", "Device inference passes", c["requests"]),
            ("needle_request_failures_total", "counter", "Passes the firmware reported as failed", c["failures"]),
            ("needle_http_errors_total", "counter", "HTTP requests answered with an error status", c["http_errors"]),
            ("needle_prefill_tokens_total", "counter", "Prompt tokens prefilled on the device", c["prefill_tokens"]),
            ("needle_prefill_milliseconds_total", "counter", "Device prefill time from EVT prefill", c["prefill_ms"]),
            ("needle_decode_tokens_total", "counter", "Tokens generated on the device", c["decode_tokens"]),
            ("needle_decode_milliseconds_total", "counter", "Device decode time from EVT done", c["decode_ms"]),
        ]
        if last[0] is not None:
            rows.append(("needle_last_prefill_tokens_per_second", "gauge", "Prefill rate of the latest pass", last[0]))
        if last[1] is not None:
            rows.append(("needle_last_decode_tokens_per_second", "gauge", "Decode rate of the latest pass", last[1]))
        out = []
        for name, kind, help_text, value in rows:
            out += [f"# HELP {name} {help_text}", f"# TYPE {name} {kind}", f"{name} {value:g}"]
        return "\n".join(out) + "\n"


METRICS = Metrics()


class RequestError(ValueError):
    """An OpenAI-style request the device cannot serve (HTTP 400)."""

    def __init__(self, message, code="invalid_request"):
        super().__init__(message)
        self.code = code


def openai_models():
    return {"object": "list", "data": [
        {"id": model, "object": "model", "created": 0, "owned_by": "needle3-esp32",
         "phase": phase, "tools": [tool["name"] for tool in SCHEMAS[phase]]}
        for model, phase in OPENAI_MODELS.items()]}


def parse_chat_request(body):
    """Map a /v1/chat/completions body onto (phase, prompt, offered tool names)."""
    if not isinstance(body, dict):
        raise RequestError("body must be a JSON object")
    if body.get("stream"):
        raise RequestError("streaming is not supported by this device bridge", "unsupported")
    model = body.get("model") or "needle3"
    if model not in OPENAI_MODELS:
        raise RequestError(f"unknown model {model!r}; available: {', '.join(OPENAI_MODELS)}", "model_not_found")
    phase = OPENAI_MODELS[model]
    messages = body.get("messages")
    if not isinstance(messages, list) or not messages:
        raise RequestError("messages must be a nonempty list")
    for message in messages:
        if not isinstance(message, dict) or message.get("role") not in ("system", "developer", "user"):
            raise RequestError("only system and user messages are supported; the device keeps no "
                               "conversation state, so assistant and tool turns cannot be replayed", "unsupported")
    content = next((m.get("content") for m in reversed(messages) if m["role"] == "user"), None)
    if isinstance(content, list):
        content = "".join(part.get("text", "") for part in content
                          if isinstance(part, dict) and part.get("type") == "text")
    if not isinstance(content, str):
        raise RequestError("a user message with text content is required")
    known = [tool["name"] for tool in SCHEMAS[phase]]
    tools = body.get("tools")
    if tools is None:
        offered = known
    else:
        if not isinstance(tools, list) or not tools:
            raise RequestError("tools must be a nonempty list when given")
        offered = []
        for tool in tools:
            fn = tool.get("function") if isinstance(tool, dict) else None
            if not isinstance(fn, dict) or tool.get("type", "function") != "function" or not fn.get("name"):
                raise RequestError("each tool must be {\"type\": \"function\", \"function\": {\"name\": ...}}")
            offered.append(fn["name"])
        unknown = [name for name in offered if name not in known]
        if unknown:
            raise RequestError(
                f"tool(s) {', '.join(unknown)} are not in the {model} firmware schema. Tool schemas are "
                f"compiled into the firmware at build time and cannot be supplied per request; "
                f"available tools: {', '.join(known)}", "unsupported_tool")
    if body.get("tool_choice") not in (None, "auto", "required"):
        raise RequestError("tool_choice must be omitted, \"auto\" or \"required\"", "unsupported")
    return model, phase, content, offered


def chat_completion(model, result, offered):
    """Shape a device result as an OpenAI chat.completion."""
    calls = result.get("function_calls") or []
    outside = [call.get("name") for call in calls if call.get("name") not in offered]
    if outside:
        raise RuntimeError(f"model called tool(s) outside the requested set: {', '.join(outside)}")
    tool_calls = [{"id": f"call_{uuid.uuid4().hex[:24]}", "type": "function",
                   "function": {"name": call.get("name"),
                                "arguments": json.dumps(call.get("arguments") or {}, separators=(",", ":"))}}
                  for call in calls]
    prompt_tokens = result.get("prefill_tokens")
    completion_tokens = result.get("decode_tokens")
    usage = None
    if prompt_tokens is not None and completion_tokens is not None:
        usage = {"prompt_tokens": prompt_tokens, "completion_tokens": completion_tokens,
                 "total_tokens": prompt_tokens + completion_tokens}
    return {
        "id": f"chatcmpl-{uuid.uuid4().hex}",
        "object": "chat.completion",
        "created": int(time.time()),
        "model": model,
        "choices": [{
            "index": 0,
            "message": {"role": "assistant", "content": None if tool_calls else result.get("raw", ""),
                        "tool_calls": tool_calls or None},
            "finish_reason": "tool_calls" if tool_calls else "stop",
        }],
        "usage": usage,
        # Device-side detail: local tools already ran on the ESP32 in this request.
        "needle": {k: result.get(k) for k in (
            "phase", "results", "confidence", "prefill_ms", "prefill_tps",
            "decode_ms", "decode_tps", "latency_ms", "retried")},
    }


class Handler(BaseHTTPRequestHandler):
    device = None

    def setup(self):
        super().setup()
        self.connection.settimeout(10)

    def do_GET(self):
        if self.path == "/models":
            self._json(200, CATALOG)
            return
        if self.path == "/v1/models":
            self._json(200, openai_models())
            return
        if self.path == "/metrics":
            payload = METRICS.render().encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; version=0.0.4")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
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
        if self.path == "/v1/chat/completions":
            self._chat()
            return
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

    def _chat(self):
        def fail(status, message, code):
            self._json(status, {"error": {"message": message, "type": code, "code": code}})
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 65536:
                raise RequestError("request body must be 1..65536 bytes")
            try:
                body = json.loads(self.rfile.read(length))
            except (ValueError, UnicodeError) as exc:
                raise RequestError(f"invalid JSON: {exc}") from exc
            model, phase, prompt, offered = parse_chat_request(body)
            try:
                validate_prompt(prompt)
            except ValueError as exc:
                raise RequestError(str(exc)) from exc
            result = self.device.complete(prompt, phase=phase)
            if not result["success"]:
                fail(502, f"device reported {result.get('error')}", "device_error")
                return
            self._json(200, chat_completion(model, result, offered))
        except RequestError as exc:
            fail(400, str(exc), exc.code)
        except (RuntimeError, ValueError) as exc:
            fail(502, str(exc), "device_error")
        except TimeoutError as exc:
            fail(504, str(exc), "timeout")
        except (ConnectionError, serial.SerialException, OSError) as exc:
            fail(503, str(exc), "unavailable")

    def _json(self, status, data):
        if status >= 400:
            METRICS.error()
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
