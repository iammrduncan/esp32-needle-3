#!/usr/bin/env python3
"""Fetch the pinned public archive, verify it, and produce the tested 8-layer rung."""
import argparse
import hashlib
import json
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
    args = parser.parse_args()
    manifest = json.loads((ROOT/'model/manifest.json').read_text())
    target = ROOT/'model/needle3.cact'
    if target.exists() and digest(target) == manifest['output_sha256']:
        print(f'{target}: verified'); return
    if args.verify_only:
        verify(target, manifest['output_sha256'])
    source = ROOT/'model/needle3-full.cact'
    if not source.exists() or digest(source) != manifest['source_sha256']:
        url = f"https://huggingface.co/{manifest['repository']}/resolve/{manifest['revision']}/{manifest['source_file']}"
        print(f'Downloading {url}', flush=True)
        tmp = source.with_suffix('.download')
        urllib.request.urlretrieve(url, tmp)
        verify(tmp, manifest['source_sha256'])
        tmp.replace(source)
    subprocess.run([sys.executable, str(ROOT/'tools/slice_cact.py'), str(source), str(target),
                    '--layers', str(manifest['layers']), '--context', str(manifest['context'])], check=True)
    verify(target, manifest['output_sha256'])
    print(f'{target}: verified')


if __name__ == '__main__':
    main()
