#include "ftp_client.h"
#include <WiFiClientSecure.h>
#include <SD.h>
#include <string.h>

bool BambuFtpClient::readResponse(WiFiClientSecure& ctrl, int expectedCode,
                                  char* buf, size_t bufLen) {
    uint32_t start = millis();
    size_t   pos   = 0;
    buf[0] = '\0';

    while (millis() - start < FTP_TIMEOUT_MS) {
        while (ctrl.available() && pos < bufLen - 1) {
            char c = ctrl.read();
            buf[pos++] = c;
            buf[pos]   = '\0';
            if (c == '\n') goto done;
        }
        delay(10);
    }
done:
    int code = atoi(buf);
    return code == expectedCode;
}

bool BambuFtpClient::parsePasv(const char* resp, char* outIp, uint16_t& outPort) {
    const char* p = strchr(resp, '(');
    if (!p) return false;
    int a, b, c, d, p1, p2;
    if (sscanf(p + 1, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &p1, &p2) != 6) return false;
    snprintf(outIp, 16, "%d.%d.%d.%d", a, b, c, d);
    outPort = (uint16_t)(p1 * 256 + p2);
    return true;
}

void BambuFtpClient::parseListLine(const char* line, char* outName, size_t maxLen) {
    outName[0] = '\0';
    if (!line || line[0] == '\0') return;

    // Unix ls -l: "-rwxr-xr-x 1 user group 1234 Jan 01 filename"
    // Skip 8 whitespace-delimited tokens to reach filename
    const char* p = line;
    int spaces = 0;
    while (*p) {
        if (*p == ' ' || *p == '\t') {
            while (*p == ' ' || *p == '\t') p++;
            spaces++;
            if (spaces == 8) {
                size_t len = strlen(p);
                while (len > 0 && (p[len-1] == '\r' || p[len-1] == '\n')) len--;
                size_t copy = len < maxLen - 1 ? len : maxLen - 1;
                strncpy(outName, p, copy);
                outName[copy] = '\0';
                return;
            }
        } else {
            p++;
        }
    }
}

bool BambuFtpClient::listFiles(const char* ip, const char* accessCode,
                               char files[][FTP_FILENAME_LEN + 1], uint8_t& count) {
    count = 0;

    WiFiClientSecure ctrl;
    ctrl.setInsecure();
    ctrl.setTimeout(10);
    if (!ctrl.connect(ip, FTP_PORT)) return false;

    char buf[256];

    if (!readResponse(ctrl, 220, buf, sizeof(buf))) { ctrl.stop(); return false; }
    ctrl.printf("USER bblp\r\n");
    if (!readResponse(ctrl, 331, buf, sizeof(buf))) { ctrl.stop(); return false; }
    ctrl.printf("PASS %s\r\n", accessCode);
    if (!readResponse(ctrl, 230, buf, sizeof(buf))) { ctrl.stop(); return false; }

    ctrl.print("TYPE I\r\n");
    readResponse(ctrl, 200, buf, sizeof(buf));

    ctrl.print("PASV\r\n");
    if (!readResponse(ctrl, 227, buf, sizeof(buf))) { ctrl.stop(); return false; }

    char dataIp[16];
    uint16_t dataPort;
    if (!parsePasv(buf, dataIp, dataPort)) { ctrl.stop(); return false; }

    WiFiClientSecure data;
    data.setInsecure();
    data.setTimeout(10);
    if (!data.connect(ip, dataPort)) {
        if (!data.connect(dataIp, dataPort)) { ctrl.stop(); return false; }
    }

    ctrl.print("LIST /sdcard/\r\n");
    readResponse(ctrl, 150, buf, sizeof(buf));

    String line;
    uint32_t timeout = millis();
    while (data.connected() || data.available()) {
        while (data.available()) {
            char c = (char)data.read();
            if (c == '\n') {
                if (line.length() > 0) {
                    char fname[FTP_FILENAME_LEN + 1];
                    parseListLine(line.c_str(), fname, sizeof(fname));
                    size_t flen = strlen(fname);
                    bool isGcode = flen > 6 && strcmp(fname + flen - 6, ".gcode") == 0;
                    bool is3mf   = flen > 4 && strcmp(fname + flen - 4, ".3mf")  == 0;
                    if ((isGcode || is3mf) && count < FTP_MAX_FILES)
                        strlcpy(files[count++], fname, FTP_FILENAME_LEN + 1);
                }
                line = "";
            } else if (c != '\r') {
                line += c;
            }
            timeout = millis();
        }
        if (millis() - timeout > FTP_TIMEOUT_MS) break;
        delay(5);
    }

    data.stop();
    readResponse(ctrl, 226, buf, sizeof(buf));
    ctrl.print("QUIT\r\n");
    ctrl.stop();

    return count > 0;
}

bool BambuFtpClient::uploadFile(const char* ip, const char* accessCode,
                                const char* localSdPath, const char* remoteFilename) {
    File f = SD.open(localSdPath);
    if (!f) return false;

    WiFiClientSecure ctrl;
    ctrl.setInsecure();
    ctrl.setTimeout(10);
    if (!ctrl.connect(ip, FTP_PORT)) { f.close(); return false; }

    char buf[256];

    if (!readResponse(ctrl, 220, buf, sizeof(buf))) { ctrl.stop(); f.close(); return false; }
    ctrl.printf("USER bblp\r\n");
    if (!readResponse(ctrl, 331, buf, sizeof(buf))) { ctrl.stop(); f.close(); return false; }
    ctrl.printf("PASS %s\r\n", accessCode);
    if (!readResponse(ctrl, 230, buf, sizeof(buf))) { ctrl.stop(); f.close(); return false; }

    ctrl.print("TYPE I\r\n");
    readResponse(ctrl, 200, buf, sizeof(buf));

    ctrl.print("PASV\r\n");
    if (!readResponse(ctrl, 227, buf, sizeof(buf))) { ctrl.stop(); f.close(); return false; }

    char dataIp[16];
    uint16_t dataPort;
    if (!parsePasv(buf, dataIp, dataPort)) { ctrl.stop(); f.close(); return false; }

    WiFiClientSecure data;
    data.setInsecure();
    data.setTimeout(30); // generous timeout for upload
    if (!data.connect(ip, dataPort)) {
        if (!data.connect(dataIp, dataPort)) { ctrl.stop(); f.close(); return false; }
    }

    ctrl.printf("STOR /sdcard/%s\r\n", remoteFilename);
    if (!readResponse(ctrl, 150, buf, sizeof(buf))) {
        data.stop(); ctrl.stop(); f.close(); return false;
    }

    // Stream file in 512-byte chunks
    uint8_t chunk[512];
    while (f.available()) {
        int n = f.read(chunk, sizeof(chunk));
        if (n <= 0) break;
        data.write(chunk, (size_t)n);
    }

    f.close();
    data.stop();

    bool ok = readResponse(ctrl, 226, buf, sizeof(buf));
    ctrl.print("QUIT\r\n");
    ctrl.stop();

    return ok;
}
