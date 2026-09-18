#!/usr/bin/env python3
"""HTTP bridge: inference and tool execution happen on the ESP32-S3."""

import argparse
import json
import re
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import serial


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
    def __init__(self, port, baud, boot_timeout, request_timeout, think):
        self.serial = serial.Serial(port, baudrate=baud, timeout=1, write_timeout=5, exclusive=True)
        self.lock = threading.Lock()
        self.request_timeout = request_timeout
        self.ready_line = None
        self.available = False
        self.think = think
        self._set_think()
        until = time.monotonic() + boot_timeout
        while time.monotonic() < until:
            line = self._line()
            if line:
                print(line, flush=True)
            if line.startswith("EVT  READY"):
                self.ready_line = line
                self._set_think()
            if line == f"EVT think={int(self.think)}":
                self.ready_line = self.ready_line or "attached to running firmware"
                self.available = True
                current = self.state()
                if current.get("schema") != "telemetry-router-v1":
                    raise RuntimeError("flash the telemetry router firmware before starting this bridge")
                return
            if line.startswith("ERR "):
                raise RuntimeError(f"ESP32 boot error: {line}")
        raise TimeoutError("ESP32 did not become ready; check serial port and firmware boot logs")

    def _line(self):
        # Preserve spaces in tokens. strip() corrupts the generated text.
        return self.serial.readline().decode("utf-8", "replace").rstrip("\r\n")

    def _set_think(self):
        self.serial.write(f"!think {int(self.think)}\n".encode())
        self.serial.flush()

    def _require_ready(self):
        if not self.available:
            raise ConnectionError("serial stream lost synchronization; wait for the board to finish, then restart the bridge")

    def state(self):
        with self.lock:
            self._require_ready()
            self.serial.write(b"!status\n")
            self.serial.flush()
            until = time.monotonic() + 5
            while time.monotonic() < until:
                line = self._line()
                if line.startswith("STATE "):
                    return json.loads(line[6:])
            self.available = False
            raise TimeoutError("ESP32 status request timed out")

    def complete(self, prompt):
        validate_prompt(prompt)
        with self.lock:
            self._require_ready()
            start = time.monotonic()
            self.serial.write(prompt.encode() + b"\n")
            self.serial.flush()
            output, calls, results = [], None, None
            prefill_ms = decode_ms = prefill_tps = decode_tps = None
            tokens = confidence = error = None
            done = False
            try:
                while time.monotonic() - start < self.request_timeout:
                    line = self._line()
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
            }


class Handler(BaseHTTPRequestHandler):
    device = None

    def setup(self):
        super().setup()
        self.connection.settimeout(10)

    def do_GET(self):
        if self.path not in ("/health", "/state"):
            self._json(404, {"error": "not found"})
            return
        try:
            state = self.device.state()
            self._json(200, {"ready": True, **state} if self.path == "/health" else state)
        except (TimeoutError, ConnectionError, serial.SerialException, OSError) as exc:
            self._json(503, {"ready": False, "error": str(exc)})

    def do_POST(self):
        if self.path != "/complete":
            self._json(404, {"error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 4096:
                raise ValueError("request body must be 1..4096 bytes")
            body = json.loads(self.rfile.read(length))
            if not isinstance(body, dict) or "input" not in body:
                raise ValueError('body must be an object with an "input" string')
            result = self.device.complete(body["input"])
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
    parser.add_argument("--boot-timeout", type=float, default=300)
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
