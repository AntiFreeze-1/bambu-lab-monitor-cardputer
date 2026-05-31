#pragma once
#include <Arduino.h>
#include "config.h"

class BambuFtpClient {
public:
    // Connects to printer FTP (implicit TLS, port 990), lists .gcode/.3mf files
    // from /sdcard/, writes filenames into files[][]. Returns true on success.
    bool listFiles(const char* ip,
                   const char* accessCode,
                   char        files[][FTP_FILENAME_LEN + 1],
                   uint8_t&    count);

private:
    bool readResponse(WiFiClientSecure& ctrl, int expectedCode, char* buf, size_t bufLen);
    bool parsePasv(const char* resp, char* outIp, uint16_t& outPort);
    void parseListLine(const char* line, char* outName, size_t maxLen);
};
