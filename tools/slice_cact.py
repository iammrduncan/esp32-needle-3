#!/usr/bin/env python3
"""Slice a published Needle 3 .cact archive to an endpoint-preserving rung.

This copies already quantized tensor bytes. It needs only the Python standard
library and does not download or requantize model weights.
"""

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path


TAG = 0x05E12A84
HEADER = struct.Struct("<48If")
RECORD = struct.Struct("<BBHIIIIQQII")
ALIGN = 64


@dataclass
class Tensor:
    dtype: int
    shape: tuple[int, ...]
    group: int
    bits: int
    data: bytes

    def rows(self, indices: list[int], axis: int = 0) -> "Tensor":
        shape = list(self.shape)
        if axis >= len(shape):
            raise ValueError(f"axis {axis} invalid for {shape}")
        if any(i < 0 or i >= shape[axis] for i in indices):
            raise ValueError(f"row selection out of bounds for {shape}")
        if self.dtype == 3:
            if axis != 0 or len(shape) != 2:
                raise ValueError("CQ selection must be along matrix rows")
            padded = (shape[1] + self.group - 1) // self.group * self.group
            bits = 2 if self.bits == 5 else self.bits
            packed_row = padded * bits // 8
            norm_row = padded // self.group * 2
            n_packed = shape[0] * packed_row
            packed = self.data[:n_packed]
            norms = self.data[n_packed:]
            if len(norms) != shape[0] * norm_row:
                raise ValueError("bad CQ tensor length")
            data = b"".join(packed[i * packed_row:(i + 1) * packed_row] for i in indices)
            data += b"".join(norms[i * norm_row:(i + 1) * norm_row] for i in indices)
        elif self.dtype in (1, 2):
            width = 2 if self.dtype == 1 else 4
            outer = 1
            for x in shape[:axis]:
                outer *= x
            inner = width
            for x in shape[axis + 1:]:
                inner *= x
            block = shape[axis] * inner
            data = b"".join(
                self.data[o * block + i * inner:o * block + (i + 1) * inner]
                for o in range(outer) for i in indices
            )
        else:
            raise ValueError(f"unsupported sliced tensor dtype {self.dtype}")
        shape[axis] = len(indices)
        return Tensor(self.dtype, tuple(shape), self.group, self.bits, data)


def load(path: Path) -> tuple[list[int | float], bytes, list[Tensor]]:
    raw = path.read_bytes()
    header = list(HEADER.unpack_from(raw))
    if header[0] != TAG:
        raise ValueError("not a Needle 3 archive")
    cb_end = HEADER.size + header[2] * 4
    codebooks = raw[HEADER.size:cb_end]
    tensors = []
    for i in range(header[1]):
        record = RECORD.unpack_from(raw, cb_end + i * RECORD.size)
        dtype, ndim = record[:2]
        shape = tuple(record[3:3 + ndim])
        offset, length, group, bits = record[7:]
        if offset + length > len(raw):
            raise ValueError(f"tensor {i} exceeds archive")
        tensors.append(Tensor(dtype, shape, group, bits, raw[offset:offset + length]))
    return header, codebooks, tensors


def layer_order(count: int) -> list[int]:
    if count < 2:
        raise ValueError("model needs at least two layers")
    selected = [0, count - 1]
    order = selected.copy()
    while len(order) < count:
        gaps = [(b - a, a, b) for a, b in zip(sorted(selected), sorted(selected)[1:]) if b - a > 1]
        _, left, right = max(gaps, key=lambda item: (item[0], -item[1]))
        middle = (left + right) // 2
        selected.append(middle)
        order.append(middle)
    return order


