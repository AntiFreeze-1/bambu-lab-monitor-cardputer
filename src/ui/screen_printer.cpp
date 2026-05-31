#include "screen_printer.h"
#include "../printer_manager.h"
#include "../utils.h"
#include "ui_manager.h"

static int s_scrollOff = 0;

static constexpr int ROW_H   = 38;
static constexpr int START_Y = 24;

void ScreenPrinter::draw(LGFX_Sprite& s) {
    PrinterManager& pm     = PrinterManager::instance();
    uint8_t         active = pm.activeIndex();
    uint8_t         count  = pm.printerCount();

    s.setFont(&fonts::Font2);
    s.setTextColor(s.color565(200, 200, 200), TFT_BLACK);
    s.drawString("Select Printer", 4, 4);

    // Clamp scroll
    if (count > 0 && s_scrollOff >= (int)count) s_scrollOff = (int)count - 1;

    int visibleRows = (CONTENT_H - START_Y - 14) / ROW_H;
    if (visibleRows < 1) visibleRows = 1;

    for (int vi = 0; vi < visibleRows; vi++) {
        int i = s_scrollOff + vi;
        if (i >= (int)count) break;

        const PrinterState& st = pm.stateAt((uint8_t)i);
        if (st.ip[0] == '\0') continue;

        int y = START_Y + vi * ROW_H;
        bool isActive = (i == (int)active);
        uint16_t bg = isActive ? s.color565(0, 50, 100) : s.color565(20, 20, 20);
        s.fillRect(2, y - 2, DISP_W - 4, 36, bg);
        s.drawRect(2, y - 2, DISP_W - 4, 36,
                   isActive ? s.color565(0, 120, 200) : s.color565(50, 50, 50));

        // Key hint: [1]-[9] for first 9, then "nn:" for the rest
        char keyBuf[5];
        if (i < 9) snprintf(keyBuf, sizeof(keyBuf), "[%d]", i + 1);
        else        snprintf(keyBuf, sizeof(keyBuf), "%d:", i + 1);
        s.setFont(&fonts::Font0);
        s.setTextColor(s.color565(120, 120, 120), bg);
        s.drawString(keyBuf, 6, y);

        if (isActive) s.fillCircle(22, y + 4, 3, s.color565(0, 200, 60));

        // Model name (auto-detected) or IP as fallback
        const char* label = (st.deviceModel[0] != '\0') ? st.deviceModel : st.ip;
        s.setFont(&fonts::Font2);
        s.setTextColor(TFT_WHITE, bg);
        s.drawString(label, 28, y);

        uint16_t mqttCol = st.mqttConnected
            ? s.color565(0, 200, 60) : s.color565(200, 60, 60);
        s.setFont(&fonts::Font0);
        s.setTextColor(mqttCol, bg);
        s.drawString(st.mqttConnected ? "MQTT OK" : "MQTT --", DISP_W - 52, y);

        s.setFont(&fonts::Font0);
        s.setTextColor(s.color565(100, 100, 100), bg);
        s.drawString(st.ip, 28, y + 14);

        if (st.gcodeState != GcodeState::IDLE && st.gcodeState != GcodeState::UNKNOWN) {
            char summary[24];
            snprintf(summary, sizeof(summary), "%s %d%%",
                     gcodeStateName(st.gcodeState), st.progressPct);
            s.setTextColor(s.color565(160, 160, 160), bg);
            s.drawString(summary, 28, y + 23);
        }
    }

    // Scroll indicator
    if ((int)count > visibleRows) {
        int trackH = visibleRows * ROW_H;
        int thumbH = max(4, trackH * visibleRows / (int)count);
        int thumbY = START_Y + (trackH - thumbH) * s_scrollOff / max(1, (int)count - visibleRows);
        s.fillRect(DISP_W - 4, START_Y, 3, trackH, s.color565(40, 40, 40));
        s.fillRect(DISP_W - 4, thumbY,  3, thumbH, s.color565(100, 100, 100));
    }

    s.setFont(&fonts::Font0);
    s.setTextColor(s.color565(80, 80, 80), TFT_BLACK);
    s.drawString("Hold Enter = Settings", 4, CONTENT_H - 12);
}

void ScreenPrinter::handleKey(char c, bool /*fn*/, bool enter, bool /*del*/) {
    static uint32_t enterPressMs = 0;
    PrinterManager& pm    = PrinterManager::instance();
    uint8_t         count = pm.printerCount();

    // Keys 1–9 switch to printer index 0–8
    if (c >= '1' && c <= '9') {
        uint8_t idx = (uint8_t)(c - '1');
        if (idx < count) {
            pm.setActiveIndex(idx);
            char msg[32];
            snprintf(msg, sizeof(msg), "Switched to printer %d", idx + 1);
            UIManager::instance().showHint(msg, 1500);
            UIManager::instance().markDirty();
        }
        return;
    }

    if (c == '\x11') { // up — scroll list
        if (s_scrollOff > 0) { s_scrollOff--; UIManager::instance().markDirty(); }
        return;
    }
    if (c == '\x12') { // down — scroll list
        if (s_scrollOff < (int)count - 1) { s_scrollOff++; UIManager::instance().markDirty(); }
        return;
    }

    // Long-press Enter → Settings
    if (enter) {
        if (enterPressMs == 0) enterPressMs = millis();
        else if (millis() - enterPressMs >= LONG_PRESS_MS) {
            enterPressMs = 0;
            UIManager::instance().setScreen(Screen::SETTINGS);
        }
    } else {
        enterPressMs = 0;
    }
}

const char* ScreenPrinter::hintText() {
    return "1-9 switch  Up/Dn scroll  Hold Enter=cfg";
}
