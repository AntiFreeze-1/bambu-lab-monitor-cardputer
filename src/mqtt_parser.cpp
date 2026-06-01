#include "mqtt_parser.h"
#include <ArduinoJson.h>

static GcodeState parseGcodeState(const char* s) {
    if (!s) return GcodeState::UNKNOWN;
    if (strcmp(s, "IDLE")    == 0) return GcodeState::IDLE;
    if (strcmp(s, "PREPARE") == 0) return GcodeState::PREPARE;
    if (strcmp(s, "RUNNING") == 0) return GcodeState::RUNNING;
    if (strcmp(s, "PAUSE")   == 0) return GcodeState::PAUSE;
    if (strcmp(s, "FINISH")  == 0) return GcodeState::FINISH;
    if (strcmp(s, "FAILED")  == 0) return GcodeState::FAILED;
    return GcodeState::UNKNOWN;
}

static const char* hwVerToModel(const char* hwVer) {
    struct { const char* hwv; const char* model; } map[] = {
        { "AP04", "X1 Carbon" },
        { "AP05", "X1 Carbon" },
        { "AP06", "X1 Carbon" },
        { "TH03", "P1S"       },
        { "TH02", "P1P"       },
        { "SP03", "A1 Mini"   },
        { "SP04", "A1"        },
        { "SP05", "A1"        },
    };
    for (auto& e : map) {
        if (strcmp(hwVer, e.hwv) == 0) return e.model;
    }
    return nullptr;
}

void MqttParser::parse(const byte* data, unsigned int len, PrinterState& state) {
    // Build filter — use direct subscript assignment (more reliable in ArduinoJson 7)
    JsonDocument filter;
    filter["print"]["mc_percent"]           = true;
    filter["print"]["mc_remaining_time"]    = true;
    filter["print"]["layer_num"]            = true;
    filter["print"]["total_layer_num"]      = true;
    filter["print"]["nozzle_temper"]        = true;
    filter["print"]["nozzle_target_temper"] = true;
    filter["print"]["bed_temper"]           = true;
    filter["print"]["bed_target_temper"]    = true;
    filter["print"]["spd_lvl"]              = true;
    filter["print"]["gcode_state"]          = true;
    filter["print"]["gcode_file"]           = true;
    filter["print"]["subtask_name"]         = true;
    filter["print"]["ams"]                  = true;  // entire nested block
    filter["print"]["vt_tray"]              = true;
    // Version info — pass entire info block (it's small)
    filter["info"]                          = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, data, len, DeserializationOption::Filter(filter));
    if (err) return;

    // ── Print status ─────────────────────────────────────────────────────────
    JsonObject print = doc["print"];
    if (!print.isNull()) {
        if (!print["mc_percent"].isNull())
            state.progressPct  = print["mc_percent"].as<uint8_t>();
        if (!print["mc_remaining_time"].isNull())
            state.remainingMin = print["mc_remaining_time"].as<uint16_t>();
        if (!print["layer_num"].isNull())
            state.layerCurrent = print["layer_num"].as<uint16_t>();
        if (!print["total_layer_num"].isNull())
            state.layerTotal   = print["total_layer_num"].as<uint16_t>();
        if (!print["nozzle_temper"].isNull())
            state.nozzleTemp   = print["nozzle_temper"].as<float>();
        if (!print["nozzle_target_temper"].isNull())
            state.nozzleTarget = print["nozzle_target_temper"].as<float>();
        if (!print["bed_temper"].isNull())
            state.bedTemp      = print["bed_temper"].as<float>();
        if (!print["bed_target_temper"].isNull())
            state.bedTarget    = print["bed_target_temper"].as<float>();
        if (!print["spd_lvl"].isNull())
            state.speedLevel   = static_cast<SpeedLevel>(print["spd_lvl"].as<uint8_t>());
        if (!print["gcode_state"].isNull())
            state.gcodeState   = parseGcodeState(print["gcode_state"]);

        // subtask_name is more human-readable than gcode_file path
        const char* subtask = print["subtask_name"] | "";
        if (subtask[0] != '\0') {
            strlcpy(state.currentFile, subtask, sizeof(state.currentFile));
        } else {
            const char* gcodeFile = print["gcode_file"] | "";
            if (gcodeFile[0] != '\0') {
                const char* slash = strrchr(gcodeFile, '/');
                strlcpy(state.currentFile, slash ? slash + 1 : gcodeFile,
                        sizeof(state.currentFile));
            }
        }

        // Add current file to file list cache
        if (state.currentFile[0] != '\0') {
            bool found = false;
            for (int i = 0; i < state.fileCount; i++) {
                if (strcmp(state.fileList[i], state.currentFile) == 0) { found = true; break; }
            }
            if (!found && state.fileCount < FTP_MAX_FILES)
                strlcpy(state.fileList[state.fileCount++], state.currentFile, FTP_FILENAME_LEN + 1);
        }

        // ── AMS ──────────────────────────────────────────────────────────────
        JsonObject amsTop = print["ams"];
        if (!amsTop.isNull()) {
            JsonArray amsArr = amsTop["ams"].as<JsonArray>();
            if (!amsArr.isNull()) {
                uint8_t unitIdx = 0;
                for (JsonObject unit : amsArr) {
                    if (unitIdx >= 4) break;
                    AmsUnit& u = state.amsUnits[unitIdx++];
                    u.present  = true;
                    for (auto& t : u.trays) t.valid = false;
                    uint8_t ti = 0;
                    JsonArray trayArr = unit["tray"].as<JsonArray>();
                    if (!trayArr.isNull()) {
                        for (JsonObject tray : trayArr) {
                            if (ti >= 4) break;
                            AmsTray& t = u.trays[ti++];
                            t.valid    = true;
                            t.remain   = tray["remain"].as<uint8_t>();
                            const char* tname = tray["tray_id_name"] | "";
                            const char* ttype = tray["tray_type"] | "";
                            strlcpy(t.type, tname[0] ? tname : ttype, sizeof(t.type));
                            const char* hex = tray["tray_color"] | "FFFFFFFF";
                            t.color = (uint32_t)strtoul(hex, nullptr, 16) >> 8;
                        }
                    }
                }
                if (unitIdx > state.amsUnitCount) state.amsUnitCount = unitIdx;
                if (state.amsUnitCount > 0) state.hasAms = true;
            }
        }

        // ── External spool ───────────────────────────────────────────────────
        if (!print["vt_tray"].isNull()) state.hasAmsLite = true;
    }

    // ── Version info (get_version response) ──────────────────────────────────
    JsonObject infoObj = doc["info"];
    if (!infoObj.isNull()) {
        JsonArray modules = infoObj["module"].as<JsonArray>();
        for (JsonObject mod : modules) {
            const char* modName = mod["name"] | "";
            if (strcmp(modName, "ota") == 0) {
                const char* hwVer = mod["hw_ver"] | "";
                if (hwVer[0] != '\0') {
                    const char* mapped = hwVerToModel(hwVer);
                    if (mapped) {
                        strlcpy(state.deviceModel, mapped, sizeof(state.deviceModel));
                    } else if (state.deviceModel[0] == '\0' ||
                               strcmp(state.deviceModel, "Bambu Printer") == 0) {
                        strlcpy(state.deviceModel, hwVer, sizeof(state.deviceModel));
                    }
                }
                break;
            }
        }
    }
}
