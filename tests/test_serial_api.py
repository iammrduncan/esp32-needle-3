import contextlib
import io
import json
import threading
import time
import unittest
from collections import deque
from pathlib import Path
from unittest import mock

from tools import serial_api
from tools.serial_api import CATALOG, Device, validate_prompt

# What this firmware reports for "!status"; tests vary it per case.
STATE = {"device": "esp32-s3", "schema": "agent-watch-v2", "layers": 8}


class Serial:
    def __init__(self, lines):
        self.lines = deque((line + '\n').encode() for line in lines)
        self.written = []

    def readline(self):
        return self.lines.popleft() if self.lines else b''

    def write(self, data):
        self.written.append(data)

    def flush(self):
        pass


def device(lines, timeout=0.05):
    d = Device.__new__(Device)
    d.serial = Serial(lines)
    d.lock = threading.RLock()
    d.available = True
    d.request_timeout = timeout
    d.think = False
    return d


class Port:
    """Enough of pyserial.Serial to exercise the attach path.

    Talks back the way the firmware does - "!think N" earns its ack, "!status"
    earns a STATE frame - but only once it is idle, because the console echoes
    and answers probes even while it is still working. That echo is the whole
    hazard these tests are about. The chip resets if the control lines move
    around the open, so the open is pinned down here too.
    """

    def __init__(self, lines=(), busy=0.0, state=STATE):
        self.q = deque((line + "\n").encode() for line in lines)
        self.written = []
        self.busy = busy          # seconds of priming left when we arrive
        self.state = state        # what this firmware answers for "!status"
        self.dtr = True           # pyserial's own default: the thing to guard
        self.rts = True
        self.port = self.baudrate = self.timeout = None
        self.write_timeout = self.exclusive = None
        self._idle_after = 0.0

    def open(self):
        self._idle_after = time.monotonic() + self.busy

    def close(self):
        pass

    def reset_input_buffer(self):
        self.q.clear()
        self._idle_after = time.monotonic() + self.busy

    def readline(self):
        if self.q:
            return self.q.popleft()
        if time.monotonic() < self._idle_after or not self.written:
            return b""
        # A real readline() never blocks here (timeout=1) and never returns a
        # second reply for one write, so the probe is consumed as it is answered.
        last = self.written.pop()     # the console consumes what it echoes
        if last.startswith(b"!status"):
            return b"STATE " + json.dumps(self.state).encode() + b"\n"
        if last.startswith(b"!think"):
            return last.strip() + b"\n"
        return b"ERR unknown_command\n"

    def write(self, data):
        self.written.append(data)

    def flush(self):
        pass


