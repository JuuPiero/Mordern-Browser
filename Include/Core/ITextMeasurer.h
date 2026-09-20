#pragma once

#include <string>

namespace Core {

// A visual style applied to a run of laid-out text. Kept intentionally small
// (no font selection, no CSS) since Layout only needs enough information to
// wrap text and to tell the renderer how to draw each word.
struct TextStyle {
    float scale = 0.5f;
    bool bold = false;
};

// Layout needs to know how wide a word is and how tall a line is in order to
// wrap text, but actual glyph metrics come from the platform's text/font
// system (citro2d on the 3DS). This interface lets Core/Layout.cpp stay free
// of any rendering-backend headers; Platform/N3DS provides the real
// implementation on top of citro2d.
class ITextMeasurer {
public:
    virtual ~ITextMeasurer() = default;

    virtual float MeasureWord(const std::string &word, const TextStyle &style) = 0;
    virtual float SpaceWidth(const TextStyle &style) = 0;
    virtual float LineHeight(const TextStyle &style) = 0;
};

}  // namespace Core
