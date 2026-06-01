#include "screen_files.h"
#include "../utils.h"
#include "../printer_manager.h"
#include "ui_manager.h"
#include <SD.h>

// ── State ─────────────────────────────────────────────────────────────────────

static int  s_selectedIdx  = 0;
static int  s_scrollOffset = 0;

// Cardputer SD card mode
static bool    s_sdMode     = false;
static int     s_sdSelected = 0;
static int     s_sdScroll   = 0;
static char    s_sdFiles[FTP_MAX_FILES][FTP_FILENAME_LEN + 1];
static uint8_t s_sdCount    = 0;

static constexpr int VISIBLE_ROWS = 7;
static constexpr int ROW_H        = 13;

static void loadSdFiles() {
    s_sdCount = 0;
    File root = SD.open("/");
    if (!root) return;
    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
            const char* n = entry.name();
            size_t nlen = strlen(n);
            bool isGcode = nlen > 6 && strcmp(n + nlen - 6, ".gcode") == 0;
            bool is3mf   = nlen > 4 && strcmp(n + nlen - 4, ".3mf")   == 0;
            if ((isGcode || is3mf) && s_sdCount < FTP_MAX_FILES)
                strlcpy(s_sdFiles[s_sdCount++], n, FTP_FILENAME_LEN + 1);
        }
        entry.close();
    }
    root.close();
}

// ── onEnter ───────────────────────────────────────────────────────────────────

void ScreenFiles::onEnter(PrinterState& /*state*/) {
    s_selectedIdx  = 0;
    s_scrollOffset = 0;
    s_sdMode       = false;
    s_sdSelected   = 0;
    s_sdScroll     = 0;
    s_sdCount      = 0;
}

// ── draw ──────────────────────────────────────────────────────────────────────

void ScreenFiles::draw(LGFX_Sprite& s, const PrinterState& st) {
    s.setFont(&fonts::Font0);

    if (s_sdMode) {
        // ── Cardputer SD card view ────────────────────────────────────────────
        char header[48];
        snprintf(header, sizeof(header), "SD Card: %d file%s  [s]=printer",
                 s_sdCount, s_sdCount == 1 ? "" : "s");
        s.setTextColor(s.color565(100, 200, 100), TFT_BLACK);
        s.drawString(header, 4, 3);

        if (s_sdCount == 0) {
            s.setTextColor(s.color565(80, 80, 80), TFT_BLACK);
            s.drawString("No .gcode/.3mf on SD card", 4, 20);
            s.drawString("Insert SD and re-enter screen", 4, 34);
            return;
        }

        int y = 16;
        for (int i = 0; i < VISIBLE_ROWS; i++) {
            int idx = s_sdScroll + i;
            if (idx >= s_sdCount) break;
            bool selected = (idx == s_sdSelected);
            uint16_t bg = selected ? s.color565(0, 100, 60) : TFT_BLACK;
            s.fillRect(0, y, DISP_W, ROW_H, bg);
            if (selected) {
                s.setTextColor(TFT_WHITE, bg);
                s.drawString("\x10", 2, y + 2);
            }
            char truncated[36];
            truncateFilename(s_sdFiles[idx], truncated, 34);
            s.setTextColor(selected ? TFT_WHITE : s.color565(200, 200, 200), bg);
            s.drawString(truncated, 12, y + 2);
            y += ROW_H;
        }

        // Scroll indicator
        if (s_sdCount > VISIBLE_ROWS) {
            int trackH = VISIBLE_ROWS * ROW_H;
            int thumbH = max(4, trackH * VISIBLE_ROWS / s_sdCount);
            int thumbY = 16 + trackH * s_sdScroll / s_sdCount;
            s.fillRect(DISP_W - 4, 16, 3, trackH, s.color565(40, 40, 40));
            s.fillRect(DISP_W - 4, thumbY, 3, thumbH, s.color565(100, 100, 100));
        }
        return;
    }

    // ── Printer file list view ────────────────────────────────────────────────
    char header[48];
    if (st.fileCount == 0) {
        snprintf(header, sizeof(header), "No files  [r]=refresh  [s]=SD");
    } else {
        snprintf(header, sizeof(header), "[%s] %d file%s  [r]=FTP  [s]=SD",
                 st.fileListFresh ? "FTP" : "cached",
                 st.fileCount, st.fileCount == 1 ? "" : "s");
    }
    s.setTextColor(s.color565(140, 140, 140), TFT_BLACK);
    s.drawString(header, 4, 3);

    if (st.fileCount == 0) {
        s.setTextColor(s.color565(80, 80, 80), TFT_BLACK);
        s.drawString("Press 'r' to browse printer SD", 4, 20);
        s.drawString("or 's' for Cardputer SD upload", 4, 34);
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
            s.drawString("\x10", 2, y + 2);
        }
        char truncated[36];
        truncateFilename(st.fileList[idx], truncated, 34);
        s.setTextColor(selected ? TFT_WHITE : s.color565(200, 200, 200), bg);
        s.drawString(truncated, 12, y + 2);
        y += ROW_H;
    }

    if (st.fileCount > VISIBLE_ROWS) {
        int trackH = VISIBLE_ROWS * ROW_H;
        int thumbH = max(4, trackH * VISIBLE_ROWS / st.fileCount);
        int thumbY = 16 + trackH * s_scrollOffset / st.fileCount;
        s.fillRect(DISP_W - 4, 16, 3, trackH, s.color565(40, 40, 40));
        s.fillRect(DISP_W - 4, thumbY, 3, thumbH, s.color565(100, 100, 100));
    }
}

