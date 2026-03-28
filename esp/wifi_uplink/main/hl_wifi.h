#ifndef HL_WIFI_H
#define HL_WIFI_H

#include <stdint.h>
#include "lwip/sockets.h"

// Step 3a: callback type — pointer to a void function with no parameters
typedef void (*connect_callback_t)(void);

// Step 5a: convenience type for socket addresses
typedef struct sockaddr_in sockaddr_in_t;

// Initialize WiFi and connect. Calls the callback when connected.
void hl_wifi_init(connect_callback_t callback);

// Step 5b: create a socket address from IP string and port number
sockaddr_in_t hl_wifi_make_addr(char *ip, uint16_t port);

// Step 6a: open a TCP connection. Returns socket fd, or -1 on error.
int hl_wifi_tcp_connect(sockaddr_in_t addr);

// Step 7a: transmit data over a TCP socket
void hl_wifi_tcp_tx(int sock, void *buffer, uint16_t length);

#endif // HL_WIFI_H