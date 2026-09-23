#!/usr/bin/env bash
# Measure whatever is in the MAIN checkout on one board, inside that board's lock.
#
# The candidate travels by file copy from the main tree, never by `git fetch origin`:
# this container has no credentials, and a stale-origin fetch reverts the worker and
# makes the reading meaningless (#300). The worker's sdkconfig is gitignored, so it is
# deleted to force regeneration from the tracked defaults (#288/#300 both bit here).
set -uo pipefail
B="${1:?board}"
MAIN=/workspace/esp32-needle-3
W=/root/board-pool/board$B
BRANCH=autoresearch/decode-tps-2026-09-18
cd "$MAIN" || { echo "SYNC_FAIL main"; exit 1; }
git -C "$W" fetch -q "$MAIN" "$BRANCH" && git -C "$W" reset --hard -q FETCH_HEAD || { echo "SYNC_FAIL reset"; exit 1; }
for f in engine/src/nd_model.c engine/src/nd_quant.c engine/src/nd_sample.c \
         engine/include/nd_quant.h engine/include/nd_model.h \
         esp32/main/main.c esp32/main/CMakeLists.txt \
         esp32/components/needle/CMakeLists.txt esp32/sdkconfig.defaults; do
    cp "$MAIN/$f" "$W/$f" || { echo "COPY_FAIL $f"; exit 1; }
done
rm -f "$W/esp32/sdkconfig"
echo "candidate_engine_md5=$(md5sum < $W/engine/src/nd_quant.c | cut -c1-12) $(md5sum < $W/engine/src/nd_model.c | cut -c1-12)"
echo "main_engine_md5=$(md5sum < $MAIN/engine/src/nd_quant.c | cut -c1-12) $(md5sum < $MAIN/engine/src/nd_model.c | cut -c1-12)"
# A candidate that failed to apply must not be able to pose as the accepted image.
cmp -s "$MAIN/engine/src/nd_quant.c" "$W/engine/src/nd_quant.c" || { echo "CANDIDATE_NOT_APPLIED"; exit 1; }
bash .auto/measure.sh
