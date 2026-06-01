#include "screen_settings.h"
#include "../settings.h"
#include "../printer_manager.h"
#include "../utils.h"
#include "ui_manager.h"

// ── Field definitions ─────────────────────────────────────────────────────────

enum class FieldType : uint8_t { TEXT, PASSWORD, SPINNER, BUTTON };

struct Field {
    const char*        label;
    char*              buf;
    size_t             maxLen;
    FieldType          type;
    uint8_t*           spinnerVal;
    uint8_t            spinnerMin;
    uint8_t            spinnerMax;
    uint8_t            buttonId;          // 0=none, 1=import SD, 2=export SD
    const char* const* spinnerNames;      // nullptr if unused; display names[*spinnerVal]
};

static AppSettings s_draft;
static int         s_fieldIdx    = 0;
static int         s_scrollOff   = 0;
static uint32_t    s_cursorBlink = 0;
static bool        s_cursorOn    = true;

static constexpr int MAX_FIELDS = 6 + PRINTER_COUNT_MAX * 3 + 2;
static Field   fields[MAX_FIELDS];
static int     s_fieldCount = 0;

static char s_ipLabels[PRINTER_COUNT_MAX][12];
static char s_snLabels[PRINTER_COUNT_MAX][12];
static char s_cdLabels[PRINTER_COUNT_MAX][12];

static constexpr int VISIBLE_FIELDS = 6;
static constexpr int ROW_H          = 15;

// Theme color names
static const char* const THEME_NAMES[9] = {
    "Blue", "Green", "Purple", "Orange", "Red",
    "AMS-1", "AMS-2", "AMS-3", "AMS-4"
};

// Screen timeout display names (0–60): 0 = "Never", 1–60 = "1m".."60m"
static char s_scrOffNames[61][8];
static const char* s_scrOffNamePtrs[61];
static bool s_scrOffNamesBuilt = false;

static void buildScrOffNames() {
    if (s_scrOffNamesBuilt) return;
    snprintf(s_scrOffNames[0], sizeof(s_scrOffNames[0]), "Never");
    s_scrOffNamePtrs[0] = s_scrOffNames[0];
    for (int i = 1; i <= 60; i++) {
        snprintf(s_scrOffNames[i], sizeof(s_scrOffNames[i]), "%dm", i);
        s_scrOffNamePtrs[i] = s_scrOffNames[i];
    }
    s_scrOffNamesBuilt = true;
}

static void buildFields() {
    buildScrOffNames();

    int fi = 0;
    fields[fi++] = { "WiFi SSID",     s_draft.wifiSsid, sizeof(s_draft.wifiSsid),
                     FieldType::TEXT,     nullptr, 0, 0, 0, nullptr };
    fields[fi++] = { "WiFi Pass",     s_draft.wifiPass, sizeof(s_draft.wifiPass),
                     FieldType::PASSWORD, nullptr, 0, 0, 0, nullptr };
    fields[fi++] = { "Printer Count", nullptr, 0,
                     FieldType::SPINNER,  &s_draft.printerCount, 1, PRINTER_COUNT_MAX, 0, nullptr };
    fields[fi++] = { "Brightness",    nullptr, 0,
                     FieldType::SPINNER,  &s_draft.brightness, 1, 10, 0, nullptr };
    fields[fi++] = { "Theme Color",   nullptr, 0,
                     FieldType::SPINNER,  &s_draft.themeColor, 0, 8, 0, THEME_NAMES };
    fields[fi++] = { "Scr Off (min)", nullptr, 0,
                     FieldType::SPINNER,  &s_draft.screenTimeoutMin, 0, 60, 0, s_scrOffNamePtrs };

    for (int i = 0; i < s_draft.printerCount && i < PRINTER_COUNT_MAX; i++) {
        snprintf(s_ipLabels[i], sizeof(s_ipLabels[i]), "P%d IP",     i + 1);
        snprintf(s_snLabels[i], sizeof(s_snLabels[i]), "P%d Serial", i + 1);
        snprintf(s_cdLabels[i], sizeof(s_cdLabels[i]), "P%d Code",   i + 1);

        fields[fi++] = { s_ipLabels[i], s_draft.printers[i].ip,
                         sizeof(s_draft.printers[i].ip),     FieldType::TEXT,     nullptr, 0, 0, 0, nullptr };
        fields[fi++] = { s_snLabels[i], s_draft.printers[i].serial,
                         sizeof(s_draft.printers[i].serial), FieldType::TEXT,     nullptr, 0, 0, 0, nullptr };
        fields[fi++] = { s_cdLabels[i], s_draft.printers[i].code,
                         sizeof(s_draft.printers[i].code),   FieldType::PASSWORD, nullptr, 0, 0, 0, nullptr };
    }

    // SD card import/export buttons
    fields[fi++] = { "Import from SD", nullptr, 0, FieldType::BUTTON, nullptr, 0, 0, 1, nullptr };
    fields[fi++] = { "Export to SD",   nullptr, 0, FieldType::BUTTON, nullptr, 0, 0, 2, nullptr };

    s_fieldCount = fi;

    if (s_fieldIdx >= s_fieldCount) {
        s_fieldIdx = s_fieldCount - 1;
        s_scrollOff = max(0, s_fieldIdx - VISIBLE_FIELDS + 1);
    }
}

