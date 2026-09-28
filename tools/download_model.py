#!/usr/bin/env python3
"""Fetch the pinned public archive, verify it, and slice the requested depth.

The depth comes from --layers, else IE_PARAM_LAYERS, else the manifest default
(8). Supported depths are the ones with a recorded output hash in
model/manifest.json (2-8); the result is written to model/needle3.cact, which
is what `make flash` writes to the model partition.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as file:
        return hashlib.file_digest(file, 'sha256').hexdigest()


def verify(path, expected):
    if not path.is_file() or digest(path) != expected:
        raise SystemExit(f'{path}: missing or SHA-256 mismatch')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--layers', type=int, default=None)
    args = parser.parse_args()
    manifest = json.loads((ROOT/'model/manifest.json').read_text())
    layers = args.layers or int(os.environ.get('IE_PARAM_LAYERS') or manifest['default_layers'])
    outputs = manifest['outputs']
    if str(layers) not in outputs:
        raise SystemExit(f'unsupported depth {layers}; supported: {", ".join(sorted(outputs, key=int))}')
    expected = outputs[str(layers)]['sha256']
    target = ROOT/'model/needle3.cact'
    if target.exists() and digest(target) == expected:
        print(f'{target}: verified ({layers} layers)'); return
    if args.verify_only:
        verify(target, expected)
    source = ROOT/'model/needle3-full.cact'
    if not source.exists() or digest(source) != manifest['source_sha256']:
        url = f"https://huggingface.co/{manifest['repository']}/resolve/{manifest['revision']}/{manifest['source_file']}"
        print(f'Downloading {url}', flush=True)
        tmp = source.with_suffix('.download')
        urllib.request.urlretrieve(url, tmp)
        verify(tmp, manifest['source_sha256'])
        tmp.replace(source)
    subprocess.run([sys.executable, str(ROOT/'tools/slice_cact.py'), str(source), str(target),
                    '--layers', str(layers), '--context', str(manifest['context'])], check=True)
    verify(target, expected)
    print(f'{target}: verified ({layers} layers)')


if __name__ == '__main__':
    main()
