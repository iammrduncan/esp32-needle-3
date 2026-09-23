#!/usr/bin/env bash
set -u

repo=/home/mbench01/github/esp32-needle-3
state_dir=/home/mbench01/.local/state/needle3-hourly-mentor
prompt="$repo/.auto/hourly_mentor.md"
codex_bin=/home/mbench01/.local/bin/codex
lock=/tmp/needle3-hourly-mentor.lock

mkdir -p "$state_dir"
exec 8>"$lock"
if ! flock -n 8; then
    exit 0
fi

stamp=$(date '+%Y%m%dT%H%M%S%z')
log="$state_dir/$stamp.log"
last_tmp="$state_dir/latest.md.tmp"
last="$state_dir/latest.md"

{
    echo "mentor_start=$(date --iso-8601=seconds)"
    timeout --signal=TERM 12m "$codex_bin" exec \
        --ephemeral \
        --color never \
        --model gpt-6-sol \
        --sandbox danger-full-access \
        --config 'approval_policy="never"' \
        --config 'model_reasoning_effort="high"' \
        --cd "$repo" \
        --output-last-message "$last_tmp" \
        - < "$prompt"
    rc=$?
    if [ -s "$last_tmp" ]; then
        mv "$last_tmp" "$last"
    else
        rm -f "$last_tmp"
    fi
    echo "mentor_rc=$rc"
    echo "mentor_end=$(date --iso-8601=seconds)"
    exit "$rc"
} >>"$log" 2>&1