// ── onEnter ───────────────────────────────────────────────────────────────────

void ScreenSettings::onEnter() {
    memcpy(&s_draft, &Settings::instance().data, sizeof(AppSettings));
    if (s_draft.printerCount < 1)                  s_draft.printerCount = 1;
    if (s_draft.printerCount > PRINTER_COUNT_MAX)  s_draft.printerCount = PRINTER_COUNT_MAX;
    if (s_draft.brightness < 1 || s_draft.brightness > 10) s_draft.brightness = 7;
    if (s_draft.themeColor > 8)                            s_draft.themeColor = 0;
    if (s_draft.screenTimeoutMin > 60)                     s_draft.screenTimeoutMin = 60;
    buildFields();
    s_fieldIdx  = 0;
    s_scrollOff = 0;
}

// ── draw ──────────────────────────────────────────────────────────────────────

void ScreenSettings::draw(LGFX_Sprite& s) {
    if (millis() - s_cursorBlink > 500) {
        s_cursorBlink = millis();
        s_cursorOn    = !s_cursorOn;
    }

    s.setFont(&fonts::Font0);
    s.setTextColor(TFT_WHITE, TFT_BLACK);
    s.drawString("Settings", 4, 2);
    s.drawFastHLine(0, 11, DISP_W, s.color565(60, 60, 60));

    int y = 14;
    for (int i = 0; i < VISIBLE_FIELDS; i++) {
        int fi = s_scrollOff + i;
        if (fi >= s_fieldCount) break;

        const Field& f    = fields[fi];
        bool isActive     = (fi == s_fieldIdx);
        uint16_t bg       = isActive ? s.color565(0, 50, 110) : TFT_BLACK;
        uint16_t labelCol = isActive ? TFT_WHITE : s.color565(130, 130, 130);

        s.fillRect(0, y, DISP_W, ROW_H - 1, bg);

        if (f.type == FieldType::BUTTON) {
            uint16_t btnCol = isActive ? s.color565(0, 200, 120) : s.color565(80, 160, 100);
            s.setTextColor(btnCol, bg);
            char btnLabel[32];
            snprintf(btnLabel, sizeof(btnLabel), isActive ? "> %s <" : "  %s  ", f.label);
            s.drawString(btnLabel, 4, y + 3);
        } else {
            s.setTextColor(labelCol, bg);
            s.drawString(f.label, 4, y + 3);

            if (f.type == FieldType::SPINNER) {
                char spinBuf[24];
                const char* valStr = nullptr;
                char numBuf[8];
                if (f.spinnerNames) {
                    valStr = f.spinnerNames[*f.spinnerVal];
                } else {
                    snprintf(numBuf, sizeof(numBuf), "%d", (int)(*f.spinnerVal));
                    valStr = numBuf;
                }
                if (isActive)
                    snprintf(spinBuf, sizeof(spinBuf), "< %s >", valStr);
                else
                    snprintf(spinBuf, sizeof(spinBuf), "%s", valStr);
                s.setTextColor(isActive ? TFT_WHITE : s.color565(180, 180, 180), bg);
                s.drawString(spinBuf, 90, y + 3);
            } else {
                char display[68];
                size_t vlen = strlen(f.buf);
                if (f.type == FieldType::PASSWORD && !isActive) {
                    memset(display, '*', vlen);
                    display[vlen] = '\0';
                } else {
                    strlcpy(display, f.buf, sizeof(display));
                }
                if (isActive && s_cursorOn) {
                    size_t dlen = strlen(display);
                    if (dlen < sizeof(display) - 1) {
                        display[dlen]     = '_';
                        display[dlen + 1] = '\0';
                    }
                }
                char truncated[28];
                truncateFilename(display, truncated, 26);
                s.setTextColor(isActive ? TFT_WHITE : s.color565(180, 180, 180), bg);
                s.drawString(truncated, 90, y + 3);
            }
        }

        y += ROW_H;
    }

    // Scroll indicator
    if (s_fieldCount > VISIBLE_FIELDS) {
        int trackH = VISIBLE_FIELDS * ROW_H;
        int thumbH = max(3, trackH * VISIBLE_FIELDS / s_fieldCount);
        int thumbY = 14 + (trackH - thumbH) * s_scrollOff / max(1, s_fieldCount - VISIBLE_FIELDS);
        s.fillRect(DISP_W - 3, 14, 2, trackH, s.color565(40, 40, 40));
        s.fillRect(DISP_W - 3, thumbY, 2, thumbH, s.color565(130, 130, 130));
    }
}

