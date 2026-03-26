#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

// GPIO 35 = ADC1 Channel 7
#define ADC_CHANNEL    ADC_CHANNEL_7
#define ADC_UNIT       ADC_UNIT_1
#define ADC_ATTEN_VAL  ADC_ATTEN_DB_11    // full 0-3.3V range
#define ADC_BITWIDTH   ADC_BITWIDTH_12    // 12-bit: 0-4095

// Reference voltage in mV (3.3V with 11dB attenuation)
#define V_REF_MV       3300

// LMT86 linear approximation constants (from datasheet Table, 0-50°C range)
// At T1=0°C,  V1=2069 mV
// At T2=50°C, V2=1748 mV
// Slope = (V2-V1)/(T2-T1) = (1748-2069)/(50-0) = -321/50 = -6.42 mV/°C
#define V1_MV          2069    // voltage at T1
#define T1_C           0       // T1 in Celsius
#define SLOPE_NUM      (-321)  // numerator of slope (V2-V1)
#define SLOPE_DEN      50      // denominator of slope (T2-T1)

static const char *TAG = "TEMPERATURE";
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

    ESP_LOGI(TAG, "ADC1 Channel %d initialized (GPIO 35)", ADC_CHANNEL);
}

// ============================================================
// Step 5 (Quest 1): sample — read one raw ADC value
// ============================================================

static int sample(void)
{
    int raw_value;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_value));
    return raw_value;
}

// ============================================================
// Step 3: convert — raw ADC value to temperature in °C
// ============================================================
//
// The conversion happens in two stages:
//
// Stage 1: raw ADC → voltage in mV
//   voltage_mV = (raw * 3300) / 4095
//
// Stage 2: voltage → temperature in °C
//   Using the linear two-point formula from the LMT86 datasheet:
//   T = T1 + (V - V1) / slope
//   T = 0 + (voltage_mV - 2069) / (-6.42)
//   T = (2069 - voltage_mV) / 6.42
//
// To avoid floating point, we use integer math:
//   T = T1 + (voltage_mV - V1) * SLOPE_DEN / SLOPE_NUM
//
// The order of operations matters for precision:
// multiply before divide to keep as much precision as possible.

static int8_t convert(int raw)
{
    // Stage 1: raw ADC → voltage in mV
    // raw=0 → 0 mV, raw=4095 → 3300 mV
    int voltage_mV = (raw * V_REF_MV) / 4095;

    // Stage 2: voltage → temperature using integer math
    // T = T1 + (voltage_mV - V1) * SLOPE_DEN / SLOPE_NUM
    // Note: SLOPE_NUM is negative (-321), so the division flips the sign correctly
    // (voltage below V1 means positive temperature, voltage above V1 means negative)
    int temp = T1_C + ((voltage_mV - V1_MV) * SLOPE_DEN) / SLOPE_NUM;

    return (int8_t)temp;
}

// ============================================================
// Main — Step 4: continuous reading (like Quest 1)
// ============================================================

void app_main(void)
{
    adc_init();

    while (1) {
        int raw = sample();
        int8_t temp_c = convert(raw);

        // Print both raw and converted values for verification
        /*printf("Raw: %d, Voltage: %d mV, Temperature: %d °C\n",
               raw,
               (raw * V_REF_MV) / 4095,
               temp_c);*/
        printf("%d\n", raw);

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}