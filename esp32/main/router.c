/* Real, bounded operations for a telemetry node. No external sensors needed. */
#include "router.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static esp_timer_handle_t sample_timer, countdown;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static struct {
    uint32_t period, samples, free_heap, fired;
    int64_t sampled_at, deadline;
    int timer_state; /* 0 idle, 1 running, 2 expired */
} state = { .period = 30 };
static uint32_t model_layers;
static size_t archive_bytes;

static void sample(void *unused)
{
    uint32_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&mux);
    state.samples++;
    state.free_heap = free_heap;
    state.sampled_at = now;
    portEXIT_CRITICAL(&mux);
}

static void expire(void *unused)
{
    portENTER_CRITICAL(&mux);
    state.timer_state = 2;
    state.fired++;
    portEXIT_CRITICAL(&mux);
}

static cJSON *snapshot(void)
{
    int64_t now = esp_timer_get_time(), sampled_at, deadline;
    uint32_t period, samples, free_heap, fired;
    int status;
    portENTER_CRITICAL(&mux);
    period = state.period; samples = state.samples;
    free_heap = state.free_heap; sampled_at = state.sampled_at;
    deadline = state.deadline; fired = state.fired;
    status = state.timer_state;
    portEXIT_CRITICAL(&mux);
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "device", "esp32-s3");
    cJSON_AddStringToObject(o, "schema", "telemetry-router-v1");
    cJSON_AddNumberToObject(o, "layers", model_layers);
    cJSON_AddNumberToObject(o, "model_bytes", archive_bytes);
    cJSON_AddNumberToObject(o, "uptime_ms", now / 1000);
    cJSON_AddNumberToObject(o, "free_internal_bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(o, "free_psram_bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(o, "sample_period_s", period);
    cJSON_AddNumberToObject(o, "samples", samples);
    cJSON_AddNumberToObject(o, "last_sample_uptime_ms", sampled_at / 1000);
    cJSON_AddNumberToObject(o, "last_sample_free_internal_bytes", free_heap);
    cJSON_AddStringToObject(o, "timer_status", status == 1 ? "running" : status == 2 ? "expired" : "idle");
    cJSON_AddNumberToObject(o, "timer_remaining_ms", status == 1 && deadline > now ? (deadline - now) / 1000 : 0);
    cJSON_AddNumberToObject(o, "timer_fired_count", fired);
    return o;
}

static void emit(const char *tag, const cJSON *object)
{
    char *json = cJSON_PrintUnformatted(object);
    if (json) { printf("%s %s\n", tag, json); free(json); }
    else printf("ERR json_allocation\n");
    fflush(stdout);
}

void router_print_state(void)
{
    cJSON *o = snapshot();
    emit("STATE", o);
    cJSON_Delete(o);
}

void router_init(uint32_t layers, size_t model_bytes)
{
    model_layers = layers;
    archive_bytes = model_bytes;
    esp_timer_create_args_t a = { .callback = sample, .name = "telemetry" };
    ESP_ERROR_CHECK(esp_timer_create(&a, &sample_timer));
    a.callback = expire; a.name = "countdown";
    ESP_ERROR_CHECK(esp_timer_create(&a, &countdown));
    sample(NULL);
    ESP_ERROR_CHECK(esp_timer_start_periodic(sample_timer, 30000000));
}

/* Validate every call before executing any. Grammar is only the first check. */
static int valid(const cJSON *call)
{
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(call, "name");
    const cJSON *args = cJSON_GetObjectItemCaseSensitive(call, "arguments");
    if (!cJSON_IsString(name) || !cJSON_IsObject(args)) return 0;
    if (!strcmp(name->valuestring, "get_status")) return cJSON_GetArraySize(args) == 0;
    if (strcmp(name->valuestring, "set_sampling_interval") && strcmp(name->valuestring, "set_timer")) return 0;
    const cJSON *seconds = cJSON_GetObjectItemCaseSensitive(args, "seconds");
    return cJSON_GetArraySize(args) == 1 && cJSON_IsNumber(seconds) &&
           isfinite(seconds->valuedouble) && seconds->valuedouble >= 1 &&
           seconds->valuedouble <= 3600 && floor(seconds->valuedouble) == seconds->valuedouble;
}

void router_dispatch(const char *generated)
{
    const char *start = strstr(generated, "<tool_call>");
    const char *end = start ? strstr(start, "</tool_call>") : NULL;
    if (!start || !end) { printf("ERR incomplete_call\nEND\n"); fflush(stdout); return; }
    start += strlen("<tool_call>");
    const char *parsed_end = NULL;
    cJSON *calls = cJSON_ParseWithLengthOpts(start, end - start, &parsed_end, 0);
    if (!cJSON_IsArray(calls) || cJSON_GetArraySize(calls) > 16) goto invalid;
    while (parsed_end < end && (*parsed_end == ' ' || *parsed_end == '\n')) parsed_end++;
    if (parsed_end != end) goto invalid;
    const cJSON *call;
    cJSON_ArrayForEach(call, calls) if (!valid(call)) goto invalid;
    emit("JSON", calls);
    cJSON *results = cJSON_CreateArray();
    cJSON_ArrayForEach(call, calls) {
        const char *name = cJSON_GetObjectItemCaseSensitive(call, "name")->valuestring;
        esp_err_t err = ESP_OK;
        if (strcmp(name, "get_status")) {
            const cJSON *args = cJSON_GetObjectItemCaseSensitive(call, "arguments");
            int seconds = cJSON_GetObjectItemCaseSensitive(args, "seconds")->valueint;
            int sampling = !strcmp(name, "set_sampling_interval");
            esp_timer_handle_t timer = sampling ? sample_timer : countdown;
            err = esp_timer_stop(timer);
            if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
                err = sampling ? esp_timer_start_periodic(timer, (uint64_t)seconds * 1000000) :
                                 esp_timer_start_once(timer, (uint64_t)seconds * 1000000);
            }
            if (err == ESP_OK) {
                int64_t deadline = esp_timer_get_time() + (int64_t)seconds * 1000000;
                portENTER_CRITICAL(&mux);
                if (sampling) state.period = seconds;
                else { state.deadline = deadline; state.timer_state = 1; }
                portEXIT_CRITICAL(&mux);
            }
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", name);
        cJSON_AddBoolToObject(item, "success", err == ESP_OK);
        if (err != ESP_OK) cJSON_AddStringToObject(item, "error", esp_err_to_name(err));
        cJSON_AddItemToObject(item, "state", snapshot());
        cJSON_AddItemToArray(results, item);
    }
    emit("RESULT", results);
    cJSON_Delete(results);
    cJSON_Delete(calls);
    printf("END\n"); fflush(stdout);
    return;
invalid:
    cJSON_Delete(calls);
    printf("ERR invalid_call\nEND\n"); fflush(stdout);
}
