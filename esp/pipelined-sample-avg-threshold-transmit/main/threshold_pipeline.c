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

// Queue and window sizes
#define TX_BUFFER_SIZE    10
#define AVG_BUFFER_SIZE   10
#define THRES_BUFFER_SIZE 10
#define AVG_WINDOW_SIZE    7
#define THRESHOLD         100   // new — only forward values that differ by > 100

// Stage interface type
typedef struct {
    QueueHandle_t input;
    QueueHandle_t output;
} stage_interface_t;

// Global queues and stage interfaces
QueueHandle_t     tx_queue;
QueueHandle_t     avg_queue;
QueueHandle_t     thres_queue;
stage_interface_t avg_pair;
stage_interface_t thres_pair;

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

// TaskAvg — running average over last AVG_WINDOW_SIZE values
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
        int new_val;
        while (xQueueReceive(input_queue, &new_val, 10) != pdPASS)
            ;

        int old_val   = buffer[index];
        buffer[index] = new_val;
        sum          -= old_val;
        sum          += new_val;
        index         = (index + 1) % AVG_WINDOW_SIZE;

        int value = sum / AVG_WINDOW_SIZE;
        while (xQueueSendToBack(output_queue, &value, 10) != pdTRUE)
            ;
    }
}

// TaskThreshold — only forwards values that differ from last by > THRESHOLD
void TaskThreshold(void *pvParameters)
{
    // Cast pvParameters to stage interface pointer
    stage_interface_t *pair = (stage_interface_t *)pvParameters;
    QueueHandle_t input_queue  = pair->input;
    QueueHandle_t output_queue = pair->output;

    // Initialize last to a value guaranteed to be > THRESHOLD away from any
    // real ADC reading. ADC range is 0–4095, so 1<<16 = 65536 is safe.
    int last = 1 << 16;

    while (1) {
        // Receive averaged value from avg_queue
        int current;
        while (xQueueReceive(input_queue, &current, 10) != pdPASS)
            ;

        // Compute absolute difference from last transmitted value
        int diff = current - last;
        if (diff < 0) diff = -diff;   // abs() without including math.h

        // Only forward if change exceeds threshold
        if (diff > THRESHOLD) {
            while (xQueueSendToBack(output_queue, &current, 10) != pdTRUE)
                ;
            last = current;   // update last only when we actually transmit
        }
    }
}

// TaskTransmit — reads from thres_queue and prints
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

// app_main — setup and task creation
void app_main(void)
{
    adc_init();

    // Create all three queues
    tx_queue    = xQueueCreate(TX_BUFFER_SIZE,    sizeof(int));
    avg_queue   = xQueueCreate(AVG_BUFFER_SIZE,   sizeof(int));
    thres_queue = xQueueCreate(THRES_BUFFER_SIZE, sizeof(int));

    // Wire stage interfaces
    // TaskAvg:       reads from tx_queue,    writes to avg_queue
    // TaskThreshold: reads from avg_queue,   writes to thres_queue
    avg_pair   = (stage_interface_t){tx_queue,  avg_queue};
    thres_pair = (stage_interface_t){avg_queue, thres_queue};

    // Create tasks — priority 0 (see Step 4 for why)
    xTaskCreate(TaskSample,    "Sample",    4096, tx_queue,    0, NULL);
    xTaskCreate(TaskAvg,       "Avg",       4096, &avg_pair,   0, NULL);
    xTaskCreate(TaskThreshold, "Threshold", 4096, &thres_pair, 0, NULL);
    xTaskCreate(TaskTransmit,  "Transmit",  4096, thres_queue, 0, NULL);
}