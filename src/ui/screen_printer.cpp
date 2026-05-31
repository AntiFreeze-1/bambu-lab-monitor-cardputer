#include "screen_printer.h"
#include "../printer_manager.h"
#include "../utils.h"
#include "ui_manager.h"

void ScreenPrinter::draw(LGFX_Sprite& s) {
    PrinterManager& pm = PrinterManager::instance();
    uint8_t active = pm.activeIndex();

    s.setFont(&fonts::Font2);
    s.setTextColor(s.color565(200, 200, 200), TFT_BLACK);
    s.drawString("Select Printer", 4, 4);

    for (int i = 0; i < PRINTER_COUNT; i++) {
        const PrinterState& st = pm.stateAt(i);
        if (st.name[0] == '\0' && st.ip[0] == '\0') continue;

        int y = 24 + i * 38;

        // Row background
        bool isActive = (i == active);
        uint16_t bg = isActive ? s.color565(0, 50, 100) : s.color565(20, 20, 20);
        s.fillRect(2, y - 2, DISP_W - 4, 36, bg);
        s.drawRect(2, y - 2, DISP_W - 4, 36, isActive ? s.color565(0, 120, 200) : s.color565(50, 50, 50));

        // Key hint
        char keyBuf[4];
        snprintf(keyBuf, sizeof(keyBuf), "[%d]", i + 1);
        s.setFont(&fonts::Font0);
        s.setTextColor(s.color565(120, 120, 120), bg);
        s.drawString(keyBuf, 6, y);

        // Active dot
        if (isActive) {
            s.fillCircle(22, y + 4, 3, s.color565(0, 200, 60));
        }

        // Printer name
        s.setFont(&fonts::Font2);
        s.setTextColor(TFT_WHITE, bg);
        s.drawString(st.name[0] ? st.name : "(no name)", 28, y);

        // MQTT status
        uint16_t mqttCol = st.mqttConnected ? s.color565(0, 200, 60) : s.color565(200, 60, 60);
        s.setFont(&fonts::Font0);
        s.setTextColor(mqttCol, bg);
        s.drawString(st.mqttConnected ? "MQTT OK" : "MQTT --", DISP_W - 52, y);

        // IP address
        s.setFont(&fonts::Font0);
        s.setTextColor(s.color565(100, 100, 100), bg);
        s.drawString(st.ip, 28, y + 14);

        // Print state summary
        if (st.gcodeState != GcodeState::IDLE && st.gcodeState != GcodeState::UNKNOWN) {
            char summary[24];
            snprintf(summary, sizeof(summary), "%s %d%%",
                     gcodeStateName(st.gcodeState), st.progressPct);
            s.setTextColor(s.color565(160, 160, 160), bg);
            s.drawString(summary, 28, y + 23);
        }
    }

    // Settings hint at bottom
    s.setFont(&fonts::Font0);
    s.setTextColor(s.color565(80, 80, 80), TFT_BLACK);
    s.drawString("Hold Enter = Settings", 4, CONTENT_H - 12);
}

void ScreenPrinter::handleKey(char c, bool /*fn*/, bool enter, bool /*del*/) {
    static uint32_t enterPressMs = 0;

    if (c == '1') {
        PrinterManager::instance().setActiveIndex(0);
        UIManager::instance().showHint("Switched to Printer 1", 1500);
        UIManager::instance().markDirty();
        return;
    }
    if (c == '2') {
        PrinterManager::instance().setActiveIndex(1);
        UIManager::instance().showHint("Switched to Printer 2", 1500);
        UIManager::instance().markDirty();
        return;
    }

    // Long-press Enter → Settings
    if (enter) {
        if (enterPressMs == 0) {
            enterPressMs = millis();
        } else if (millis() - enterPressMs >= LONG_PRESS_MS) {
            enterPressMs = 0;
            UIManager::instance().setScreen(Screen::SETTINGS);
        }
    } else {
        enterPressMs = 0;
    }
}

const char* ScreenPrinter::hintText() {
    return "1/2 switch  Hold Enter=settings";
}
