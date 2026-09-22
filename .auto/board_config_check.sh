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

SHIPPING='engine esp32 host tools'
shatree() { local r=$1 p h=""; for p in $SHIPPING; do h="$h$(git -C "$r" rev-parse "HEAD:$p" 2>/dev/null)"; done
             [ ${#h} -ge 160 ] && { printf %s "$h" | sha1sum | cut -c1-12; } || echo MISSING; }

if [ -n "${SELFTEST:-}" ]; then
    a='CONFIG_X=1 CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=y'
    b="$a"
    c='CONFIG_X=1 CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE=y'
    compare "selftest equal" "$a" "$b" || { echo "SELFTEST_FAILED equal configs reported drift"; exit 2; }
    if compare "selftest different" "$a" "$c"; then
        echo "SELFTEST_FAILED differing configs reported IN SYNC"; exit 2
    fi
    # The tree-hash side must not be able to pass vacuously: an unreadable repo
    # returns MISSING rather than an empty string that could equal another empty
    # string, and two real shipping states have to hash differently.
    [ "$(shatree "/nonexistent-$$")" = MISSING ] || { echo "SELFTEST_FAILED unreadable repo did not report MISSING"; exit 2; }
    e1=$(git -C "$ROOT" rev-parse HEAD:engine 2>/dev/null)
    e0=$(git -C "$ROOT" rev-parse "$(git -C "$ROOT" log --format=%H -2 -- engine | tail -1):engine" 2>/dev/null)
    if [ -z "$e1" ] || [ -z "$e0" ] || [ "$e1" = "$e0" ]; then
        echo "SELFTEST_FAILED the shipping tree hash cannot distinguish two engine commits"; exit 2
    fi
    echo "SELFTEST_OK the comparison can distinguish a one-key config difference and two shipping trees"
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
# Compare TREE HASHES of the paths that actually feed the build, not HEAD ids:
# log_experiment commits on every keep, so HEAD moves constantly while workers stay
# at the last synced commit. Matching on HEAD would report drift on almost every
# run, and a check that cries wolf gets ignored - which is how drift gets measured
# as a result. `.auto/**` commits cannot change what a worker compiles.
want=$(shatree "$ROOT")
got_w=$(shatree /root/board-pool/board1)
for b in 1 2 3; do
    d=/root/board-pool/board$b
    [ -d "$d/.git" ] || { echo "source board$b  absent (no checkout)"; sync_drift=$((sync_drift + 1)); continue; }
    got=$(shatree "$d")
    if [ "$got" = "$want" ]; then
        echo "source board$b  builds identical shipping trees (engine/esp32/host/tools)"
    else
        echo "source board$b  STALE shipping trees: $got vs $want (head $(git -C "$d" rev-parse --short HEAD 2>/dev/null) vs $(git -C "$ROOT" rev-parse --short HEAD))"
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
