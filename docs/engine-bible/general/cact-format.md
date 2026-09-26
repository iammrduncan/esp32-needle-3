# The `.cact` model format and loader

Needle runs directly from one deployment archive. The format avoids a dynamic
graph loader, tensor names, post-load weight conversion, and a second full copy
of the model—properties that matter more on a microcontroller than a flexible
desktop serialization format.

## Physical layout

All multibyte values are little-endian.

```text
offset 0x000   196-byte fixed header (49 32-bit words; final word is float)
offset 0x0c4   codebooks: cb2[4], cb3[8], cb4[16] as float32
then           num_tensors × 44-byte directory records
then           tensor payloads, each aligned to 64 bytes
```

A directory record contains:

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 1 | dtype |
| 1 | 1 | rank |
| 2 | 2 | padding |
| 4 | 16 | four 32-bit shape fields |
| 20 | 8 | payload offset |
| 28 | 8 | payload length |
| 36 | 4 | quantization group size |
| 40 | 4 | bits per weight |

Supported dtype codes are FP16, FP32, Cactus-Quantized (CQ), and raw. Raw is
used for the embedded tokenizer.

## Header responsibilities

The header carries tensor count/codebook length plus architecture geometry:
vocabulary, widths, layer/head counts, context/window fields, Hadamard width,
mHC lanes, convolution taps, engram geometry and sites, n-gram orders, masks,
and RoPE theta. This lets one engine binary bind different supported rungs
without names.

Nameless positional binding is cheap but strict. The engine assumes a canonical
tensor order: embedding, per-layer blocks, shared mHC parameters, Hadamard
permutations, per-site engram tensors, final norm, optional heads, tokenizer.
Adding or removing a tensor requires updating the exporter/slicer and binder as
one versioned contract.

## Zero-copy validation

`nd_cact_open()`:

1. checks the 196-byte minimum;
2. decodes every header word without casting an untrusted/possibly unaligned
   mapped address;
3. checks the magic tag `0x05E12A84`;
4. rejects unsupported/inconsistent model geometry;
5. bounds-checks codebooks and the complete directory;
6. requires all 28 codebook entries.

`nd_cact_tensor()` decodes a record, and `nd_cact_data()` separately proves
`offset + length` lies inside the mapped archive. On ESP32, firmware also derives
the real model length from the last record before mapping. The current target
rejects archives whose final offset/length need nonzero high 32 bits.

The loader does not checksum the blob at runtime. Integrity is established by
the build/download path and manifest; production designs that need fault or
adversarial resistance should add signed/hash-verified storage or secure boot.

## CQ payload layout

For a CQ matrix, the reduction dimension is padded to a multiple of `group`.
Each row has:

- `in_pad * bits / 8` bytes of packed, LSB-first codebook indices;
- `in_pad / group` FP16 group norms, stored in a norm plane after all packed
  rows.

The shared float codebooks are pre-scaled for the transform convention. CQ2,
CQ3, and CQ4 use slices of 4, 8, and 16 values respectively.

The helper geometry is:

```text
in_pad   = ceil(input_width / group) * group
rowbytes = in_pad * bits / 8
ngroup   = in_pad / group
```

Assembly dispatch depends on this exact layout. The CQ2 kernel requires group
128, 32 packed bytes and 64 adjacent pairs per group, aligned rows, and ordinary
FP16 norms. The eligibility check is part of correctness, not optional overhead.

## Slicing without requantization

`tools/slice_cact.py` creates the tested eight-layer rung by copying existing
quantized bytes. It does not decode/retrain/requantize weights.

The slicer:

- chooses endpoint-preserving layers: first, last, then recursively fills the
  largest gaps;
- copies each selected per-layer tensor block;
- slices layer-major mHC scalars and the correct lane-row ranges from `phi_*`;
- retains shared Hadamard permutations;
- keeps only engram sites whose original layer survives and remaps their layer
  indices;
- slices compatible optional head state;
- retains the tied embedding and raw tokenizer;
- updates layer count, global mask, site count/list, and optional context cap;
- rewrites directory offsets with 64-byte alignment.

CQ row slicing copies packed rows and norm rows separately, preserving their two
planes. FP16/FP32 slicing computes the outer/inner strides for the selected axis.

## Pinned artifact

The manifest records:

```text
repository       Cactus-Compute/needle3
revision         9da75122d4ca11aa4a667281c9c8ba38a7eed679
source bytes     35,335,380
source SHA-256   c9d915eca282ed42d1a09b143b592adb4cc6744ffe2d294adf5cfc5548170c38
layers/context   8 / 384
output bytes     16,155,796
output SHA-256   bcf34a7aced455cfc0ee5c466e264211994a6d1e8dfd4bae8dee25e210305b16
```

`tools/download_model.py` accepts an already-valid output, otherwise downloads
the full archive at the exact revision to a temporary path, verifies it, runs
the slicer, then verifies the output. A moving upstream branch is never used as
the reproduction input.

## What worked

- mapping immutable tensor bytes directly from a dedicated flash partition;
- positional binding with strict geometry validation;
- copying only selected hot spans into PSRAM while retaining original offsets;
- reusing packed CQ bytes across C and assembly paths;
- independent host utilities for header/directory/row/GEMV inspection;
- NumPy reference reconstruction for a first-token cross-check.

## Failure modes and rules

- A valid tag does not prove every record is in bounds: validate each pointer.
- Tensor slot mistakes can yield plausible shapes and even small logit deltas;
  bind by named slot tables in code and gate full behavior.
- Slicing layer blocks without remapping mHC rows and engram sites produces a
  structurally valid but semantically wrong model.
- Do not infer the runtime cache allocation from a similarly named header
  field—inspect the engine binding.
- A copied tier must translate offsets consistently and fail safe to flash when
  allocation is unavailable.
- Expanded or alternate layouts need an explicit memory and byte-stream budget;
  the archive's compactness is part of the runtime design.

