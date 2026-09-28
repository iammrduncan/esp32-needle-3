#!/bin/bash
# Verify every tree snapshot in this directory restores to its recorded PROV_ENGINE.
#
# A snapshot that has never been restored is a claim, not a recovery path - the same rule this
# campaign applies to every gate ("can this check fail?"). Run this before trusting .auto/trees,
# and after any change to a worker, to confirm the repository can still rebuild the trees alone.
set -u
cd "$(dirname "$0")/../.." || exit 1          # repo root
fail=0
for T in .auto/trees/board*-*.tar.gz; do
    [ -f "$T" ] || continue
    want=$(basename "$T" .tar.gz | sed 's/^board[0-9]*-//')
    tmp=$(mktemp -d)
    tar -xzf "$T" -C "$tmp" 2>/dev/null || { echo "UNPACK_FAILED $T"; fail=1; rm -rf "$tmp"; continue; }
    got=$(cd "$tmp" && cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c 2>/dev/null \
          | md5sum | cut -c1-12)
    if [ "$got" = "$want" ]; then
        echo "SNAPSHOT_OK $(basename "$T") hash=$got"
    else
        echo "SNAPSHOT_MISMATCH $(basename "$T") want=$want got=$got"
        fail=1
    fi
    rm -rf "$tmp"
done
# The final campaign and benchmark source trees are stored unpacked so that a
# fresh clone can inspect and overlay them without access to the pool workers.
for spec in \
    final-b1w3-source:1ada94b5b3d0c038f5f47e80006958df \
    final-matrix-source:1bd2ba7c68ce57848a92d3406abf9612 \
    final-one-layer-source:b8b3b60013fd92c702bb5ee50f6a07f2 \
    final-b2resid2-source:d2b3c85ae2e45eee69a6de7393ea0c7d \
    final-b3p1fix-source:9e4ca21af947fbde703efd702f867614; do
    name=${spec%%:*}
    want=${spec#*:}
    tree=".auto/trees/$name"
    if [ ! -d "$tree/engine/src" ] || [ ! -d "$tree/engine/include" ] ||
       [ ! -d "$tree/esp32/main" ]; then
        echo "FINAL_SOURCE_MISSING $name"
        fail=1
        continue
    fi
    got=$(cd "$tree" && cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c \
          | md5sum | cut -d' ' -f1)
    if [ "$got" = "$want" ]; then
        echo "FINAL_SOURCE_OK $name hash=$got"
    else
        echo "FINAL_SOURCE_MISMATCH $name want=$want got=$got"
        fail=1
    fi
done
# LIVE DRIFT: a snapshot that matches its own filename is still useless if the worker it came from
# has moved on. The first version of this script could not see that - it compared each snapshot only
# against the hash embedded in its filename, so it reported OK while board 3's live tree had already
# changed. With --live it also hashes each worker's current sources and reports LIVE_DRIFT on a
# mismatch, which is the case that actually matters when the snapshots are the recovery path.
if [ "${1:-}" = "--live" ]; then
    for T in .auto/trees/board*-*.tar.gz; do
        [ -f "$T" ] || continue
        b=$(basename "$T" | sed 's/^board\([0-9]*\)-.*/\1/')
        w="/root/board-pool/board$b"
        [ -d "$w" ] || continue
        want=$(basename "$T" .tar.gz | sed 's/^board[0-9]*-//')
        live=$(cd "$w" && cat engine/src/*.c engine/src/*.S engine/include/*.h esp32/main/*.c 2>/dev/null \
               | md5sum | cut -c1-12)
        if [ "$live" = "$want" ]; then
            echo "LIVE_MATCH board$b hash=$live"
        else
            echo "LIVE_DRIFT board$b snapshot=$want live=$live"
        fi
    done
fi

# A missing directory or an empty glob must fail, not pass vacuously.
if ! ls .auto/trees/board*-*.tar.gz >/dev/null 2>&1; then
    echo "NO_SNAPSHOTS_FOUND"; fail=1
fi
[ "$fail" = 0 ] || { echo "TREE_SNAPSHOTS_FAILED"; exit 1; }
echo "TREE_SNAPSHOTS_OK"
