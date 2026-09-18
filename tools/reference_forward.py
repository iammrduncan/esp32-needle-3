#!/usr/bin/env python3
"""Independent NumPy reference for one Needle 3 token from a sliced .cact.

The first token exercises the quantized projections, mHC, attention, and
Monarch Hadamard MLP without needing a KV cache. It is a correctness check for
the embedded C forward pass, not an implementation intended for deployment.
"""

import argparse
import math
import subprocess
from pathlib import Path

import numpy as np

from slice_cact import load


def sigmoid(x):
    return 1 / (1 + np.exp(-np.clip(x, -60, 60)))


def norm(x, scale=None):
    y = x / np.sqrt(np.mean(x * x, axis=-1, keepdims=True) + 1e-6)
    return y if scale is None else y * (1 + scale)


def sinkhorn(x):
    x = x.astype(np.float64)
    for _ in range(20):
        x -= np.log(np.exp(x - x.max(axis=-1, keepdims=True)).sum(axis=-1, keepdims=True)) + x.max(axis=-1, keepdims=True)
        x -= np.log(np.exp(x - x.max(axis=-2, keepdims=True)).sum(axis=-2, keepdims=True)) + x.max(axis=-2, keepdims=True)
    return np.exp(x).astype(np.float32)


def fwht(x):
    x = x.copy()
    n = x.shape[-1]
    stride = 1
    while stride < n:
        pair = x.reshape(*x.shape[:-1], -1, 2 * stride)
        a = pair[..., :stride].copy()
        b = pair[..., stride:].copy()
        pair[..., :stride] = a + b
        pair[..., stride:] = a - b
        stride *= 2
    return x / np.sqrt(np.float32(n))


