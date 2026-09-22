/* Experiment 21: die-temperature and PSRAM-integrity logger, for the 120 MHz
 * octal-memory stability campaign. Built only with -DNEEDLE_THERMAL_DIAG=ON,
 * which is also the only reason the `main` component asks for `esp_driver_tsens`
 * - with the option OFF the component list, and the shipping image, are
 * unchanged.
 *
 * Why it exists: IDF's own risk statement for octal PSRAM (and for flash) at
 * 120 MHz is that accesses "crash randomly" after roughly a 20 Celsius swing
 * from the temperature the chip powered on at. A soak is only evidence if it
 * reports the temperature range it actually covered; without a number, "no
 * crash in 30 minutes" is unfalsifiable. The running model already streams
 * 3.59 MB of 2-bit weights through PSRAM and the flash mapping every token, and
 * the byte-exact suite proves what came out - so this file adds the two things
 * the suite cannot see: the die temperature, and a witness that reads
 * continuously even while the board is idle.
 */
#include <stdio.h>

#include "driver/temperature_sensor.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_rom_crc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TD_BYTES  (256u * 1024u)   /* PSRAM pattern window             */
#define TD_EVERY_MS 2000           /* one line every 2 s               */

static void td_task(void *arg);

void nd_thermal_diag_start(void)
{
    xTaskCreate(td_task, "therm_diag", 3072, NULL, 2, NULL);
}

static void td_task(void *arg)
{
    temperature_sensor_handle_t ts = NULL;
    uint8_t *pat = (uint8_t *)heap_caps_malloc(TD_BYTES, MALLOC_CAP_SPIRAM);
    /* -20..100 C fails install on this part (measured: celsius=-999 forever);
     * the ESP32-S3 sensor's documented range is -10..80 C. */
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    uint32_t crc0 = 0, bad = 0;
    float c, cmin = 999.0f, cmax = -999.0f;

    (void)arg;
    for (uint32_t i = 0; i < TD_BYTES; i++)
        pat[i] = (uint8_t)(i * 31u + (i >> 8) * 7u);
    esp_err_t e1 = temperature_sensor_install(&cfg, &ts);
    esp_err_t e2 = ESP_OK;

    /* The codes are reported rather than a fake temperature: a sensor that
     * fails to start must be visible in the log, not read as -999 C. */
    printf("THERDIAG armed install=%s enable_pending\n", esp_err_to_name(e1));
    fflush(stdout);
    if (e1 == ESP_OK) {
        e2 = temperature_sensor_enable(ts);
        if (e2 != ESP_OK) {
            printf("THERDIAG enable=%s\n", esp_err_to_name(e2));
            fflush(stdout);
        }
    }
    if (e1 != ESP_OK || e2 != ESP_OK) {
        if (e1 == ESP_OK)
            temperature_sensor_uninstall(ts);
        ts = NULL;
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(TD_EVERY_MS));
        c = 0.0f;
        if (ts == NULL || temperature_sensor_get_celsius(ts, &c) != ESP_OK)
            c = -999.0f;                      /* no sensor: report, do not lie */
        if (c > -900.0f) {
            if (c < cmin) cmin = c;
            if (c > cmax) cmax = c;
        }
        {
            uint32_t crc = pat ? esp_rom_crc32_le(~0u, pat, TD_BYTES) : 0;

            if (crc0 == 0)
                crc0 = crc;
            else if (crc != crc0)
                bad++;                        /* silent PSRAM corruption */
            printf("TDIAG celsius=%.1f cmin=%.1f cmax=%.1f psram_crc=%08x "
                   "psram_crc_bad=%u internal_free=%u psram_free=%u\n",
                   (double)c, (double)cmin, (double)cmax, (unsigned)crc0,
                   (unsigned)bad,
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            fflush(stdout);
        }
    }
}
