#!/usr/bin/env python3
"""Capture model routing and real second-pass tool execution on the ESP32."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
CASES = [
    ("translation", "Translate good morning into Spanish", "qwen", []),
    ("coding", "Write a Python function to deduplicate a list", "gpt_oss", []),
    ("architecture", "Design a secure architecture for a fleet of agent watches", "opus", []),
    ("status", "How much free memory does this device have?", "needle", [("get_status", {})]),
    ("sampling", "Sample telemetry every 5 seconds", "needle", [("set_sampling_interval", {"seconds": 5})]),
    ("timer", "Start a 60 second timer", "needle", [("set_timer", {"seconds": 60})]),
    ("batch", "Sample every 10 seconds and start a 30 second timer", "needle", [("set_sampling_interval", {"seconds": 10}), ("set_timer", {"seconds": 30})]),
]


def http(base, path, data=None):
    request = urllib.request.Request(base + path, data=json.dumps(data).encode() if data is not None else None,
                                     headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=600) as response:
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
    hashes = {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in
              ['tools/model-routes.json', 'tools/demo-tools.json', 'tools/model-catalog.json', 'model/needle3.cact']}
    doc = {'recorded_at': datetime.now(timezone.utc).isoformat(),
           'source': 'real ESP32-S3; host orchestrates two on-device inference passes',
           'hashes': hashes, 'catalog': http(args.url, '/models'),
           'before': http(args.url, '/health'), 'cases': []}
    for case_id, prompt, expected_model, expected in CASES:
        response = http(args.url, '/agent', {'input': prompt})
        expected_calls = [{'name': name, 'arguments': arguments} for name, arguments in expected]
        choice = (response.get('selected_model') or {}).get('key')
        execution = response.get('execution')
        row = {'id': case_id, 'input': prompt, 'expected_model': expected_model,
               'expected_calls': expected_calls,
               'route_match': choice == expected_model,
               'tools_match': (execution['function_calls'] if execution else []) == expected_calls,
               'response': response, 'state_after': http(args.url, '/state')}
        doc['cases'].append(row)
        save(args.out, doc)
        print(json.dumps({'id': case_id, 'selected': choice, 'route_match': row['route_match'],
                          'tools_match': row['tools_match'], 'outcome': response['outcome'],
                          'latency_ms': response['latency_ms']}), flush=True)
    until = time.monotonic() + 35
    while True:
        doc['after'] = http(args.url, '/state')
        if doc['after']['timer_status'] == 'expired' or time.monotonic() > until:
            break
        time.sleep(1)
    doc['verification'] = {
        'routes_match': all(row['route_match'] for row in doc['cases']),
        'tools_match': all(row['tools_match'] for row in doc['cases']),
        'requests_succeeded': all(row['response']['success'] for row in doc['cases']),
        'no_external_calls': all(not row['response']['remote_called'] for row in doc['cases']),
        'local_has_two_passes': all(row['response']['inference_passes'] == 2 for row in doc['cases'] if row['expected_model'] == 'needle'),
        'external_stops_at_selection': all(row['response']['execution'] is None for row in doc['cases'] if row['expected_model'] != 'needle'),
        'telemetry_progressed': doc['after']['samples'] > doc['before']['samples'],
        'sampling_interval_applied': doc['after']['sample_period_s'] == 10,
        'timer_expired': doc['after']['timer_status'] == 'expired' and doc['after']['timer_fired_count'] > doc['before']['timer_fired_count'],
    }
    save(args.out, doc)
    print(json.dumps(doc['verification']), flush=True)
    if not all(doc['verification'].values()):
        raise SystemExit('Hardware capture has mismatches; retained for review.')


if __name__ == '__main__':
    main()
