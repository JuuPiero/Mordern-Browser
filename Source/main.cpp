// MordernBrowser bring-up smoke test.
//
// This does not render a web page yet — it boots gfx (citro2d/citro3d),
// then exercises the two vendored engines end to end (HTML parsing via
// lexbor, script evaluation via QuickJS) so the whole toolchain + Vendor
// stack is proven to link and run on real hardware/citra before any
// layout/rendering work is built on top of it.

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <cstdio>
#include <cstring>

#include "lexbor/html/interfaces/document.h"

#include "quickjs.h"

// This QuickJS fork calls debug_log() unconditionally during engine init
// (JS_NewContext / JS_AddIntrinsicBaseObjects); the host must provide it.
extern "C" void debug_log(const char *msg, int a) {
    (void)a;
    std::printf("[qjs] %s\n", msg);
}

static const lxb_char_t kSampleHtml[] =
    "<!doctype html><html><head><title>MordernBrowser</title></head>"
    "<body><h1>It works</h1></body></html>";

// Parses kSampleHtml with lexbor and returns its <title> text, proving the
// vendored HTML engine is linked in correctly.
static const char *ParseSampleTitle(char *out, size_t out_size) {
    lxb_html_document_t *document = lxb_html_document_create();
    if (document == nullptr) {
        return "lexbor: failed to create document";
    }

    lxb_status_t status = lxb_html_document_parse(
        document, kSampleHtml, sizeof(kSampleHtml) - 1);
    if (status != LXB_STATUS_OK) {
        lxb_html_document_destroy(document);
        return "lexbor: failed to parse HTML";
    }

    size_t title_len = 0;
    const lxb_char_t *title = lxb_html_document_title(document, &title_len);
    if (title == nullptr) {
        std::snprintf(out, out_size, "lexbor: <title> is empty");
    } else {
        std::snprintf(out, out_size, "lexbor: title=\"%.*s\"",
                       static_cast<int>(title_len), title);
    }

    lxb_html_document_destroy(document);
    return out;
}

// Evaluates a small script with QuickJS and returns its string result,
// proving the vendored JS engine is linked in correctly.
static const char *EvalSampleScript(char *out, size_t out_size) {
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    static const char script[] = "'MordernBrowser v' + (0.0 + 1)";
    JSValue result = JS_Eval(ctx, script, sizeof(script) - 1, "<init>",
                              JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        std::snprintf(out, out_size, "quickjs: script threw an exception");
    } else {
        const char *str = JS_ToCString(ctx, result);
        std::snprintf(out, out_size, "quickjs: result=\"%s\"",
                      str != nullptr ? str : "(null)");
        if (str != nullptr) {
            JS_FreeCString(ctx, str);
        }
    }

    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return out;
}

int main() {
    char lexborLine[128];
    char quickjsLine[128];
    ParseSampleTitle(lexborLine, sizeof(lexborLine));
    EvalSampleScript(quickjsLine, sizeof(quickjsLine));

    gfxInitDefault();
    consoleInit(GFX_BOTTOM, nullptr);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

    std::printf("MordernBrowser bring-up\n");
    std::printf("%s\n", lexborLine);
    std::printf("%s\n\n", quickjsLine);
    std::printf("Press Start to exit.\n");

    C2D_TextBuf textBuf = C2D_TextBufNew(4096);
    C2D_Text titleText, statusText;
    C2D_TextParse(&titleText, textBuf, "MordernBrowser");
    C2D_TextOptimize(&titleText);

    char statusLine[256];
    std::snprintf(statusLine, sizeof(statusLine), "%s\n%s", lexborLine,
                  quickjsLine);
    C2D_TextParse(&statusText, textBuf, statusLine);
    C2D_TextOptimize(&statusText);

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) {
            break;
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, C2D_Color32(0x20, 0x24, 0x2b, 0xFF));
        C2D_SceneBegin(top);
        C2D_DrawText(&titleText, C2D_WithColor, 16.0f, 16.0f, 0.5f, 0.9f, 0.9f,
                     C2D_Color32(0xff, 0xff, 0xff, 0xff));
        C2D_DrawText(&statusText, C2D_WithColor, 16.0f, 56.0f, 0.5f, 0.6f, 0.6f,
                     C2D_Color32(0x9a, 0xd6, 0x7a, 0xff));
        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
