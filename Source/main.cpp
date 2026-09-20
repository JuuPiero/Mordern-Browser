// MordernBrowser entry point / composition root.
//
// This is the only file that constructs concrete Platform::N3DS types and
// wires them into Core::BrowserApp -- see the layering note at the top of
// Include/Core and Include/Platform/N3DS for why the split exists. Page
// layout/CSS is litehtml's job (Vendor/litehtml); HtmlContainer is our
// citro2d-only drawing backend for it.

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <cstdio>

#include <litehtml/document.h>

#include "Core/BrowserApp.h"
#include "Platform/N3DS/Chrome.h"
#include "Platform/N3DS/CurlNetworkClient.h"
#include "Platform/N3DS/HtmlContainer.h"

// This QuickJS fork calls debug_log() unconditionally during engine init;
// the host must provide it. Page scripting isn't wired up yet (see
// Include/Core/BrowserApp.h), so QuickJS itself isn't instantiated here yet,
// but this keeps Vendor/quickjs linkable for when it is.
extern "C" void debug_log(const char *msg, int) { std::printf("[qjs] %s\n", msg); }

namespace {

constexpr float kTopScreenWidth = 400.0f;
constexpr float kTopScreenHeight = 240.0f;

}  // namespace

int main() {
    romfsInit();
    gfxInitDefault();
    gfxSet3D(false);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C2D_Font font = C2D_FontLoad("romfs:/font.bcfnt");  // DejaVu Sans w/ Vietnamese glyphs; see Assets/romfs

    Platform::N3DS::CurlNetworkClient network("romfs:/cacert.pem");
    Platform::N3DS::HtmlContainer container(font, network);

    Core::BrowserApp app(
        network, container, [&container] { return container.TakePageTitle(); }, kTopScreenWidth);
    Platform::N3DS::Chrome chrome(app, container, font);

    app.NewTab();  // opens the built-in home page

    litehtml::position clip{0, 0, kTopScreenWidth, kTopScreenHeight};

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) {
            break;
        }

        chrome.HandleInput();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        if (!app.HasActiveTab()) {
            C2D_TargetClear(top, C2D_Color32(0x2b, 0x24, 0x20, 0xFF));
            C2D_SceneBegin(top);
        } else {
            const Core::Tab &tab = app.ActiveTab();
            C2D_TargetClear(top, C2D_Color32(0x2b, 0x24, 0x20, 0xFF));
            C2D_SceneBegin(top);
            if (tab.IsLoading()) {
                // Nothing drawn yet this load -- the cleared background is
                // the whole "Loading..." state for now.
            } else if (!tab.HasError() && tab.Document()) {
                tab.Document()->draw(0, 0, -tab.ScrollY(), &clip);
            }
        }

        chrome.Draw(bottom);

        C3D_FrameEnd(0);

        // Perform any navigation Chrome queued this frame now, after the
        // "Loading..." frame above has already been handed to the GPU for
        // display -- Fetch() blocks, so this is when the UI actually stalls.
        app.ProcessPendingNavigation();
    }

    if (font != nullptr) {
        C2D_FontFree(font);
    }
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    romfsExit();
    return 0;
}