// ── handleKey ─────────────────────────────────────────────────────────────────

void ScreenFiles::handleKey(char c, bool /*fn*/, bool enter, bool /*del*/) {
    PrinterManager& pm = PrinterManager::instance();
    PrinterState&   st = pm.activeState();

    // Toggle between printer files and Cardputer SD card
    if (c == 's' || c == 'S') {
        s_sdMode = !s_sdMode;
        if (s_sdMode) {
            s_sdSelected = 0;
            s_sdScroll   = 0;
            loadSdFiles();
        } else {
            s_selectedIdx  = 0;
            s_scrollOffset = 0;
        }
        return;
    }

    // ── Cardputer SD mode ─────────────────────────────────────────────────────
    if (s_sdMode) {
        if (c == '\x11') { // up
            if (s_sdSelected > 0) {
                s_sdSelected--;
                if (s_sdSelected < s_sdScroll) s_sdScroll = s_sdSelected;
            }
        } else if (c == '\x12') { // down
            if (s_sdSelected < s_sdCount - 1) {
                s_sdSelected++;
                if (s_sdSelected >= s_sdScroll + VISIBLE_ROWS)
                    s_sdScroll = s_sdSelected - VISIBLE_ROWS + 1;
            }
        } else if (enter) {
            if (s_sdCount == 0) return;

            const char* fname = s_sdFiles[s_sdSelected];
            char localPath[FTP_FILENAME_LEN + 2];
            snprintf(localPath, sizeof(localPath), "/%s", fname);

            UIManager::instance().showHint("Uploading...", 30000);

            bool ok = pm.uploadFileToActive(localPath, fname);

            if (ok) {
                UIManager::instance().showHint("Upload OK! Starting print...", 3000);
                pm.sendStartPrint(fname);
            } else {
                UIManager::instance().showHint("Upload failed", 3000);
            }
        }
        return;
    }

    // ── Printer file list mode ────────────────────────────────────────────────
    if (c == 'r' || c == 'R') {
        UIManager::instance().showHint("Connecting FTP...", 8000);
        pm.refreshFileList(pm.activeIndex(), nullptr);
        s_selectedIdx  = 0;
        s_scrollOffset = 0;
        UIManager::instance().showHint(
            st.fileListFresh ? "FTP OK" : "FTP failed, using cache", 2000);
        return;
    }

    if (c == '\x11') { // up
        if (s_selectedIdx > 0) {
            s_selectedIdx--;
            if (s_selectedIdx < s_scrollOffset) s_scrollOffset = s_selectedIdx;
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
    return s_sdMode
        ? "opt+i/k  Enter=upload+print  s=printer"
        : "opt+i/k  Enter=print  r=FTP  s=SD card";
}
