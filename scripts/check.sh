#!/bin/sh
# Report prerequisites for the make targets. Installs nothing; exits 1 if a
# target cannot run on this machine in the current mode.
PYTHON=${PYTHON:-python3}
missing=0
have() { command -v "$1" >/dev/null 2>&1; }
ok() { printf 'ok       %s\n' "$*"; }
warn() { printf 'missing  %s\n' "$*"; }

if have "$PYTHON"; then ok "python: $($PYTHON --version 2>&1)"; else warn "python ($PYTHON)"; missing=1; fi
if "$PYTHON" -c 'import ensurepip, venv' 2>/dev/null; then ok "python venv (setup, serve, test)"
else warn "python venv/ensurepip (setup, serve, test); use scripts/ie-step.sh container mode"; missing=1; fi
if have idf.py; then ok "ESP-IDF: $(idf.py --version 2>/dev/null)"
elif [ -n "$CONTAINER" ]; then ok "ESP-IDF via $CONTAINER image ${IDF_IMAGE} (build)"
else warn "ESP-IDF 5.5.2 (idf.py) or podman/docker for ${IDF_IMAGE} (build)"; missing=1; fi
if have esptool.py || "$PYTHON" -c 'import esptool' 2>/dev/null; then ok "esptool (flash)"; else warn "esptool (flash)"; missing=1; fi
for tool in cmake cc ctest; do
    if have "$tool"; then ok "$tool (test)"; else warn "$tool (test)"; missing=1; fi
done
if have curl; then ok "curl (health)"; else warn "curl (health)"; missing=1; fi
for port in "$FLASH_PORT" "$SERIAL_PORT"; do
    [ -n "$port" ] || continue
    if [ -c "$(readlink -f "$port")" ]; then ok "serial port $port"; else warn "serial port $port (flash, serve)"; fi
done
exit $missing
