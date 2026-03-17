#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

// UART0 is the one connected to USB
#define UART_PORT    UART_NUM_0
#define BUF_SIZE     256

// --- UART Setup ---
// This replaces the "toy" printf-based serial with the full UART driver.
// After this, we can both SEND and RECEIVE data over the USB serial line.
//
// What each function does:
//   uart_param_config  — sets baud rate, data bits, stop bits, parity
//   uart_driver_install — allocates RX/TX ring buffers and enables the driver
//   uart_set_pin       — assigns GPIO pins (we keep defaults for UART0)

static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,           // must match your terminal (minicom/livebook)
        .data_bits = UART_DATA_8_BITS, // 8 data bits
        .parity    = UART_PARITY_DISABLE, // no parity
        .stop_bits = UART_STOP_BITS_1, // 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // no hardware flow control
        .source_clk = UART_SCLK_APB,  // use APB clock
    };

    // Install the driver with a 256-byte RX buffer, no TX buffer
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0));
}

// --- Counting Function ---
// Takes a number and counts down to zero, sending each number over UART.
//
// Uses uart_write_bytes() instead of printf().
// uart_write_bytes(port, data_pointer, data_length) sends raw bytes
// out through the UART hardware.

static void countdown(uint8_t from)
{
    char msg[64];

    for (int i = from; i >= 0; i--) {
        int len = snprintf(msg, sizeof(msg), "%d...\r\n", i);
        uart_write_bytes(UART_PORT, msg, len);
        vTaskDelay(500 / portTICK_PERIOD_MS);  // half second between numbers
    }

    const char *done = "Done!\r\n";
    uart_write_bytes(UART_PORT, done, strlen(done));
}

// --- Read a line from UART ---
// Reads one byte at a time until it gets a newline ('\n' or '\r').
// Returns the number of characters read.
//
// uart_read_bytes(port, buffer, length, timeout) blocks until either:
//   - 'length' bytes have been received, OR
//   - 'timeout' ticks have passed
// It returns the number of bytes actually read (0 if timeout with no data).

static int read_line(char *buf, int max_len)
{
    int pos = 0;

    while (pos < max_len - 1) {
        uint8_t byte;
        int len = uart_read_bytes(UART_PORT, &byte, 1, 100 / portTICK_PERIOD_MS);

        if (len > 0) {
            // Echo the character back so you can see what you type
            uart_write_bytes(UART_PORT, (const char *)&byte, 1);

            if (byte == '\n' || byte == '\r') {
                // Send newline back
                const char *nl = "\r\n";
                uart_write_bytes(UART_PORT, nl, 2);
                break;
            }

            buf[pos++] = (char)byte;
        }
    }

    buf[pos] = '\0';  // null-terminate the string
    return pos;
}

// --- Main ---

void app_main(void)
{
    uart_init();

    // Small delay to let the terminal connect
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Step 3: Test the counting function with a hardcoded value
    const char *hello = "\r\n=== Serial Count ===\r\n";
    uart_write_bytes(UART_PORT, hello, strlen(hello));

    const char *test_msg = "Test: counting down from 5...\r\n";
    uart_write_bytes(UART_PORT, test_msg, strlen(test_msg));
    countdown(5);

    // Step 4 & 5: Read input from user and count down
    while (1) {
        const char *prompt = "\r\nEnter a number (0-255): ";
        uart_write_bytes(UART_PORT, prompt, strlen(prompt));

        char input[16];
        int chars_read = read_line(input, sizeof(input));

        if (chars_read > 0) {
            int num = atoi(input);  // convert string to integer

            if (num < 0 || num > 255) {
                const char *err = "Please enter 0-255.\r\n";
                uart_write_bytes(UART_PORT, err, strlen(err));
            } else {
                char start_msg[64];
                int len = snprintf(start_msg, sizeof(start_msg),
                                   "Counting down from %d:\r\n", num);
                uart_write_bytes(UART_PORT, start_msg, len);
                countdown((uint8_t)num);
            }
        }
    }
}