// ── handleKey ─────────────────────────────────────────────────────────────────

void ScreenSettings::handleKey(char c, bool /*fn*/, bool enter, bool /*del*/,
                                bool tab, bool backspace) {
    if (s_fieldIdx >= s_fieldCount) return;
    Field& f = fields[s_fieldIdx];

    // Save shortcut
    if (c == '*') {
        memcpy(&Settings::instance().data, &s_draft, sizeof(AppSettings));
        Settings::instance().save();
        delay(200);
        ESP.restart();
        return;
    }

    // Spinner handling
    if (f.type == FieldType::SPINNER) {
        if (c == '+' || c == '=') {
            if (*f.spinnerVal < f.spinnerMax) {
                (*f.spinnerVal)++;
                buildFields();
            }
            return;
        }
        if (c == '-') {
            if (*f.spinnerVal > f.spinnerMin) {
                (*f.spinnerVal)--;
                buildFields();
            }
            return;
        }
    }

    // Button handling — Enter activates the button
    if (f.type == FieldType::BUTTON && enter) {
        if (f.buttonId == 1) {
            bool ok = Settings::instance().importFromSd();
            if (ok) {
                memcpy(&s_draft, &Settings::instance().data, sizeof(AppSettings));
                buildFields();
                UIManager::instance().showHint("Imported from SD!", 2000);
            } else {
                UIManager::instance().showHint("No config on SD card", 2000);
            }
        } else if (f.buttonId == 2) {
            memcpy(&Settings::instance().data, &s_draft, sizeof(AppSettings));
            bool ok = Settings::instance().exportToSd();
            UIManager::instance().showHint(ok ? "Exported to SD!" : "SD write failed", 2000);
        }
        return;
    }

    // Text field: backspace
    if (f.type == FieldType::TEXT || f.type == FieldType::PASSWORD) {
        if (backspace || c == '\x08') {
            size_t len = strlen(f.buf);
            if (len > 0) f.buf[len - 1] = '\0';
            return;
        }
    }

    // Navigate: Tab or down arrow
    if (tab || c == '\x12') {
        if (s_fieldIdx < s_fieldCount - 1) {
            s_fieldIdx++;
            if (s_fieldIdx >= s_scrollOff + VISIBLE_FIELDS)
                s_scrollOff = s_fieldIdx - VISIBLE_FIELDS + 1;
        } else {
            s_fieldIdx  = 0;
            s_scrollOff = 0;
        }
        return;
    }

    // Navigate: up arrow
    if (c == '\x11') {
        if (s_fieldIdx > 0) {
            s_fieldIdx--;
            if (s_fieldIdx < s_scrollOff) s_scrollOff = s_fieldIdx;
        }
        return;
    }

    // Enter: advance field
    if (enter) {
        if (s_fieldIdx < s_fieldCount - 1) {
            s_fieldIdx++;
            if (s_fieldIdx >= s_scrollOff + VISIBLE_FIELDS)
                s_scrollOff = s_fieldIdx - VISIBLE_FIELDS + 1;
        }
        return;
    }

    // Printable character → append to text/password field
    if ((f.type == FieldType::TEXT || f.type == FieldType::PASSWORD)
        && c >= 0x20 && c < 0x7F) {
        size_t len = strlen(f.buf);
        if (len < f.maxLen - 1) {
            f.buf[len]     = c;
            f.buf[len + 1] = '\0';
        }
    }
}

const char* ScreenSettings::hintText() {
    return "Tab/Enter=next  +/-=val  *=save&reboot";
}
