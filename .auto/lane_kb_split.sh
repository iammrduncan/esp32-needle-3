#!/usr/bin/env bash
# Board lane: re-measure nd_cq_prepare's internal split (copy / FWHT / rescale / whole call)
# on the CANDIDATE engine (radix-4 fused transform + cold-text refund). Why: three transform
# wins since the last split reading (#378 unroll, #379 pairing, #404 fusion), so the ledger's
# "transform is 80.6 % of prepare" denominator is stale, and every future ranking of the
# remaining phases is priced off it. This is the tree's own kbench default dispatch - no new
# bench body, so no transcription risk (#363's class) - and it never runs the request path.
set -uo pipefail
if ! command -v idf.py >/dev/null 2>/dev/null; then . /opt/esp/idf/export.sh >/dev/null 2>&1 || true; fi
BDIR=esp32
ENG=engine/src/nd_quant.c
ASM=$(ls engine/src/*.S | tr '\n' ' ')
BATCH=/root/board-pool/batches
LOGF=$BATCH/K-kbsplit-b${NEEDLE_BOARD:-?}-$(date -u +%H%M%S).log
BD=build-kbsplit-${NEEDLE_BOARD:-x}
echo "[kb] provenance board=${NEEDLE_BOARD:-local} engine=$(md5sum $ENG | cut -c1-12) asm=$(cat $ASM | md5sum | cut -c1-12) head=$(git rev-parse --short HEAD)"
echo "[kb] build_start=$(date -u +%FT%TZ)"
idf.py -C "$BDIR" -B "../$BD" -DNEEDLE_KBENCH=ON build > "$BATCH/K-kbsplit-build-b${NEEDLE_BOARD:-?}.log" 2>&1
rc=$?; echo "[kb] build_rc=$rc (log $BATCH/K-kbsplit-build-b${NEEDLE_BOARD:-?}.log)"
[ $rc -eq 0 ] || { tail -20 "$BATCH/K-kbsplit-build-b${NEEDLE_BOARD:-?}.log"; exit 1; }
ELF=$(ls -t ../$BD/esp32/*.elf ../$BD/*.elf 2>/dev/null | head -1)
[ -n "${ELF:-}" ] || { echo "[kb] NO ELF from $BD"; exit 1; }
# Positive control (run #359's rule): prove the bench driver is really in the image before
# flashing, rather than trusting build_rc, which cannot fail on an uncalled static body.
kbd=$(strings "$ELF" | grep -c 'KBENCH_DONE')
echo "[kb] elf=$(basename $ELF) kbench_driver_in_elf=$kbd"
[ "${kbd:-0}" -ge 1 ] || { echo "[kb] REFUSING to flash: bench driver absent"; exit 1; }
idf.py -C "$BDIR" -B "../$BD" -p "${FLASH_PORT:-/dev/ttyACM0}" flash > "$BATCH/K-kbsplit-flash-b${NEEDLE_BOARD:-?}.log" 2>&1
echo "[kb] flash_rc=$?"
python3 - "${SERIAL_PORT:-/dev/ttyACM1}" "${FLASH_PORT:-/dev/ttyACM0}" "$LOGF" <<'PY'
import sys, time, subprocess
console, flash, out = sys.argv[1], sys.argv[2], sys.argv[3]
subprocess.run(["python3", "-m", "esptool", "--chip", "esp32s3", "-p", flash, "run"],
               capture_output=True, timeout=90)
import serial
s = serial.Serial(console, 115200, timeout=0.5); s.dtr = True
buf, t0 = b"", time.time()
while time.time() - t0 < 150:
    d = s.read(4096)
    if d:
        buf += d
        if b"KBENCH_DONE" in buf: break
s.close(); open(out, "wb").write(buf)
kb = [l for l in buf.decode("utf8", "replace").splitlines() if l.startswith("KB")]
print("\n".join(kb[:60]) if kb else "[kb] NO KB OUTPUT (%d bytes)" % len(buf))
print("[kb] kbench_done=%s kb_lines=%d bytes=%d log=%s" % (b"KBENCH_DONE" in buf, len(kb), len(buf), out))
PY
echo "[kb] done=$(date -u +%FT%TZ)"
