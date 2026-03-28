#ifndef HL_WIFI_H
#define HL_WIFI_H

#include <stdint.h>
#include "lwip/sockets.h"

typedef void (*connect_callback_t)(void);
typedef struct sockaddr_in sockaddr_in_t;

void hl_wifi_init(connect_callback_t callback);
sockaddr_in_t hl_wifi_make_addr(char *ip, uint16_t port);
int hl_wifi_tcp_connect(sockaddr_in_t addr);
void hl_wifi_tcp_tx(int sock, void *buffer, uint16_t length);

#endif