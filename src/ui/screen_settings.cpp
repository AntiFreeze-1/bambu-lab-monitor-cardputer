#include "screen_settings.h"
#include "../settings.h"
#include "../printer_manager.h"
#include "ui_manager.h"

// ── Form field definitions ────────────────────────────────────────────────────

struct Field {
    const char* label;
    char*       buf;
    size_t      maxLen;
    bool        isPassword;
};

static AppSettings s_draft;
static int         s_fieldIdx    = 0;
static uint32_t    s_cursorBlink = 0;
static bool        s_cursorOn    = true;

static Field fields[10];

static void buildFields() {
    fields[0] = { "WiFi SSID",     s_draft.wifiSsid,           sizeof(s_draft.wifiSsid),           false };
    fields[1] = { "WiFi Pass",     s_draft.wifiPass,           sizeof(s_draft.wifiPass),           true  };
    fields[2] = { "P1 Name",       s_draft.printers[0].name,   sizeof(s_draft.printers[0].name),   false };
    fields[3] = { "P1 IP",         s_draft.printers[0].ip,     sizeof(s_draft.printers[0].ip),     false };
    fields[4] = { "P1 Serial",     s_draft.printers[0].serial, sizeof(s_draft.printers[0].serial), false };
    fields[5] = { "P1 Code",       s_draft.printers[0].code,   sizeof(s_draft.printers[0].code),   true  };
    fields[6] = { "P2 Name",       s_draft.printers[1].name,   sizeof(s_draft.printers[1].name),   false };
    fields[7] = { "P2 IP",         s_draft.printers[1].ip,     sizeof(s_draft.printers[1].ip),     false };
    fields[8] = { "P2 Serial",     s_draft.printers[1].serial, sizeof(s_draft.printers[1].serial), false };
    fields[9] = { "P2 Code",       s_draft.printers[1].code,   sizeof(s_draft.printers[1].code),   true  };
}

static constexpr int FIELD_COUNT   = 10;
static constexpr int VISIBLE_FIELDS = 6;
static constexpr int ROW_H          = 15;
static int           s_scrollOff    = 0;

void ScreenSettings::onEnter() {
    // Copy current settings into draft for editing
    memcpy(&s_draft, &Settings::instance().data, sizeof(AppSettings));
    buildFields();
    s_fieldIdx  = 0;
    s_scrollOff = 0;
}

void ScreenSettings::draw(LGFX_Sprite& s) {
    // Cursor blink
    if (millis() - s_cursorBlink > 500) {
        s_cursorBlink = millis();
        s_cursorOn    = !s_cursorOn;
    }

    s.setFont(&fonts::Font0);
    s.setTextColor(TFT_WHITE, TFT_BLACK);
    s.drawString("Settings", 4, 2);

    // Separator
    s.drawFastHLine(0, 11, DISP_W, s.color565(60, 60, 60));

    int y = 14;
    for (int i = 0; i < VISIBLE_FIELDS; i++) {
        int fi = s_scrollOff + i;
        if (fi >= FIELD_COUNT) break;

        const Field& f    = fields[fi];
        bool isActive     = (fi == s_fieldIdx);
        uint16_t bg       = isActive ? s.color565(0, 50, 110) : TFT_BLACK;
        uint16_t labelCol = isActive ? TFT_WHITE : s.color565(130, 130, 130);

        s.fillRect(0, y, DISP_W, ROW_H - 1, bg);

        // Label
        s.setFont(&fonts::Font0);
        s.setTextColor(labelCol, bg);
        s.drawString(f.label, 4, y + 3);

        // Value (or masked password)
        char display[68];
        size_t vlen = strlen(f.buf);
        if (f.isPassword && !isActive) {
            memset(display, '*', vlen);
            display[vlen] = '\0';
        } else {
            strlcpy(display, f.buf, sizeof(display));
        }

        // Append cursor if active
        if (isActive && s_cursorOn) {
            size_t dlen = strlen(display);
            if (dlen < sizeof(display) - 1) {
                display[dlen]     = '_';
                display[dlen + 1] = '\0';
            }
        }

        uint16_t valCol = isActive ? TFT_WHITE : s.color565(180, 180, 180);
        s.setTextColor(valCol, bg);

        // Truncate display to fit right side
        char truncated[28];
        truncateFilename(display, truncated, 26);
        s.drawString(truncated, 72, y + 3);

        y += ROW_H;
    }

    // Scroll indicator
    if (FIELD_COUNT > VISIBLE_FIELDS) {
        int trackH = VISIBLE_FIELDS * ROW_H;
        int thumbH = max(3, trackH * VISIBLE_FIELDS / FIELD_COUNT);
        int thumbY = 14 + (trackH - thumbH) * s_scrollOff / (FIELD_COUNT - VISIBLE_FIELDS);
        s.fillRect(DISP_W - 3, 14, 2, trackH, s.color565(40, 40, 40));
        s.fillRect(DISP_W - 3, thumbY, 2, thumbH, s.color565(130, 130, 130));
    }

    // Save row at bottom
    int saveY = 14 + VISIBLE_FIELDS * ROW_H + 2;
    s.setFont(&fonts::Font0);
    s.setTextColor(s.color565(0, 200, 80), TFT_BLACK);
    s.drawString("[ Enter on last field or press * to save ]", 4, saveY);
}

void ScreenSettings::handleKey(char c, bool /*fn*/, bool enter, bool /*del*/,
                                bool tab, bool backspace) {
    Field& f = fields[s_fieldIdx];

    if (backspace || c == '\x08') {
        size_t len = strlen(f.buf);
        if (len > 0) f.buf[len - 1] = '\0';
        return;
    }

    // Tab or down arrow = next field
    if (tab || c == '\x12') {
        if (s_fieldIdx < FIELD_COUNT - 1) {
            s_fieldIdx++;
            if (s_fieldIdx >= s_scrollOff + VISIBLE_FIELDS)
                s_scrollOff = s_fieldIdx - VISIBLE_FIELDS + 1;
        } else {
            // On last field, Tab wraps to first
            s_fieldIdx  = 0;
            s_scrollOff = 0;
        }
        return;
    }

    // Up arrow = previous field
    if (c == '\x11') {
        if (s_fieldIdx > 0) {
            s_fieldIdx--;
            if (s_fieldIdx < s_scrollOff) s_scrollOff = s_fieldIdx;
        }
        return;
    }

    // Enter: advance field, or save on last field
    if (enter) {
        if (s_fieldIdx < FIELD_COUNT - 1) {
            s_fieldIdx++;
            if (s_fieldIdx >= s_scrollOff + VISIBLE_FIELDS)
                s_scrollOff = s_fieldIdx - VISIBLE_FIELDS + 1;
        } else {
            // Save and restart
            memcpy(&Settings::instance().data, &s_draft, sizeof(AppSettings));
            Settings::instance().save();
            delay(200);
            ESP.restart();
        }
        return;
    }

    // '*' key = save from any field
    if (c == '*') {
        memcpy(&Settings::instance().data, &s_draft, sizeof(AppSettings));
        Settings::instance().save();
        delay(200);
        ESP.restart();
        return;
    }

    // Printable character → append to current field
    if (c >= 0x20 && c < 0x7F) {
        size_t len = strlen(f.buf);
        if (len < f.maxLen - 1) {
            f.buf[len]     = c;
            f.buf[len + 1] = '\0';
        }
    }
}

const char* ScreenSettings::hintText() {
    return "Tab/Enter=next  *=save+reboot";
}
