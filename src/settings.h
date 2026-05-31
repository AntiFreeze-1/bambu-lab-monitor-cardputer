#pragma once
#include <Arduino.h>
#include "config.h"

struct PrinterCfg {
    // name removed — auto-detected from MQTT (stored in PrinterState.deviceModel)
    char ip[16];
    char serial[20];
    char code[16];
};

struct AppSettings {
    char       wifiSsid[64];
    char       wifiPass[64];
    uint8_t    printerCount;                    // 1–99
    PrinterCfg printers[PRINTER_COUNT_MAX];
};

class Settings {
public:
    static Settings& instance();

    void load();
    void save();
    bool isConfigured() const;

    AppSettings data;

private:
    Settings() = default;
};
