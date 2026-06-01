#include "screen_speed.h"
#include "../utils.h"
#include "../printer_manager.h"
#include "ui_manager.h"

static int s_selectedIdx = 1; // 0-based index into SpeedLevel 1..4

static const SpeedLevel LEVELS[4] = {
    SpeedLevel::QUIET, SpeedLevel::NORMAL, SpeedLevel::SPORT, SpeedLevel::LUDICROUS
};

static const int NOZZLE_PRESETS[] = {0, 190, 200, 210, 215, 220, 230, 240, 250, 260};
static const int BED_PRESETS[]    = {0,  35,  45,  55,  60,  65,  70,  90};
static const int NOZZLE_COUNT     = 10;
static const int BED_COUNT        =  8;
static int s_nozzleIdx = 5;  // default 220
static int s_bedIdx    = 4;  // default 60

static int nearestIdx(const int* presets, int count, float target) {
    int best = 0;
    float bestDiff = fabsf((float)presets[0] - target);
    for (int i = 1; i < count; i++) {
        float diff = fabsf((float)presets[i] - target);
        if (diff < bestDiff) { bestDiff = diff; best = i; }
    }
    return best;
}

void ScreenSpeed::onEnter(const PrinterState& state) {
    s_selectedIdx = (int)state.speedLevel - 1;
    if (s_selectedIdx < 0 || s_selectedIdx > 3) s_selectedIdx = 1;

    s_nozzleIdx = nearestIdx(NOZZLE_PRESETS, NOZZLE_COUNT, state.nozzleTarget);
    s_bedIdx    = nearestIdx(BED_PRESETS,    BED_COUNT,    state.bedTarget);
}

void ScreenSpeed::draw(LGFX_Sprite& s, const PrinterState& state) {
    s.setFont(&fonts::Font2);
    s.setTextColor(s.color565(200, 200, 200), TFT_BLACK);
    s.drawString("Ctrl", 4, 4);

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

    // Divider
    int divY = 22 + 4 * 18 - 2;
    s.drawFastHLine(2, divY, DISP_W - 4, s.color565(50, 50, 50));

    // Temperature rows — use Font0 for compact display
    s.setFont(&fonts::Font0);
    int tempY = divY + 4;

    // Nozzle row
    {
        int actualN  = (int)state.nozzleTarget;
        int presetN  = NOZZLE_PRESETS[s_nozzleIdx];
        bool differs = (presetN != actualN);
        s.setTextColor(s.color565(150, 150, 150), TFT_BLACK);
        s.drawString("Nozzle:", 4, tempY);
        char buf[20];
        snprintf(buf, sizeof(buf), "%d", actualN);
        s.setTextColor(TFT_WHITE, TFT_BLACK);
        s.drawString(buf, 46, tempY);
        s.setTextColor(s.color565(100, 100, 100), TFT_BLACK);
        s.drawString("->", 70, tempY);
        if (presetN == 0)
            snprintf(buf, sizeof(buf), "off");
        else
            snprintf(buf, sizeof(buf), "[%d]", presetN);
        s.setTextColor(differs ? s.color565(255, 220, 0) : TFT_WHITE, TFT_BLACK);
        s.drawString(buf, 84, tempY);
        s.setTextColor(s.color565(100, 100, 100), TFT_BLACK);
        s.drawString("n/N", 120, tempY);
    }

    tempY += 12;

    // Bed row
    {
        int actualB  = (int)state.bedTarget;
        int presetB  = BED_PRESETS[s_bedIdx];
        bool differs = (presetB != actualB);
        s.setTextColor(s.color565(150, 150, 150), TFT_BLACK);
        s.drawString("Bed:", 4, tempY);
        char buf[20];
        snprintf(buf, sizeof(buf), "%d", actualB);
        s.setTextColor(TFT_WHITE, TFT_BLACK);
        s.drawString(buf, 46, tempY);
        s.setTextColor(s.color565(100, 100, 100), TFT_BLACK);
        s.drawString("->", 70, tempY);
        if (presetB == 0)
            snprintf(buf, sizeof(buf), "off");
        else
            snprintf(buf, sizeof(buf), "[%d]", presetB);
        s.setTextColor(differs ? s.color565(255, 220, 0) : TFT_WHITE, TFT_BLACK);
        s.drawString(buf, 84, tempY);
        s.setTextColor(s.color565(100, 100, 100), TFT_BLACK);
        s.drawString("b/B", 120, tempY);
    }
}

void ScreenSpeed::handleKey(char c, bool /*fn*/, bool enter, bool /*del*/) {
    if (c >= '1' && c <= '4') {
        s_selectedIdx = c - '1';
        PrinterManager::instance().sendSpeedCommand(LEVELS[s_selectedIdx]);
        UIManager::instance().showHint("Speed updated!", 1500);
        return;
    }

    if (c == 'n') {
        if (s_nozzleIdx < NOZZLE_COUNT - 1) s_nozzleIdx++;
        return;
    }
    if (c == 'N') {
        if (s_nozzleIdx > 0) s_nozzleIdx--;
        return;
    }
    if (c == 'b') {
        if (s_bedIdx < BED_COUNT - 1) s_bedIdx++;
        return;
    }
    if (c == 'B') {
        if (s_bedIdx > 0) s_bedIdx--;
        return;
    }

    if (c == '\x11') { // up
        if (s_selectedIdx > 0) s_selectedIdx--;
    } else if (c == '\x12') { // down
        if (s_selectedIdx < 3) s_selectedIdx++;
    } else if (enter) {
        PrinterManager::instance().sendSpeedCommand(LEVELS[s_selectedIdx]);
        PrinterManager::instance().sendSetTemps(
            NOZZLE_PRESETS[s_nozzleIdx], BED_PRESETS[s_bedIdx]);
        UIManager::instance().showHint("Speed+temps applied!", 1500);
    }
}

const char* ScreenSpeed::hintText() {
    return "1-4 speed  n/N nozzle  b/B bed  Enter=apply";
}
