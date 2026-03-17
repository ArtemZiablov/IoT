#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#define BLINK_GPIO 2  // GPIO 2 = onboard LED on most ESP32 boards
                      // Change to 13 if using an external LED on GPIO 13

void hello_task(void *pvParameter)
{
    while (1) {
        printf("Hello world!\n");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void blinky(void *pvParameter)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while (1) {
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    nvs_flash_init();
    xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
    xTaskCreate(&blinky, "blinky", 2048, NULL, 5, NULL);
}