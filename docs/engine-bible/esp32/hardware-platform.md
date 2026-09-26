# ESP32-S3 hardware platform

## Qualified target

This port targets an ESP32-S3 module in the **N32R16 class**:

- two Xtensa LX7 cores at the documented 240 MHz maximum;
- 32 MiB octal flash;
- 16 MiB octal PSRAM;
- single-precision scalar floating-point support;
- shared external-memory/cache path;
- ESP-IDF 5.5.2 in the audited Pi environment;
- Xtensa GCC 14.2.0 in that environment.

The exact board identity still belongs in every run record. “ESP32-S3” alone
does not establish flash size, PSRAM presence/mode, silicon revision, cache
configuration, thermal timing support, or serial endpoint.

Primary hardware references are catalogued in [sources](../sources.md). The
checked-in configuration, not a family data sheet, defines the built image.

## Checked-in target configuration

[`esp32/sdkconfig.defaults`](../../../esp32/sdkconfig.defaults) requests:

| Resource | Setting |
|---|---|
| CPU | 240 MHz |
| Flash | 32 MiB, octal/OPI DTR, 80 MHz |
| PSRAM | octal, 80 MHz, malloc-capable |
| Data cache | 64 KiB, 64-byte line |
| Instruction cache | 32 KiB, 32-byte line |
| FreeRTOS tick | 100 Hz |
| Compiler | performance optimization |
| Main task stack | 8 KiB |
| Task watchdog | disabled for this long inference workload |

`sdkconfig.defaults` is an input, not proof of an image. Resolve it against the
generated `esp32/sdkconfig`, compilation database, linker map, ELF, and boot
banner. ESP-IDF defaults and Kconfig dependencies can override assumptions.

## Flash layout

[`esp32/partitions.csv`](../../../esp32/partitions.csv) assigns:

| Region | Offset | Size |
|---|---:|---:|
| NVS | `0x9000` | `0x6000` |
| PHY init | `0xf000` | `0x1000` |
| Factory app | `0x10000` | `0x200000` |
| Model data (`0x40`) | `0x210000` | `0x1df0000` |

The pinned sliced model is 16,155,796 bytes and fits the model partition. The
firmware discovers its true directory extent and memory-maps only those bytes.
This layout assumes 32 MiB flash; a smaller module is not a drop-in target.

## Architectural facts that mattered

### External memory is the floor

The quantized matrices are much larger than internal RAM and are streamed from
memory-mapped flash or PSRAM. Once the CQ2 walker became efficient, the cost of
moving packed indices and norms dominated instruction shaving. Byte budgets
correctly rejected several ideas that looked cheaper in arithmetic.

### Caches are shared constraints

Both cores generate traffic through a shared cache/external-memory subsystem.
Two cores help when row work is independent and sufficiently large, but cannot
double bandwidth. Code, LUT, scratch, prefix state, and streamed weights also
compete for finite cache/internal memory.

The retained 64 KiB data cache with 64-byte lines helped this streaming
workload. This is workload evidence, not a universal ESP32-S3 setting; larger
cache or line choices consume memory and can hurt smaller/random workloads.

### The floating-point unit is scalar

The actual core configuration has 16 single-precision FP registers. Assembly
experiments found no usable packed/quad float SIMD for the gather-heavy CQ2
body. Wide TIE load instructions move contiguous data, while CQ2 table lookup
addresses depend on packed nibbles. Their existence therefore did not solve
the kernel's real operation.

### Alignment is part of the ISA contract

The model format aligns tensor blobs to 64 bytes, but row/group cursors can
still have narrower modulo alignment. Several proposed 64/128-bit load paths
were neutral, negative, or illegal because actual addresses did not meet their
assumptions. Inspect production pointers and every tail, not just base tensors.

## Clocking result

Raising octal memory to 120 MHz produced a real `+3.51%` result in the campaign,
but the installed ESP-IDF timing-retune path returned
`ESP_ERR_NOT_SUPPORTED` for the flash model. It is classified as
vendor-blocked, not shippable. A longer soak does not cure an unsupported
timing model; reopening requires supported calibration for the exact hardware.

The CPU's 240 MHz setting is documented operation, not an overclock.

## Capacity and placement caveats

- `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096` biases small allocations internal;
  explicit capability allocation is still preferable for critical buffers.
- Flash-mapped rodata is not equivalent to DMA-capable memory. The GDMA probe
  could not directly consume it, and a PSRAM bounce erased the benefit.
- Internal RAM is shared among static data, stacks, heap, cache choices, and
  IRAM code. “Free bytes” must be measured on the exact image and on both cores.
- The verified hot PSRAM-resident weight tier was 12 MiB. Larger apparent wins
  came from stale or mismatched trees and are not accepted capacity evidence.

## Port qualification checklist

1. Print chip/module, flash, PSRAM, frequency, and cache facts at boot.
2. Hash the app and model actually flashed.
3. Verify partition bounds against the model directory, not file-name lore.
4. Measure addresses, capabilities, alignment, and free internal/PSRAM bytes.
5. Inspect the actual Xtensa configuration before selecting instructions.
6. Re-run single- and dual-core bandwidth tests on every module/config change.
7. Treat unsupported clock/timing modes as experiments, never release defaults.
