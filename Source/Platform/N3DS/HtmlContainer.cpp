#include "Platform/N3DS/HtmlContainer.h"

#include <3ds.h>
#include <cctype>
#include <cstring>

#include "Core/Url.h"

namespace Platform::N3DS {
namespace {

constexpr float kDefaultFontSizePx = 16.0f;
constexpr int kFallbackFontHeight = 30;  // used only if font_ is null (system font)
constexpr int kFallbackFontAscent = 25;

u32 ToC2DColor(const litehtml::web_color &c) { return C2D_Color32(c.red, c.green, c.blue, c.alpha); }

// litehtml gives us pixel-perfect boxes; citro2d rectangles want non-negative
// sizes, so clamp defensively rather than let a rounding quirk assert/crash.
void DrawRect(float x, float y, float w, float h, u32 color) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    C2D_DrawRectSolid(x, y, 0.35f, w, h, color);
}

}  // namespace

HtmlContainer::HtmlContainer(C2D_Font font, Core::INetworkClient &network)
    : font_(font), network_(network) {
    textBuf_ = C2D_TextBufNew(2048);
}

HtmlContainer::~HtmlContainer() { C2D_TextBufDelete(textBuf_); }

std::string HtmlContainer::TakeClickedHref() {
    std::string href = std::move(clickedHref_);
    clickedHref_.clear();
    return href;
}

std::string HtmlContainer::TakePageTitle() {
    std::string title = std::move(pendingTitle_);
    pendingTitle_.clear();
    return title;
}

litehtml::uint_ptr HtmlContainer::create_font(const litehtml::font_description &descr,
                                                const litehtml::document * /*doc*/,
                                                litehtml::font_metrics *fm) {
    auto *handle = new FontHandle();
    handle->bold = descr.weight >= 600;

    int nativeHeight = kFallbackFontHeight;
    int nativeAscent = kFallbackFontAscent;
    if (font_ != nullptr) {
        FINF_s *info = C2D_FontGetInfo(font_);
        if (info != nullptr && info->height > 0) {
            nativeHeight = info->height;
            nativeAscent = info->ascent;
        }
    }

    float requestedPx = static_cast<float>(descr.size);
    handle->scale = requestedPx / static_cast<float>(nativeHeight);
    handle->lineFeed = nativeHeight;
    handle->ascent = nativeAscent;

    if (fm != nullptr) {
        fm->font_size = requestedPx;
        fm->height = nativeHeight * handle->scale;
        fm->ascent = nativeAscent * handle->scale;
        fm->descent = fm->height - fm->ascent;
        fm->x_height = fm->ascent * 0.5f;
        fm->ch_width = fm->ascent;
        fm->draw_spaces = false;
    }

    return reinterpret_cast<litehtml::uint_ptr>(handle);
}

void HtmlContainer::delete_font(litehtml::uint_ptr hFont) { delete reinterpret_cast<FontHandle *>(hFont); }

litehtml::pixel_t HtmlContainer::text_width(const char *text, litehtml::uint_ptr hFont) {
    auto *handle = reinterpret_cast<FontHandle *>(hFont);
    if (text == nullptr || *text == '\0') {
        return 0;
    }
    C2D_TextBufClear(textBuf_);
    C2D_Text parsed;
    C2D_TextFontParse(&parsed, font_, textBuf_, text);
    float width = 0.0f;
    C2D_TextGetDimensions(&parsed, handle->scale, handle->scale, &width, nullptr);
    return width;
}

