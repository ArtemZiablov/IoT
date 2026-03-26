#ifndef HL_WIFI_H
#define HL_WIFI_H

// Initialize WiFi in station mode and connect to the configured AP.
// This function blocks until connected (or max retries reached).
void hl_wifi_init(void);

#endif // HL_WIFI_H