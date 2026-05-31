#pragma once
#include <Arduino.h>
#include "types.h"

// Convert RGB24 → RGB565 for LovyanGFX
inline uint16_t rgb24to565(uint32_t rgb24) {
    uint8_t r = (rgb24 >> 16) & 0xFF;
    uint8_t g = (rgb24 >>  8) & 0xFF;
    uint8_t b =  rgb24        & 0xFF;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

// "1h23m" or "45m" or "< 1m"
void formatEta(uint16_t minutes, char* buf, size_t len);

// "220°/220°"
void formatTemp(float actual, float target, char* buf, size_t len);

// Truncate filename with "…" suffix at maxChars
void truncateFilename(const char* src, char* dst, uint8_t maxChars);

// Human-readable speed label
const char* speedLevelName(SpeedLevel lvl);

// Percentage string for speed level
const char* speedLevelPct(SpeedLevel lvl);

// Human-readable gcode state
const char* gcodeStateName(GcodeState s);

// Strip directory path, return filename portion only
const char* filenameOnly(const char* path);
