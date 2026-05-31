#include <M5Cardputer.h>
#include <WiFi.h>
#include "settings.h"
#include "printer_manager.h"
#include "ui/ui_manager.h"
#include "ui/screen_settings.h"

static void showSplash(const char* line1, const char* line2 = nullptr) {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setFont(&fonts::Font4);
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.drawString(line1, 120, line2 ? 55 : 67);
    if (line2) {
        M5Cardputer.Display.setFont(&fonts::Font2);
        M5Cardputer.Display.setTextColor(M5Cardputer.Display.color565(140,140,140), TFT_BLACK);
        M5Cardputer.Display.drawString(line2, 120, 85);
    }
    M5Cardputer.Display.setTextDatum(TL_DATUM);
}

static bool connectWifi(const char* ssid, const char* pass) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    uint32_t start = millis();
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) return false;
        delay(300);
        dots++;
        char msg[32];
        snprintf(msg, sizeof(msg), "Connecting%s", dots % 4 == 0 ? "   " :
                                                    dots % 4 == 1 ? ".  " :
                                                    dots % 4 == 2 ? ".. " : "...");
        showSplash("Connecting WiFi", msg);
    }
    return true;
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(150);
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    // Title splash
    showSplash("Bambu Monitor", "Loading...");
    delay(600);

    Settings::instance().load();

    if (!Settings::instance().isConfigured()) {
        // First boot — go straight to settings
        UIManager::instance().begin();
        ScreenSettings::onEnter();
        UIManager::instance().setScreen(Screen::SETTINGS);
        return; // Skip WiFi + MQTT setup; will restart after save
    }

    const AppSettings& cfg2 = Settings::instance().data;

    if (!connectWifi(cfg2.wifiSsid, cfg2.wifiPass)) {
        showSplash("WiFi Failed", "Check settings (hold Enter on Prnt)");
        // Still launch UI so user can navigate to settings
        UIManager::instance().begin();
        UIManager::instance().setScreen(Screen::PRINTER_SELECT);
        return;
    }

    showSplash("WiFi OK", WiFi.localIP().toString().c_str());
    delay(800);

    PrinterManager::instance().begin();
    UIManager::instance().begin();
    UIManager::instance().setScreen(Screen::STATUS);
}

void loop() {
    M5Cardputer.update(); // must be first — refreshes keyboard state
    PrinterManager::instance().loop();
    UIManager::instance().loop();
}
