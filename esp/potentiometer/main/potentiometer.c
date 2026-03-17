#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

// We use ADC1 Channel 6 = GPIO 34 on ESP32
#define ADC_CHANNEL    ADC_CHANNEL_6
#define ADC_UNIT       ADC_UNIT_1
#define ADC_ATTEN      ADC_ATTEN_DB_11    // full range 0-3.3V
#define ADC_BITWIDTH   ADC_BITWIDTH_12    // 12-bit: values 0-4095

static const char *TAG = "POTENTIOMETER";

// Global ADC handle — initialized once, used everywhere
static adc_oneshot_unit_handle_t adc_handle;

// ============================================================
// ADC initialization
// ============================================================
// Three steps:
//   1. Create a unit handle for ADC1
//   2. Configure the channel (attenuation + bitwidth)
//   After this, we can call adc_oneshot_read() anytime.

static void adc_init(void)
{
    // Step 1: create ADC1 unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // Step 2: configure channel 6 with 12dB attenuation, 12-bit width
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_config));

    ESP_LOGI(TAG, "ADC1 Channel %d initialized (GPIO 34)", ADC_CHANNEL);
}

// ============================================================
// Step 5: sample function (refactored from the loop)
// ============================================================
// Performs a single ADC conversion and returns the raw value (0-4095).
// The return type is int because that's what adc_oneshot_read() expects.

static int sample(void)
{
    int raw_value;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_value));
    return raw_value;
}

// ============================================================
// Main
// ============================================================

void app_main(void)
{
    adc_init();

    // Step 3: continuously read and print raw ADC values
    while (1) {
        int value = sample();
        printf("%d\n", value);
        vTaskDelay(200 / portTICK_PERIOD_MS);  // read 5 times per second
    }
}
