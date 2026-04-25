#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#include "hl_wifi.h"

static const char *TAG = "main";

#define BROKER_URL "mqtt://broker.hivemq.com"
#define TOPIC      "org/sdu/course/iot/year/2026/chat/channel/42"

static char nick[10];
static esp_mqtt_client_handle_t client;

void TaskChat(void *pvParameters);

// ============================================================
// TaskChat — prompts for nickname, then loops reading serial
// ============================================================

void TaskChat(void *pvParameters)
{
    char line[128];
    char buffer[150];

    // Get nickname here — this task has proper stdio access
    printf("Enter your nickname (max 9 chars): ");
    fflush(stdout);

    if (fgets(nick, sizeof(nick), stdin) != NULL) {
        nick[strcspn(nick, "\n")] = '\0';
    }
    printf("Welcome, %s!\n", nick);
    fflush(stdout);

    // Main chat loop
    while (1) {
        // Small delay to feed the watchdog and let IDLE run
        vTaskDelay(10 / portTICK_PERIOD_MS);

        if (fgets(line, sizeof(line), stdin) == NULL) continue;
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        sprintf(buffer, "%s: %s\n", nick, line);
        esp_mqtt_client_publish(client, TOPIC, buffer, 0, 1, 0);
        printf("Sent: %s", buffer);
        fflush(stdout);
    }
}

// ============================================================
// MQTT event handler
// ============================================================

static void mqtt_event_handler(void* handler_args,
                                esp_event_base_t base,
                                int32_t event_id,
                                void* event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            esp_mqtt_client_subscribe(event->client, TOPIC, 1);
            ESP_LOGI(TAG, "Subscribed to: %s", TOPIC);
            xTaskCreate(TaskChat, "TaskChat", 4096, NULL, 1, NULL);
            break;

        case MQTT_EVENT_DATA:
            printf("%.*s\r\n", event->data_len, event->data);
            fflush(stdout);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error: type=%d", event->error_handle->error_type);
            break;

        default:
            ESP_LOGI(TAG, "MQTT event id=%d", (int)event_id);
            break;
    }
}

// ============================================================
// connected_callback — called by hl_wifi when IP is obtained
// ============================================================

void connected_callback(void)
{
    ESP_LOGI(TAG, "WiFi connected! Starting MQTT...");

    // No serial I/O here — this runs inside the WiFi event task
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "Connecting to MQTT broker at %s...", BROKER_URL);
}

// ============================================================
// app_main
// ============================================================

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing WiFi...");
    hl_wifi_init(connected_callback);
}