void HtmlContainer::draw_text(litehtml::uint_ptr /*hdc*/, const char *text, litehtml::uint_ptr hFont,
                                litehtml::web_color color, const litehtml::position &pos) {
    if (text == nullptr || *text == '\0') {
        return;
    }
    auto *handle = reinterpret_cast<FontHandle *>(hFont);
    C2D_TextBufClear(textBuf_);
    C2D_Text parsed;
    C2D_TextFontParse(&parsed, font_, textBuf_, text);
    C2D_TextOptimize(&parsed);

    u32 c2dColor = ToC2DColor(color);
    float x = static_cast<float>(pos.x);
    float y = static_cast<float>(pos.y);
    C2D_DrawText(&parsed, C2D_WithColor, x, y, 0.4f, handle->scale, handle->scale, c2dColor);
    if (handle->bold) {
        C2D_DrawText(&parsed, C2D_WithColor, x + 1.0f, y, 0.4f, handle->scale, handle->scale, c2dColor);
    }
}

litehtml::pixel_t HtmlContainer::pt_to_px(float pt) const { return pt * 96.0f / 72.0f; }

litehtml::pixel_t HtmlContainer::get_default_font_size() const { return kDefaultFontSizePx; }

const char *HtmlContainer::get_default_font_name() const { return "sans-serif"; }

void HtmlContainer::draw_list_marker(litehtml::uint_ptr /*hdc*/, const litehtml::list_marker &marker) {
    if (marker.marker_type == litehtml::list_style_type_none) {
        return;
    }
    auto *handle = reinterpret_cast<FontHandle *>(marker.font);
    const char *glyph = "\xE2\x80\xA2";  // "•" for disc/circle and anything else we don't special-case

    C2D_TextBufClear(textBuf_);
    C2D_Text parsed;
    C2D_TextFontParse(&parsed, font_, textBuf_, glyph);
    C2D_TextOptimize(&parsed);
    C2D_DrawText(&parsed, C2D_WithColor, static_cast<float>(marker.pos.x), static_cast<float>(marker.pos.y),
                 0.4f, handle->scale, handle->scale, ToC2DColor(marker.color));
}

// -- Images ------------------------------------------------------------
//
// Full decode+GPU-texture upload is a substantial subsystem on its own
// (PICA200 wants swizzled/tiled textures, not the linear PNG/JPEG output);
// this pass reads just the intrinsic width/height from the file header so
// layout reserves the right amount of space, and paints a placeholder box
// instead of real pixels. See the project notes for the follow-up.
bool ReadPngSize(const std::string &bytes, int *w, int *h) {
    static const unsigned char kSig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (bytes.size() < 24 || std::memcmp(bytes.data(), kSig, 8) != 0) {
        return false;
    }
    auto be32 = [&](size_t off) {
        const auto *p = reinterpret_cast<const unsigned char *>(bytes.data() + off);
        return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    };
    *w = be32(16);
    *h = be32(20);
    return true;
}

bool ReadJpegSize(const std::string &bytes, int *w, int *h) {
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.data());
    size_t len = bytes.size();
    if (len < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return false;
    }
    size_t pos = 2;
    while (pos + 4 <= len) {
        if (data[pos] != 0xFF) {
            ++pos;
            continue;
        }
        unsigned char marker = data[pos + 1];
        if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7)) {
            pos += 2;
            continue;
        }
        if (pos + 4 > len) {
            break;
        }
        size_t segmentLen = (data[pos + 2] << 8) | data[pos + 3];
        bool isSof = (marker >= 0xC0 && marker <= 0xCF) && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (isSof && pos + 9 <= len) {
            *h = (data[pos + 5] << 8) | data[pos + 6];
            *w = (data[pos + 7] << 8) | data[pos + 8];
            return true;
        }
        pos += 2 + segmentLen;
    }
    return false;
}

void HtmlContainer::load_image(const char *src, const char *baseurl, bool /*redraw_on_ready*/) {
    if (src == nullptr) {
        return;
    }
    std::string url = Core::ResolveUrl(src, baseurl != nullptr ? baseurl : baseUrl_);
    if (url.empty()) {
        url = src;
    }
    if (imageCache_.count(url) != 0) {
        return;
    }

    ImageInfo info;
    Core::FetchResult fetch = network_.Fetch(url);
    if (fetch.success) {
        if (!ReadPngSize(fetch.body, &info.width, &info.height)) {
            ReadJpegSize(fetch.body, &info.width, &info.height);
        }
    }
    if (info.width <= 0 || info.height <= 0) {
        info.width = 32;
        info.height = 32;
    }
    imageCache_[url] = info;
}

