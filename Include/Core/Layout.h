#pragma once

#include <string>
#include <vector>

#include "Core/HtmlDocument.h"
#include "Core/ITextMeasurer.h"

namespace Core {

enum class RunKind {
    Text,
    Heading,
    Link,
    ListMarker,
};

// A single word, already positioned. Layout does all the wrapping work up
// front so the renderer's per-frame job is just "draw the words whose y is
// currently visible" plus a hit-test for taps.
struct LaidWord {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float scale = 0.5f;
    bool bold = false;
    RunKind kind = RunKind::Text;
    std::string href;  // non-empty when kind == Link
};

struct LayoutResult {
    std::vector<LaidWord> words;
    float contentHeight = 0.0f;
};

// Walks `document`'s body and produces a vertical flow of wrapped text at
// `contentWidth` px, measuring words with `measurer`. This is intentionally a
// text-flow layout, not a CSS box model: block tags (p, div, h1-h6, li, ...)
// start a new line, inline tags (a, b, strong, em, span, ...) carry style
// into the surrounding paragraph's word stream, and everything else (canvas
// position, floats, tables-as-grids, images) is out of scope for now.
LayoutResult BuildLayout(const HtmlDocument &document, float contentWidth,
                          ITextMeasurer &measurer);

// Returns the href of the link word under content-space point (x, y) (i.e.
// already offset by scroll), or an empty string if there isn't one there.
std::string HitTestLink(const LayoutResult &layout, float x, float y);

}  // namespace Core
