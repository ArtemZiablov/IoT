#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

/*
#define INPUT_PIN  27    // GPIO 27 — safe pin for input
#define POLL_MS    50    // check every 50ms

void app_main(void)
{
    // Configure GPIO 27 as input with internal pull-down
    gpio_reset_pin(INPUT_PIN);
    gpio_set_direction(INPUT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(INPUT_PIN, GPIO_PULLDOWN_ONLY);

    // Read initial state
    int previous = gpio_get_level(INPUT_PIN);

    printf("Reporter ready. Watching GPIO %d...\n", INPUT_PIN);

    while (1) {
        int current = gpio_get_level(INPUT_PIN);

        if (current != previous) {
            if (current == 1) {
                printf("1\n");   // rise: low -> high
            } else {
                printf("0\n");   // fall: high -> low
            }
            previous = current;
        }

        vTaskDelay(POLL_MS / portTICK_PERIOD_MS);
    }
}
*/

#define INPUT_PIN  27

// A queue to safely pass events from the ISR to the main task
static QueueHandle_t gpio_evt_queue = NULL;

// Interrupt Service Routine (ISR)
// Called by hardware when GPIO changes. Must be very short — no printf here!
// Instead, send the pin number to a queue for the main task to process.
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// Task that processes GPIO events from the queue
static void gpio_task(void *arg)
{
    uint32_t gpio_num;

    while (1) {
        // Block until an event arrives from the ISR
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY)) {
            int level = gpio_get_level(gpio_num);
            printf("%d\n", level);
        }
    }
}

void app_main(void)
{
    // Configure GPIO 27 as input with pull-down
    gpio_reset_pin(INPUT_PIN);
    gpio_set_direction(INPUT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(INPUT_PIN, GPIO_PULLDOWN_ONLY);

    // Trigger interrupt on BOTH edges (rise and fall)
    gpio_set_intr_type(INPUT_PIN, GPIO_INTR_ANYEDGE);

    // Create a queue for GPIO events (holds up to 10 events)
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start the task that will process events
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    // Install the GPIO ISR service and attach our handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(INPUT_PIN, gpio_isr_handler, (void *)INPUT_PIN);

    printf("Reporter (interrupt) ready. Watching GPIO %d...\n", INPUT_PIN);
}