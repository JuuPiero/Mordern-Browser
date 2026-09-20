#include "Platform/N3DS/C2DTextMeasurer.h"

#include <citro2d.h>

namespace Platform::N3DS {
namespace {

// Per the citro2d header docs: "The default 3DS system font has a glyph
// height of 30px, and the baseline is at 25px." 1.2x gives a bit of
// breathing room between lines, matching typical proportional-font leading.
constexpr float kBaseGlyphHeight = 30.0f;
constexpr float kLineSpacing = 1.2f;

}  // namespace

C2DTextMeasurer::C2DTextMeasurer() { scratchBuf_ = C2D_TextBufNew(256); }

C2DTextMeasurer::~C2DTextMeasurer() { C2D_TextBufDelete(scratchBuf_); }

float C2DTextMeasurer::MeasureString(const char *str, float scale) {
    C2D_TextBufClear(scratchBuf_);
    C2D_Text text;
    C2D_TextParse(&text, scratchBuf_, str);
    float width = 0.0f;
    C2D_TextGetDimensions(&text, scale, scale, &width, nullptr);
    return width;
}

float C2DTextMeasurer::MeasureWord(const std::string &word, const Core::TextStyle &style) {
    if (word.empty()) {
        return 0.0f;
    }
    return MeasureString(word.c_str(), style.scale);
}

float C2DTextMeasurer::SpaceWidth(const Core::TextStyle &style) {
    // citro2d doesn't allocate a glyph for whitespace, so a lone " " can
    // measure as 0 width. Derive the space's advance from the difference
    // between "x x" and two copies of "x" instead of trusting it directly.
    float pairWidth = MeasureString("x x", style.scale);
    float singleWidth = MeasureString("x", style.scale);
    float space = pairWidth - 2.0f * singleWidth;
    return space > 0.5f ? space : style.scale * 6.0f;
}

float C2DTextMeasurer::LineHeight(const Core::TextStyle &style) {
    return kBaseGlyphHeight * style.scale * kLineSpacing;
}

}  // namespace Platform::N3DS
