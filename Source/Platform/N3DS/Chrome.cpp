#include "Platform/N3DS/Chrome.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Core/Url.h"
#include "Platform/N3DS/Keyboard.h"

namespace Platform::N3DS {
namespace {

constexpr float kScreenW = 320.0f;
constexpr float kScreenH = 240.0f;
constexpr float kTopScreenW = 400.0f;
constexpr float kTopScreenH = 240.0f;

constexpr float kTabStripH = 24.0f;
constexpr float kNavRowH = 32.0f;
constexpr float kNavButtonW = 32.0f;
constexpr float kNewTabW = 24.0f;
constexpr float kTapMoveThreshold = 10.0f;
constexpr float kMaxTabChipW = 90.0f;
constexpr float kMinTabChipW = 40.0f;

constexpr u32 kChromeBg = C2D_Color32(0x18, 0x1a, 0x20, 0xFF);
constexpr u32 kActiveTabBg = C2D_Color32(0x30, 0x36, 0x42, 0xFF);
constexpr u32 kInactiveTabBg = C2D_Color32(0x20, 0x22, 0x28, 0xFF);
constexpr u32 kButtonBg = C2D_Color32(0x2a, 0x2e, 0x36, 0xFF);
constexpr u32 kAddressBarBg = C2D_Color32(0x24, 0x28, 0x30, 0xFF);
constexpr u32 kTouchpadBg = C2D_Color32(0x14, 0x16, 0x1a, 0xFF);
constexpr u32 kTextColor = C2D_Color32(0xE6, 0xE6, 0xE6, 0xFF);
constexpr u32 kDisabledColor = C2D_Color32(0x60, 0x62, 0x68, 0xFF);
constexpr u32 kHintColor = C2D_Color32(0x50, 0x52, 0x58, 0xFF);

std::string Truncate(const std::string &text, size_t maxLen) {
    if (text.size() <= maxLen) {
        return text;
    }
    return text.substr(0, maxLen > 1 ? maxLen - 1 : 0) + "\xE2\x80\xA6";  // "…"
}

}  // namespace

Chrome::Chrome(Core::BrowserApp &app, HtmlContainer &container, C2D_Font font)
    : app_(app), container_(container), font_(font) {
    textBuf_ = C2D_TextBufNew(1024);
}

void Chrome::MapToContent(float touchX, float touchY, float *outX, float *outY) const {
    float relX = (touchX - contentTouch_.x) / contentTouch_.w;
    float relY = (touchY - contentTouch_.y) / contentTouch_.h;
    *outX = relX * kTopScreenW;
    *outY = relY * kTopScreenH;
}

void Chrome::HandleInput() {
    backButton_ = {0.0f, kTabStripH, kNavButtonW, kNavRowH};
    forwardButton_ = {kNavButtonW, kTabStripH, kNavButtonW, kNavRowH};
    reloadButton_ = {kNavButtonW * 2.0f, kTabStripH, kNavButtonW, kNavRowH};
    addressBar_ = {kNavButtonW * 3.0f, kTabStripH, kScreenW - kNavButtonW * 3.0f, kNavRowH};
    newTabButton_ = {kScreenW - kNewTabW, 0.0f, kNewTabW, kTabStripH};
    contentTouch_ = {0.0f, kTabStripH + kNavRowH, kScreenW, kScreenH - kTabStripH - kNavRowH};

    tabChips_.clear();
    int tabCount = app_.TabCount();
    if (tabCount > 0) {
        float chipW = (kScreenW - kNewTabW) / static_cast<float>(tabCount);
        chipW = std::min(kMaxTabChipW, std::max(kMinTabChipW, chipW));
        for (int i = 0; i < tabCount; ++i) {
            tabChips_.push_back({i * chipW, 0.0f, chipW, kTabStripH});
        }
    }

    touchPosition pos;
    hidTouchRead(&pos);
    u32 down = hidKeysDown();
    u32 held = hidKeysHeld();
    u32 up = hidKeysUp();

    if (down & KEY_TOUCH) {
        touchStart_ = pos;
        touchLast_ = pos;
        touchTravel_ = 0.0f;
        wasTouching_ = true;
        return;
    }

    if ((held & KEY_TOUCH) && wasTouching_) {
        float dx = static_cast<float>(pos.px) - static_cast<float>(touchLast_.px);
        float dy = static_cast<float>(pos.py) - static_cast<float>(touchLast_.py);
        touchTravel_ += std::fabs(dx) + std::fabs(dy);

        if (contentTouch_.Contains(static_cast<float>(touchStart_.px),
                                    static_cast<float>(touchStart_.py))) {
            HandleContentTouch(pos, true);
        }
        touchLast_ = pos;
        return;
    }

    if ((up & KEY_TOUCH) && wasTouching_) {
        wasTouching_ = false;
        if (touchTravel_ < kTapMoveThreshold) {
            float x = static_cast<float>(touchStart_.px);
            float y = static_cast<float>(touchStart_.py);

            if (newTabButton_.Contains(x, y) || (y < kTabStripH)) {
                HandleTabStripTap(x, y);
            } else if (addressBar_.Contains(x, y)) {
                HandleAddressBarTap();
            } else if (y >= kTabStripH && y < kTabStripH + kNavRowH) {
                HandleNavButtonTap(x, y);
            } else if (contentTouch_.Contains(x, y)) {
                HandleContentTouch(touchStart_, false);
            }
        }
    }
}

void Chrome::HandleTabStripTap(float x, float y) {
    (void)y;
    if (newTabButton_.Contains(x, kTabStripH * 0.5f)) {
        app_.NewTab();
        return;
    }
    for (size_t i = 0; i < tabChips_.size(); ++i) {
        if (tabChips_[i].Contains(x, kTabStripH * 0.5f)) {
            app_.SetActiveTab(static_cast<int>(i));
            return;
        }
    }
}

void Chrome::HandleAddressBarTap() {
    if (!app_.HasActiveTab()) {
        return;
    }
    KeyboardResult result = ShowKeyboard("Enter an address", app_.ActiveTab().Url());
    if (result.confirmed && !result.text.empty()) {
        app_.Navigate(app_.ActiveTabIndex(), result.text);
    }
}

void Chrome::HandleNavButtonTap(float x, float y) {
    (void)y;
    if (!app_.HasActiveTab()) {
        return;
    }
    int active = app_.ActiveTabIndex();
    if (backButton_.Contains(x, kTabStripH + kNavRowH * 0.5f)) {
        app_.GoBack(active);
    } else if (forwardButton_.Contains(x, kTabStripH + kNavRowH * 0.5f)) {
        app_.GoForward(active);
    } else if (reloadButton_.Contains(x, kTabStripH + kNavRowH * 0.5f)) {
        app_.Reload(active);
    }
}

void Chrome::HandleContentTouch(const touchPosition &pos, bool dragging) {
    if (!app_.HasActiveTab()) {
        return;
    }
    Core::Tab &tab = app_.ActiveTab();
    if (!tab.Document()) {
        return;
    }

    if (dragging) {
        float dy = static_cast<float>(pos.py) - static_cast<float>(touchLast_.py);
        float scale = kTopScreenH / contentTouch_.h;
        float maxScroll = std::max(0.0f, tab.ContentHeight() - kTopScreenH);
        float newScroll = std::min(maxScroll, std::max(0.0f, tab.ScrollY() - dy * scale));
        tab.SetScrollY(newScroll);
        return;
    }

    // A tap (not a drag): let litehtml do its own hit-testing against the
    // render tree (it calls our container's on_anchor_click for us).
    float contentX = 0.0f, contentY = 0.0f;
    MapToContent(static_cast<float>(pos.px), static_cast<float>(pos.py), &contentX, &contentY);
    contentY += tab.ScrollY();

    auto noRedraw = [](const litehtml::position &) {};
    tab.Document()->on_lbutton_down(contentX, contentY, contentX, contentY, noRedraw);
    tab.Document()->on_lbutton_up(contentX, contentY, contentX, contentY, noRedraw);

    std::string href = container_.TakeClickedHref();
    if (!href.empty()) {
        std::string resolved = Core::ResolveUrl(href, tab.Url());
        app_.Navigate(app_.ActiveTabIndex(), resolved.empty() ? href : resolved);
    }
}

void Chrome::Draw(C3D_RenderTarget *target) {
    C2D_TargetClear(target, kChromeBg);
    C2D_SceneBegin(target);

    DrawTabStrip(0.42f);
    DrawNavButtons(0.5f);
    DrawAddressBar(0.42f);
    DrawContentTouchpad();
}

void Chrome::DrawTabStrip(float scale) {
    C2D_TextBufClear(textBuf_);

    for (size_t i = 0; i < tabChips_.size(); ++i) {
        const Rect &chip = tabChips_[i];
        bool active = static_cast<int>(i) == app_.ActiveTabIndex();
        C2D_DrawRectSolid(chip.x, chip.y, 0.3f, chip.w - 1.0f, chip.h,
                          active ? kActiveTabBg : kInactiveTabBg);

        std::string label = Truncate(app_.TabAt(static_cast<int>(i)).Title(), 14);
        if (label.empty()) {
            label = "New tab";
        }
        C2D_Text text;
        C2D_TextFontParse(&text, font_, textBuf_, label.c_str());
        C2D_TextOptimize(&text);
        C2D_DrawText(&text, C2D_WithColor, chip.x + 4.0f, chip.y + 5.0f, 0.35f, scale, scale,
                     kTextColor);
    }

    C2D_DrawRectSolid(newTabButton_.x, newTabButton_.y, 0.3f, newTabButton_.w, newTabButton_.h,
                      kButtonBg);
    C2D_Text plus;
    C2D_TextFontParse(&plus, font_, textBuf_, "+");
    C2D_TextOptimize(&plus);
    C2D_DrawText(&plus, C2D_WithColor, newTabButton_.x + 8.0f, newTabButton_.y + 4.0f, 0.35f, 0.55f,
                 0.55f, kTextColor);
}

void Chrome::DrawNavButtons(float scale) {
    bool hasTab = app_.HasActiveTab();
    bool canBack = hasTab && app_.ActiveTab().CanGoBack();
    bool canForward = hasTab && app_.ActiveTab().CanGoForward();

    C2D_DrawRectSolid(backButton_.x, backButton_.y, 0.3f, backButton_.w - 1.0f, backButton_.h,
                      kButtonBg);
    C2D_DrawRectSolid(forwardButton_.x, forwardButton_.y, 0.3f, forwardButton_.w - 1.0f,
                      forwardButton_.h, kButtonBg);
    C2D_DrawRectSolid(reloadButton_.x, reloadButton_.y, 0.3f, reloadButton_.w - 1.0f,
                      reloadButton_.h, kButtonBg);

    C2D_TextBufClear(textBuf_);
    C2D_Text back, forward, reload;
    C2D_TextFontParse(&back, font_, textBuf_, "<");
    C2D_TextFontParse(&forward, font_, textBuf_, ">");
    C2D_TextFontParse(&reload, font_, textBuf_, "R");
    C2D_TextOptimize(&back);
    C2D_TextOptimize(&forward);
    C2D_TextOptimize(&reload);

    C2D_DrawText(&back, C2D_WithColor, backButton_.x + 12.0f, backButton_.y + 8.0f, 0.35f, scale,
                 scale, canBack ? kTextColor : kDisabledColor);
    C2D_DrawText(&forward, C2D_WithColor, forwardButton_.x + 12.0f, forwardButton_.y + 8.0f, 0.35f,
                 scale, scale, canForward ? kTextColor : kDisabledColor);
    C2D_DrawText(&reload, C2D_WithColor, reloadButton_.x + 11.0f, reloadButton_.y + 8.0f, 0.35f,
                 scale, scale, hasTab ? kTextColor : kDisabledColor);
}

void Chrome::DrawAddressBar(float scale) {
    C2D_DrawRectSolid(addressBar_.x, addressBar_.y, 0.3f, addressBar_.w - 2.0f, addressBar_.h,
                      kAddressBarBg);

    std::string url = app_.HasActiveTab() ? app_.ActiveTab().Url() : "";
    if (app_.HasActiveTab() && app_.ActiveTab().IsLoading()) {
        url = "Loading...";
    }
    std::string label = Truncate(url, 40);

    C2D_TextBufClear(textBuf_);
    C2D_Text text;
    C2D_TextFontParse(&text, font_, textBuf_, label.empty() ? "Tap to enter an address" : label.c_str());
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, addressBar_.x + 6.0f, addressBar_.y + 9.0f, 0.35f, scale,
                 scale, label.empty() ? kHintColor : kTextColor);
}

void Chrome::DrawContentTouchpad() {
    C2D_DrawRectSolid(contentTouch_.x, contentTouch_.y, 0.3f, contentTouch_.w, contentTouch_.h,
                      kTouchpadBg);

    if (!app_.HasActiveTab() || app_.ActiveTab().HasError() || app_.ActiveTab().IsLoading()) {
        return;
    }

    C2D_TextBufClear(textBuf_);
    C2D_Text hint;
    C2D_TextFontParse(&hint, font_, textBuf_, "Drag to scroll * tap to follow a link");
    C2D_TextOptimize(&hint);
    C2D_DrawText(&hint, C2D_WithColor, contentTouch_.x + 8.0f, contentTouch_.y + 8.0f, 0.35f, 0.35f,
                 0.35f, kHintColor);
}

}  // namespace Platform::N3DS
