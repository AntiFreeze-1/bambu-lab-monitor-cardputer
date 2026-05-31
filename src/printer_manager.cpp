#include "printer_manager.h"
#include "settings.h"
#include <string.h>
#include <stdio.h>

PrinterManager& PrinterManager::instance() {
    static PrinterManager inst;
    return inst;
}

void PrinterManager::begin() {
    const AppSettings& cfg = Settings::instance().data;

    for (int i = 0; i < PRINTER_COUNT; i++) {
        PrinterState& s = _states[i];
        memset(&s, 0, sizeof(s));

        strlcpy(s.name,       cfg.printers[i].name,   sizeof(s.name));
        strlcpy(s.ip,         cfg.printers[i].ip,     sizeof(s.ip));
        strlcpy(s.serial,     cfg.printers[i].serial, sizeof(s.serial));
        strlcpy(s.accessCode, cfg.printers[i].code,   sizeof(s.accessCode));

        // Heuristic: first printer is AMS, second is AMS Lite by default.
        // User can override by editing config or extending settings.
        s.hasAms     = (i == 0);
        s.hasAmsLite = (i == 1);
        s.speedLevel = SpeedLevel::NORMAL;

        delete _clients[i];
        _clients[i] = new BambuMqttClient(&_states[i]);
        _clients[i]->begin();
    }
}

void PrinterManager::loop() {
    for (int i = 0; i < PRINTER_COUNT; i++) {
        if (_clients[i]) _clients[i]->loop();
    }
}

PrinterState& PrinterManager::activeState() {
    return _states[_activeIdx];
}

PrinterState& PrinterManager::stateAt(uint8_t idx) {
    return _states[idx < PRINTER_COUNT ? idx : 0];
}

void PrinterManager::setActiveIndex(uint8_t idx) {
    if (idx < PRINTER_COUNT) _activeIdx = idx;
}

void PrinterManager::sendSpeedCommand(SpeedLevel lvl) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"print\":{\"sequence_id\":\"0\",\"command\":\"print_speed\",\"param\":\"%d\"}}",
        (int)lvl);
    if (_clients[_activeIdx]) _clients[_activeIdx]->publish(buf);
}

void PrinterManager::sendStartPrint(const char* filename) {
    // Strip any leading path component
    const char* fname = strrchr(filename, '/');
    fname = fname ? fname + 1 : filename;

    // Determine base path based on extension
    const char* basePath = "/sdcard/";

    char url[FTP_FILENAME_LEN + 20];
    snprintf(url, sizeof(url), "file://%s%s", basePath, fname);

    // Build subtask_name without extension
    char subtask[FTP_FILENAME_LEN + 1];
    strlcpy(subtask, fname, sizeof(subtask));
    char* dot = strrchr(subtask, '.');
    if (dot) *dot = '\0';

    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"print\":{\"sequence_id\":\"0\",\"command\":\"start\","
        "\"param\":\"\",\"url\":\"%s\","
        "\"subtask_name\":\"%s\","
        "\"bed_type\":\"auto\","
        "\"timelapse\":false,"
        "\"bed_leveling\":true,"
        "\"flow_cali\":false,"
        "\"vibration_cali\":true,"
        "\"layer_inspect\":false,"
        "\"use_ams\":true}}",
        url, subtask);

    if (_clients[_activeIdx]) _clients[_activeIdx]->publish(buf);
}

void PrinterManager::sendResume() {
    const char* payload = "{\"print\":{\"sequence_id\":\"0\",\"command\":\"resume\"}}";
    if (_clients[_activeIdx]) _clients[_activeIdx]->publish(payload);
}

void PrinterManager::refreshFileList(uint8_t printerIdx, void (*loadingCb)()) {
    if (printerIdx >= PRINTER_COUNT) return;
    PrinterState& s = _states[printerIdx];

    if (loadingCb) loadingCb();

    char ftpFiles[FTP_MAX_FILES][FTP_FILENAME_LEN + 1];
    uint8_t ftpCount = 0;

    bool ok = _ftp.listFiles(s.ip, s.accessCode, ftpFiles, ftpCount);

    if (ok && ftpCount > 0) {
        // Replace file list with fresh FTP results
        memset(s.fileList, 0, sizeof(s.fileList));
        s.fileCount = 0;
        for (int i = 0; i < ftpCount && i < FTP_MAX_FILES; i++) {
            strlcpy(s.fileList[s.fileCount++], ftpFiles[i], FTP_FILENAME_LEN + 1);
        }
        s.fileListFresh = true;
    }
    // If FTP failed, keep existing MQTT-cached list (fileListFresh remains false)
}
