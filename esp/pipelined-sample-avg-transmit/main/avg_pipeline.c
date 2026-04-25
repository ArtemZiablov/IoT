#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

// ADC config
#define ADC_CHANNEL   ADC_CHANNEL_7
#define ADC_UNIT      ADC_UNIT_1
#define ADC_ATTEN_VAL ADC_ATTEN_DB_11
#define ADC_BITWIDTH  ADC_BITWIDTH_12

static adc_oneshot_unit_handle_t adc_handle;

// Queue sizes
#define TX_BUFFER_SIZE  10
#define AVG_BUFFER_SIZE 10
#define AVG_WINDOW_SIZE 7

// Stage interface type
typedef struct {
    QueueHandle_t input;
    QueueHandle_t output;
} stage_interface_t;

// Global queues and stage interface
QueueHandle_t     tx_queue;
QueueHandle_t     avg_queue;
stage_interface_t avg_pair;

// ADC init
static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .atten    = ADC_ATTEN_VAL,
        .bitwidth = ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_config));
}

// TaskSample — reads ADC, pushes raw values to tx_queue
void TaskSample(void *pvParameters)
{
    QueueHandle_t output_queue = (QueueHandle_t)pvParameters;

    while (1) {
        int value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &value));
        while (xQueueSendToBack(output_queue, &value, 10) != pdTRUE)
            ;
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

// TaskAvg — reads raw values from input queue, writes running average to output queue
void TaskAvg(void *pvParameters)
{
    stage_interface_t *pair = (stage_interface_t *)pvParameters;
    QueueHandle_t input_queue  = pair->input;
    QueueHandle_t output_queue = pair->output;

    int     buffer[AVG_WINDOW_SIZE];
    memset(buffer, 0, AVG_WINDOW_SIZE * sizeof(buffer[0]));
    uint8_t index = 0;
    int     sum   = 0;

    while (1) {
        // Receive
        int new_val;
        while (xQueueReceive(input_queue, &new_val, 10) != pdPASS)
            ;

        // Update circular buffer and running sum
        int old_val   = buffer[index];
        buffer[index] = new_val;
        sum          -= old_val;
        sum          += new_val;
        index         = (index + 1) % AVG_WINDOW_SIZE;

        // Output average
        int value = sum / AVG_WINDOW_SIZE;
        while (xQueueSendToBack(output_queue, &value, 10) != pdTRUE)
            ;
    }
}

// TaskTransmit — reads averaged values from avg_queue and prints
void TaskTransmit(void *pvParameters)
{
    QueueHandle_t input_queue = (QueueHandle_t)pvParameters;
    int value;

    while (1) {
        while (xQueueReceive(input_queue, &value, 10) != pdPASS)
            ;
        printf("%d\n", value);
    }
}

void app_main(void)
{
    adc_init();

    // Create both queues
    tx_queue  = xQueueCreate(TX_BUFFER_SIZE,  sizeof(int));
    avg_queue = xQueueCreate(AVG_BUFFER_SIZE, sizeof(int));

    // Wire the stage interface: TaskAvg reads from tx_queue, writes to avg_queue
    avg_pair = (stage_interface_t){tx_queue, avg_queue};

    // TaskSample writes raw ADC values to tx_queue
    xTaskCreate(TaskSample,  "Sample",   4096, tx_queue,   1, NULL);

    // TaskAvg reads from tx_queue (via avg_pair), writes averaged values to avg_queue
    xTaskCreate(TaskAvg,     "Avg",      4096, &avg_pair,  1, NULL);

    // TaskTransmit reads from avg_queue (averaged values)
    xTaskCreate(TaskTransmit, "Transmit", 4096, avg_queue,  1, NULL);
}