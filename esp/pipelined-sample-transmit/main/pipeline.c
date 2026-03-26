#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"        // Step 2a: must come AFTER FreeRTOS.h
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

// ADC config (same as Quest 3, using GPIO 35 / Channel 7)
#define ADC_CHANNEL    ADC_CHANNEL_7
#define ADC_UNIT       ADC_UNIT_1
#define ADC_ATTEN_VAL  ADC_ATTEN_DB_11
#define ADC_BITWIDTH   ADC_BITWIDTH_12

static adc_oneshot_unit_handle_t adc_handle;

// Step 2b: Queue buffer size
#define TX_BUFFER_SIZE 10

// Step 2c: Global queue handle
QueueHandle_t tx_queue;

// ADC initialization (same as Quest 3)
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
}

// Step 3: TaskSample — reads ADC and pushes to queue
//
// This replaces the old sample() function.
// Instead of returning a value, it pushes the value into a queue.
//
// pvParameters is the queue handle, passed from xTaskCreate.
// We cast it to QueueHandle_t because pvParameters is void*.

void TaskSample(void *pvParameters)
{
    // Step 3c: cast the parameter to a queue handle
    QueueHandle_t output_queue = (QueueHandle_t)pvParameters;

    // Step 3d: infinite loop — read ADC, push to queue
    while (1) {
        // Read the ADC (same code as the old sample() function)
        int value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &value));

        // Push the value to the back of the queue
        // If the queue is full, retry every 10 ticks until there's space
        while (xQueueSendToBack(output_queue, &value, 10) != pdTRUE)
            ;

        // Small delay to control sampling rate
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

// Step 4: TaskTransmit — pulls from queue and prints
//
// This replaces the old printf in the main loop.
// Instead of reading a value directly, it receives values from a queue.

void TaskTransmit(void *pvParameters)
{
    // Step 4c: cast the parameter to a queue handle
    QueueHandle_t input_queue = (QueueHandle_t)pvParameters;

    // Step 4d: declare value variable
    int value;

    // Step 4e: infinite loop — receive from queue, print
    while (1) {
        // Block until a value is available in the queue
        // Retry every 10 ticks if the queue is empty
        while (xQueueReceive(input_queue, &value, 10) != pdPASS)
            ;

        // Print the received value
        printf("%d\n", value);
    }
}

// Step 5: app_main — clean, only setup and task creation
void app_main(void)
{
    // Initialize ADC
    adc_init();

    // Step 2d: create the queue (holds 10 ints)
    tx_queue = xQueueCreate(TX_BUFFER_SIZE, sizeof(int));

    // Step 3b: create the Sample task, pass tx_queue as parameter
    xTaskCreate(TaskSample, "Sample", 4096, tx_queue, 1, NULL);

    // Step 4b: create the Transmit task, pass tx_queue as parameter
    xTaskCreate(TaskTransmit, "Transmit", 4096, tx_queue, 1, NULL);

    // app_main returns here — the tasks run independently forever
}
