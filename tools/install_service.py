#!/usr/bin/env python3
"""Install and start the local API as a systemd user service (Linux)."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def quote(value):
    # systemd has its own quoting, percent specifiers and ExecStart $ expansion.
    return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"').replace('%', '%%').replace('$', '$$') + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', default='/dev/ttyACM1')
    args = parser.parse_args()
    python = ROOT/'.venv/bin/python'
    if not python.exists():
        raise SystemExit('Run make setup first.')
    if any(c in str(ROOT) + args.serial for c in '\r\n'):
        raise SystemExit('Paths must not contain line breaks.')
    unit = Path.home()/'.config/systemd/user/needle3-api.service'
    unit.parent.mkdir(parents=True, exist_ok=True)
    unit.write_text(f'''[Unit]
Description=Needle 3 ESP32 telemetry router API

[Service]
Type=simple
ExecStart={quote(python)} -u {quote(ROOT/'tools/serial_api.py')} --serial {quote(args.serial)} --host 127.0.0.1 --port 8081
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
''')
    subprocess.run(['systemctl', '--user', 'daemon-reload'], check=True)
    subprocess.run(['systemctl', '--user', 'enable', 'needle3-api.service'], check=True)
    subprocess.run(['systemctl', '--user', 'restart', 'needle3-api.service'], check=True)
    print(f'Installed {unit}\nCheck readiness: curl http://127.0.0.1:8081/health')


if __name__ == '__main__':
    main()
