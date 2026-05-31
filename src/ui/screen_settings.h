#pragma once
#include <M5Cardputer.h>
#include "../types.h"

namespace ScreenSettings {
    void onEnter();
    void draw(LGFX_Sprite& s);
    void handleKey(char c, bool isFn, bool enter, bool del, bool tab, bool backspace);
    const char* hintText();
}
