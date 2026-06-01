#include "ui_manager.h"
#include "screen_status.h"
#include "screen_ams.h"
#include "screen_speed.h"
#include "screen_files.h"
#include "screen_printer.h"
#include "screen_settings.h"
#include "../printer_manager.h"

// Tab labels (5 chars max to fit 48px width)
static const char* TAB_LABELS[TAB_COUNT] = { "Stat", "AMS", "Spd", "File", "Prnt" };

UIManager& UIManager::instance() {
    static UIManager inst;
    return inst;
}

void UIManager::begin() {
    _sprite = new LGFX_Sprite(&M5Cardputer.Display);
    _sprite->setColorDepth(16);
    _sprite->createSprite(DISP_W, CONTENT_H);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(150);
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    _dirty    = true;
    _tabDirty = true;
}

void UIManager::setScreen(Screen s) {
    if (_screen == s) return;
    _screen   = s;
    _dirty    = true;
    _tabDirty = true;

    // Notify screens on enter
    PrinterState& st = PrinterManager::instance().activeState();
    if (s == Screen::SPEED)    ScreenSpeed::onEnter(st);
    if (s == Screen::FILES)    ScreenFiles::onEnter(st);
    if (s == Screen::SETTINGS) ScreenSettings::onEnter();
}

void UIManager::showHint(const char* msg, uint32_t durationMs) {
    strlcpy(_hintMsg, msg, sizeof(_hintMsg));
    _hintExpiry = millis() + durationMs;
    _dirty = true;
}

void UIManager::loop() {
    handleKeyboard();

    uint32_t now = millis();
    if (_dirty || now - _lastDrawMs >= UI_REFRESH_MS) {
        _lastDrawMs = now;
        _dirty = false;

        if (_tabDirty) {
            drawTabBar();
            _tabDirty = false;
        }

        dispatchDraw();

        // Hint bar
        const char* hint = nullptr;
        if (now < _hintExpiry) {
            hint = _hintMsg;
        } else {
            switch (_screen) {
                case Screen::STATUS:         hint = ScreenStatus::hintText();   break;
                case Screen::AMS:            hint = ScreenAms::hintText();      break;
                case Screen::SPEED:          hint = ScreenSpeed::hintText();    break;
                case Screen::FILES:          hint = ScreenFiles::hintText();    break;
                case Screen::PRINTER_SELECT: hint = ScreenPrinter::hintText();  break;
                case Screen::SETTINGS:       hint = ScreenSettings::hintText(); break;
                default: hint = "Tab=next  Shift+Tab=prev"; break;
            }
        }
        drawHintBar(hint ? hint : "");
    }
}

void UIManager::drawTabBar() {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, 0, DISP_W, TAB_H, d.color565(20, 20, 20));

    for (int i = 0; i < TAB_COUNT; i++) {
        int x = i * TAB_W;
        bool active = ((int)_screen == i);
        uint16_t bg = active ? d.color565(0, 120, 200) : d.color565(20, 20, 20);
        uint16_t fg = TFT_WHITE;
        d.fillRect(x, 0, TAB_W - 1, TAB_H, bg);
        d.setFont(&fonts::Font0);
        d.setTextColor(fg, bg);
        // Center label
        int tw = strlen(TAB_LABELS[i]) * 6;
        d.drawString(TAB_LABELS[i], x + (TAB_W - tw) / 2, (TAB_H - 8) / 2);
    }
}

void UIManager::drawHintBar(const char* hint) {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, HINT_Y, DISP_W, HINT_H, d.color565(10, 10, 10));
    d.setFont(&fonts::Font0);
    d.setTextColor(d.color565(160, 160, 160), d.color565(10, 10, 10));
    d.drawString(hint, 4, HINT_Y + 3);
}

void UIManager::dispatchDraw() {
    _sprite->fillSprite(TFT_BLACK);

    PrinterState& st = PrinterManager::instance().activeState();

    switch (_screen) {
        case Screen::STATUS:         ScreenStatus::draw(*_sprite, st);   break;
        case Screen::AMS:            ScreenAms::draw(*_sprite, st);      break;
        case Screen::SPEED:          ScreenSpeed::draw(*_sprite, st);    break;
        case Screen::FILES:          ScreenFiles::draw(*_sprite, st);    break;
        case Screen::PRINTER_SELECT: ScreenPrinter::draw(*_sprite);      break;
        case Screen::SETTINGS:       ScreenSettings::draw(*_sprite);     break;
        default: break;
    }

    _sprite->pushSprite(0, CONTENT_Y);
}

void UIManager::handleKeyboard() {
    if (!M5Cardputer.Keyboard.isChange()) return;
    if (!M5Cardputer.Keyboard.isPressed()) return;

    auto& ks  = M5Cardputer.Keyboard.keysState();
    bool  fn  = ks.fn;
    bool  opt = ks.opt;

    // Typed character (library gives value_second when shift/ctrl/capslock held)
    char typed = (!ks.word.empty()) ? ks.word[0] : 0;

    // Map opt or fn + i/k to synthetic up/down chars.
    // The ADV keyboard has no HID arrow codes in its ASCII map, so we use
    // dedicated modifier+letter combos as the reliable navigation method.
    if ((opt || fn) && typed == 'i') typed = '\x11'; // up
    if ((opt || fn) && typed == 'k') typed = '\x12'; // down

    _dirty = true;

    // Screen/tab navigation — intercept before passing to screen handlers,
    // except when in Settings where Tab navigates form fields.
    if (_screen != Screen::SETTINGS) {
        // Next tab: plain Tab OR fn+period
        if ((ks.tab && !ks.shift) || (fn && typed == '.')) {
            setScreen((Screen)(((int)_screen + 1) % TAB_COUNT));
            _tabDirty = true;
            return;
        }
        // Prev tab: Shift+Tab OR fn+comma
        if ((ks.tab && ks.shift) || (fn && typed == ',')) {
            setScreen((Screen)(((int)_screen - 1 + TAB_COUNT) % TAB_COUNT));
            _tabDirty = true;
            return;
        }
    }

    // Dispatch to the active screen
    if (_screen == Screen::SETTINGS) {
        ScreenSettings::handleKey(typed, fn, ks.enter, ks.del, ks.tab, ks.del);
    } else {
        switch (_screen) {
            case Screen::STATUS:         ScreenStatus::handleKey(typed, fn, ks.enter, ks.del);  break;
            case Screen::AMS:            ScreenAms::handleKey(typed, fn, ks.enter, ks.del);     break;
            case Screen::SPEED:          ScreenSpeed::handleKey(typed, fn, ks.enter, ks.del);   break;
            case Screen::FILES:          ScreenFiles::handleKey(typed, fn, ks.enter, ks.del);   break;
            case Screen::PRINTER_SELECT: ScreenPrinter::handleKey(typed, fn, ks.enter, ks.del); break;
            default: break;
        }
    }
}
