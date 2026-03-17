#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

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

// Helper: send a string over UART
static void uart_send(const char *str)
{
    uart_write_bytes(UART_PORT, str, strlen(str));
}

// Helper: read a line from UART (with echo)
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
// Step 2: Echo command
// ============================================================
// Prints all arguments EXCEPT the first one (which is the command name).
// Example: argv = ["echo", "hello,", "world"], argc = 3
//   → prints "hello, world"

static void command_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        uart_send(argv[i]);
        if (i < argc - 1) {
            uart_send(" ");
        }
    }
    uart_send("\r\n");
}

// ============================================================
// Step 7: Additional commands
// ============================================================

// pin-high <pin> — sets a pin as output and drives it HIGH
static void command_pin_high(int argc, char *argv[])
{
    if (argc < 2) {
        uart_send("Usage: pin-high <pin>\r\n");
        return;
    }
    int pin = atoi(argv[1]);
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 1);

    char msg[64];
    snprintf(msg, sizeof(msg), "GPIO %d set HIGH\r\n", pin);
    uart_send(msg);
}

// pin-low <pin> — sets a pin as output and drives it LOW
static void command_pin_low(int argc, char *argv[])
{
    if (argc < 2) {
        uart_send("Usage: pin-low <pin>\r\n");
        return;
    }
    int pin = atoi(argv[1]);
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);

    char msg[64];
    snprintf(msg, sizeof(msg), "GPIO %d set LOW\r\n", pin);
    uart_send(msg);
}

// pin-read <pin> — sets a pin as input and reads its value
static void command_pin_read(int argc, char *argv[])
{
    if (argc < 2) {
        uart_send("Usage: pin-read <pin>\r\n");
        return;
    }
    int pin = atoi(argv[1]);
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
    int level = gpio_get_level(pin);

    char msg[64];
    snprintf(msg, sizeof(msg), "%d\r\n", level);
    uart_send(msg);
}

// reverse <words...> — prints the arguments in reverse order
static void command_reverse(int argc, char *argv[])
{
    for (int i = argc - 1; i >= 1; i--) {
        uart_send(argv[i]);
        if (i > 1) {
            uart_send(" ");
        }
    }
    uart_send("\r\n");
}

// ============================================================
// Step 4: Dispatch table
// ============================================================

// A type for any command function: takes (int argc, char *argv[])
typedef void (*command_t)(int argc, char *argv[]);

// A dispatch entry: a command name paired with its function
typedef struct {
    const char *name;
    command_t   func;
} dispatch_entry_t;

// The dispatch table — maps command names to functions
#define COMMAND_COUNT 5

static dispatch_entry_t command_dispatch_table[COMMAND_COUNT] = {
    { "echo",     command_echo },
    { "pin-high", command_pin_high },
    { "pin-low",  command_pin_low },
    { "pin-read", command_pin_read },
    { "reverse",  command_reverse },
};

// ============================================================
// Step 5: Dispatch function
// ============================================================

// Maximum number of arguments we support
#define MAX_ARGS 16

// Takes a raw input string like "echo hello world"
// Splits it into ["echo", "hello", "world"]
// Looks up "echo" in the dispatch table
// Calls the matching function with the full argv array

static void dispatch(char *input)
{
    char *argv[MAX_ARGS];
    int argc = 0;

    // Split the string at spaces by inserting '\0' characters
    // "echo hello world"  becomes  "echo\0hello\0world\0"
    //  ^argv[0] ^argv[1]  ^argv[2]
    char *ptr = input;
    while (*ptr != '\0' && argc < MAX_ARGS) {
        // Skip leading spaces
        while (*ptr == ' ') ptr++;
        if (*ptr == '\0') break;

        // Mark the start of this argument
        argv[argc++] = ptr;

        // Find the end of this argument
        while (*ptr != ' ' && *ptr != '\0') ptr++;

        // If we hit a space, replace it with '\0' to terminate the argument
        if (*ptr == ' ') {
            *ptr = '\0';
            ptr++;
        }
    }

    if (argc == 0) return;  // empty input

    // Look up the command name (argv[0]) in the dispatch table
    for (int i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(argv[0], command_dispatch_table[i].name) == 0) {
            // Found it! Call the function
            command_dispatch_table[i].func(argc, argv);
            return;
        }
    }

    // Command not found
    char msg[128];
    snprintf(msg, sizeof(msg),
             "I'm sorry, but I don't recognize the command '%s'. "
             "My sincerest apologies.\r\n", argv[0]);
    uart_send(msg);
}

// ============================================================
// Step 6: Main loop — read commands forever
// ============================================================

void app_main(void)
{
    uart_init();
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    uart_send("\r\n=== Command and Control ===\r\n");
    uart_send("Commands: echo, pin-high, pin-low, pin-read, reverse\r\n");

    while (1) {
        uart_send("\r\n> ");

        char input[BUF_SIZE];
        int len = read_line(input, sizeof(input));

        if (len > 0) {
            dispatch(input);
        }
    }
}
