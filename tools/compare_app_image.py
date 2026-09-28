#!/usr/bin/env python3
"""Compare two ESP32 app images, ignoring build metadata.

ESP-IDF embeds build metadata in every app image: the esp_app_desc_t version
string (from `git describe`), compile time and date, and the ELF SHA-256, plus
a trailing checksum and image SHA-256 that cover those bytes. Two builds of the
same source therefore differ even when every code and data byte matches. This
tool masks exactly those fields and reports whether anything else differs.

Exit status: 0 when the images match outside the masked fields, 1 otherwise.
"""
import argparse
import struct
import sys
from pathlib import Path

HEADER = 24          # esp_image_header_t
SEGMENT_HEADER = 8   # esp_image_segment_header_t of the first (DROM) segment
DESC = HEADER + SEGMENT_HEADER
DESC_MAGIC = 0xABCD5432
# Offsets inside esp_app_desc_t (ESP-IDF v5.x).
MASKED = {
    "version": (16, 32),
    "time": (80, 16),
    "date": (96, 16),
    "app_elf_sha256": (144, 32),
}
TRAILER = 33         # checksum byte (after padding) + appended SHA-256


def describe(image: bytes) -> dict:
    magic, = struct.unpack_from("<I", image, DESC)
    if magic != DESC_MAGIC:
        raise SystemExit(f"app descriptor magic {magic:#x} != {DESC_MAGIC:#x}")
    field = lambda off, n: image[DESC + off:DESC + off + n].split(b"\0")[0].decode(errors="replace")
    return {
        "version": field(16, 32),
        "project": field(48, 32),
        "time": field(80, 16),
        "date": field(96, 16),
        "idf": field(112, 32),
    }


def masked(image: bytes) -> bytearray:
    out = bytearray(image)
    for off, n in MASKED.values():
        out[DESC + off:DESC + off + n] = bytes(n)
    out[-TRAILER:] = bytes(TRAILER)
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args()
    ref, cand = args.reference.read_bytes(), args.candidate.read_bytes()
    for name, image in (("reference", ref), ("candidate", cand)):
        print(f"{name}: {len(image)} bytes {describe(image)}")
    if len(ref) != len(cand):
        print("DIFFERENT: sizes differ")
        return 1
    diff = sum(1 for x, y in zip(masked(ref), masked(cand)) if x != y)
    raw = sum(1 for x, y in zip(ref, cand) if x != y)
    if diff:
        print(f"DIFFERENT: {diff} bytes differ outside build metadata ({raw} total)")
        return 1
    print(f"MATCH: identical outside build metadata ({raw} metadata bytes differ)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
