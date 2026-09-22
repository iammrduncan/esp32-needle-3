#!/bin/bash
# Config-sync gate for the three-board pool.
#
# `esp32/sdkconfig` is gitignored, so `git reset --hard`/`git fetch` does NOT
# restore it - a worker keeps whatever build config the last experiment wrote.
# Run #288 caught all three boards silently running a 1000 Hz tick, and run #293's
# assertion-level batch left board2 at ASSERTIONS_DISABLE and board3 at
# ASSERTIONS_SILENT (found by this script on run #300), i.e. a control measured
# against two candidates for a reason nobody intended. Cache-line width and tick
# rate produced this campaign's largest config wins (#238 +11.9 %, #240 +0.34 %),
# so an unannounced difference in those keys is not noise, it is the result.
#
# The authority is the LOCAL esp32/sdkconfig - the one .auto/measure.sh builds and
# flashes - because that is the config every accepted number was measured at.
#
# KEYS are matched as PREFIXES, not exact names, and this is load-bearing: the
# first version anchored `^KEY=` and therefore matched nothing for the keys that
# matter most, because IDF spells them `CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_
# DISABLE=y` and `CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y`. It reported "IN SYNC" on
# a pool that was demonstrably at three different assertion levels. SELFTEST=1
# proves the comparison can actually report drift.
#
#   bash .auto/board_config_check.sh            # non-zero exit on drift
#   SELFTEST=1 bash .auto/board_config_check.sh # must report drift and exit 1
set -uo pipefail
ROOT=/workspace/esp32-needle-3
KEYS='CONFIG_FREERTOS_HZ CONFIG_ESP32S3_DATA_CACHE CONFIG_ESP32S3_INSTRUCTION_CACHE
CONFIG_COMPILER_OPTIMIZATION CONFIG_SPIRAM CONFIG_ESPTOOLPY_FLASHFREQ
CONFIG_ESPTOOLPY_FLASHMODE CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH
CONFIG_FREE_RTOS_UNIVERSAL'
PAT=$(echo "$KEYS" | tr -s ' \n' '\n' | grep -v '^$' | paste -sd'|' -)

get() { [ -f "$1" ] && grep -hE "^($PAT)" "$1" | sort | tr '\n' ' ' || echo MISSING; }

# compare <label> <baseline> <candidate>; echoes status, returns 1 on drift.
compare() {
    if [ "$3" = MISSING ]; then echo "$1   clean (no sdkconfig; regenerates from its own defaults)"
    elif [ "$3" = "$2" ]; then echo "$1   IN SYNC"
    else echo "$1   DRIFTED"; echo "     base: $2"; echo "     cand: $3"; return 1; fi
    return 0
}

if [ -n "${SELFTEST:-}" ]; then
    a='CONFIG_X=1 CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=y'
    b="$a"
    c='CONFIG_X=1 CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE=y'
    compare "selftest equal" "$a" "$b" || { echo "SELFTEST_FAILED equal configs reported drift"; exit 2; }
    if compare "selftest different" "$a" "$c"; then
        echo "SELFTEST_FAILED differing configs reported IN SYNC"; exit 2
    fi
    echo "SELFTEST_OK the comparison can distinguish a one-key config difference"
    exit 0
fi

canon="$ROOT/esp32/sdkconfig"
base=$(get "$canon")

# Source sync is checked FIRST and is the more load-bearing of the two: a worker on
# an old commit builds different firmware no matter what its sdkconfig says, and
# run #300 found boards 2 and 3 two commits behind with `sdkconfig.defaults` still
# carrying run #293's assertion-level change - so deleting their sdkconfig would
# have regenerated the drift straight back. Sync by LOCAL path: `git fetch origin`
# from this container has no credentials and would reset a worker to a stale
# baseline, which is how a control reads low and invalidates a whole batch.
sync_drift=0
want_head=$(git -C "$ROOT" rev-parse HEAD)
want_def=$(git -C "$ROOT" hash-object esp32/sdkconfig.defaults)
for b in 1 2 3; do
    d=/root/board-pool/board$b
    [ -d "$d/.git" ] || { echo "board$b   absent (no checkout)"; continue; }
    got=$(git -C "$d" rev-parse HEAD 2>/dev/null)
    gdef=$(git -C "$d" hash-object esp32/sdkconfig.defaults 2>/dev/null)
    if [ "$got" = "$want_head" ] && [ "$gdef" = "$want_def" ]; then
        echo "source board$b  AT HEAD $(printf %s "$want_head" | cut -c1-7)"
    else
        echo "source board$b  STALE head=$(printf %s "$got" | cut -c1-7) want=$(printf %s "$want_head" | cut -c1-7) defaults=$(printf %s "$gdef" | cut -c1-7) want=$(printf %s "$want_def" | cut -c1-7)"
        echo "        fix: git -C $d fetch $ROOT autoresearch/decode-tps-2026-09-18 && git -C $d reset --hard FETCH_HEAD"
        sync_drift=$((sync_drift + 1))
    fi
done
if [ "$base" = MISSING ]; then
    echo "CONFIG_BASELINE_UNAVAILABLE: $canon not configured (idf.py regenerates it)"; exit 0
fi
echo "baseline  $canon"
echo "          $(echo "$base" | tr ' ' '\n' | grep -E 'ASSERTIONS|CACHE_LINE|CACHE_SIZE|FREERTOS_HZ|SPIRAM_SPEED|SPIRAM_MODE|FLASHFREQ' | tr '\n' ' ')"
drift=0
for b in 1 2 3; do
    compare "board$b  " "$base" "$(get /root/board-pool/board$b/esp32/sdkconfig)" || drift=$((drift + 1))
done
if [ "$drift" != 0 ] || [ "$sync_drift" != 0 ]; then
    echo "CONFIG_DRIFT build_config=$drift source=$sync_drift - normalise build config with: rm /root/board-pool/boardN/esp32/sdkconfig, and source with a local-path fetch + reset --hard"
    exit 1
fi
echo "CONFIG_SYNC_OK boards=3 source=3"
