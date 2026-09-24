#!/bin/bash
# Which functions still call ROM memcpy, and how often? Runs #424/#425 priced the mechanism
# (a plain small memcpy is a ROM library call under this target's -fno-builtin-memcpy, ~27
# cycles of call setup + spills), so the remaining prize is exactly this list, weighted by how
# often each function runs per token.
#
#   census.sh <worker> [source-stem ...]     e.g. census.sh /root/board-pool/board1
. /opt/esp/idf/export.sh >/dev/null 2>&1
W=${1:?worker}; shift
STEMS=${*:-nd_model nd_quant nd_sample}
for s in $STEMS; do
    O=$(find "$W/esp32/build" -name "$s.c.obj" | head -1)
    [ -f "$O" ] || { echo "[$s] NO_OBJECT"; continue; }
    xtensa-esp32s3-elf-objdump -dr "$O" 2>/dev/null |
        awk '/^[0-9a-f]+ <.*>:$/{fn=$2} /memcpy/{c[fn]++} END{for (f in c) printf "%4d  %s\n", c[f], f}' |
        sort -rn | head -12 | sed "s/^/[$s] /"
done
