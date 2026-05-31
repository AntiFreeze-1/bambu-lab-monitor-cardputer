#pragma once

// MQTT broker (Bambu printers)
#define MQTT_PORT           8883
#define MQTT_BUFFER_SIZE    16384   // Bambu payloads can be 4–8 KB
#define MQTT_RECONNECT_MS   5000
#define MQTT_USER           "bblp"

// FTP (implicit-TLS, port 990)
#define FTP_PORT            990
#define FTP_TIMEOUT_MS      8000
#define FTP_MAX_FILES       50
#define FTP_FILENAME_LEN    64

// Application
#define PRINTER_COUNT       2
#define WIFI_CONNECT_TIMEOUT_MS  15000
#define UI_REFRESH_MS       333     // ~3 fps
#define LONG_PRESS_MS       800     // long-press threshold
