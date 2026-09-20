#include "Platform/N3DS/CurlNetworkClient.h"

#include <3ds.h>
#include <curl/curl.h>

#include <cstdlib>
#include <malloc.h>

namespace Platform::N3DS {
namespace {

constexpr u32 kSocAlign = 0x1000;
constexpr u32 kSocBufferSize = 0x100000;

size_t WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *body = static_cast<std::string *>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

}  // namespace

CurlNetworkClient::CurlNetworkClient(std::string caBundlePath)
    : caBundlePath_(std::move(caBundlePath)) {
    socBuffer_ = memalign(kSocAlign, kSocBufferSize);
    if (socBuffer_ != nullptr &&
        socInit(static_cast<u32 *>(socBuffer_), kSocBufferSize) == 0) {
        socReady_ = true;
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
}

CurlNetworkClient::~CurlNetworkClient() {
    if (socReady_) {
        curl_global_cleanup();
        socExit();
    }
    free(socBuffer_);
}

Core::FetchResult CurlNetworkClient::Fetch(const std::string &url) {
    Core::FetchResult result;

    if (!socReady_) {
        result.error = "Network is not available (SOC init failed)";
        return result;
    }

    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        result.error = "curl_easy_init failed";
        return result;
    }

    char errorBuffer[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MordernBrowser/0.1 (Nintendo 3DS)");
    curl_easy_setopt(curl, CURLOPT_CAINFO, caBundlePath_.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        result.error = errorBuffer[0] != '\0' ? errorBuffer : curl_easy_strerror(code);
        curl_easy_cleanup(curl);
        return result;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    result.httpStatus = status;

    char *finalUrl = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &finalUrl);
    result.finalUrl = finalUrl != nullptr ? finalUrl : url;

    char *contentType = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
    result.contentType = contentType != nullptr ? contentType : "";

    curl_easy_cleanup(curl);

    if (status >= 400) {
        result.error = "HTTP " + std::to_string(status);
        return result;
    }

    result.success = true;
    return result;
}

}  // namespace Platform::N3DS
