#!/usr/bin/env python3
"""Capture reproducible hardware evidence; never synthesizes model responses."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
CASES = [
    ("status", "How much free memory does this device have?", [("get_status", {})]),
    ("sampling", "Sample telemetry every 5 seconds", [("set_sampling_interval", {"seconds": 5})]),
    ("timer", "Start a 60 second timer", [("set_timer", {"seconds": 60})]),
    ("batch", "Sample every 10 seconds and start a 30 second timer", [("set_sampling_interval", {"seconds": 10}), ("set_timer", {"seconds": 30})]),
    ("status_after", "Show device status", [("get_status", {})]),
    ("unsupported", "What is the weather in Tokyo?", []),
]


def http(base, path, data=None):
    request = urllib.request.Request(base + path, data=json.dumps(data).encode() if data is not None else None,
                                     headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=300) as response:
        return json.load(response)


def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix('.tmp')
    tmp.write_text(json.dumps(data, indent=2) + '\n')
    tmp.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8081')
    parser.add_argument('--out', type=Path, default=ROOT/'demo/recording.json')
    args = parser.parse_args()
    doc = {'recorded_at': datetime.now(timezone.utc).isoformat(), 'source': 'real ESP32-S3 UART via local HTTP bridge',
           'schema_sha256': hashlib.sha256((ROOT/'tools/demo-tools.json').read_bytes()).hexdigest(),
           'model_sha256': hashlib.sha256((ROOT/'model/needle3.cact').read_bytes()).hexdigest(),
           'before': http(args.url, '/health'), 'cases': []}
    for case_id, prompt, expected in CASES:
        response = http(args.url, '/complete', {'input': prompt})
        expected_calls = [{'name': name, 'arguments': arguments} for name, arguments in expected]
        row = {'id': case_id, 'input': prompt, 'expected_calls': expected_calls,
               'exact_match': response.get('success') and response.get('function_calls') == expected_calls,
               'response': response, 'state_after': http(args.url, '/state')}
        doc['cases'].append(row)
        save(args.out, doc)
        print(json.dumps({'id': case_id, 'match': row['exact_match'], 'calls': response.get('function_calls'),
                          'latency_ms': response.get('latency_ms')}), flush=True)
    # Prove callbacks continue independently of the model's blocking request loop.
    until = time.monotonic() + 35
    while True:
        doc['after'] = http(args.url, '/state')
        if doc['after']['timer_status'] == 'expired' or time.monotonic() > until:
            break
        time.sleep(1)
    doc['verification'] = {
        'supported_cases_match': all(row['exact_match'] for row in doc['cases'] if row['id'] != 'unsupported'),
        'telemetry_progressed': doc['after']['samples'] > doc['before']['samples'],
        'sampling_interval_applied': doc['after']['sample_period_s'] == 10,
        'timer_expired': doc['after']['timer_status'] == 'expired' and doc['after']['timer_fired_count'] > doc['before']['timer_fired_count'],
        'unsupported_rejected': doc['cases'][-1]['exact_match'],
    }
    save(args.out, doc)
    print(json.dumps(doc['verification']), flush=True)
    if not all(v for k, v in doc['verification'].items() if k != 'unsupported_rejected'):
        raise SystemExit('Hardware verification failed; recording retained for diagnosis.')


if __name__ == '__main__':
    main()
