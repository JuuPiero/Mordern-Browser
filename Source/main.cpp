// MordernBrowser entry point / composition root.
//
// This is the only file that constructs concrete Platform::N3DS types and
// wires them into Core::BrowserApp -- see the layering note at the top of
// Include/Core and Include/Platform/N3DS for why the split exists.

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <cstdio>

#include "Core/BrowserApp.h"
#include "Platform/N3DS/C2DTextMeasurer.h"
#include "Platform/N3DS/Chrome.h"
#include "Platform/N3DS/ContentRenderer.h"
#include "Platform/N3DS/CurlNetworkClient.h"

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

    Platform::N3DS::C2DTextMeasurer measurer;
    Platform::N3DS::CurlNetworkClient network("romfs:/cacert.pem");

    Core::BrowserApp app(network, measurer, kTopScreenWidth);
    Platform::N3DS::ContentRenderer content;
    Platform::N3DS::Chrome chrome(app, content);

    app.NewTab();  // opens the built-in home page

    int renderedTab = -1;
    int renderedGeneration = -1;

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) {
            break;
        }

        chrome.HandleInput();

        if (app.HasActiveTab()) {
            const Core::Tab &tab = app.ActiveTab();
            if (app.ActiveTabIndex() != renderedTab || tab.Generation() != renderedGeneration) {
                content.SetLayout(tab.Layout());
                renderedTab = app.ActiveTabIndex();
                renderedGeneration = tab.Generation();
            }
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        if (!app.HasActiveTab()) {
            content.DrawMessage(top, "No tabs open", kTopScreenWidth, kTopScreenHeight);
        } else {
            const Core::Tab &tab = app.ActiveTab();
            if (tab.IsLoading()) {
                content.DrawMessage(top, "Loading...", kTopScreenWidth, kTopScreenHeight);
            } else if (tab.HasError()) {
                content.DrawMessage(top, tab.Error(), kTopScreenWidth, kTopScreenHeight);
            } else {
                content.Draw(top, tab.ScrollY(), kTopScreenWidth, kTopScreenHeight);
            }
        }

        chrome.Draw(bottom);

        C3D_FrameEnd(0);

        // Perform any navigation Chrome queued this frame now, after the
        // "Loading..." frame above has already been handed to the GPU for
        // display -- Fetch() blocks, so this is when the UI actually stalls.
        app.ProcessPendingNavigation();
    }

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    romfsExit();
    return 0;
}
