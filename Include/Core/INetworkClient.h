#pragma once

#include <string>

namespace Core {

struct FetchResult {
    bool success = false;
    long httpStatus = 0;
    std::string finalUrl;    // after following redirects
    std::string contentType;
    std::string body;
    std::string error;       // human-readable failure reason when !success
};

// Blocking HTTP(S) fetch, abstracted so Core never has to know about libcurl
// or the 3DS sockets service. Platform/N3DS/CurlNetworkClient implements it.
class INetworkClient {
public:
    virtual ~INetworkClient() = default;
    virtual FetchResult Fetch(const std::string &url) = 0;
};

}  // namespace Core
