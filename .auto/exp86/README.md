# Console-stall discriminator (prepared, not yet run)

## Question
Seven gate sessions stop mid-suite with the host-side signature "inference exceeded
request timeout" while the app's own emit counters froze at `chars=816 lines=17`
(run #418) and only a chip reset recovers. Two candidate mechanisms remain:
  A. the stdout/console TX path stops accepting bytes, or
  B. the request/console RX path stops accepting input while TX still works.
They are distinguished by whether ANY further emission happens after the stall.

## Patch (diagnostic only; no shipping behaviour change intended to be kept)
Add to `esp32/main/main.c`, next to the other statics:

    /* Diagnostic only: proves whether the console TX path is still able to emit
     * after a request-path stall. Runs unconditionally once a request is in
     * flight, so it answers A vs B without needing the console to be readable. */
    static volatile uint32_t s_tbeat;
    static void tbeat_task(void *arg) {
        (void)arg;
        for (;;) { s_tbeat++; vTaskDelay(pdMS_TO_TICKS(200)); }
    }

Start it right before the request loop (`for (;;)` that reads lines):

    xTaskCreatePinnedToCore(tbeat_task, "tbeat", 2048, NULL, 3, NULL, 1);

and print, from inside `run_inference` immediately before returning its result AND
after the emit block, one line:

    printf("EVT tbeat=%lu chars=%lu\n", (unsigned long)s_tbeat, (unsigned long)s_emit_chars);

Then on the host, drive N+3 requests (past the observed 16-17 ceiling) and read the
console. Decision rule:
  * `EVT tbeat=` keeps advancing after the last `EVT done`  -> TX alive: the stall is
    in the RX/request path (B). Look at the line reader and its DTR/EOF handling.
  * `EVT tbeat=` stops with it -> the console write path itself is stuck (A). Then the
    suspect is the UART0-backed stdout/VFS write (per-event `fflush(stdout)`, #475),
    and the fix is a bounded non-blocking emit or a dedicated console task.

## Harvest
Run as a lane so the board is primed by the harness. Read the console with
`tools/serial_api.py`'s `Device` (DTR asserted), NOT a bare pyserial reader (#331 fact
4). Board nodes come from the wrapper's `$SERIAL_PORT` / `$FLASH_PORT` - never the
`/dev/ttyACM*` aliases (#331 fact 2). Reset with the console port closed (#331 fact 3).

## Cost / risk
One build, one flash, ~10 min. Nothing in the inference path changes; the counter task
is pinned to core 1, which runs only the splitter worker, so it can perturb timing a
little - it is a diagnostic image and must never be measured for tok/s (#410 rule).
