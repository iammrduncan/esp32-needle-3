# needle-pi local setup

Created with https://github.com/Hackers-in-the-Loop/pod-pi at commit c95ed7c574ab9f7a2a313bb808ea30f88e1fac7e.
Keep /home/mbench01/github/pod-pi in place; the launchers refer to it.

## Start and attach

The host Podman prerequisites are installed. The session is running:

```sh
podman info
needle-pi start
needle-pi check
needle-pi
```

Detach with Ctrl-b, then d. Reconnect with needle-pi.
Window 0 is Pi; Ctrl-b 1 opens the shell.
The project /home/mbench01/github/esp32-needle-3 is mounted read-write
at /workspace/esp32-needle-3, where both terminal windows start.
The instance home and session history persist under this directory's home/.

## Provider

home/.pi/agent/models.json defines sp02/qwen3.8-flash-next at
http://sp02.local:8888/v1 with a 262144-token context window.
The endpoint currently needs no authentication; its API key is a dummy value.
instance.json maps sp02.local to 192.168.1.30 for container DNS.
If that address changes, edit the --add-host argument, then stop and recreate.

## Extensions

home/.pi/agent/settings.json contains:

- npm:pi-web-access@0.29.0
- npm:pi-subagents@0.68.0
- npm:pi-goal-x@0.31.4
- npm:@dietrichgebert/ponytail@4.10.0
- npm:pi-autoresearch@1.8.1

The pod-pi startup script installs these with pi install before launching Pi.
In Pi, /autoresearch shows help. Supply a specific optimization target to start
an experiment loop. No optimization loop is started by this setup.

## Lifecycle

needle-pi status, needle-pi logs, needle-pi stop, and needle-pi start manage
this instance. Changes to container options or recipes need needle-pi stop
followed by needle-pi recreate; recreation discards container-only changes
but preserves mounted home, workspace, and project files.

## Verification status

Verified on 2026-09-18:

- Rootless Podman with crun; container needle-pi is running.
- Pi 0.84.4 and all five pinned packages installed; needle-pi check passed.
- All five extensions appear in the live Pi startup output.
- The named needle-pi conversation used sp02/qwen3.8-flash-next to call bash
  (pwd) and read (README.md), and returned NEEDLE_PI_READY.
- /autoresearch returned its command help in the live terminal.
- The repository worktree remained unchanged by verification.

The saved conversation is under home/.pi/agent/sessions/--workspace-esp32-needle-3--/.


## ESP32 hardware access

The host needle3-api.service has been stopped and disabled so needle-pi owns
serial access. Container ports /dev/ttyACM0 (flash) and /dev/ttyACM1 (UART,
115200 baud) are aliases to /dev/needle-pi/flash and /dev/needle-pi/console.
The host's 99-needle-pi-serial.rules matches only this board's USB identities
and maintains those device nodes. The container mounts only /dev/needle-pi,
so re-enumeration refreshes the nodes without recreating the container.

The container uses Espressif's ESP-IDF v5.5.2 image with the pod-pi runtime.
ESP-IDF is activated automatically for Pi and interactive shells.

The container's Python environment and build directories live under
home/project-venv, home/esp32-build, and home/host-build and are mounted over
.venv, esp32/build, and host/build in the project. This keeps the host's existing
Python environment and CMake caches intact. Source files and model data are shared.

Inside needle-pi, or in needle-pi shell after cd /workspace/esp32-needle-3:

```sh
make build              # compile firmware
make test               # Python and native host tests
make flash-app          # flash application/bootloader/partition table
make flash              # also write the model partition
needle-api              # run API in foreground; Ctrl-C stops it
```

The API is available on the host at http://127.0.0.1:8081 while needle-api runs.
Stop the API or serial monitor before flashing or using the console directly.
The host systemd API service should stay disabled while Pi manages the board.
To hand control back later, first stop all Pi serial/API processes, then run
systemctl --user enable --now needle3-api.service on the host.

