#!/usr/bin/env bash
set -euo pipefail
if [ "$EUID" -ne 0 ]; then echo "Run this installer with sudo." >&2; exit 1; fi
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
install -d -o root -g root -m 755 /usr/local/libexec /dev/needle-pi
install -o root -g root -m 755 "$source_dir/needle-pi-serial-device" /usr/local/libexec/needle-pi-serial-device
install -o root -g root -m 644 "$source_dir/99-needle-pi-serial.rules" /etc/udev/rules.d/99-needle-pi-serial.rules
udevadm control --reload-rules
udevadm trigger --action=add --subsystem-match=tty --sysname-match='ttyACM*'
udevadm settle
ls -l /dev/needle-pi
