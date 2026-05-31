#pragma once
#include <Arduino.h>

struct PrinterCfg {
    char name[32];
    char ip[16];
    char serial[20];
    char code[16];
};

struct AppSettings {
    char       wifiSsid[64];
    char       wifiPass[64];
    PrinterCfg printers[2];
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
