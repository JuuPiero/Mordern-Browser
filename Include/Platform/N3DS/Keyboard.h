#pragma once

#include <string>

namespace Platform::N3DS {

struct KeyboardResult {
    bool confirmed = false;
    std::string text;
};

// Blocks and shows the system software keyboard applet (this darkens/pauses
// the rest of the app, which is how every 3DS text-entry UI works).
KeyboardResult ShowKeyboard(const std::string &hintText, const std::string &initialText);

}  // namespace Platform::N3DS
