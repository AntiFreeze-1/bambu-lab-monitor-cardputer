#include "utils.h"
#include <stdio.h>
#include <string.h>

void formatEta(uint16_t minutes, char* buf, size_t len) {
    if (minutes == 0) {
        snprintf(buf, len, "< 1m");
    } else if (minutes < 60) {
        snprintf(buf, len, "%um", minutes);
    } else {
        snprintf(buf, len, "%uh%02um", minutes / 60, minutes % 60);
    }
}

void formatTemp(float actual, float target, char* buf, size_t len) {
    snprintf(buf, len, "%.0f\xB0/%.0f\xB0", actual, target);
}

void truncateFilename(const char* src, char* dst, uint8_t maxChars) {
    size_t slen = strlen(src);
    if (slen <= maxChars) {
        strlcpy(dst, src, maxChars + 1);
    } else {
        strncpy(dst, src, maxChars - 3);
        dst[maxChars - 3] = '.';
        dst[maxChars - 2] = '.';
        dst[maxChars - 1] = '.';
        dst[maxChars]     = '\0';
    }
}

const char* speedLevelName(SpeedLevel lvl) {
    switch (lvl) {
        case SpeedLevel::QUIET:     return "Quiet";
        case SpeedLevel::NORMAL:    return "Normal";
        case SpeedLevel::SPORT:     return "Sport";
        case SpeedLevel::LUDICROUS: return "Ludicrous";
        default:                    return "?";
    }
}

const char* speedLevelPct(SpeedLevel lvl) {
    switch (lvl) {
        case SpeedLevel::QUIET:     return "50%";
        case SpeedLevel::NORMAL:    return "100%";
        case SpeedLevel::SPORT:     return "124%";
        case SpeedLevel::LUDICROUS: return "166%";
        default:                    return "?";
    }
}

const char* gcodeStateName(GcodeState s) {
    switch (s) {
        case GcodeState::IDLE:    return "IDLE";
        case GcodeState::PREPARE: return "PREPARE";
        case GcodeState::RUNNING: return "RUNNING";
        case GcodeState::PAUSE:   return "PAUSED";
        case GcodeState::FINISH:  return "DONE";
        case GcodeState::FAILED:  return "FAILED";
        default:                  return "UNKNOWN";
    }
}

const char* filenameOnly(const char* path) {
    const char* last = strrchr(path, '/');
    return last ? last + 1 : path;
}
