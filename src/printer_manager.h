#pragma once
#include <vector>
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
    uint8_t       activeIndex()  const { return _activeIdx; }
    uint8_t       printerCount() const { return (uint8_t)_states.size(); }
    void          setActiveIndex(uint8_t idx);

    void sendSpeedCommand(SpeedLevel lvl);
    void sendStartPrint(const char* filename);
    void sendResume();
    void sendPause();
    void sendLightCommand(bool on);
    void sendSetTemps(int nozzleC, int bedC);

    void refreshFileList(uint8_t printerIdx, void (*loadingCb)() = nullptr);
    bool uploadFileToActive(const char* localSdPath, const char* remoteFilename);

private:
    PrinterManager() = default;

    std::vector<PrinterState>     _states;
    std::vector<BambuMqttClient*> _clients;
    BambuFtpClient                _ftp;
    uint8_t                       _activeIdx = 0;
};
