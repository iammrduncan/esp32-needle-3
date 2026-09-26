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
# A missing directory or an empty glob must fail, not pass vacuously.
if ! ls .auto/trees/board*-*.tar.gz >/dev/null 2>&1; then
    echo "NO_SNAPSHOTS_FOUND"; fail=1
fi
[ "$fail" = 0 ] || { echo "TREE_SNAPSHOTS_FAILED"; exit 1; }
echo "TREE_SNAPSHOTS_OK"
