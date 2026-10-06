# Matrix firmware: source and build reproducibility

The shipping firmware is the **matrix** image measured in the
[layer benchmark](benchmarks.md): 6.15 decode tok/s at eight layers, 11/12
exact calls. It is B1w3 (the 6.22 tok/s campaign candidate) plus one
archive-bound check in the PSRAM weight-tier copy, which keeps shallow model
archives from being read past their end. The unpromoted B1w3 tree itself stays
in the [research archive](research-archive.md).

## Source

`engine/` and `esp32/main/` on this branch are the archive overlay
`.auto/trees/final-matrix-source`. The concatenation hash the campaign used as
provenance still matches:

```sh
cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c | md5sum
# 1bd2ba7c68ce57848a92d3406abf9612
```

Research-only firmware (`kbench.c` with its `exp_pairs.h` fixture, `kb_wide.S`,
`dot4_tie728.S`, `thermal_diag.c`) is compiled only with `-DNEEDLE_KBENCH=ON` or
`-DNEEDLE_THERMAL_DIAG=ON`; both options default to OFF, so `make build`
produces the shipping app only.

## Build comparison

Built on 2026-09-28 with `docker.io/espressif/idf:v5.5.2` (`make build`):

| Artifact | Benchmarked (`benchmarks/config.json`) | From source |
| --- | --- | --- |
| App `needle_demo.bin` | `4a89784b…4451eb6`, 312,880 bytes | 312,880 bytes, different SHA-256 |
| Partition table | `461ee5cb…2c13592` | identical |

The two app images differ in **101 bytes, all build metadata**: the
`esp_app_desc_t` version string (`git describe` of the build checkout), compile
time and date, the ELF SHA-256 recorded in the descriptor, and the image's
trailing checksum and SHA-256, which cover those bytes. Every code and data
byte is identical. Check it with:

```sh
python3 tools/compare_app_image.py <benchmarked needle-matrix.bin> esp32/build/needle_demo.bin
# MATCH: identical outside build metadata (101 metadata bytes differ)
```

The bootloader (21,520 bytes) differs from the benchmarked `bootloader-current.bin`
in 37 bytes: its embedded build date/time string and its trailing checksum and
SHA-256. A from-source
flash is therefore the benchmarked firmware, but its image hash will not equal
`4a89784b…`. Pin the source commit and use this comparison, not the image hash,
to identify it. The benchmark package keeps the exact benchmarked binaries as
release assets (see [`benchmarks/README.md`](../benchmarks/README.md)).
