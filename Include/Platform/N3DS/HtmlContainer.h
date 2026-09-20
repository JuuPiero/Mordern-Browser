#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <citro2d.h>
#include <litehtml/document_container.h>

#include "Core/INetworkClient.h"

namespace Platform::N3DS {

// citro2d implementation of litehtml's document_container -- the one
// interface litehtml uses for every pixel it draws. litehtml owns HTML
// parsing, the CSS cascade, and box-model layout; this class only turns its
// draw_*() calls into citro2d/citro3d GPU commands. It also brokers
// litehtml's synchronous "fetch me a resource" callbacks (import_css,
// load_image) through Core::INetworkClient, and surfaces link taps.
class HtmlContainer : public litehtml::document_container {
public:
    // `font` may be nullptr to fall back to the system font. `network` is
    // used for external stylesheets and images (litehtml calls back into
    // this synchronously during parsing/layout, same as our page fetch).
    HtmlContainer(C2D_Font font, Core::INetworkClient &network);
    ~HtmlContainer() override;

    // Called by Chrome when a link is actually clicked (see
    // on_anchor_click below); consumed once by main.cpp/BrowserApp per tap.
    std::string TakeClickedHref();

    // litehtml::document_container:
    litehtml::uint_ptr create_font(const litehtml::font_description &descr, const litehtml::document *doc,
                                    litehtml::font_metrics *fm) override;
    void delete_font(litehtml::uint_ptr hFont) override;
    litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr hFont) override;
    void draw_text(litehtml::uint_ptr hdc, const char *text, litehtml::uint_ptr hFont,
                    litehtml::web_color color, const litehtml::position &pos) override;
    litehtml::pixel_t pt_to_px(float pt) const override;
    litehtml::pixel_t get_default_font_size() const override;
    const char *get_default_font_name() const override;
    void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker &marker) override;
    void load_image(const char *src, const char *baseurl, bool redraw_on_ready) override;
    void get_image_size(const char *src, const char *baseurl, litehtml::size &sz) override;
    void draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer &layer, const std::string &url,
                     const std::string &base_url) override;
    void draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                          const litehtml::web_color &color) override;
    void draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                               const litehtml::background_layer::linear_gradient &gradient) override;
    void draw_radial_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                               const litehtml::background_layer::radial_gradient &gradient) override;
    void draw_conic_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                              const litehtml::background_layer::conic_gradient &gradient) override;
    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders &borders, const litehtml::position &draw_pos,
                       bool root) override;
    void set_caption(const char *caption) override;
    void set_base_url(const char *base_url) override;
    void link(const std::shared_ptr<litehtml::document> &doc, const litehtml::element::ptr &el) override;
    void on_anchor_click(const char *url, const litehtml::element::ptr &el) override;
    void on_mouse_event(const litehtml::element::ptr &el, litehtml::mouse_event event) override;
    void set_cursor(const char *cursor) override;
    void transform_text(std::string &text, litehtml::text_transform tt) override;
    void import_css(std::string &text, const std::string &url, std::string &baseurl) override;
    void set_clip(const litehtml::position &pos, const litehtml::border_radiuses &bdr_radius) override;
    void del_clip() override;
    void get_viewport(litehtml::position &viewport) const override;
    litehtml::element::ptr create_element(const char *tag_name, const litehtml::string_map &attributes,
                                           const std::shared_ptr<litehtml::document> &doc) override;
    void get_media_features(litehtml::media_features &media) const override;
    void get_language(std::string &language, std::string &culture) const override;

    // Called once per frame from main.cpp; caption/base_url updates land
    // here so BrowserApp can pick up a <title> change after doc->render().
    std::string TakePageTitle();

private:
    struct FontHandle {
        float scale;
        bool bold;
        int lineFeed;
        int ascent;
    };

    struct ImageInfo {
        int width = 0;
        int height = 0;
    };

    C2D_Font font_;
    Core::INetworkClient &network_;
    C2D_TextBuf textBuf_;
    std::string baseUrl_;
    std::string pendingTitle_;
    std::string clickedHref_;
    bool clipping_ = false;
    litehtml::position clipRect_{};
    std::unordered_map<std::string, ImageInfo> imageCache_;
};

}  // namespace Platform::N3DS
