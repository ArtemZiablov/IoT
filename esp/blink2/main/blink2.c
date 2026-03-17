#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

typedef struct {
    int pin;
    int delay_ms;
} blink_params_t;

void TaskBlink(void *pvParameters)
{
    blink_params_t *params = (blink_params_t *)pvParameters;
    uint8_t state = 1;

    gpio_reset_pin(params->pin);
    gpio_set_direction(params->pin, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(params->pin, state);
        vTaskDelay(params->delay_ms / portTICK_PERIOD_MS);
        state = !state;
    }
}

// Static so they stay in memory while tasks run
static blink_params_t led1_params = { .pin = 33, .delay_ms = 500 };
static blink_params_t led2_params = { .pin = 13, .delay_ms = 200 };

void app_main(void)
{
    xTaskCreate(TaskBlink, "Blink1", 4096, &led1_params, 1, NULL);
    xTaskCreate(TaskBlink, "Blink2", 4096, &led2_params, 1, NULL);
}