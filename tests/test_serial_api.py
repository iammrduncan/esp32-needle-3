import json
import threading
import unittest
from collections import deque

from tools.serial_api import Device, validate_prompt


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
    d.lock = threading.Lock()
    d.available = True
    d.request_timeout = timeout
    d.think = False
    return d


class ProtocolTests(unittest.TestCase):
    def test_multi_call_results_are_consumed_through_end(self):
        calls = [{'name': 'get_status', 'arguments': {}}, {'name': 'set_timer', 'arguments': {'seconds': 60}}]
        results = [{'name': c['name'], 'success': True, 'state': {}} for c in calls]
        d = device(['TOK  leading ', 'EVT prefill tokens=10 ms=20 tps=500',
                    'EVT done tokens=20 ms=100 tps=200', 'JSON ' + json.dumps(calls),
                    'RESULT ' + json.dumps(results), 'END', 'STATE {"samples":7}'])
        response = d.complete('Start a timer and show status')
        self.assertTrue(response['success'])
        self.assertEqual(response['raw'], ' leading ')
        self.assertEqual(response['function_calls'], calls)
        self.assertEqual(len(response['results']), 2)
        self.assertEqual(d.state()['samples'], 7)

    def test_firmware_error_is_drained_before_next_request(self):
        d = device(['ERR incomplete_call', 'END', 'STATE {"samples":8}'])
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
