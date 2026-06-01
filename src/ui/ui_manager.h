#pragma once
#include <M5Cardputer.h>
#include "../types.h"

// Display layout constants
static constexpr int DISP_W       = 240;
static constexpr int DISP_H       = 135;
static constexpr int TAB_H        = 16;
static constexpr int HINT_H       = 14;
static constexpr int CONTENT_Y    = TAB_H;
static constexpr int CONTENT_H    = DISP_H - TAB_H - HINT_H;  // 105
static constexpr int HINT_Y       = DISP_H - HINT_H;           // 121
static constexpr int TAB_COUNT    = 5;   // STATUS..PRINTER_SELECT
static constexpr int TAB_W        = DISP_W / TAB_COUNT;        // 48

class UIManager {
public:
    static UIManager& instance();

    void begin();
    void loop();

    void setScreen(Screen s);
    Screen currentScreen() const { return _screen; }

    // Show a temporary status message in the hint bar
    void showHint(const char* msg, uint32_t durationMs = 2000);

    // Force a full redraw next loop
    void markDirty() { _dirty = true; }

    LGFX_Sprite& sprite() { return *_sprite; }

private:
    UIManager() = default;

    void drawTabBar();
    void drawHintBar(const char* hint);
    void dispatchDraw();
    void handleKeyboard();

    Screen       _screen     = Screen::STATUS;
    LGFX_Sprite* _sprite     = nullptr;
    bool         _dirty      = true;
    bool         _tabDirty   = true;
    uint32_t     _lastDrawMs = 0;

    char     _hintMsg[48]   = {};
    uint32_t _hintExpiry    = 0;
};
