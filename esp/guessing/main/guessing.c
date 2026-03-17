#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_system.h"
#include "driver/uart.h"

// ============================================================
// ADC config (same as Quest 1)
// ============================================================

#define ADC_CHANNEL    ADC_CHANNEL_6
#define ADC_UNIT       ADC_UNIT_1
#define ADC_ATTEN_VAL  ADC_ATTEN_DB_11    // ADC_ATTEN_DB_11 in v5.1.2 = full 0-3.3V
#define ADC_BITWIDTH   ADC_BITWIDTH_12

static adc_oneshot_unit_handle_t adc_handle;

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

// Step 5 from Quest 1 — read one ADC value
static int sample(void)
{
    int raw_value;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_value));
    return raw_value;
}

// ============================================================
// UART setup (same as Quest 3)
// ============================================================

#define UART_PORT    UART_NUM_0
#define BUF_SIZE     256

static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_PIN_NO_CHANGE,
                    UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0));
}

static void uart_send(const char *str)
{
    uart_write_bytes(UART_PORT, str, strlen(str));
}

static int read_line(char *buf, int max_len)
{
    int pos = 0;
    while (pos < max_len - 1) {
        uint8_t byte;
        int len = uart_read_bytes(UART_PORT, &byte, 1, 100 / portTICK_PERIOD_MS);
        if (len > 0) {
            uart_write_bytes(UART_PORT, (const char *)&byte, 1);
            if (byte == '\n' || byte == '\r') {
                uart_send("\r\n");
                break;
            }
            buf[pos++] = (char)byte;
        }
    }
    buf[pos] = '\0';
    return pos;
}

// ============================================================
// Game constants
// ============================================================

// 12-bit ADC: values 0–4095
// Binary search needs log2(4096) = 12 tries to guarantee finding the number
#define MAX_VALUE   4095
#define MAX_TRIES   12

// ============================================================
// Main
// ============================================================

void app_main(void)
{
    adc_init();
    uart_init();
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Step 2: do-forever loop
    while (1) {

        // Step 3: Greeting
        uart_send("\r\nShall we play a game? (y/n): ");
        char answer[16];
        read_line(answer, sizeof(answer));

        if (answer[0] == 'n' || answer[0] == 'N') {
            uart_send("Goodbye!\r\n");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            esp_restart();  // reboot the ESP32
        }

        if (answer[0] != 'y' && answer[0] != 'Y') {
            uart_send("Please answer y or n.\r\n");
            continue;  // ask again
        }

        // Step 4: Produce a number from the potentiometer
        int secret = sample();

        // Step 5: Welcome message
        char msg[128];
        snprintf(msg, sizeof(msg),
            "\r\nI am thinking about a number between 0 and %d (both included).\r\n"
            "I bet you can't guess it in %d tries!\r\n",
            MAX_VALUE, MAX_TRIES);
        uart_send(msg);

        // Step 6: Game loop variables
        bool done = false;
        uint8_t count = 0;

        while (!done) {

            // Step 7: Query user (with bonus ternary for "time"/"times")
            snprintf(msg, sizeof(msg),
                "\r\nYou have guessed %d %s. Please try again: ",
                count, count == 1 ? "time" : "times");
            uart_send(msg);

            // Step 8: Fetch answer and parse to integer
            char input[16];
            int chars = read_line(input, sizeof(input));

            if (chars == 0) continue;

            char *endptr;
            long guess = strtol(input, &endptr, 10);

            // Check if parsing failed (endptr == input means no digits found)
            if (endptr == input) {
                uart_send("That's not a number. Try again.\r\n");
                continue;
            }

            count++;

            // Step 9 & 10: Evaluate response and produce feedback
            if (guess == secret) {
                snprintf(msg, sizeof(msg),
                    "\r\nYou got it! The number was %d.\r\n", secret);
                uart_send(msg);

                if (count <= MAX_TRIES) {
                    snprintf(msg, sizeof(msg),
                        "You guessed it in %d %s — within the limit of %d. Well done!\r\n",
                        count, count == 1 ? "try" : "tries", MAX_TRIES);
                } else {
                    snprintf(msg, sizeof(msg),
                        "But it took you %d tries — more than %d. Better luck next time!\r\n",
                        count, MAX_TRIES);
                }
                uart_send(msg);
                done = true;

            } else if (guess < secret) {
                uart_send("Too low!\r\n");
            } else {
                uart_send("Too high!\r\n");
            }

            // Step 12: Bonus — "the only winning move is not to play"
            if (!done && count >= MAX_TRIES) {
                snprintf(msg, sizeof(msg),
                    "\r\nYou've used all %d tries. The number was %d.\r\n"
                    "A strange game. The only winning move is not to play.\r\n",
                    MAX_TRIES, secret);
                uart_send(msg);
                done = true;
            }
        }
    }
}
