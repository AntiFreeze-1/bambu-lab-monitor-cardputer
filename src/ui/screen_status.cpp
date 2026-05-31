#include "screen_status.h"
#include "../utils.h"
#include "../printer_manager.h"

static uint16_t stateColor(GcodeState s, LGFX_Sprite& sp) {
    switch (s) {
        case GcodeState::RUNNING: return sp.color565(0,  200,  0);
        case GcodeState::PAUSE:   return sp.color565(220, 180, 0);
        case GcodeState::FAILED:  return sp.color565(220,  40, 40);
        case GcodeState::FINISH:  return sp.color565(0,  160, 220);
        default:                  return sp.color565(100, 100, 100);
    }
}

// Temp color: white at target, yellow within 5°, red beyond 10°
static uint16_t tempColor(float actual, float target, LGFX_Sprite& sp) {
    float diff = abs(actual - target);
    if (target < 5.0f)  return TFT_WHITE; // target is 0 (off)
    if (diff <= 5.0f)   return TFT_WHITE;
    if (diff <= 15.0f)  return sp.color565(220, 180, 0);
    return sp.color565(220, 60, 60);
}

void ScreenStatus::draw(LGFX_Sprite& s, const PrinterState& st) {
    int y = 4;
    s.setFont(&fonts::Font2);

    // ── Row 1: printer name + state ──────────────────────────────────────────
    uint16_t dotCol = stateColor(st.gcodeState, s);
    s.setTextColor(TFT_WHITE, TFT_BLACK);
    s.drawString(st.name, 4, y);

    // State dot + label on right side
    const char* stateName = gcodeStateName(st.gcodeState);
    int stateX = DISP_W - strlen(stateName) * 7 - 14;
    s.fillCircle(stateX - 6, y + 5, 4, dotCol);
    s.setTextColor(dotCol, TFT_BLACK);
    s.drawString(stateName, stateX, y);

    y += 14;

    // ── Row 2: progress bar + % + ETA ────────────────────────────────────────
    const int barX = 4, barW = 160, barH = 9;
    s.drawRect(barX, y, barW, barH, s.color565(80, 80, 80));
    int fill = (int)(barW - 2) * st.progressPct / 100;
    if (fill > 0) s.fillRect(barX + 1, y + 1, fill, barH - 2, s.color565(0, 180, 60));

    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", st.progressPct);
    s.setTextColor(TFT_WHITE, TFT_BLACK);
    s.drawString(pct, barX + barW + 4, y);

    if (st.gcodeState == GcodeState::RUNNING && st.remainingMin > 0) {
        char eta[12];
        formatEta(st.remainingMin, eta, sizeof(eta));
        char etaStr[18];
        snprintf(etaStr, sizeof(etaStr), "~%s", eta);
        int etaX = DISP_W - strlen(etaStr) * 7 - 2;
        s.setTextColor(s.color565(160, 160, 160), TFT_BLACK);
        s.drawString(etaStr, etaX, y);
    }

    y += 14;

    // ── Row 3: layer count ───────────────────────────────────────────────────
    if (st.layerTotal > 0) {
        char layerBuf[32];
        snprintf(layerBuf, sizeof(layerBuf), "Layer %u / %u", st.layerCurrent, st.layerTotal);
        s.setTextColor(s.color565(200, 200, 200), TFT_BLACK);
        s.drawString(layerBuf, 4, y);
    }
    y += 14;

    // ── Row 4: temperatures ──────────────────────────────────────────────────
    char nozzleBuf[20], bedBuf[20];
    formatTemp(st.nozzleTemp, st.nozzleTarget, nozzleBuf, sizeof(nozzleBuf));
    formatTemp(st.bedTemp,    st.bedTarget,    bedBuf,    sizeof(bedBuf));

    s.setTextColor(s.color565(150, 150, 150), TFT_BLACK);
    s.drawString("Nozzle:", 4, y);
    s.setTextColor(tempColor(st.nozzleTemp, st.nozzleTarget, s), TFT_BLACK);
    s.drawString(nozzleBuf, 52, y);

    s.setTextColor(s.color565(150, 150, 150), TFT_BLACK);
    s.drawString("Bed:", 128, y);
    s.setTextColor(tempColor(st.bedTemp, st.bedTarget, s), TFT_BLACK);
    s.drawString(bedBuf, 156, y);

    y += 14;

    // ── Row 5: speed + connection indicator ─────────────────────────────────
    s.setTextColor(s.color565(150, 150, 150), TFT_BLACK);
    s.drawString("Spd:", 4, y);
    s.setTextColor(TFT_WHITE, TFT_BLACK);
    char spdBuf[24];
    snprintf(spdBuf, sizeof(spdBuf), "%s (%s)",
             speedLevelName(st.speedLevel), speedLevelPct(st.speedLevel));
    s.drawString(spdBuf, 32, y);

    // MQTT connection dot
    uint16_t mqttCol = st.mqttConnected ? s.color565(0,200,0) : s.color565(200,50,50);
    s.fillCircle(DISP_W - 8, y + 5, 4, mqttCol);

    y += 14;

    // ── Row 6: current filename ──────────────────────────────────────────────
    if (st.currentFile[0] != '\0') {
        char truncated[34];
        truncateFilename(filenameOnly(st.currentFile), truncated, 33);
        s.setTextColor(s.color565(120, 120, 120), TFT_BLACK);
        s.drawString(truncated, 4, y);
    }
}

void ScreenStatus::handleKey(char /*c*/, bool /*fn*/, bool /*enter*/, bool /*del*/) {
    // No actions on status screen
}

const char* ScreenStatus::hintText() {
    return ", . tabs  MQTT live";
}
