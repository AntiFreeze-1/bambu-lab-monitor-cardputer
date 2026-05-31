#pragma once
#include "types.h"
#include "mqtt_client.h"
#include "ftp_client.h"

class PrinterManager {
public:
    static PrinterManager& instance();

    void begin();
    void loop();

    PrinterState& activeState();
    PrinterState& stateAt(uint8_t idx);
    uint8_t       activeIndex() const { return _activeIdx; }
    void          setActiveIndex(uint8_t idx);

    void sendSpeedCommand(SpeedLevel lvl);
    void sendStartPrint(const char* filename);
    void sendResume();

    // Synchronous FTP refresh (shows loading overlay via callback).
    // Merges FTP results with MQTT-cached currentFile.
    void refreshFileList(uint8_t printerIdx, void (*loadingCb)() = nullptr);

private:
    PrinterManager() = default;

    PrinterState      _states[PRINTER_COUNT];
    BambuMqttClient*  _clients[PRINTER_COUNT] = {};
    BambuFtpClient    _ftp;
    uint8_t           _activeIdx = 0;
};
