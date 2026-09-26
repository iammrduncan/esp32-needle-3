# Serial transport, reset, and state

Serial transport is part of the measured system. It controls request framing,
device reset state, ordering, retry semantics, and which bytes the firmware sees.
The campaign's two unresolved goldens are the strongest warning against calling
it “just I/O.”

## Physical endpoints

The tested boards expose separate flash/USB-JTAG and console ports. Early
single-board defaults use `/dev/ttyACM0` and `/dev/ttyACM1`; the three-board lab
uses stable `/dev/needle-pi/boardN-*` nodes. Always pass the wrapper-provided
`FLASH_PORT` and `SERIAL_PORT` and verify that both resolve to the same board.
Raw ACM numbering can change after reset/re-enumeration.

Only one process may own a console. The board-pool lock, bridge, monitor, test
harness, and leftover child processes can all contend for it. Stop bridge
children by PID; killing only a parent process name repeatedly left the port
claimed.

## Safe attach

On this ESP32-S3 setup, DTR/RTS transitions are connected to boot/reset control.
PySerial may assert them while opening unless configured first. The safe order
implemented by [`tools/serial_api.py`](../../../tools/serial_api.py) is:

1. construct `serial.Serial()` closed;
2. set port, baud, timeouts, and exclusive ownership;
3. set `dtr=False` and `rts=False`;
4. open;
5. set both false again.

Opening differently can reset the application or strand it in ROM download
mode, which looks like dead firmware. Do not diagnose a kernel wedge until the
attach path is known safe.

## Protocol shape

Requests are newline-terminated, single-line UTF-8. `!route ` selects the route
schema; unprefixed text uses the local-tool schema. Control commands include
status and think-mode toggles. Inference replies are line records ending at an
explicit `END`; `!status` emits one `STATE` record and `!think` emits only its
acknowledgement. Token fragments preserve spaces—using `.strip()` would corrupt
output.

The protocol has no transaction ID. Correct pairing therefore depends on
draining stale lines before a write and reading through the terminator. A
leftover `END`, state line, or toggle acknowledgement can shift every following
case and even make a response appear to come from the wrong schema.

## Startup probes and log integrity

The bridge may attach while the firmware is still priming. A status probe sent
during work can later be echoed/interleaved with boot output. The implementation
tracks credit for its own probes and marks a startup log noisy when a line can
only be an echoed probe reply. Otherwise a benchmark can parse transport noise
as boot evidence.

`reset_input_buffer()` alone is insufficient because already-delivered lines
may be in a higher-level reader. The retained bounded drain reads and surfaces
stale lines before sending the next command. A settle-to-quiet algorithm was
tested and regressed breadth; a fixed short drain plus explicit warning stayed.

## RX ring and the two disputed cases

A lossless firmware RX-ring change fixed requests longer than the UART's
practical direct-read window (>128 bytes). With engine bytes unchanged it also
flipped exactly:

- `heldout_interval_one`;
- `heldout_long_tools_note_only`.

The final trees are consequently 18/20 device exact. Evidence against a numeric
kernel bug is substantial: the host exactly reproduced the device's
`interval_one` answer and showed the same repeated-tool tendency (not identical
bytes) for long-tools; each image is deterministic and order-independent;
passing cases have token delta zero; and a 10-request B3 soak completed with
five repeated tool calls byte-identical. But the
old oracle cannot be rewritten automatically. Owner choices are to retain the
ring and re-baseline/replace the two fixtures, remove it and lose correct long
request support/the 20-case suite, or keep both as release blockers.

## Timeout and retry policy

A request timeout marks the connection unavailable. The bridge retries the
same case once after closing/reopening safely and proving state. It never skips
the case. A retry marker is included in the result so a recovered transport
failure cannot masquerade as a clean first attempt.

Hard reset is opt-in for diagnostics because it destroys request history and
forces multi-minute prefix priming. Mid-suite reset changes timer/sampler state
and was correlated with the two state-sensitive held-out cases. A gated run
must not combine pre- and post-reset cases as one context.

When a hard reset is necessary:

- verify flash and console nodes name the same board;
- close the console;
- keep IO0/DTR in normal-boot state;
- pulse EN/RTS;
- reopen with safe modem-line state;
- wait for both prefix caches and a fresh readiness proof.

## Retracted causal story

An early stdout-stall/heartbeat story was retracted because inference reads
used `_line_quiet`, suppressing the relevant logging. Later FIFO/request-length
and RX-ring experiments supplied the useful discriminator. Preserve the
retraction: a plausible serial story is not evidence until its observable can
actually be present on the tested path.

## Error taxonomy

| Symptom | First checks |
|---|---|
| No boot/application text | DTR/RTS attach, ROM download mode, correct console node |
| Port busy | pool lock, bridge/service, orphaned `serial_api.py`, monitor |
| Wrong-schema-looking answer | stale drain, missing prior `END`, think-toggle ack |
| Late-suite timeout | console re-enumeration, reader ownership, firmware progress |
| Case changes after recovery | hard reset and persistent timer/sampler/prefix state |
| Flash fails while console works | correct flash leg, boot-mode reset sequence |
| Apparent board hang during priming | growing log/progress, long boot timeout |

## Reusable transport requirements

For a new runtime, add explicit request IDs and length/checksum framing if the
firmware budget permits. Until then:

- serialize transactions;
- reject newlines/control prefixes in input;
- bound byte length and response time;
- drain visibly, never silently;
- require an explicit terminator and complete metric/call/result set;
- keep reset separate from ordinary retry;
- record reconnect/reset markers in benchmark output;
- define fixture reset/order/state policy;
- validate long requests above the hardware FIFO size;
- keep board identity stable across both serial legs.
