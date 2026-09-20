#include "Platform/N3DS/ContentRenderer.h"

namespace Platform::N3DS {
namespace {

constexpr u32 kBackgroundColor = 0xFF2B2420;  // ABGR: matches citro2d's byte order
constexpr u32 kTextColor = 0xFFD6D6D6;
constexpr u32 kHeadingColor = 0xFFFFFFFF;
constexpr u32 kLinkColor = 0xFFFEA86E;
constexpr u32 kErrorColor = 0xFF6E6EFE;

u32 ColorForWord(Core::RunKind kind) {
    switch (kind) {
        case Core::RunKind::Heading: return kHeadingColor;
        case Core::RunKind::Link: return kLinkColor;
        default: return kTextColor;
    }
}

}  // namespace

ContentRenderer::ContentRenderer() {
    buf_ = C2D_TextBufNew(256);
    messageBuf_ = C2D_TextBufNew(256);
}

ContentRenderer::~ContentRenderer() {
    C2D_TextBufDelete(messageBuf_);
    C2D_TextBufDelete(buf_);
}

void ContentRenderer::SetLayout(const Core::LayoutResult &layout) {
    words_.clear();

    size_t totalChars = 1;
    for (const Core::LaidWord &w : layout.words) {
        totalChars += w.text.size();
    }
    buf_ = C2D_TextBufResize(buf_, totalChars + layout.words.size() + 16);

    words_.reserve(layout.words.size());
    for (const Core::LaidWord &w : layout.words) {
        PreparedWord prepared;
        prepared.x = w.x;
        prepared.y = w.y;
        prepared.height = w.height;
        prepared.scale = w.scale;
        prepared.kind = w.kind;
        prepared.bold = w.bold;
        C2D_TextParse(&prepared.text, buf_, w.text.c_str());
        C2D_TextOptimize(&prepared.text);
        words_.push_back(prepared);
    }
}

void ContentRenderer::Draw(C3D_RenderTarget *target, float scrollY, float viewportWidth,
                            float viewportHeight) {
    (void)viewportWidth;
    C2D_TargetClear(target, kBackgroundColor);
    C2D_SceneBegin(target);

    for (const PreparedWord &word : words_) {
        float y = word.y - scrollY;
        if (y + word.height < 0.0f || y > viewportHeight) {
            continue;
        }

        u32 color = ColorForWord(word.kind);
        C2D_DrawText(&word.text, C2D_WithColor, word.x, y, 0.4f, word.scale, word.scale, color);
        if (word.bold) {
            // No bold system-font variant is available; fake it by
            // overdrawing one pixel to the right.
            C2D_DrawText(&word.text, C2D_WithColor, word.x + 1.0f, y, 0.4f, word.scale,
                         word.scale, color);
        }
    }
}

void ContentRenderer::DrawMessage(C3D_RenderTarget *target, const std::string &message,
                                    float viewportWidth, float viewportHeight) {
    C2D_TargetClear(target, kBackgroundColor);
    C2D_SceneBegin(target);

    C2D_TextBufClear(messageBuf_);
    C2D_Text text;
    C2D_TextParse(&text, messageBuf_, message.c_str());
    C2D_TextOptimize(&text);

    float width = 0.0f, height = 0.0f;
    C2D_TextGetDimensions(&text, 0.6f, 0.6f, &width, &height);
    float x = (viewportWidth - width) * 0.5f;
    float y = (viewportHeight - height) * 0.5f;
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.4f, 0.6f, 0.6f, kErrorColor);
}

}  // namespace Platform::N3DS
