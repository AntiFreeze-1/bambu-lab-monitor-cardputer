#include "screen_files.h"
#include "../utils.h"
#include "../printer_manager.h"
#include "ui_manager.h"

static int  s_selectedIdx  = 0;
static int  s_scrollOffset = 0;

static constexpr int VISIBLE_ROWS = 7;
static constexpr int ROW_H        = 13;

void ScreenFiles::onEnter(PrinterState& state) {
    (void)state;
    s_selectedIdx  = 0;
    s_scrollOffset = 0;
}

void ScreenFiles::draw(LGFX_Sprite& s, const PrinterState& st) {
    s.setFont(&fonts::Font0);

    // Header line
    char header[48];
    if (st.fileCount == 0) {
        snprintf(header, sizeof(header), "No files  [r]=refresh");
    } else {
        snprintf(header, sizeof(header), "[%s] %d file%s  [r]=refresh",
                 st.fileListFresh ? "FTP" : "cached",
                 st.fileCount, st.fileCount == 1 ? "" : "s");
    }
    s.setTextColor(s.color565(140, 140, 140), TFT_BLACK);
    s.drawString(header, 4, 3);

    if (st.fileCount == 0) {
        s.setTextColor(s.color565(80, 80, 80), TFT_BLACK);
        s.drawString("Press 'r' to browse SD card", 4, 20);
        s.drawString("or wait for MQTT to cache", 4, 34);
        return;
    }

    int y = 16;
    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = s_scrollOffset + i;
        if (idx >= st.fileCount) break;

        bool selected = (idx == s_selectedIdx);
        uint16_t bg = selected ? s.color565(0, 80, 160) : TFT_BLACK;
        s.fillRect(0, y, DISP_W, ROW_H, bg);

        if (selected) {
            s.setTextColor(TFT_WHITE, bg);
            s.drawString("\x10", 2, y + 2); // ► character
        }

        char truncated[36];
        truncateFilename(st.fileList[idx], truncated, 34);
        s.setTextColor(selected ? TFT_WHITE : s.color565(200, 200, 200), bg);
        s.drawString(truncated, 12, y + 2);

        y += ROW_H;
    }

    // Scroll indicator
    if (st.fileCount > VISIBLE_ROWS) {
        int trackH = VISIBLE_ROWS * ROW_H;
        int thumbH = max(4, trackH * VISIBLE_ROWS / st.fileCount);
        int thumbY = 16 + trackH * s_scrollOffset / st.fileCount;
        s.fillRect(DISP_W - 4, 16, 3, trackH, s.color565(40, 40, 40));
        s.fillRect(DISP_W - 4, thumbY, 3, thumbH, s.color565(100, 100, 100));
    }
}

void ScreenFiles::handleKey(char c, bool /*fn*/, bool enter, bool /*del*/) {
    PrinterManager& pm = PrinterManager::instance();
    PrinterState&   st = pm.activeState();

    if (c == 'r' || c == 'R') {
        // Show loading overlay then refresh
        UIManager::instance().showHint("Connecting FTP...", 8000);
        pm.refreshFileList(pm.activeIndex(), nullptr);
        s_selectedIdx  = 0;
        s_scrollOffset = 0;
        UIManager::instance().showHint(
            st.fileListFresh ? "FTP OK" : "FTP failed, cached", 2000);
        return;
    }

    if (c == '\x11') { // up
        if (s_selectedIdx > 0) {
            s_selectedIdx--;
            if (s_selectedIdx < s_scrollOffset)
                s_scrollOffset = s_selectedIdx;
        }
    } else if (c == '\x12') { // down
        if (s_selectedIdx < st.fileCount - 1) {
            s_selectedIdx++;
            if (s_selectedIdx >= s_scrollOffset + VISIBLE_ROWS)
                s_scrollOffset = s_selectedIdx - VISIBLE_ROWS + 1;
        }
    } else if (enter) {
        if (st.fileCount == 0) return;

        const char* fname = st.fileList[s_selectedIdx];

        if (st.gcodeState == GcodeState::PAUSE &&
            strcmp(filenameOnly(st.currentFile), filenameOnly(fname)) == 0) {
            pm.sendResume();
            UIManager::instance().showHint("Resuming...", 2000);
        } else {
            pm.sendStartPrint(fname);
            UIManager::instance().showHint("Print started!", 2000);
        }
    }
}

const char* ScreenFiles::hintText() {
    return "Up/Dn sel  Enter=print  r=FTP";
}
