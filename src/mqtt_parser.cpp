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
    JsonDocument filter;

    // Print status fields
    JsonObject fp = filter["print"].to<JsonObject>();
    fp["mc_percent"]            = true;
    fp["mc_remaining_time"]     = true;
    fp["layer_num"]             = true;
    fp["total_layer_num"]       = true;
    fp["nozzle_temper"]         = true;
    fp["nozzle_target_temper"]  = true;
    fp["bed_temper"]            = true;
    fp["bed_target_temper"]     = true;
    fp["spd_lvl"]               = true;
    fp["gcode_state"]           = true;
    fp["gcode_file"]            = true;
    fp["subtask_name"]          = true;
    fp["stg_cur"]               = true;
    fp["ams"]                   = true;
    fp["vt_tray"]               = true;

    // Version info fields
    filter["info"]["module"][0]["name"]   = true;
    filter["info"]["module"][0]["hw_ver"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, data, len, DeserializationOption::Filter(filter));
    if (err) return;

    // ── Parse print status ────────────────────────────────────────────────────
    JsonObject print = doc["print"];
    if (!print.isNull()) {
        if (!print["mc_percent"].isNull())
            state.progressPct   = print["mc_percent"].as<uint8_t>();
        if (!print["mc_remaining_time"].isNull())
            state.remainingMin  = print["mc_remaining_time"].as<uint16_t>();
        if (!print["layer_num"].isNull())
            state.layerCurrent  = print["layer_num"].as<uint16_t>();
        if (!print["total_layer_num"].isNull())
            state.layerTotal    = print["total_layer_num"].as<uint16_t>();
        if (!print["nozzle_temper"].isNull())
            state.nozzleTemp    = print["nozzle_temper"].as<float>();
        if (!print["nozzle_target_temper"].isNull())
            state.nozzleTarget  = print["nozzle_target_temper"].as<float>();
        if (!print["bed_temper"].isNull())
            state.bedTemp       = print["bed_temper"].as<float>();
        if (!print["bed_target_temper"].isNull())
            state.bedTarget     = print["bed_target_temper"].as<float>();
        if (!print["spd_lvl"].isNull())
            state.speedLevel    = static_cast<SpeedLevel>(print["spd_lvl"].as<uint8_t>());
        if (!print["gcode_state"].isNull())
            state.gcodeState    = parseGcodeState(print["gcode_state"]);

        // subtask_name is the human-readable filename; prefer it over gcode_file
        const char* subtaskName = print["subtask_name"];
        if (subtaskName && subtaskName[0] != '\0') {
            strlcpy(state.currentFile, subtaskName, sizeof(state.currentFile));
        } else {
            const char* gcodeFile = print["gcode_file"];
            if (gcodeFile && gcodeFile[0] != '\0') {
                const char* slash = strrchr(gcodeFile, '/');
                strlcpy(state.currentFile, slash ? slash + 1 : gcodeFile,
                        sizeof(state.currentFile));
            }
        }

        // Add current file to file list cache if not already there
        if (state.currentFile[0] != '\0') {
            bool found = false;
            for (int i = 0; i < state.fileCount; i++) {
                if (strcmp(state.fileList[i], state.currentFile) == 0) {
                    found = true; break;
                }
            }
            if (!found && state.fileCount < FTP_MAX_FILES)
                strlcpy(state.fileList[state.fileCount++], state.currentFile,
                        FTP_FILENAME_LEN + 1);
        }

        // ── AMS ──────────────────────────────────────────────────────────────
        JsonObject amsObj = print["ams"];
        if (!amsObj.isNull()) {
            JsonArray amsArr = amsObj["ams"].as<JsonArray>();
            if (!amsArr.isNull()) {
                state.amsUnitCount = 0;
                for (JsonObject unit : amsArr) {
                    if (state.amsUnitCount >= 4) break;
                    AmsUnit& u  = state.amsUnits[state.amsUnitCount++];
                    u.present   = true;
                    for (auto& t : u.trays) t.valid = false;
                    uint8_t ti = 0;
                    JsonArray trayArr = unit["tray"].as<JsonArray>();
                    if (!trayArr.isNull()) {
                        for (JsonObject tray : trayArr) {
                            if (ti >= 4) break;
                            AmsTray& t = u.trays[ti++];
                            t.valid    = true;
                            t.remain   = tray["remain"].as<uint8_t>();
                            const char* trayType = tray["tray_type"] | "";
                            const char* trayName = tray["tray_id_name"];
                            strlcpy(t.type,
                                    (trayName && trayName[0]) ? trayName : trayType,
                                    sizeof(t.type));
                            const char* hex = tray["tray_color"] | "FFFFFFFF";
                            t.color = (uint32_t)strtoul(hex, nullptr, 16) >> 8;
                        }
                    }
                }
                if (state.amsUnitCount > 0) state.hasAms = true;
            }
        }

        // ── External spool (vt_tray) ─────────────────────────────────────────
        JsonObject vtObj = print["vt_tray"];
        if (!vtObj.isNull()) {
            state.hasAmsLite = true;
        }
    }

    // ── Parse get_version response ───────────────────────────────────────────
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
