#ifndef CONFIG_H
#define CONFIG_H

#include "secrets.h"

// Wi-Fi Credentials
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;

// WebSocket Server Port (Default is 81)
const int WEBSOCKET_PORT = 81;

#endif // CONFIG_H
