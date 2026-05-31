#include "mqtt_parser.h"
#include <ArduinoJson.h>

static GcodeState parseGcodeState(const char* s) {
    if (!s)                       return GcodeState::UNKNOWN;
    if (strcmp(s, "IDLE")    == 0) return GcodeState::IDLE;
    if (strcmp(s, "PREPARE") == 0) return GcodeState::PREPARE;
    if (strcmp(s, "RUNNING") == 0) return GcodeState::RUNNING;
    if (strcmp(s, "PAUSE")   == 0) return GcodeState::PAUSE;
    if (strcmp(s, "FINISH")  == 0) return GcodeState::FINISH;
    if (strcmp(s, "FAILED")  == 0) return GcodeState::FAILED;
    return GcodeState::UNKNOWN;
}

void MqttParser::parse(const byte* data, unsigned int len, PrinterState& state) {
    // Filter keeps only the fields we need, dramatically reducing ArduinoJson memory
    JsonDocument filter;
    JsonObject fp = filter["print"].to<JsonObject>();
    fp["mc_percent"]           = true;
    fp["mc_remaining_time"]    = true;
    fp["layer_num"]            = true;
    fp["total_layer_num"]      = true;
    fp["nozzle_temper"]        = true;
    fp["nozzle_target_temper"] = true;
    fp["bed_temper"]           = true;
    fp["bed_target_temper"]    = true;
    fp["spd_lvl"]              = true;
    fp["gcode_state"]          = true;
    fp["gcode_file"]           = true;
    fp["ams"]                  = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, data, len, DeserializationOption::Filter(filter));
    if (err) return;

    JsonObject print = doc["print"];
    if (print.isNull()) return;

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

    const char* gcodeFile = print["gcode_file"];
    if (gcodeFile && gcodeFile[0] != '\0') {
        // Store just the filename, not the full path
        const char* slash = strrchr(gcodeFile, '/');
        strlcpy(state.currentFile, slash ? slash + 1 : gcodeFile, sizeof(state.currentFile));

        // Append to file list if not already present
        bool found = false;
        for (int i = 0; i < state.fileCount; i++) {
            if (strcmp(state.fileList[i], state.currentFile) == 0) {
                found = true;
                break;
            }
        }
        if (!found && state.fileCount < FTP_MAX_FILES) {
            strlcpy(state.fileList[state.fileCount++], state.currentFile, FTP_FILENAME_LEN + 1);
        }
    }

    // AMS
    JsonObject amsObj = print["ams"];
    if (!amsObj.isNull()) {
        JsonArray amsArr = amsObj["ams"].as<JsonArray>();
        if (!amsArr.isNull()) {
            state.amsUnitCount = 0;
            for (JsonObject unit : amsArr) {
                if (state.amsUnitCount >= 4) break;
                AmsUnit& u   = state.amsUnits[state.amsUnitCount++];
                u.present    = true;
                uint8_t ti   = 0;
                JsonArray trays = unit["tray"].as<JsonArray>();
                // reset all trays first
                for (auto& t : u.trays) t.valid = false;
                for (JsonObject tray : trays) {
                    if (ti >= 4) break;
                    AmsTray& t = u.trays[ti++];
                    t.valid    = true;
                    t.remain   = tray["remain"].as<uint8_t>();
                    strlcpy(t.type, tray["tray_type"] | "", sizeof(t.type));
                    const char* hex = tray["tray_color"] | "FFFFFFFF";
                    // RRGGBBAA → drop alpha byte
                    t.color = (uint32_t)strtoul(hex, nullptr, 16) >> 8;
                }
            }
        }
    }
}
