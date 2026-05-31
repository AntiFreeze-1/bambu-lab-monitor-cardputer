#pragma once
#include <M5Cardputer.h>
#include "../types.h"

namespace ScreenFiles {
    void onEnter(PrinterState& state);
    void draw(LGFX_Sprite& s, const PrinterState& state);
    void handleKey(char c, bool isFn, bool enter, bool del);
    const char* hintText();
}
