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
