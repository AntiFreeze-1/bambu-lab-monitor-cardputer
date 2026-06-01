#pragma once
#include <Arduino.h>
#include "config.h"

struct PrinterCfg {
    char ip[16];
    char serial[20];
    char code[16];
};

struct AppSettings {
    char       wifiSsid[64];
    char       wifiPass[64];
    uint8_t    printerCount;
    PrinterCfg printers[PRINTER_COUNT_MAX];
    uint8_t    brightness;       // 1–10 (default 7)
    uint8_t    themeColor;       // 0–8 (default 0)
    uint8_t    screenTimeoutMin; // 0=never, 1–60 min (default 5)
};

class Settings {
public:
    static Settings& instance();

    void load();
    void save();
    bool isConfigured() const;

    bool importFromSd();
    bool exportToSd();

    AppSettings data;

private:
    Settings() = default;
};
