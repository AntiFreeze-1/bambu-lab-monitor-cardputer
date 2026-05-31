#include "screen_ams.h"
#include "../utils.h"

static constexpr int CELL_W  = 56;
static constexpr int CELL_H  = 40;
static constexpr int CELL_PAD = 3;

void ScreenAms::draw(LGFX_Sprite& s, const PrinterState& st) {
    if (st.amsUnitCount == 0) {
        s.setFont(&fonts::Font2);
        s.setTextColor(s.color565(120, 120, 120), TFT_BLACK);
        s.drawString("No AMS data yet", 4, 44);
        s.drawString("Waiting for MQTT...", 4, 60);
        return;
    }

    int unitY = 2;
    for (uint8_t u = 0; u < st.amsUnitCount && u < 4; u++) {
        const AmsUnit& unit = st.amsUnits[u];
        if (!unit.present) continue;

        // Unit label
        char label[16];
        snprintf(label, sizeof(label), "AMS %d", u + 1);
        s.setFont(&fonts::Font0);
        s.setTextColor(s.color565(150, 150, 150), TFT_BLACK);
        s.drawString(label, 4, unitY);
        unitY += 10;

        for (int t = 0; t < 4; t++) {
            int cellX = CELL_PAD + t * (CELL_W + CELL_PAD);
            int cellY = unitY;

            const AmsTray& tray = unit.trays[t];

            if (!tray.valid) {
                // Empty tray
                s.fillRect(cellX, cellY, CELL_W, CELL_H, s.color565(30, 30, 30));
                s.drawRect(cellX, cellY, CELL_W, CELL_H, s.color565(60, 60, 60));
                s.setFont(&fonts::Font0);
                s.setTextColor(s.color565(80, 80, 80), s.color565(30, 30, 30));
                s.drawString("---", cellX + (CELL_W - 18) / 2, cellY + (CELL_H - 8) / 2);
            } else {
                // Tray color background
                uint16_t col565 = rgb24to565(tray.color);
                s.fillRect(cellX, cellY, CELL_W, CELL_H, col565);
                s.drawRect(cellX, cellY, CELL_W, CELL_H, TFT_WHITE);

                // Fill level bar (vertical, bottom-anchored)
                int fillH = (int)(CELL_H - 2) * tray.remain / 100;
                int fillY = cellY + 1 + (CELL_H - 2 - fillH);
                // Dark overlay for empty portion
                if (fillH < CELL_H - 2) {
                    s.fillRect(cellX + 1, cellY + 1, CELL_W - 2, CELL_H - 2 - fillH,
                               s.color565(0, 0, 0));
                    // Restore fill color for filled portion
                    s.fillRect(cellX + 1, fillY, CELL_W - 2, fillH, col565);
                }

                // Text overlay — choose contrasting color
                uint8_t r = (tray.color >> 16) & 0xFF;
                uint8_t g = (tray.color >>  8) & 0xFF;
                uint8_t b =  tray.color        & 0xFF;
                uint16_t luma = (uint16_t)(r * 299 + g * 587 + b * 114) / 1000;
                uint16_t txtCol = (luma > 128) ? TFT_BLACK : TFT_WHITE;

                // Remain % at top
                char pctBuf[6];
                snprintf(pctBuf, sizeof(pctBuf), "%d%%", tray.remain);
                s.setFont(&fonts::Font0);
                s.setTextColor(txtCol, col565);
                s.drawString(pctBuf, cellX + 3, cellY + 3);

                // Type at bottom
                s.drawString(tray.type, cellX + 3, cellY + CELL_H - 10);
            }
        }

        unitY += CELL_H + 4;
        if (unitY >= CONTENT_H - 10) break;
    }
}

void ScreenAms::handleKey(char /*c*/, bool /*fn*/, bool /*enter*/, bool /*del*/) {}

const char* ScreenAms::hintText() {
    return ", . tabs";
}
