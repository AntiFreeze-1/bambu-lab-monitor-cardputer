#include "screen_speed.h"
#include "../utils.h"
#include "../printer_manager.h"
#include "ui_manager.h"

static int s_selectedIdx = 1; // 0-based index into SpeedLevel 1..4

static const SpeedLevel LEVELS[4] = {
    SpeedLevel::QUIET, SpeedLevel::NORMAL, SpeedLevel::SPORT, SpeedLevel::LUDICROUS
};

void ScreenSpeed::onEnter(const PrinterState& state) {
    s_selectedIdx = (int)state.speedLevel - 1;
    if (s_selectedIdx < 0 || s_selectedIdx > 3) s_selectedIdx = 1;
}

void ScreenSpeed::draw(LGFX_Sprite& s, const PrinterState& state) {
    s.setFont(&fonts::Font2);
    s.setTextColor(s.color565(200, 200, 200), TFT_BLACK);
    s.drawString("Print Speed", 4, 4);

    int currentIdx = (int)state.speedLevel - 1;

    for (int i = 0; i < 4; i++) {
        int y = 22 + i * 18;
        bool isCurrent  = (i == currentIdx);
        bool isSelected = (i == s_selectedIdx);

        uint16_t bg = isSelected ? s.color565(0, 80, 160) : TFT_BLACK;
        s.fillRect(2, y - 1, DISP_W - 4, 16, bg);

        // Number key indicator
        char numBuf[4];
        snprintf(numBuf, sizeof(numBuf), "[%d]", i + 1);
        s.setTextColor(s.color565(130, 130, 130), bg);
        s.drawString(numBuf, 6, y);

        // Speed name
        uint16_t fg = isSelected ? TFT_WHITE : (isCurrent ? s.color565(0,220,100) : s.color565(200,200,200));
        s.setTextColor(fg, bg);
        s.drawString(speedLevelName(LEVELS[i]), 32, y);

        // Percentage
        s.setTextColor(s.color565(130, 130, 130), bg);
        s.drawString(speedLevelPct(LEVELS[i]), 120, y);

        // Current indicator
        if (isCurrent) {
            s.setTextColor(s.color565(0, 220, 100), bg);
            s.drawString("<", 170, y);
        }
    }
}

void ScreenSpeed::handleKey(char c, bool /*fn*/, bool enter, bool /*del*/) {
    if (c >= '1' && c <= '4') {
        s_selectedIdx = c - '1';
        PrinterManager::instance().sendSpeedCommand(LEVELS[s_selectedIdx]);
        UIManager::instance().showHint("Speed updated!", 1500);
        return;
    }

    if (c == '\x11') { // up
        if (s_selectedIdx > 0) s_selectedIdx--;
    } else if (c == '\x12') { // down
        if (s_selectedIdx < 3) s_selectedIdx++;
    } else if (enter) {
        PrinterManager::instance().sendSpeedCommand(LEVELS[s_selectedIdx]);
        UIManager::instance().showHint("Speed updated!", 1500);
    }
}

const char* ScreenSpeed::hintText() {
    return "1-4 or Up/Dn  Enter=apply";
}
