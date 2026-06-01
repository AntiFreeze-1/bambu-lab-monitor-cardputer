#include "printer_manager.h"
#include "settings.h"
#include <string.h>
#include <stdio.h>

static void deriveModelFromSerial(const char* serial, char* out, size_t outLen) {
    struct { const char* prefix; const char* model; } map[] = {
        { "01S", "X1 Carbon" },
        { "01P", "P1S"       },
        { "00W", "X1E"       },
        { "00M", "X1"        },
        { "030", "A1 Mini"   },
        { "039", "A1"        },
        { "BL-P","P1P"       },
    };
    for (auto& e : map) {
        if (strncmp(serial, e.prefix, strlen(e.prefix)) == 0) {
            strlcpy(out, e.model, outLen);
            return;
        }
    }
    strlcpy(out, "Bambu Printer", outLen);
}

PrinterManager& PrinterManager::instance() {
    static PrinterManager inst;
    return inst;
}

void PrinterManager::begin() {
    const AppSettings& cfg = Settings::instance().data;
    uint8_t count = cfg.printerCount;

    // Clean up any existing clients
    for (auto* c : _clients) delete c;
    _clients.clear();
    _states.clear();
    _states.resize(count);

    for (int i = 0; i < count; i++) {
        PrinterState& s = _states[i];
        memset(&s, 0, sizeof(s));

        strlcpy(s.ip,         cfg.printers[i].ip,     sizeof(s.ip));
        strlcpy(s.serial,     cfg.printers[i].serial, sizeof(s.serial));
        strlcpy(s.accessCode, cfg.printers[i].code,   sizeof(s.accessCode));

        // Seed deviceModel from serial prefix; MQTT get_version may refine it
        deriveModelFromSerial(s.serial, s.deviceModel, sizeof(s.deviceModel));

        s.hasAms     = false;  // determined dynamically from AMS MQTT data
        s.hasAmsLite = false;
        s.speedLevel = SpeedLevel::NORMAL;

        _clients.push_back(new BambuMqttClient(&_states[i]));
        _clients.back()->begin();
    }

    if (_activeIdx >= count) _activeIdx = 0;
}

void PrinterManager::loop() {
    for (auto* c : _clients) {
        if (c) c->loop();
    }
}

PrinterState& PrinterManager::activeState() {
    return _states[_activeIdx];
}

PrinterState& PrinterManager::stateAt(uint8_t idx) {
    return _states[idx < _states.size() ? idx : 0];
}

void PrinterManager::setActiveIndex(uint8_t idx) {
    if (idx < (uint8_t)_states.size()) _activeIdx = idx;
}

void PrinterManager::sendSpeedCommand(SpeedLevel lvl) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"print\":{\"sequence_id\":\"0\",\"command\":\"print_speed\",\"param\":\"%d\"}}",
        (int)lvl);
    if (_activeIdx < _clients.size() && _clients[_activeIdx])
        _clients[_activeIdx]->publish(buf);
}

void PrinterManager::sendStartPrint(const char* filename) {
    const char* fname = strrchr(filename, '/');
    fname = fname ? fname + 1 : filename;

    char url[FTP_FILENAME_LEN + 20];
    snprintf(url, sizeof(url), "file:///sdcard/%s", fname);

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

    if (_activeIdx < _clients.size() && _clients[_activeIdx])
        _clients[_activeIdx]->publish(buf);
}

void PrinterManager::sendResume() {
    const char* payload = "{\"print\":{\"sequence_id\":\"0\",\"command\":\"resume\"}}";
    if (_activeIdx < _clients.size() && _clients[_activeIdx])
        _clients[_activeIdx]->publish(payload);
}

void PrinterManager::sendPause() {
    const char* p = "{\"print\":{\"sequence_id\":\"0\",\"command\":\"pause\"}}";
    if (_activeIdx < _clients.size() && _clients[_activeIdx])
        _clients[_activeIdx]->publish(p);
}

void PrinterManager::sendLightCommand(bool on) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"system\":{\"sequence_id\":\"0\",\"command\":\"led_control\","
        "\"led_node\":\"chamber_light\",\"led_mode\":\"%s\"}}",
        on ? "on" : "off");
    if (_activeIdx < _clients.size() && _clients[_activeIdx])
        _clients[_activeIdx]->publish(buf);
}

void PrinterManager::sendSetTemps(int nozzleC, int bedC) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"print\":{\"sequence_id\":\"0\",\"command\":\"gcode_line\","
        "\"param\":\"M104 S%d\\nM140 S%d\\n\"}}",
        nozzleC, bedC);
    if (_activeIdx < _clients.size() && _clients[_activeIdx])
        _clients[_activeIdx]->publish(buf);
}

void PrinterManager::refreshFileList(uint8_t printerIdx, void (*loadingCb)()) {
    if (printerIdx >= (uint8_t)_states.size()) return;
    PrinterState& s = _states[printerIdx];

    if (loadingCb) loadingCb();

    char ftpFiles[FTP_MAX_FILES][FTP_FILENAME_LEN + 1];
    uint8_t ftpCount = 0;

    bool ok = _ftp.listFiles(s.ip, s.accessCode, ftpFiles, ftpCount);

    if (ok && ftpCount > 0) {
        memset(s.fileList, 0, sizeof(s.fileList));
        s.fileCount = 0;
        for (int i = 0; i < ftpCount && i < FTP_MAX_FILES; i++) {
            strlcpy(s.fileList[s.fileCount++], ftpFiles[i], FTP_FILENAME_LEN + 1);
        }
        s.fileListFresh = true;
    }
}

bool PrinterManager::uploadFileToActive(const char* localSdPath, const char* remoteFilename) {
    PrinterState& s = activeState();
    return _ftp.uploadFile(s.ip, s.accessCode, localSdPath, remoteFilename);
}
