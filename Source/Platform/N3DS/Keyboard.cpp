#include "Platform/N3DS/Keyboard.h"

#include <3ds.h>

namespace Platform::N3DS {

KeyboardResult ShowKeyboard(const std::string &hintText, const std::string &initialText) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, 512);
    swkbdSetHintText(&swkbd, hintText.c_str());
    if (!initialText.empty()) {
        swkbdSetInitialText(&swkbd, initialText.c_str());
    }
    swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);

    char buffer[512];
    SwkbdButton button = swkbdInputText(&swkbd, buffer, sizeof(buffer));

    KeyboardResult result;
    result.confirmed = (button == SWKBD_BUTTON_CONFIRM);
    if (result.confirmed) {
        result.text.assign(buffer);
    }
    return result;
}

}  // namespace Platform::N3DS
