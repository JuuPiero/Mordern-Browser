#pragma once

#include "Core/ITextMeasurer.h"

typedef struct C2D_TextBuf_s *C2D_TextBuf;
typedef struct C2D_Font_s *C2D_Font;

namespace Platform::N3DS {

// citro2d-backed implementation of Core::ITextMeasurer. Layout only ever
// asks this for widths/heights, never draws with it directly
// (ContentRenderer/Chrome own their own text buffers for that), so this can
// stay a small, single-purpose scratchpad. Must be measured with the same
// C2D_Font the renderer draws with, or wrapping widths won't match glyphs.
class C2DTextMeasurer : public Core::ITextMeasurer {
public:
    // `font` may be nullptr to fall back to the built-in system font.
    explicit C2DTextMeasurer(C2D_Font font);
    ~C2DTextMeasurer() override;

    float MeasureWord(const std::string &word, const Core::TextStyle &style) override;
    float SpaceWidth(const Core::TextStyle &style) override;
    float LineHeight(const Core::TextStyle &style) override;

private:
    float MeasureString(const char *str, float scale);

    C2D_Font font_;
    C2D_TextBuf scratchBuf_;
};

}  // namespace Platform::N3DS
