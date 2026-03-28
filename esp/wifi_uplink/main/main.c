#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "hl_wifi.h"

static const char *TAG = "main";

// ============================================================
// CHANGE THIS to your Mac's IP address on the hotspot network
// ============================================================
#define SERVER_IP   "172.20.10.13"
#define SERVER_PORT 8000

// ============================================================
// Step 4 & 8: TaskCount — connects and sends a counter
// ============================================================

void TaskCount(void *pvParameters)
{
    // Step 5d: create endpoint address
    sockaddr_in_t addr = hl_wifi_make_addr(SERVER_IP, SERVER_PORT);

    // Step 6c: connect to the server
    int sock = hl_wifi_tcp_connect(addr);
    if (sock == -1) {
        ESP_LOGE(TAG, "Failed to connect to %s:%d", SERVER_IP, SERVER_PORT);
        vTaskDelete(NULL);  // remove this task — does not return
    }

    // Step 8: send an incrementing counter every second
    uint16_t counter = 1;
    char buffer[7];  // max "65535\n" + null = 7 bytes

    while (1) {
        // Format the counter as a string with newline
        sprintf(buffer, "%u\n", counter);

        // Transmit over TCP
        hl_wifi_tcp_tx(sock, buffer, strlen(buffer));

        ESP_LOGI(TAG, "Sent: %u", counter);
        counter++;

        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 1 second delay
    }
}

// ============================================================
// Step 3b: callback — called when WiFi connects
// ============================================================

void connected_callback(void)
{
    ESP_LOGI(TAG, "WiFi connected! Starting TaskCount...");

    // Step 4: create the task from the callback
    xTaskCreate(TaskCount, "TaskCount", 4096, NULL, 5, NULL);
}

// ============================================================
// app_main
// ============================================================

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing WiFi...");

    // Step 3c-i: pass the callback to hl_wifi_init
    hl_wifi_init(connected_callback);
}