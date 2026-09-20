#pragma once

#include <string>
#include <vector>

#include <citro2d.h>

#include "Core/Layout.h"

namespace Platform::N3DS {

// Draws a page's laid-out words to a render target (the top screen, in
// practice), offset by the tab's current scroll position. Owns its own
// citro2d text buffer, separate from C2DTextMeasurer's scratch buffer.
class ContentRenderer {
public:
    ContentRenderer();
    ~ContentRenderer();

    ContentRenderer(const ContentRenderer &) = delete;
    ContentRenderer &operator=(const ContentRenderer &) = delete;

    // Re-parses `layout`'s words for drawing. Call whenever the visible
    // tab's page changes (navigation, reload, or switching tabs).
    void SetLayout(const Core::LayoutResult &layout);

    void Draw(C3D_RenderTarget *target, float scrollY, float viewportWidth,
              float viewportHeight);

    // Centered status text, used for loading/error states instead of a page.
    void DrawMessage(C3D_RenderTarget *target, const std::string &message,
                      float viewportWidth, float viewportHeight);

private:
    struct PreparedWord {
        C2D_Text text;
        float x;
        float y;
        float height;
        float scale;
        Core::RunKind kind;
        bool bold;
    };

    C2D_TextBuf buf_;
    C2D_TextBuf messageBuf_;
    std::vector<PreparedWord> words_;
};

}  // namespace Platform::N3DS