class AttachTests(unittest.TestCase):
    """Attaching must survive a board that is still warming its caches.

    After a reset the firmware spends ~2 min priming two model caches, and the
    console echoes anything written to it while it works. A handshake that
    treats those echoed replies as boot errors aborts the run; one that leaves
    them queued corrupts the very next request. Both aborted the first
    three-board baseline batch.
    """

    LINES = ()
    BUSY = 0.0
    STATE = STATE

    def setUp(self):
        self.ports = []

        def factory(*args, **kwargs):
            port = Port(self.LINES, busy=self.BUSY, state=self.STATE)
            self.ports.append(port)
            return port

        patcher = mock.patch.object(serial_api.serial, "Serial", factory)
        patcher.start()
        self.addCleanup(patcher.stop)

    def attach(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            dev = Device("/dev/null", 115200, 2, 0.05, False)
        return dev, out.getvalue()

    def test_open_does_not_put_the_board_in_reset(self):
        dev, _ = self.attach()
        self.assertTrue(dev.available)
        for port in self.ports:
            self.assertFalse(port.dtr, "DTR must be low across the open")
            self.assertFalse(port.rts, "RTS must be low across the open")

    def test_idle_board_keeps_its_startup_log(self):
        self.LINES = ["EVT bench tokens=6 ms=2220 ms_per_tok=370 tps=2.703",
                      "EVT prof proj2bit   210.0 ms  56.8%",
                      "EVT  READY"]
        dev, log = self.attach()
        self.assertTrue(dev.available)
        self.assertIn("proj2bit", log)        # bench.py parses the profile
        self.assertIn("ms_per_tok=370", log)  # and the boot bench

    def test_busy_board_is_attached_after_a_quiet_reattach(self):
        self.BUSY = 0.4                       # still priming when we arrive
        self.LINES = ["EVT priming 8/143  6%  elapsed=3s  eta=43s"]
        dev, log = self.attach()
        self.assertTrue(dev.available)
        self.assertGreater(len(self.ports), 1, "busy board must be reattached")
        # Reconnected, so the console is empty for the first case, and the only
        # thing written since is the status we actually needed.
        # The "!status" is popped as it is answered, like a console consuming
        # what it echoed, so nothing is left queued for the first case.
        self.assertEqual(self.ports[-1].written, [])
        self.assertEqual(self.ports[-1].q, deque())

    def test_echoed_probe_output_is_not_handed_on_as_boot_output(self):
        # What a mid-priming board prints: progress, then the firmware answering
        # echoed probes, then (only later) the real boot bench. The log is
        # interleaved at the first echo, so everything after it - including a
        # perfectly ordinary boot-bench line - is unsafe to parse.
        self.BUSY = 0.4
        self.LINES = ["EVT priming 8/143", "ERR unknown_command", "END",
                      "EVT bench tokens=6 ms=2220 ms_per_tok=370 tps=2.703"]
        dev, log = self.attach()
        self.assertTrue(dev.available)
        self.assertIn(serial_api._NOISY_LOG, log)
        self.assertNotIn("ms_per_tok", log)   # nothing after the WARN is echoed

    def test_echoed_state_is_not_the_answer_to_a_later_request(self):
        # A "!status" written while busy is echoed, so the firmware answers it
        # inside the next reply. The frame we are owed is the one that arrives
        # after our ask, not the stale one already queued.
        self.BUSY = 0.4
        dev, _ = self.attach()
        self.ports[-1].q.append(
            (b"STATE " + json.dumps({**self.STATE, "samples": 3}).encode() + b"\n"))
        self.assertEqual(dev.state()["samples"], 3)

    def test_wrong_schema_is_still_rejected(self):
        self.STATE = {**STATE, "schema": "some-other-firmware"}
        with self.assertRaises(RuntimeError):
            self.attach()


class ProtocolTests(unittest.TestCase):
    def test_catalog_maps_each_firmware_route_to_one_model(self):
        routes = json.loads((Path(__file__).resolve().parents[1]/'tools/model-routes.json').read_text())
        names = [name for model in CATALOG.values() for name in model['route_tools']]
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(set(names), {tool['name'] for tool in routes})

    def test_external_choice_stops_after_one_inference(self):
        for model in ('qwen', 'gpt_oss', 'opus'):
            calls = [{'name': CATALOG[model]['route_tools'][0], 'arguments': {}}]
            d = device(['EVT done tokens=10 ms=100 tps=100', 'JSON ' + json.dumps(calls),
                        'RESULT [{"success":true}]', 'END'])
            response = d.agent('Help with my task')
            self.assertEqual(response['selected_model']['key'], model)
            self.assertEqual(response['outcome'], 'external_selected')
            self.assertIsNone(response['execution'])
            self.assertFalse(response['remote_called'])
            self.assertEqual(d.serial.written, [b'!route Help with my task\n'])

    def test_local_choice_runs_original_task_under_tool_schema(self):
        d = device(['EVT done tokens=10 ms=100 tps=100',
                    'JSON [{"name":"set_timer","arguments":{}}]',
                    'RESULT [{"success":true}]', 'END',
                    'EVT done tokens=10 ms=100 tps=100',
                    'JSON [{"name":"set_timer","arguments":{"seconds":60}}]',
                    'RESULT [{"success":true,"state":{"timer_status":"running"}}]', 'END'])
        response = d.agent('Start a 60 second timer')
        self.assertEqual(response['outcome'], 'local_executed')
        self.assertEqual(response['inference_passes'], 2)
        self.assertEqual(response['execution']['function_calls'][0]['name'], 'set_timer')
        self.assertEqual(d.serial.written, [b'!route Start a 60 second timer\n', b'Start a 60 second timer\n'])

    def test_invalid_route_does_not_execute_tools(self):
        d = device(['ERR invalid_route', 'END'])
        response = d.agent('some task')
        self.assertFalse(response['success'])
        self.assertEqual(len(d.serial.written), 1)

    def test_multi_call_results_are_consumed_through_end(self):
        calls = [{'name': 'get_status', 'arguments': {}}, {'name': 'set_timer', 'arguments': {'seconds': 60}}]
        results = [{'name': c['name'], 'success': True, 'state': {}} for c in calls]
        d = device(['TOK  leading ', 'EVT prefill tokens=10 ms=20 tps=500',
                    'EVT done tokens=20 ms=100 tps=200', 'JSON ' + json.dumps(calls),
                    'RESULT ' + json.dumps(results), 'END', 'STATE ' + json.dumps({**STATE, "samples": 7})])
        response = d.complete('Start a timer and show status')
        self.assertTrue(response['success'])
        self.assertEqual(response['raw'], ' leading ')
        self.assertEqual(response['function_calls'], calls)
        self.assertEqual(len(response['results']), 2)
        self.assertEqual(d.state()['samples'], 7)

    def test_firmware_error_is_drained_before_next_request(self):
        d = device(['ERR incomplete_call', 'END', 'STATE ' + json.dumps({**STATE, "samples": 8})])
        self.assertFalse(d.complete('test')['success'])
        self.assertEqual(d.state()['samples'], 8)

    def test_no_json_is_not_reported_as_a_successful_empty_route(self):
        d = device(['EVT done tokens=1 ms=100 tps=10', 'END'])
        self.assertEqual(d.complete('test')['error'], 'incomplete_firmware_response')

    def test_timeout_prevents_late_response_being_assigned_to_next_prompt(self):
        d = device([], timeout=0.001)
        with self.assertRaises(TimeoutError):
            d.complete('test')
        with self.assertRaises(ConnectionError):
            d.complete('another prompt')
        self.assertEqual(len(d.serial.written), 1)

    def test_empty_call_can_be_valid(self):
        d = device(['EVT done tokens=2 ms=100 tps=20', 'JSON []', 'RESULT []', 'END'])
        self.assertTrue(d.complete('irrelevant')['success'])

    def test_control_commands_cannot_be_sent_as_inference(self):
        for value in ['!status', '  !think 1', 'hello\n!status', 'hello\r', 'a\x00b', '', '   ', None, 5, 'x'*256, 'é'*128]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_prompt(value)
        validate_prompt('Sample telemetry every 5 seconds')


if __name__ == '__main__':
    unittest.main()
