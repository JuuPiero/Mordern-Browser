#pragma once

#include <vector>

#include <3ds.h>
#include <citro2d.h>

#include "Core/BrowserApp.h"
#include "Platform/N3DS/ContentRenderer.h"

namespace Platform::N3DS {

// The bottom-screen touch UI: tab strip, address bar, Back/Forward/Reload,
// and a touch surface that scrolls/taps the active tab's content (which is
// rendered full-size on the top screen by `content`). Owns all input
// handling for the app other than the global Start-to-exit check in main().
class Chrome {
public:
    Chrome(Core::BrowserApp &app, ContentRenderer &content);

    // Reads the current touch/button state (call after hidScanInput()) and
    // acts on it directly against `app` (navigate, switch tabs, scroll...).
    void HandleInput();

    void Draw(C3D_RenderTarget *target);

private:
    struct Rect {
        float x, y, w, h;
        bool Contains(float px, float py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void HandleTabStripTap(float x, float y);
    void HandleAddressBarTap();
    void HandleNavButtonTap(float x, float y);
    void HandleContentTouch(const touchPosition &pos, bool touching);
    void MapToContent(float touchX, float touchY, float *outX, float *outY) const;

    void DrawTabStrip(float scale);
    void DrawAddressBar(float scale);
    void DrawNavButtons(float scale);
    void DrawContentTouchpad();

    Core::BrowserApp &app_;
    ContentRenderer &content_;

    C2D_TextBuf textBuf_;

    Rect backButton_{};
    Rect forwardButton_{};
    Rect reloadButton_{};
    Rect addressBar_{};
    Rect newTabButton_{};
    Rect contentTouch_{};
    std::vector<Rect> tabChips_;

    bool wasTouching_ = false;
    touchPosition touchStart_{};
    touchPosition touchLast_{};
    float touchTravel_ = 0.0f;
};

}  // namespace Platform::N3DS
