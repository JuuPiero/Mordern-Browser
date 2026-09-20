#pragma once

#include <string>

#include "Core/INetworkClient.h"

namespace Platform::N3DS {

// libcurl + mbedtls implementation of Core::INetworkClient. Owns the 3DS
// sockets service (SOC) buffer for its lifetime, so only one of these should
// exist at a time.
class CurlNetworkClient : public Core::INetworkClient {
public:
    // `caBundlePath` should point at a PEM CA bundle reachable through
    // stdio (e.g. "romfs:/cacert.pem") for HTTPS certificate verification.
    explicit CurlNetworkClient(std::string caBundlePath);
    ~CurlNetworkClient() override;

    CurlNetworkClient(const CurlNetworkClient &) = delete;
    CurlNetworkClient &operator=(const CurlNetworkClient &) = delete;

    bool IsReady() const { return socReady_; }

    Core::FetchResult Fetch(const std::string &url) override;

private:
    std::string caBundlePath_;
    void *socBuffer_ = nullptr;
    bool socReady_ = false;
};

}  // namespace Platform::N3DS