USB unplug/replug and board resets refresh the device nodes automatically.
Existing serial connections must close/reopen after USB disconnection, but the
Pi container and conversation can keep running. The installed host helper is
/usr/local/libexec/needle-pi-serial-device; its reviewed source and installer
are in host-serial/ in this instance directory.

container/patch-esptool.py is applied when building the image. It fixes USB PID
detection for the aliases using /sys/dev/char/<major>:<minor>, retaining esptool's
automatic USB/JTAG reset sequence. This is local to the instance image.

After a hardware reset, the current firmware warms two model caches for about
5 minutes before EVT READY. The API's 600-second boot timeout covers this.

Hardware verification:

- ESP-IDF v5.5.2, Xtensa ESP32-S3 GCC 14.2.0 and esptool are available in Pi
  and interactive shells.
- make build produced esp32/build/needle_demo.bin successfully.
- make test passed 10 Python tests and 2 native CTest tests.
- esptool flash_id connected through /dev/ttyACM0, uploaded its RAM stub,
  and detected an ESP32-S3 with 16 MB PSRAM and 32 MB octal flash.
- A direct UART !status query returned the expected agent-watch-v2 state.
- The saved needle-pi conversation resumed with all five extensions.
- needle-api started successfully and host HTTP GET /health and /state both
  returned HTTP 200 with the correct ESP32 state via 127.0.0.1:8081.
- The verification API was stopped afterwards. UART boot logs were read through
  the final device mapping, and UART DTR/RTS reset control was verified.
- A hardware reset changed the flash node inode from 913 to 923; esptool then
  reconnected successfully in the same running container. Consecutive flash_id
  checks succeeded with the compatibility fix. No firmware was written.
- Both serial ports are free, and the Pi session is idle.
  The host needle3-api.service remains disabled and inactive.


## Logout persistence repair (2026-09-18)

At 02:09:55 CDT, systemd stopped user@1000.service after the last SSH logout,
killing needle-pi. The cause was Linger=no. tmux detachment alone does not keep
the user manager alive after the final login session ends.

Fixed with loginctl enable-linger mbench01 and needle-pi enable.
Verified Linger=yes, an enabled/active needle-pi.service, and its live Podman
supervisor. The service uses Restart=on-failure. The existing conversation and
research branch survived; autoresearch was resumed from the saved context.

Normal workflow: detach with Ctrl-b then d; exit SSH normally. The user manager
and container now remain running after the last SSH session closes.
A host reboot or container crash still interrupts in-flight work. The service
starts the container again and Pi reloads its saved conversation; this does not
by itself restart an interrupted autoresearch loop. Re-enter /autoresearch with
a resume instruction after such a restart. No unattended crash-resume policy
has been added to the extension.

## Three-board pool (2026-09-18)

The installed host udev rules now match exactly these six USB identities:

- Board 1: flash 90:E5:B1:D1:C0:F8, console 5B91041931; nodes flash/console.
- Board 2: flash 90:E5:B1:D1:BE:B8, console 5B91041804; nodes board2-flash/board2-console.
- Board 3: flash 90:E5:B1:D1:C1:C8, console 5B91042020; nodes board3-flash/board3-console.

All nodes live in /dev/needle-pi, already mounted into the running container. No container recreation was required. esptool flash_id succeeded concurrently on all three; each reports ESP32-S3, 32MB flash and 16MB PSRAM.

Inside the container, /root/bin/needle-board list shows assignments. The wrapper runs a command under an exclusive per-board lock in its own checkout, e.g. /root/bin/needle-board run 2 -- bash .auto/measure.sh. /root/bin/needle-parallel -- bash .auto/measure.sh runs all three worker candidates concurrently. Workspaces, build caches and logs live under persistent home/board-pool. See home/board-pool/README.md for coordinator instructions and baseline policy.

The main measure.sh also locks board 1 for ordinary autoresearch confirmation runs. All normal measurements explicitly disable profiling, and serial selection and scratch logs are per-board. The unfinished norm-pool candidate was saved in a named Git stash and the earlier pause checkpoint. The LED-identification build was compiled separately but never flashed.
