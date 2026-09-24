import pathlib, sys
p = pathlib.Path(sys.argv[1]); s = p.read_text()
assert 'uart_driver_install' not in s, 'already applied'
ls = s.splitlines(True); last = max(n for n,l in enumerate(ls) if l.startswith('#include'))
i = sum(len(x) for x in ls[:last+1])
fn = '''
#include "driver/uart.h"
#include "esp_vfs_dev.h"

/* The default console VFS is POLLED: stdin reads a 128-byte software FIFO and
 * getchar() sleeps 20 ms on EOF. At 115200 8N1 a 20 ms window admits ~230 wire
 * bytes, so a request longer than the FIFO loses characters, the terminating
 * newline never arrives, and the reader waits for the rest of a line that has
 * already been dropped. The frozen suite's cases 1-16 are all <= 89 bytes; case
 * 17 is the FIRST request over 128 bytes (232), and the note-only tools case is
 * 135 - which is exactly the observed 16-17 requests-per-boot ceiling, and why
 * no amount of reconnecting recovered it.
 *
 * Installing the UART driver replaces that FIFO with an ISR-fed ring, and
 * esp_vfs_dev_uart_use_driver() routes stdin through it. Bytes are retained
 * rather than dropped, so this is lossless rather than a bigger drop window. */
static void console_rx_ring_enable(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &cfg));
    esp_vfs_dev_uart_use_driver(UART_NUM_0);
}
'''
s = s[:i] + fn + s[i:]
k = s.index('void app_main(void)'); b = s.index('\n', k) + 1
anchor = s.find('for (;;)', b)
assert anchor > 0
ins = '\n    console_rx_ring_enable();   /* before any console input is read */\n'
# insert at the start of the last function containing the read loop, i.e. just after its opening brace
head = max(s.rfind('\n{', 0, anchor), s.rfind('{\n', 0, anchor))
brace = s.rfind('{', 0, anchor)
s = s[:brace+1] + ins + s[brace+1:]
p.write_text(s); print('RXRING_OK decl_before_use=%d' % (s.index('console_rx_ring_enable(void)') < s.index('console_rx_ring_enable();')))
