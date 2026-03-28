#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_adc/adc_oneshot.h"

#include "hl_wifi.h"

static const char *TAG = "main";

// ============================================================
// Network configuration
// ============================================================
#define SERVER_IP   "172.20.10.13"
#define SERVER_PORT 8000

// ============================================================
// ADC configuration (same as Sampling Quest 3)
// GPIO 35 = ADC1 Channel 7, LMT86 temperature sensor
// ============================================================
#define ADC_CHANNEL    ADC_CHANNEL_7
#define ADC_UNIT       ADC_UNIT_1
#define ADC_ATTEN_VAL  ADC_ATTEN_DB_11
#define ADC_BITWIDTH   ADC_BITWIDTH_12
#define V_REF_MV       3300

// LMT86 linear approximation (0°C to 50°C range)
#define V1_MV          2069
#define T1_C           0
#define SLOPE_NUM      (-321)
#define SLOPE_DEN      50

static adc_oneshot_unit_handle_t adc_handle;

// ============================================================
// ADC initialization
// ============================================================
static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_VAL,
        .bitwidth = ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_config));
    ESP_LOGI(TAG, "ADC initialized on channel %d", ADC_CHANNEL);
}

// ============================================================
// sample — read one raw ADC value
// ============================================================
static int sample(void)
{
    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw));
    return raw;
}

// ============================================================
// convert — raw ADC value to temperature in °C
// ============================================================
static int8_t convert(int raw)
{
    int voltage_mV = (raw * V_REF_MV) / 4095;
    int temp = T1_C + ((voltage_mV - V1_MV) * SLOPE_DEN) / SLOPE_NUM;
    return (int8_t)temp;
}

// ============================================================
// TaskTemperature — reads sensor and sends over TCP
// ============================================================
void TaskTemperature(void *pvParameters)
{
    // Create endpoint address
    sockaddr_in_t addr = hl_wifi_make_addr(SERVER_IP, SERVER_PORT);

    // Connect to the server
    int sock = hl_wifi_tcp_connect(addr);
    if (sock == -1) {
        ESP_LOGE(TAG, "Failed to connect to %s:%d", SERVER_IP, SERVER_PORT);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Connected! Sending temperature readings...");

    char buffer[16];

    while (1) {
        // Read temperature
        int raw = sample();
        int8_t temp_c = convert(raw);

        // Format as string: e.g. "23\n"
        sprintf(buffer, "%d\n", temp_c);

        // Send over TCP
        hl_wifi_tcp_tx(sock, buffer, strlen(buffer));

        ESP_LOGI(TAG, "Raw: %d, Temp: %d°C, Sent: %s",
                 raw, temp_c, buffer);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// ============================================================
// Callback — called when WiFi connects
// ============================================================
void connected_callback(void)
{
    ESP_LOGI(TAG, "WiFi connected! Starting temperature task...");
    xTaskCreate(TaskTemperature, "Temperature", 4096, NULL, 5, NULL);
}

// ============================================================
// app_main
// ============================================================
void app_main(void)
{
    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize ADC (before WiFi, so it's ready when the task starts)
    adc_init();

    // Initialize WiFi and connect
    ESP_LOGI(TAG, "Initializing WiFi...");
    hl_wifi_init(connected_callback);
}