#include "settings.h"
#include <Preferences.h>
#include <SD.h>
#include <ArduinoJson.h>

static constexpr const char* SD_CONFIG_PATH = "/bambu_config.json";

Settings& Settings::instance() {
    static Settings inst;
    return inst;
}

// ── SD helpers ────────────────────────────────────────────────────────────────

static bool loadFromSd(AppSettings& d) {
    File f = SD.open(SD_CONFIG_PATH, FILE_READ);
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    strlcpy(d.wifiSsid, doc["wifi_ssid"] | "", sizeof(d.wifiSsid));
    strlcpy(d.wifiPass, doc["wifi_pass"] | "", sizeof(d.wifiPass));
    d.printerCount = doc["printer_count"] | 1;
    if (d.printerCount < 1)                 d.printerCount = 1;
    if (d.printerCount > PRINTER_COUNT_MAX) d.printerCount = PRINTER_COUNT_MAX;

    JsonArray arr = doc["printers"].as<JsonArray>();
    int i = 0;
    for (JsonObject p : arr) {
        if (i >= d.printerCount || i >= PRINTER_COUNT_MAX) break;
        strlcpy(d.printers[i].ip,     p["ip"]     | "", sizeof(d.printers[i].ip));
        strlcpy(d.printers[i].serial, p["serial"] | "", sizeof(d.printers[i].serial));
        strlcpy(d.printers[i].code,   p["code"]   | "", sizeof(d.printers[i].code));
        i++;
    }
    return d.wifiSsid[0] != '\0';
}

static bool saveToSd(const AppSettings& d) {
    JsonDocument doc;
    doc["wifi_ssid"]      = d.wifiSsid;
    doc["wifi_pass"]      = d.wifiPass;
    doc["printer_count"]  = d.printerCount;

    JsonArray arr = doc["printers"].to<JsonArray>();
    for (int i = 0; i < d.printerCount && i < PRINTER_COUNT_MAX; i++) {
        JsonObject p = arr.add<JsonObject>();
        p["ip"]     = d.printers[i].ip;
        p["serial"] = d.printers[i].serial;
        p["code"]   = d.printers[i].code;
    }

    File f = SD.open(SD_CONFIG_PATH, FILE_WRITE);
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

// ── NVS helpers ───────────────────────────────────────────────────────────────

static void loadFromNvs(AppSettings& d) {
    Preferences prefs;
    prefs.begin("bambu", true);

    prefs.getString("wifi_ssid", d.wifiSsid, sizeof(d.wifiSsid));
    prefs.getString("wifi_pass", d.wifiPass, sizeof(d.wifiPass));
    d.printerCount = prefs.getUChar("p_count", 1);
    if (d.printerCount < 1)                 d.printerCount = 1;
    if (d.printerCount > PRINTER_COUNT_MAX) d.printerCount = PRINTER_COUNT_MAX;

    for (int i = 0; i < d.printerCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "p%d_ip",     i);
        prefs.getString(key, d.printers[i].ip,     sizeof(d.printers[i].ip));
        snprintf(key, sizeof(key), "p%d_serial", i);
        prefs.getString(key, d.printers[i].serial, sizeof(d.printers[i].serial));
        snprintf(key, sizeof(key), "p%d_code",   i);
        prefs.getString(key, d.printers[i].code,   sizeof(d.printers[i].code));
    }

    prefs.end();
}

static void saveToNvs(const AppSettings& d) {
    Preferences prefs;
    prefs.begin("bambu", false);

    prefs.putString("wifi_ssid", d.wifiSsid);
    prefs.putString("wifi_pass", d.wifiPass);
    prefs.putUChar("p_count",   d.printerCount);

    for (int i = 0; i < d.printerCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "p%d_ip",     i);
        prefs.putString(key, d.printers[i].ip);
        snprintf(key, sizeof(key), "p%d_serial", i);
        prefs.putString(key, d.printers[i].serial);
        snprintf(key, sizeof(key), "p%d_code",   i);
        prefs.putString(key, d.printers[i].code);
    }

    prefs.end();
}

// ── Public API ────────────────────────────────────────────────────────────────

void Settings::load() {
    loadFromNvs(data);
    if (data.wifiSsid[0] == '\0') {
        loadFromSd(data);
    }
}

void Settings::save() {
    saveToNvs(data);
    saveToSd(data);
}

bool Settings::isConfigured() const {
    return data.wifiSsid[0] != '\0' && data.printers[0].ip[0] != '\0';
}

bool Settings::importFromSd() {
    AppSettings tmp;
    memset(&tmp, 0, sizeof(tmp));
    if (!loadFromSd(tmp)) return false;
    memcpy(&data, &tmp, sizeof(AppSettings));
    return true;
}

bool Settings::exportToSd() {
    return saveToSd(data);
}
