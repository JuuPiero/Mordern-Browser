#pragma once

#include <string>

namespace Core {

// Resolves `url` against `baseUrl` per the WHATWG URL spec (via lexbor's URL
// module). Returns an empty string if `url` cannot be parsed even with a
// base. `baseUrl` may be empty if `url` is already absolute.
std::string ResolveUrl(const std::string &url, const std::string &baseUrl);

// Turns address-bar input into a fetchable URL: a bare "example.com" becomes
// "https://example.com". Input that already has a scheme is left untouched.
// There is no default search engine yet, so non-URL-looking input is just
// passed through with a scheme guess.
std::string NormalizeTypedUrl(const std::string &input);

}  // namespace Core