def slice_model(header: list[int | float], tensors: list[Tensor], depth: int) -> tuple[list[int | float], list[Tensor]]:
    full = int(header[10])
    if not 2 <= depth <= full:
        raise ValueError(f"depth must be 2..{full}")
    selected = sorted(layer_order(full)[:depth])
    original_sites = list(header[32:32 + int(header[31])])
    block_size = 27 if header[19] else 24
    block_end = 1 + full * block_size
    shared_end = block_end + 11
    if tensors[block_end + 9].dtype != 2:
        raise ValueError("unexpected shared tensor layout")
    out = [tensors[0]]
    for layer in selected:
        out.extend(tensors[1 + layer * block_size:1 + (layer + 1) * block_size])
    # mHC parameters: six layer-major FP16 tensors, three CQ matrices with
    # `lanes` rows per layer, then two Hadamard permutations.
    lanes = int(header[15])
    for t in tensors[block_end:block_end + 6]:
        out.append(t.rows(selected))
    for t in tensors[block_end + 6:block_end + 9]:
        rows_per_layer = t.shape[0] // full
        if rows_per_layer * full != t.shape[0]:
            raise ValueError("mHC matrix is not layer-major")
        rows = [layer * rows_per_layer + row for layer in selected for row in range(rows_per_layer)]
        out.append(t.rows(rows))
    out.extend(tensors[block_end + 9:shared_end])
    kept_sites = [site for site in original_sites if site in selected]
    remap = {layer: i for i, layer in enumerate(selected)}
    for index, site in enumerate(original_sites):
        if site in remap:
            out.extend(tensors[shared_end + index * 4:shared_end + (index + 1) * 4])
    head_start = shared_end + len(original_sites) * 4
    out.append(tensors[head_start])  # final norm
    cursor = head_start + 1
    if tensors[cursor].dtype == 1 and tensors[cursor].shape == (1,):
        out.append(tensors[cursor])  # heads manifest
        cursor += 1
        probe_rows = [layer * lanes + lane for layer in [0] + [i + 1 for i in selected] for lane in range(lanes)]
        gain_rows = [0] + [i + 1 for i in selected]
        while cursor < len(tensors) - 1:
            out.append(tensors[cursor].rows(probe_rows))
            out.append(tensors[cursor + 1].rows(gain_rows))
            out.append(tensors[cursor + 2])
            out.append(tensors[cursor + 3].rows(gain_rows, axis=1))
            out.extend(tensors[cursor + 4:cursor + 6])
            cursor += 6
            # Router heads carry one extra calibration tensor.
            if cursor < len(tensors) - 1 and tensors[cursor].shape == (3,) and tensors[cursor].dtype == 1:
                out.append(tensors[cursor])
                cursor += 1
    if cursor != len(tensors) - 1 or tensors[-1].dtype != 4:
        raise ValueError("unexpected head/tokenizer layout")
    out.append(tensors[-1])
    header = header.copy()
    header[1] = len(out)
    header[10] = depth
    mask = int(header[17]) | int(header[18]) << 32
    new_mask = sum((1 << new) for new, old in enumerate(selected) if mask & (1 << old))
    header[17], header[18] = new_mask & 0xFFFFFFFF, new_mask >> 32
    header[31] = len(kept_sites)
    header[32:48] = [remap[site] for site in kept_sites] + [0] * (16 - len(kept_sites))
    return header, out


def save(path: Path, header: list[int | float], codebooks: bytes, tensors: list[Tensor]) -> None:
    start = HEADER.size + len(codebooks) + len(tensors) * RECORD.size
    directory = bytearray()
    body = bytearray()
    for t in tensors:
        offset = (start + len(body) + ALIGN - 1) & -ALIGN
        body.extend(b"\0" * (offset - start - len(body)))
        shape = list(t.shape) + [0] * (4 - len(t.shape))
        directory.extend(RECORD.pack(t.dtype, len(t.shape), 0, *shape, offset, len(t.data), t.group, t.bits))
        body.extend(t.data)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(HEADER.pack(*header) + codebooks + directory + body)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("--layers", type=int, default=2)
    parser.add_argument("--context", type=int, default=None,
                        help="cap the runtime context (useful for an ESP32 KV cache)")
    args = parser.parse_args()
    header, codebooks, tensors = load(args.source)
    sliced_header, sliced = slice_model(header, tensors, args.layers)
    if args.context is not None:
        if not 1 <= args.context <= sliced_header[13]:
            parser.error(f"--context must be 1..{sliced_header[13]}")
        sliced_header[13] = args.context
        sliced_header[16] = min(sliced_header[16], args.context)
    save(args.target, sliced_header, codebooks, sliced)
    print(f"wrote {args.target}: {args.target.stat().st_size:,} bytes, {len(sliced)} tensors, {args.layers} layers")


if __name__ == "__main__":
    main()
