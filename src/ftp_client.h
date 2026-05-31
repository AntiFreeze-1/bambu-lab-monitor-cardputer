#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "config.h"

class BambuFtpClient {
public:
    // List .gcode / .3mf files on printer SD card via implicit-TLS FTP
    bool listFiles(const char* ip,
                   const char* accessCode,
                   char        files[][FTP_FILENAME_LEN + 1],
                   uint8_t&    count);

    // Upload a file from the Cardputer's SD card to the printer's /sdcard/
    bool uploadFile(const char* printerIp,
                    const char* accessCode,
                    const char* localSdPath,
                    const char* remoteFilename);

private:
    bool readResponse(WiFiClientSecure& ctrl, int expectedCode, char* buf, size_t bufLen);
    bool parsePasv(const char* resp, char* outIp, uint16_t& outPort);
    void parseListLine(const char* line, char* outName, size_t maxLen);
};
