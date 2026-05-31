#include "settings.h"
#include <Preferences.h>

Settings& Settings::instance() {
    static Settings inst;
    return inst;
}

void Settings::load() {
    Preferences prefs;
    prefs.begin("bambu", true); // read-only

    prefs.getString("wifi_ssid", data.wifiSsid, sizeof(data.wifiSsid));
    prefs.getString("wifi_pass", data.wifiPass,  sizeof(data.wifiPass));

    data.printerCount = prefs.getUChar("p_count", 1);
    if (data.printerCount < 1)                    data.printerCount = 1;
    if (data.printerCount > PRINTER_COUNT_MAX)     data.printerCount = PRINTER_COUNT_MAX;

    for (int i = 0; i < data.printerCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "p%d_ip",     i);
        prefs.getString(key, data.printers[i].ip,     sizeof(data.printers[i].ip));
        snprintf(key, sizeof(key), "p%d_serial", i);
        prefs.getString(key, data.printers[i].serial, sizeof(data.printers[i].serial));
        snprintf(key, sizeof(key), "p%d_code",   i);
        prefs.getString(key, data.printers[i].code,   sizeof(data.printers[i].code));
    }

    prefs.end();
}

void Settings::save() {
    Preferences prefs;
    prefs.begin("bambu", false); // read-write

    prefs.putString("wifi_ssid", data.wifiSsid);
    prefs.putString("wifi_pass", data.wifiPass);
    prefs.putUChar("p_count", data.printerCount);

    for (int i = 0; i < data.printerCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "p%d_ip",     i);
        prefs.putString(key, data.printers[i].ip);
        snprintf(key, sizeof(key), "p%d_serial", i);
        prefs.putString(key, data.printers[i].serial);
        snprintf(key, sizeof(key), "p%d_code",   i);
        prefs.putString(key, data.printers[i].code);
    }

    prefs.end();
}

bool Settings::isConfigured() const {
    return data.wifiSsid[0] != '\0' && data.printers[0].ip[0] != '\0';
}
