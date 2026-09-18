#!/usr/bin/env python3
"""Render both social assets with VHS, and reject silent encoder failures."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
for command in ('vhs', 'ttyd', 'ffmpeg'):
    if not shutil.which(command):
        raise SystemExit(f'{command} is required; see demo/README.md')
outputs = [ROOT/'demo/needle3-router.mp4', ROOT/'demo/needle3-router.gif']
old = {p: p.stat().st_mtime_ns if p.exists() else None for p in outputs}
subprocess.run(['vhs', 'demo/needle3-dracula.tape'], cwd=ROOT, check=True, env=os.environ)
for path in outputs:
    if not path.exists() or path.stat().st_size < 1024 or path.stat().st_mtime_ns == old[path]:
        raise SystemExit(f'VHS did not produce {path.name}; try VHS 0.11.0 (tested version).')
    # Decode the complete output, so success is more than file existence.
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), '-f', 'null', '-'], check=True)
    print(f'{path}: {path.stat().st_size:,} bytes', flush=True)