class Ref:
    def __init__(self, path):
        self.header, codebooks, self.records = load(Path(path))
        self.cb = np.frombuffer(codebooks, np.float32)
        self.cache = {}
        self.tokens = []
        self.qraw = {}
        self.kraw = {}
        self.vraw = {}
        self.keys = {}
        self.values = {}
        self.engram_raw = {}

    def tensor(self, index, rows=None):
        if rows is None and index in self.cache:
            return self.cache[index]
        record = self.records[index]
        if rows is not None:
            record = record.rows(list(rows))
        shape = record.shape
        if record.dtype == 1:
            out = np.frombuffer(record.data, np.float16).astype(np.float32).reshape(shape)
        elif record.dtype == 2:
            out = np.frombuffer(record.data, np.float32).copy().reshape(shape)
        elif record.dtype == 3:
            nrow, dim = shape
            group = record.group
            padded = (dim + group - 1) // group * group
            rowbytes = padded * record.bits // 8
            packed = np.frombuffer(record.data[:nrow * rowbytes], np.uint8).reshape(nrow, rowbytes)
            norms = np.frombuffer(record.data[nrow * rowbytes:], np.float16).astype(np.float32).reshape(nrow, padded // group)
            positions = np.arange(padded) * record.bits
            byte = positions >> 3
            shift = positions & 7
            lo = packed[:, byte].astype(np.uint16)
            hi = packed[:, np.minimum(byte + 1, rowbytes - 1)].astype(np.uint16)
            indices = ((lo | (hi << 8)) >> shift) & ((1 << record.bits) - 1)
            cb = {2: self.cb[:4], 3: self.cb[4:12], 4: self.cb[12:28]}[record.bits]
            rotated = cb[indices].reshape(nrow, -1, group) * norms[:, :, None]
            out = fwht(rotated).reshape(nrow, padded)[:, :dim]
        else:
            raise ValueError(record.dtype)
        if rows is None:
            self.cache[index] = out
        return out

    def forward_first(self, token, logits_rows):
        h = self.header
        depth, dm, lanes = h[10], h[7], h[15]
        qhd, vhd, nh, nkv = h[11], h[12], h[8], h[9]
        pos = len(self.tokens)
        self.tokens.append(token)
        x = self.tensor(0, [token])[0] * np.sqrt(np.float32(dm))
        streams = np.broadcast_to(x, (lanes, dm)).copy()
        base = 1 + depth * 27
        a_pre, a_post, a_res = [self.tensor(base + i) for i in range(3)]
        b_pre, b_post, b_res = [self.tensor(base + i) for i in range(3, 6)]
        phi_pre, phi_post, phi_res = [base + i for i in range(6, 9)]
        p1, p2 = self.tensor(base + 9).astype(np.int32), self.tensor(base + 10).astype(np.int32)

        engrams = []
        for site in range(h[31]):
            offset = base + 11 + site * 4
            slots, tables, sub = h[20], h[22], h[21]
            heads = tables // h[26]
            e = np.zeros(dm, np.float32)
            for order_index in range(h[26]):
                order = h[27 + order_index]
                if len(self.tokens) < order:
                    continue
                for head in range(heads):
                    table = order_index * heads + head
                    seed_heads = h[25] or heads
                    acc = (0x9E3779B9 * (order_index * seed_heads + head + 1)) & 0xffffffff
                    for back in range(order):
                        acc = ((acc ^ self.tokens[-1 - back]) * 0x01000193) & 0xffffffff
                    acc ^= acc >> 15
                    row = table * slots + acc % slots
                    e[table * sub:(table + 1) * sub] = self.tensor(offset, [row])[0]
            ek = self.tensor(offset + 1) @ e
            raw = self.tensor(offset + 2) @ e
            self.engram_raw.setdefault(site, []).append(raw)
            ev = np.zeros(dm, np.float32)
            taps = self.tensor(offset + 3)
            for j in range(h[23]):
                prior = pos - j * h[24]
                if prior >= 0:
                    ev += taps[j] * self.engram_raw[site][prior]
            engrams.append((ek, ev))

        for li in range(depth):
            nx = norm(streams.reshape(-1))
            selected = np.eye(lanes, dtype=np.float32)[li % lanes]
            pre = nx @ self.tensor(phi_pre, range(li * lanes, (li + 1) * lanes)).T
            post = nx @ self.tensor(phi_post, range(li * lanes, (li + 1) * lanes)).T
            res = nx @ self.tensor(phi_res, range(li * lanes * lanes, (li + 1) * lanes * lanes)).T
            hpre = sigmoid(a_pre[li] * pre + b_pre[li] + 8 * selected - 4)
            hpost = 2 * sigmoid(a_post[li] * post + b_post[li] - 4 * (1 - selected))
            hres = sinkhorn(a_res[li] * res.reshape(lanes, lanes) + b_res[li])
            u = hpre @ streams
            old = u.copy()
            block = 1 + li * 27
            for site, (ek, ev) in enumerate(engrams):
                if h[32 + site] == li:
                    u += sigmoid((norm(u) * norm(ek)).sum() / np.sqrt(dm)) * ev
            t = norm(u, self.tensor(block))
            q = self.tensor(block + 1) @ t
            k = self.tensor(block + 2) @ t
            v = self.tensor(block + 3) @ t
            self.qraw.setdefault(li, []).append(q)
            self.kraw.setdefault(li, []).append(k)
            self.vraw.setdefault(li, []).append(v)
            def tapped(history, tensor):
                taps = self.tensor(tensor)
                return sum((taps[j] * history[pos - j] for j in range(min(pos + 1, h[19]))))
            q = tapped(self.qraw[li], block + 4)
            k = tapped(self.kraw[li], block + 5)
            v = tapped(self.vraw[li], block + 6)
            q = norm(q.reshape(nh, qhd), self.tensor(block + 7))
            k = norm(k.reshape(nkv, qhd), self.tensor(block + 8))
            v = v.reshape(nkv, vhd)
            if pos:
                half = qhd // 2
                inv = 1 / (h[48] ** (np.arange(0, qhd, 2, dtype=np.float32) / qhd))
                angle = pos * inv
                cs, sn = np.cos(angle), np.sin(angle)
                def rope(z):
                    return np.concatenate([z[..., :half] * cs - z[..., half:] * sn,
                                           z[..., half:] * cs + z[..., :half] * sn], axis=-1)
                q, k = rope(q), rope(k)
            kmax = np.abs(k).max(axis=-1, keepdims=True) / 127
            kmax = np.where(kmax > 0, kmax, 1)
            k = np.rint(k / kmax).clip(-127, 127) * kmax
            vmax = np.abs(v).max(axis=-1, keepdims=True) / 127
            vmax = np.where(vmax > 0, vmax, 1)
            v = np.rint(v / vmax).clip(-127, 127) * vmax
            self.keys.setdefault(li, []).append(k)
            self.values.setdefault(li, []).append(v)
            attn = np.empty((nh, vhd), np.float32)
            for head in range(nh):
                kvhead = head // (nh // nkv)
                keys = np.stack([item[kvhead] for item in self.keys[li]])
                values = np.stack([item[kvhead] for item in self.values[li]])
                scores = keys @ q[head] / np.sqrt(np.float32(qhd))
                weights = np.exp(scores - scores.max())
                weights /= weights.sum()
                attn[head] = weights @ values
            attn = attn.reshape(-1)
            attn *= sigmoid(self.tensor(block + 9) @ t)
            attn = self.tensor(block + 10) @ attn
            u = u + sigmoid(self.tensor(block + 12)[0]) * norm(attn, self.tensor(block + 11))
            t = norm(u, self.tensor(block + 13))
            n = h[14]
            cond = t @ self.tensor(block + 25)
            cond = np.exp(cond - cond.max())
            cond /= cond.sum()
            scale = 1 + cond @ self.tensor(block + 26)
            z = np.pad(t, (0, n - dm))
            z = np.einsum('ij,ik,jl->kl', (z * self.tensor(block + 14)).reshape(32, 32),
                          self.tensor(block + 19), self.tensor(block + 20)).reshape(-1)[p1]
            z = z * self.tensor(block + 15) * scale + self.tensor(block + 16)
            z = z * sigmoid(z)
            z = np.einsum('ij,ik,jl->kl', z.reshape(32, 32), self.tensor(block + 21),
                          self.tensor(block + 22)).reshape(-1)[p2]
            z *= self.tensor(block + 17)
            z = np.einsum('ij,ik,jl->kl', z.reshape(32, 32), self.tensor(block + 23),
                          self.tensor(block + 24)).reshape(-1)
            u += (z * self.tensor(block + 18))[:dm]
            streams = hres @ streams + hpost[:, None] * (u - old)

        shared_end = base + 11
        final = shared_end + h[31] * 4
        hidden = norm(streams.mean(axis=0), self.tensor(final))
        logits = self.tensor(0, logits_rows) @ hidden
        return logits


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('archive')
    parser.add_argument('binary')
    parser.add_argument('--tokens', type=int, nargs='+', default=[100])
    parser.add_argument('--count', type=int, default=32)
    args = parser.parse_args()
    ref = Ref(args.archive)
    for token in args.tokens:
        expected = ref.forward_first(token, range(args.count))
    output = subprocess.check_output([args.binary, args.archive, 'logits', *map(str, args.tokens)], text=True)
    got = np.array([float(x) for x in output.split()[-ref.header[5]:][:args.count]], np.float32)
    error = np.max(np.abs(expected - got))
    print('max abs error:', error)
    for i in range(min(10, args.count)):
        print(f'{i}: reference={expected[i]:.6f} native={got[i]:.6f}')
    if error > 1e-2:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
