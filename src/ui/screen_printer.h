#pragma once
#include <M5Cardputer.h>
#include "../types.h"

namespace ScreenPrinter {
    void draw(LGFX_Sprite& s);
    void handleKey(char c, bool isFn, bool enter, bool del);
    const char* hintText();
}
