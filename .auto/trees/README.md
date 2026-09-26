# Worker tree snapshots (2026-09-28)

Each tarball is the complete `engine/src`, `engine/include` and `esp32/main` of one pool worker at
handover, named `board<N>-<PROV_ENGINE prefix>.tar.gz`. They exist because the handoff's claim that
every tree is reproducible from `.auto/exp88`-`.auto/exp96` was **not quite true**: those assets carry
individual levers, but a tree's *base* state (the seed-era stack, the shippable line's bundle, the
composed attention family) lives only in the worker checkouts, uncommitted. If a worker were lost, the
tree could not be rebuilt from assets alone.

To restore a tree: unpack over a checkout of the matching lineage (or over any worker of that
lineage), then verify with the recorded hash:

```
tar -xzf board3-<hash>.tar.gz -C <worker>
cd <worker> && cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c | md5sum
```

| board | tree | PROV_ENGINE |
|---|---|---|
| 1 | shippable + plain loop + EG2 + amortised + QK outline + `qk_hd==48` (pin 5.9283) | `efa82fec3f6a9a0407328bb98e42b6d4` |
| 2 | seed + plain loop + amortised, split LUT build, shared codebook (pin 6.1250) | `a01263caeaf7d6f9a9c05bbe767434e0` |
| 3 | composed + EG2 + compact prefix + amortised + two-deep schedule + `line_too_long` fix (pin 6.1450) | `2b7d119bdc44...` (full value in the handoff's provenance section) |
