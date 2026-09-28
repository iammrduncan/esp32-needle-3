import json
import threading
import unittest
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

from tools import serial_api
from tools.serial_api import Handler, Metrics
from test_serial_api import device


class OpenAIRouteTests(unittest.TestCase):
    """The /v1 routes and /metrics, served by the real handler over HTTP."""

    def serve(self, lines):
        serial_api.METRICS = Metrics()
        Handler.device = device(lines)
        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        return f"http://127.0.0.1:{server.server_address[1]}"

    def call(self, url, body=None):
        data = None if body is None else json.dumps(body).encode()
        request = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(request, timeout=5) as response:
                raw = response.read().decode()
                return response.status, raw if url.endswith("/metrics") else json.loads(raw)
        except urllib.error.HTTPError as error:
            return error.code, json.loads(error.read())

    TIMER = ['EVT prefill tokens=143 ms=20 tps=500', 'EVT done tokens=12 ms=100 tps=120',
             'JSON [{"name":"set_timer","arguments":{"seconds":60}}]',
             'RESULT [{"name":"set_timer","success":true,"state":{}}]', 'END']

    def test_models_lists_one_id_per_firmware_phase(self):
        base = self.serve([])
        status, body = self.call(base + "/v1/models")
        self.assertEqual(status, 200)
        self.assertEqual({m["id"]: m["phase"] for m in body["data"]}, {"needle3": "tools", "needle3-router": "route"})

    def test_tool_call_is_returned_in_openai_shape(self):
        base = self.serve(self.TIMER)
        status, body = self.call(base + "/v1/chat/completions", {
            "model": "needle3", "messages": [{"role": "user", "content": "Start a 60 second timer"}],
            "tools": [{"type": "function", "function": {"name": "set_timer", "parameters": {}}}]})
        self.assertEqual(status, 200)
        choice = body["choices"][0]
        self.assertEqual(choice["finish_reason"], "tool_calls")
        call = choice["message"]["tool_calls"][0]
        self.assertEqual(call["function"]["name"], "set_timer")
        self.assertEqual(json.loads(call["function"]["arguments"]), {"seconds": 60})
        self.assertEqual(body["usage"], {"prompt_tokens": 143, "completion_tokens": 12, "total_tokens": 155})
        self.assertEqual(Handler.device.serial.written, [b"Start a 60 second timer\n"])

    def test_router_model_uses_the_route_phase(self):
        base = self.serve(['EVT done tokens=4 ms=40 tps=100',
                           'JSON [{"name":"write_code","arguments":{}}]', 'RESULT [{"success":true}]', 'END'])
        status, body = self.call(base + "/v1/chat/completions", {
            "model": "needle3-router", "messages": [{"role": "user", "content": "Write a Python function"}]})
        self.assertEqual(status, 200)
        self.assertEqual(body["choices"][0]["message"]["tool_calls"][0]["function"]["name"], "write_code")
        self.assertEqual(Handler.device.serial.written, [b"!route Write a Python function\n"])

    def test_unknown_tool_is_rejected_before_the_device_is_asked(self):
        base = self.serve(self.TIMER)
        status, body = self.call(base + "/v1/chat/completions", {
            "messages": [{"role": "user", "content": "What is the weather?"}],
            "tools": [{"type": "function", "function": {"name": "get_weather"}}]})
        self.assertEqual(status, 400)
        self.assertEqual(body["error"]["code"], "unsupported_tool")
        self.assertIn("compiled into the firmware", body["error"]["message"])
        self.assertEqual(Handler.device.serial.written, [])

    def test_call_outside_the_offered_subset_is_an_error(self):
        base = self.serve(self.TIMER)
        status, body = self.call(base + "/v1/chat/completions", {
            "messages": [{"role": "user", "content": "Start a 60 second timer"}],
            "tools": [{"type": "function", "function": {"name": "get_status"}}]})
        self.assertEqual(status, 502)
        self.assertIn("outside the requested set", body["error"]["message"])

    def test_multi_turn_and_streaming_are_refused(self):
        base = self.serve([])
        for body in ({"messages": [{"role": "user", "content": "a"}, {"role": "assistant", "content": "b"}]},
                     {"stream": True, "messages": [{"role": "user", "content": "a"}]},
                     {"model": "gpt-4", "messages": [{"role": "user", "content": "a"}]}):
            with self.subTest(body=body):
                status, _ = self.call(base + "/v1/chat/completions", body)
                self.assertEqual(status, 400)

    def test_no_call_finishes_with_stop(self):
        base = self.serve(['EVT done tokens=2 ms=20 tps=100', 'JSON []', 'RESULT []', 'END'])
        status, body = self.call(base + "/v1/chat/completions",
                                 {"messages": [{"role": "user", "content": "Tell me a joke"}]})
        self.assertEqual(status, 200)
        self.assertEqual(body["choices"][0]["finish_reason"], "stop")
        self.assertIsNone(body["choices"][0]["message"]["tool_calls"])

    def test_metrics_report_firmware_timings(self):
        base = self.serve(self.TIMER)
        self.call(base + "/v1/chat/completions", {"messages": [{"role": "user", "content": "Start a 60 second timer"}]})
        status, text = self.call(base + "/metrics")
        self.assertEqual(status, 200)
        lines = dict(line.rsplit(" ", 1) for line in text.splitlines() if not line.startswith("#"))
        self.assertEqual(lines["needle_requests_total"], "1")
        self.assertEqual(lines["needle_prefill_tokens_total"], "143")
        self.assertEqual(lines["needle_decode_tokens_total"], "12")
        self.assertEqual(lines["needle_decode_milliseconds_total"], "100")
        self.assertEqual(lines["needle_last_decode_tokens_per_second"], "120")


if __name__ == "__main__":
    unittest.main()
