#include "Core/BrowserApp.h"

#include <litehtml/document.h>

#include "Core/Url.h"

namespace Core {
namespace {

// Shown for a freshly-created tab, before the user has navigated it
// anywhere. Not fetched over the network -- parsed directly.
const char *const kHomePageHtml = R"HTML(
<!doctype html>
<html>
<head><title>New tab</title></head>
<body style="background:#20242b;color:#d6d6d6;font-family:sans-serif;padding:16px;">
<h1 style="color:#fff;">MordernBrowser</h1>
<p>Tap the address bar below to go somewhere.</p>
<p>Some starting points:</p>
<ul>
<li><a href="https://example.com/">example.com</a></li>
<li><a href="https://info.cern.ch/">info.cern.ch (the first website)</a></li>
<li><a href="https://lite.duckduckgo.com/lite/">DuckDuckGo (lite)</a></li>
</ul>
</body>
</html>
)HTML";

}  // namespace

BrowserApp::BrowserApp(INetworkClient &network, litehtml::document_container &container,
                        std::function<std::string()> takeTitle, float contentWidth)
    : network_(network),
      container_(container),
      takeTitle_(std::move(takeTitle)),
      contentWidth_(contentWidth) {}

int BrowserApp::NewTab(const std::string &startUrl) {
    tabs_.push_back(std::make_unique<Tab>());
    int index = static_cast<int>(tabs_.size()) - 1;
    activeTab_ = index;
    if (startUrl.empty()) {
        LoadHomePage(*tabs_[index], /*pushHistory=*/true);
    } else {
        Navigate(index, startUrl);
    }
    return index;
}

void BrowserApp::LoadHomePage(Tab &tab, bool pushHistory) {
    litehtml::document::ptr doc = litehtml::document::createFromString(kHomePageHtml, &container_);
    std::string title = takeTitle_();
    float height = 0.0f;
    if (doc) {
        doc->render(contentWidth_);
        height = static_cast<float>(doc->height());
    }
    tab.SetLoaded("about:home", std::move(title), std::move(doc), height);
    if (pushHistory) {
        tab.PushHistory("about:home");
    }
}

void BrowserApp::CloseTab(int index) {
    if (index < 0 || index >= TabCount()) {
        return;
    }
    tabs_.erase(tabs_.begin() + index);
    if (tabs_.empty()) {
        activeTab_ = -1;
        return;
    }
    if (activeTab_ >= TabCount()) {
        activeTab_ = TabCount() - 1;
    } else if (activeTab_ > index) {
        --activeTab_;
    }
}

void BrowserApp::Navigate(int tabIndex, const std::string &url) {
    std::string resolved = ResolveUrl(NormalizeTypedUrl(url), std::string());
    RequestLoad(*tabs_[tabIndex], resolved.empty() ? url : resolved, /*pushHistory=*/true, -1);
}

void BrowserApp::Reload(int tabIndex) {
    Tab &tab = *tabs_[tabIndex];
    if (!tab.Url().empty()) {
        RequestLoad(tab, tab.Url(), /*pushHistory=*/false, -1);
    }
}

void BrowserApp::GoBack(int tabIndex) {
    Tab &tab = *tabs_[tabIndex];
    if (!tab.CanGoBack()) {
        return;
    }
    int newIndex = tab.HistoryIndex() - 1;
    RequestLoad(tab, tab.HistoryUrlAt(newIndex), /*pushHistory=*/false, newIndex);
}

void BrowserApp::GoForward(int tabIndex) {
    Tab &tab = *tabs_[tabIndex];
    if (!tab.CanGoForward()) {
        return;
    }
    int newIndex = tab.HistoryIndex() + 1;
    RequestLoad(tab, tab.HistoryUrlAt(newIndex), /*pushHistory=*/false, newIndex);
}

void BrowserApp::RequestLoad(Tab &tab, const std::string &url, bool pushHistory,
                              int setHistoryIndex) {
    if (url == "about:home") {
        // Not fetched, so there's nothing to wait for -- just do it now.
        LoadHomePage(tab, pushHistory);
        if (setHistoryIndex >= 0) {
            tab.SetHistoryIndex(setHistoryIndex);
        }
        return;
    }

    tab.SetLoading(url);
    pending_ = PendingNavigation{true, &tab, url, pushHistory, setHistoryIndex};
}

void BrowserApp::ProcessPendingNavigation() {
    if (!pending_.active) {
        return;
    }
    PendingNavigation request = pending_;
    pending_.active = false;
    PerformLoad(*request.tab, request.url, request.pushHistory, request.setHistoryIndex);
}

void BrowserApp::PerformLoad(Tab &tab, const std::string &url, bool pushHistory,
                              int setHistoryIndex) {
    auto recordHistory = [&](const std::string &finalUrl) {
        if (pushHistory) {
            tab.PushHistory(finalUrl);
        } else if (setHistoryIndex >= 0) {
            tab.SetHistoryIndex(setHistoryIndex);
        }
    };

    FetchResult fetch = network_.Fetch(url);
    if (!fetch.success) {
        tab.SetError(url, fetch.error.empty() ? "Failed to load page" : fetch.error);
        recordHistory(url);
        return;
    }

    const std::string finalUrl = fetch.finalUrl.empty() ? url : fetch.finalUrl;

    litehtml::document::ptr doc = litehtml::document::createFromString(fetch.body, &container_);
    if (!doc) {
        tab.SetError(finalUrl, "Failed to parse page");
        recordHistory(finalUrl);
        return;
    }

    std::string title = takeTitle_();
    doc->render(contentWidth_);
    float height = static_cast<float>(doc->height());

    tab.SetLoaded(finalUrl, std::move(title), std::move(doc), height);
    recordHistory(finalUrl);
}

}  // namespace Core
