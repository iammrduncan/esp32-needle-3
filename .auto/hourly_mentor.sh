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
    deadline_epoch=$(($(date +%s) + 900))
    timeout --signal=INT --kill-after=10s 13m "$codex_bin" exec \
        --color never \
        --model gpt-6-astra \
        --sandbox danger-full-access \
        --config 'approval_policy="never"' \
        --config 'model_reasoning_effort="max"' \
        --cd "$repo" \
        --output-last-message "$last_tmp" \
        - < "$prompt"
    main_rc=$?

    if [ "$main_rc" -eq 124 ]; then
        session_id=$(sed -nE 's/^session id: ([0-9a-f-]+)$/\1/p' "$log" | head -1)
        remaining=$((deadline_epoch - $(date +%s)))
        if [ -n "$session_id" ] && [ "$remaining" -gt 0 ]; then
            echo "mentor_wrap_warning=$(date --iso-8601=seconds) remaining_seconds=$remaining session_id=$session_id"
            timeout --signal=TERM --kill-after=5s "${remaining}s" "$codex_bin" exec resume \
                --model gpt-6-astra \
                --config 'approval_policy="never"' \
                --config 'model_reasoning_effort="max"' \
                --output-last-message "$last_tmp" \
                "$session_id" \
                "Two minutes remain before the hard hourly deadline. Wrap up immediately: stop new research and tool exploration, write your best current priorities to .auto/mentor_queue.md, send any necessary concise steering to the live researcher, and return the compact report."
            rc=$?
        else
            echo "mentor_wrap_error=unable_to_resume session_id=${session_id:-missing} remaining_seconds=$remaining"
            rc=124
        fi
    else
        rc=$main_rc
    fi
    if [ -s "$last_tmp" ]; then
        mv "$last_tmp" "$last"
    else
        rm -f "$last_tmp"
    fi
    echo "mentor_rc=$rc"
    echo "mentor_end=$(date --iso-8601=seconds)"
    exit "$rc"
} >>"$log" 2>&1
