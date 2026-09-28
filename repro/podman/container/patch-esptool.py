#!/usr/bin/env python3
"""Keep esptool 4.11 USB reset detection working with hotplug-safe device aliases."""
from pathlib import Path
paths = list(Path("/opt/esp/python_env").glob("*/lib/python*/site-packages/esptool/loader.py"))
if len(paths) != 1:
    raise SystemExit("Expected exactly one ESP-IDF esptool installation")
p = paths[0]
s = p.read_text()
needle = "        active_port = self._port.port\n"
patch = '        # needle-pi: derive USB PID from the device number, including udev aliases.\n        if sys.platform.startswith("linux") and active_port.startswith("/dev/"):\n            try:\n                device_number = os.stat(active_port).st_rdev\n                usb_pid_path = "/sys/dev/char/{}:{}/device/../idProduct".format(\n                    os.major(device_number), os.minor(device_number)\n                )\n                with open(usb_pid_path) as usb_pid_file:\n                    self.cache["usb_pid"] = int(usb_pid_file.read().strip(), 16)\n                return self.cache["usb_pid"]\n            except (OSError, ValueError):\n                pass\n'
if "# needle-pi: derive USB PID" not in s:
    if s.count(needle) != 1:
        raise SystemExit("Unexpected esptool source; review alias compatibility patch")
    p.write_text(s.replace(needle, needle + patch))
print("Verified esptool alias compatibility patch:", p)
