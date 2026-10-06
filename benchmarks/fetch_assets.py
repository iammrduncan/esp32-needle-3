#!/usr/bin/env python3
"""Download and verify the benchmark package's binary assets.

The firmware images, bootloaders, partition table and host `nd_dump` builds the
benchmarks compare are release assets, not Git content. `benchmarks/assets.json`
pins each file's SHA-256; this fetches any that are missing into
`benchmarks/assets/` and refuses a file whose hash does not match.

Set NEEDLE_ASSET_URL to a format string with {tag} and {file} (for example a
mirror, or file:///path/{file} for a local copy) to fetch from elsewhere; the
hashes are checked either way.
"""

import argparse
import hashlib
import json
import os
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "benchmarks/assets"
MANIFEST = ROOT / "benchmarks/assets.json"
# Executable assets (host engine builds) keep their mode after download.
EXECUTABLE_SUFFIX = "-nd_dump"


def _sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def ensure(path):
    """Return `path` after making sure it exists and matches its pinned hash.

    Paths outside benchmarks/assets/ are returned unchanged.
    """
    path = Path(path)
    if not path.is_absolute():
        path = ROOT / path
    if path.parent != ASSET_DIR:
        return path
    manifest = json.loads(MANIFEST.read_text())
    spec = manifest["files"].get(path.name)
    if spec is None:
        raise KeyError(f"{path.name} is not listed in {MANIFEST.relative_to(ROOT)}")
    if path.is_file() and _sha256(path) == spec["sha256"]:
        return path
    url = (os.environ.get("NEEDLE_ASSET_URL") or manifest["url"]).format(
        tag=manifest["release_tag"], file=path.name)
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".download")
    print(f"DOWNLOAD {url}", flush=True)
    urllib.request.urlretrieve(url, temporary)
    actual = _sha256(temporary)
    if actual != spec["sha256"]:
        temporary.unlink()
        raise ValueError(f"SHA mismatch for {url}: {actual} != {spec['sha256']}")
    if path.name.endswith(EXECUTABLE_SUFFIX):
        os.chmod(temporary, 0o755)
    temporary.replace(path)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    for name in manifest["files"]:
        print(f"OK {ensure(ASSET_DIR / name).relative_to(ROOT)}")


if __name__ == "__main__":
    main()
