#!/usr/bin/env bash
# Run one make target the way the inference-engines launcher expects:
#
#   scripts/ie-step.sh <setup|model|build|flash|serve|health|test|check|...>
#
# Host mode runs `make <target>` directly. It needs make, a Python with venv,
# and (for build/flash) an activated ESP-IDF 5.5.2. Container mode runs the same
# target inside ${IDF_IMAGE:-docker.io/espressif/idf:v5.5.2} with podman or
# docker: the checkout is mounted at the same path, serial ports named by
# IE_PORT_FLASH / IE_PORT_SERIAL are passed through, and the network is shared
# so the bridge's HTTP port is reachable from the host.
#
# The mode is chosen once and stored in .ie-mode, because `setup` creates the
# Python venv inside whichever environment ran it and `serve`/`test` must use
# the same one. Override with IE_STEP_MODE=host|container; delete .ie-mode and
# rerun setup to switch.
set -euo pipefail

target=${1:?usage: scripts/ie-step.sh <make target>}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
IDF_IMAGE=${IDF_IMAGE:-docker.io/espressif/idf:v5.5.2}
MODE_FILE="$ROOT/.ie-mode"
have() { command -v "$1" >/dev/null 2>&1; }

host_capable() {
    have make && python3 -c 'import ensurepip, venv' 2>/dev/null && have idf.py
}

runtime() {
    if have podman; then echo podman; elif have docker; then echo docker; fi
}

mode=${IE_STEP_MODE:-}
if [ -z "$mode" ] && [ -f "$MODE_FILE" ]; then mode=$(cat "$MODE_FILE"); fi
if [ -z "$mode" ]; then
    if host_capable; then mode=host
    elif [ -n "$(runtime)" ]; then mode=container
    else
        echo "ie-step: need make, python3 venv and idf.py on the host, or podman/docker for $IDF_IMAGE" >&2
        exit 1
    fi
fi
case "$mode" in host|container) ;; *) echo "ie-step: unknown mode '$mode' in $MODE_FILE" >&2; exit 1 ;; esac
[ "$target" = setup ] && echo "$mode" > "$MODE_FILE"

if [ "$mode" = host ]; then
    echo "ie-step: $target in host mode" >&2
    exec make -C "$ROOT" "$target"
fi

rt=$(runtime)
[ -n "$rt" ] || { echo "ie-step: container mode needs podman or docker" >&2; exit 1; }
name="ie-needle-$target-$$"
args=(run --rm --init --name "$name" --network host
      -v "$ROOT:$ROOT" -w "$ROOT" -e HOME="$ROOT/.ie-home" -e PYTHONUNBUFFERED=1)
mkdir -p "$ROOT/.ie-home"
if [ "$rt" = podman ]; then
    # Run as the invoking user so build outputs stay theirs, and keep their
    # supplementary groups (dialout) so the serial devices are accessible.
    args+=(--userns=keep-id --group-add keep-groups)
else
    args+=(--user "$(id -u):$(id -g)")
    for gid in $(id -G); do args+=(--group-add "$gid"); done
fi
if [ -n "${IE_WORK_DIR:-}" ]; then
    mkdir -p "$IE_WORK_DIR"
    args+=(-v "$IE_WORK_DIR:$IE_WORK_DIR")
fi
for var in $(compgen -e | grep '^IE_' || true); do args+=(-e "$var"); done
# Serial ports: pass the real device node (by-id paths are symlinks) and point
# the Makefile at it.
for pair in IE_PORT_FLASH:FLASH_PORT IE_PORT_SERIAL:SERIAL_PORT; do
    src=${pair%%:*}; dst=${pair#*:}
    val=${!src:-}
    [ -n "$val" ] || continue
    real=$(readlink -f "$val")
    if [ ! -c "$real" ]; then
        echo "ie-step: $src=$val is not a character device" >&2
        exit 1
    fi
    args+=(--device "$real" -e "$dst=$real")
done

echo "ie-step: $target in container mode ($rt, $IDF_IMAGE)" >&2
# Run in the background and wait, so SIGTERM/SIGINT to this script can stop
# the named container even if the runtime does not forward the signal.
stop() {
    trap - TERM INT
    "$rt" stop -t 10 "$name" >/dev/null 2>&1 || true
}
trap stop TERM INT
"$rt" "${args[@]}" "$IDF_IMAGE" make "$target" &
child=$!
set +e
while :; do
    wait "$child"
    status=$?
    kill -0 "$child" 2>/dev/null || break   # a trapped signal interrupted wait
done
exit "$status"
