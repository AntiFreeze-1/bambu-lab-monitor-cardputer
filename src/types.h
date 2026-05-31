#pragma once
#include <Arduino.h>
#include "config.h"

// ── AMS ──────────────────────────────────────────────────────────────────────

struct AmsTray {
    uint32_t color;        // RGB24 (alpha stripped from Bambu's RRGGBBAA)
    uint8_t  remain;       // 0–100 fill %
    char     type[12];     // "PLA", "PETG", etc.
    bool     valid;
};

struct AmsUnit {
    AmsTray trays[4];
    bool    present;
};

// ── Print state ───────────────────────────────────────────────────────────────

enum class GcodeState : uint8_t {
    UNKNOWN, IDLE, PREPARE, RUNNING, PAUSE, FINISH, FAILED
};

enum class SpeedLevel : uint8_t {
    QUIET = 1, NORMAL = 2, SPORT = 3, LUDICROUS = 4
};

// ── Per-printer state (populated from MQTT) ───────────────────────────────────

struct PrinterState {
    // identity (filled from Settings on boot)
    char name[32];
    char ip[16];
    char serial[20];
    char accessCode[16];
    bool hasAms;
    bool hasAmsLite;

    // connection
    bool     mqttConnected;
    uint32_t lastMessageMs;

    // progress
    uint8_t  progressPct;
    uint16_t remainingMin;
    uint16_t layerCurrent;
    uint16_t layerTotal;
    GcodeState gcodeState;
    SpeedLevel speedLevel;

    // temperatures
    float nozzleTemp;
    float nozzleTarget;
    float bedTemp;
    float bedTarget;

    // current file (cached from MQTT gcode_file)
    char currentFile[FTP_FILENAME_LEN + 1];

    // AMS
    AmsUnit amsUnits[4];
    uint8_t amsUnitCount;

    // file list (FTP + MQTT cache)
    char    fileList[FTP_MAX_FILES][FTP_FILENAME_LEN + 1];
    uint8_t fileCount;
    bool    fileListFresh;   // true = populated via FTP this session
};

// ── UI ────────────────────────────────────────────────────────────────────────

enum class Screen : uint8_t {
    STATUS         = 0,
    AMS            = 1,
    SPEED          = 2,
    FILES          = 3,
    PRINTER_SELECT = 4,
    SETTINGS       = 5,
    COUNT          = 6
};