void HtmlContainer::get_image_size(const char *src, const char *baseurl, litehtml::size &sz) {
    std::string url = Core::ResolveUrl(src != nullptr ? src : "", baseurl != nullptr ? baseurl : baseUrl_);
    if (url.empty() && src != nullptr) {
        url = src;
    }
    auto it = imageCache_.find(url);
    if (it != imageCache_.end()) {
        sz.width = static_cast<float>(it->second.width);
        sz.height = static_cast<float>(it->second.height);
    } else {
        sz.width = 32;
        sz.height = 32;
    }
}

void HtmlContainer::draw_image(litehtml::uint_ptr /*hdc*/, const litehtml::background_layer &layer,
                                 const std::string & /*url*/, const std::string & /*base_url*/) {
    constexpr u32 kPlaceholderFill = C2D_Color32(0x3a, 0x3e, 0x46, 0xFF);
    constexpr u32 kPlaceholderBorder = C2D_Color32(0x55, 0x59, 0x62, 0xFF);
    const litehtml::position &box = layer.border_box;
    DrawRect(box.x, box.y, box.width, box.height, kPlaceholderFill);
    DrawRect(box.x, box.y, box.width, 1.0f, kPlaceholderBorder);
    DrawRect(box.x, box.y + box.height - 1.0f, box.width, 1.0f, kPlaceholderBorder);
    DrawRect(box.x, box.y, 1.0f, box.height, kPlaceholderBorder);
    DrawRect(box.x + box.width - 1.0f, box.y, 1.0f, box.height, kPlaceholderBorder);
}

void HtmlContainer::draw_solid_fill(litehtml::uint_ptr /*hdc*/, const litehtml::background_layer &layer,
                                      const litehtml::web_color &color) {
    if (color.alpha == 0) {
        return;
    }
    DrawRect(layer.border_box.x, layer.border_box.y, layer.border_box.width, layer.border_box.height,
             ToC2DColor(color));
}

void HtmlContainer::draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                                           const litehtml::background_layer::linear_gradient &gradient) {
    // Approximate with a flat fill using the first color stop; real gradient
    // rendering would need a small custom shader pass.
    if (!gradient.color_points.empty()) {
        draw_solid_fill(hdc, layer, gradient.color_points.front().color);
    }
}

void HtmlContainer::draw_radial_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                                           const litehtml::background_layer::radial_gradient &gradient) {
    if (!gradient.color_points.empty()) {
        draw_solid_fill(hdc, layer, gradient.color_points.front().color);
    }
}

void HtmlContainer::draw_conic_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                                          const litehtml::background_layer::conic_gradient &gradient) {
    if (!gradient.color_points.empty()) {
        draw_solid_fill(hdc, layer, gradient.color_points.front().color);
    }
}

void HtmlContainer::draw_borders(litehtml::uint_ptr /*hdc*/, const litehtml::borders &borders,
                                   const litehtml::position &box, bool /*root*/) {
    float x = static_cast<float>(box.x);
    float y = static_cast<float>(box.y);
    float w = static_cast<float>(box.width);
    float h = static_cast<float>(box.height);
    float topW = static_cast<float>(borders.top.width);
    float bottomW = static_cast<float>(borders.bottom.width);
    float leftW = static_cast<float>(borders.left.width);
    float rightW = static_cast<float>(borders.right.width);

    if (topW > 0.0f && borders.top.style != litehtml::border_style_none) {
        DrawRect(x, y, w, topW, ToC2DColor(borders.top.color));
    }
    if (bottomW > 0.0f && borders.bottom.style != litehtml::border_style_none) {
        DrawRect(x, y + h - bottomW, w, bottomW, ToC2DColor(borders.bottom.color));
    }
    if (leftW > 0.0f && borders.left.style != litehtml::border_style_none) {
        DrawRect(x, y, leftW, h, ToC2DColor(borders.left.color));
    }
    if (rightW > 0.0f && borders.right.style != litehtml::border_style_none) {
        DrawRect(x + w - rightW, y, rightW, h, ToC2DColor(borders.right.color));
    }
}

