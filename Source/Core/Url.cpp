#include "Core/Url.h"

#include "lexbor/url/url.h"

namespace Core {
namespace {

lxb_status_t AppendCallback(const lxb_char_t *data, size_t len, void *ctx) {
    static_cast<std::string *>(ctx)->append(reinterpret_cast<const char *>(data), len);
    return LXB_STATUS_OK;
}

}  // namespace

std::string ResolveUrl(const std::string &url, const std::string &baseUrl) {
    lxb_url_parser_t parser;
    if (lxb_url_parser_init(&parser, nullptr) != LXB_STATUS_OK) {
        return std::string();
    }

    lxb_url_t *base = nullptr;
    if (!baseUrl.empty()) {
        base = lxb_url_parse(&parser, nullptr,
                              reinterpret_cast<const lxb_char_t *>(baseUrl.data()),
                              baseUrl.size());
        lxb_url_parser_clean(&parser);
    }

    lxb_url_t *resolved = lxb_url_parse(&parser, base,
                                         reinterpret_cast<const lxb_char_t *>(url.data()),
                                         url.size());

    std::string result;
    if (resolved != nullptr) {
        lxb_url_serialize(resolved, AppendCallback, &result, false);
    }

    lxb_url_parser_memory_destroy(&parser);
    return result;
}

std::string NormalizeTypedUrl(const std::string &input) {
    if (input.find("://") != std::string::npos) {
        return input;
    }
    return "https://" + input;
}

}  // namespace Core
