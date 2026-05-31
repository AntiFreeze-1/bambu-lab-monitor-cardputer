#pragma once
#include "types.h"

namespace MqttParser {
    // Parse a Bambu MQTT report payload into state.
    // Uses ArduinoJson filtered parsing to minimize RAM usage.
    void parse(const byte* data, unsigned int len, PrinterState& state);
}