void HtmlContainer::set_caption(const char *caption) { pendingTitle_ = caption != nullptr ? caption : ""; }

void HtmlContainer::set_base_url(const char *base_url) { baseUrl_ = base_url != nullptr ? base_url : ""; }

void HtmlContainer::link(const std::shared_ptr<litehtml::document> & /*doc*/,
                           const litehtml::element::ptr & /*el*/) {
    // External stylesheets are fetched synchronously via import_css() below
    // when litehtml encounters a <link rel="stylesheet">; nothing to do here.
}

void HtmlContainer::on_anchor_click(const char *url, const litehtml::element::ptr & /*el*/) {
    if (url != nullptr) {
        clickedHref_ = url;
    }
}

void HtmlContainer::on_mouse_event(const litehtml::element::ptr & /*el*/, litehtml::mouse_event /*event*/) {}

void HtmlContainer::set_cursor(const char * /*cursor*/) {}

void HtmlContainer::transform_text(std::string &text, litehtml::text_transform tt) {
    switch (tt) {
        case litehtml::text_transform_uppercase:
            for (char &c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            break;
        case litehtml::text_transform_lowercase:
            for (char &c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            break;
        default:
            break;
    }
}

void HtmlContainer::import_css(std::string &text, const std::string &url, std::string &baseurl) {
    std::string resolved = Core::ResolveUrl(url, baseurl.empty() ? baseUrl_ : baseurl);
    if (resolved.empty()) {
        resolved = url;
    }
    Core::FetchResult fetch = network_.Fetch(resolved);
    if (fetch.success) {
        text = fetch.body;
        baseurl = fetch.finalUrl.empty() ? resolved : fetch.finalUrl;
    }
}

void HtmlContainer::set_clip(const litehtml::position &pos, const litehtml::border_radiuses & /*bdr_radius*/) {
    clipping_ = true;
    clipRect_ = pos;
    float px = static_cast<float>(pos.x);
    float py = static_cast<float>(pos.y);
    float pw = static_cast<float>(pos.width);
    float ph = static_cast<float>(pos.height);
    u32 left = px > 0.0f ? static_cast<u32>(px) : 0;
    u32 top = py > 0.0f ? static_cast<u32>(py) : 0;
    u32 right = static_cast<u32>(px + pw);
    u32 bottom = static_cast<u32>(py + ph);
    C3D_SetScissor(GPU_SCISSOR_NORMAL, left, top, right, bottom);
}

void HtmlContainer::del_clip() {
    clipping_ = false;
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

void HtmlContainer::get_viewport(litehtml::position &viewport) const {
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = 400;
    viewport.height = 240;
}

litehtml::element::ptr HtmlContainer::create_element(const char * /*tag_name*/,
                                                       const litehtml::string_map & /*attributes*/,
                                                       const std::shared_ptr<litehtml::document> & /*doc*/) {
    return nullptr;  // no custom element overrides; let litehtml use its defaults
}

void HtmlContainer::get_media_features(litehtml::media_features &media) const {
    media.type = litehtml::media_type_screen;
    media.width = 400;
    media.height = 240;
    media.device_width = 400;
    media.device_height = 240;
    media.color = 8;
    media.color_index = 0;
    media.monochrome = 0;
    media.resolution = 96;
}

void HtmlContainer::get_language(std::string &language, std::string &culture) const {
    language = "en";
    culture = "US";
}

}  // namespace Platform::N3DS
