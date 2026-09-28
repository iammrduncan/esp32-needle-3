#!/usr/bin/env python3
"""Install changed package lists before Pi starts; keep user settings intact."""
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    agent = Path.home() / ".pi/agent"
    agent.mkdir(parents=True, exist_ok=True)
    settings_path = agent / "settings.json"
    settings = json.loads(settings_path.read_text())
    packages = settings.get("packages", [])
    if not isinstance(packages, list):
        raise ValueError("settings.packages must be an array")
    sources = [p if isinstance(p, str) else p["source"] for p in packages]
    if any(not isinstance(s, str) or not s or s.startswith("-") for s in sources):
        raise ValueError("Invalid package source")
    digest = hashlib.sha256(json.dumps(packages, sort_keys=True).encode()).hexdigest()
    stamp = agent / ".pod-pi-packages"
    if stamp.exists() and stamp.read_text() == digest:
        return
    try:
        for source in sources:
            subprocess.run(["pi", "install", source], cwd=Path.home(), check=True,
                           env={**os.environ, "GIT_TERMINAL_PROMPT": "0"})
    finally:
        # pi install modifies settings. Preserve filters and the exact desired list.
        current = json.loads(settings_path.read_text())
        current["packages"] = packages
        temp = settings_path.with_suffix(".tmp")
        temp.write_text(json.dumps(current, indent=2) + "\n")
        temp.replace(settings_path)
    stamp.write_text(digest)


if __name__ == "__main__":
    main()
