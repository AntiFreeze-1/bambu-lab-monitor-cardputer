#include "ftp_client.h"
#include <WiFiClientSecure.h>
#include <string.h>

// Read FTP response line, return true if code matches
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
    // Format: "227 Entering Passive Mode (a,b,c,d,p1,p2)."
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

    // Unix ls -l format: "-rwxr-xr-x 1 user group 1234 Jan 01 filename"
    // Skip 8 space-delimited fields to get filename
    const char* p = line;
    int spaces = 0;
    while (*p) {
        if (*p == ' ' || *p == '\t') {
            while (*p == ' ' || *p == '\t') p++;
            spaces++;
            if (spaces == 8) {
                // rest of line is filename (strip \r\n)
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

    // Welcome banner
    if (!readResponse(ctrl, 220, buf, sizeof(buf))) { ctrl.stop(); return false; }

    // Authenticate
    ctrl.printf("USER bblp\r\n");
    if (!readResponse(ctrl, 331, buf, sizeof(buf))) { ctrl.stop(); return false; }
    ctrl.printf("PASS %s\r\n", accessCode);
    if (!readResponse(ctrl, 230, buf, sizeof(buf))) { ctrl.stop(); return false; }

    // Binary mode
    ctrl.print("TYPE I\r\n");
    readResponse(ctrl, 200, buf, sizeof(buf));

    // Passive mode
    ctrl.print("PASV\r\n");
    if (!readResponse(ctrl, 227, buf, sizeof(buf))) { ctrl.stop(); return false; }

    char dataIp[16];
    uint16_t dataPort;
    if (!parsePasv(buf, dataIp, dataPort)) { ctrl.stop(); return false; }

    // Open data channel — use printer IP directly (avoids NAT issues with PASV IP)
    WiFiClientSecure data;
    data.setInsecure();
    data.setTimeout(10);
    if (!data.connect(ip, dataPort)) {
        // Fallback: try the IP from PASV
        if (!data.connect(dataIp, dataPort)) { ctrl.stop(); return false; }
    }

    // Request directory listing
    ctrl.print("LIST /sdcard/\r\n");
    readResponse(ctrl, 150, buf, sizeof(buf)); // "150 Opening data connection"

    // Read listing data
    String line;
    uint32_t timeout = millis();
    while (data.connected() || data.available()) {
        while (data.available()) {
            char c = (char)data.read();
            if (c == '\n') {
                if (line.length() > 0) {
                    char fname[FTP_FILENAME_LEN + 1];
                    parseListLine(line.c_str(), fname, sizeof(fname));

                    // Accept .gcode and .3mf files
                    size_t flen = strlen(fname);
                    bool isGcode = flen > 6 && strcmp(fname + flen - 6, ".gcode") == 0;
                    bool is3mf   = flen > 4 && strcmp(fname + flen - 4, ".3mf")  == 0;

                    if ((isGcode || is3mf) && count < FTP_MAX_FILES) {
                        strlcpy(files[count++], fname, FTP_FILENAME_LEN + 1);
                    }
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
    readResponse(ctrl, 226, buf, sizeof(buf)); // "226 Transfer complete"
    ctrl.print("QUIT\r\n");
    ctrl.stop();

    return count > 0;
}
