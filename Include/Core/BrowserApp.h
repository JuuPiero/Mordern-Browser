#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Core/INetworkClient.h"
#include "Core/ITextMeasurer.h"
#include "Core/Tab.h"

namespace Core {

// Owns every tab and the fetch -> parse -> layout pipeline. This is the one
// class Platform/N3DS's UI code talks to; it never touches libcurl, citro2d,
// or lexbor directly itself (those are behind INetworkClient/ITextMeasurer,
// or wrapped by HtmlDocument/Layout).
class BrowserApp {
public:
    BrowserApp(INetworkClient &network, ITextMeasurer &measurer, float contentWidth);

    // Returns the new tab's index.
    int NewTab(const std::string &startUrl = std::string());
    void CloseTab(int index);
    void SetActiveTab(int index) { activeTab_ = index; }
    int ActiveTabIndex() const { return activeTab_; }
    int TabCount() const { return static_cast<int>(tabs_.size()); }

    Tab &TabAt(int index) { return *tabs_[index]; }
    const Tab &TabAt(int index) const { return *tabs_[index]; }
    Tab &ActiveTab() { return *tabs_[activeTab_]; }
    const Tab &ActiveTab() const { return *tabs_[activeTab_]; }
    bool HasActiveTab() const { return activeTab_ >= 0 && activeTab_ < TabCount(); }

    // These mark the tab as loading and return immediately; the actual
    // (blocking) network fetch happens inside ProcessPendingNavigation().
    // This split exists so the platform loop can render a "Loading..."
    // frame before the UI thread blocks on the network.
    void Navigate(int tabIndex, const std::string &url);
    void Reload(int tabIndex);
    void GoBack(int tabIndex);
    void GoForward(int tabIndex);

    bool HasPendingNavigation() const { return pending_.active; }
    // Call once per frame, after rendering, to actually perform whichever
    // navigation Navigate()/Reload()/GoBack()/GoForward() queued up (if any).
    void ProcessPendingNavigation();

private:
    struct PendingNavigation {
        bool active = false;
        Tab *tab = nullptr;
        std::string url;
        bool pushHistory = false;
        int setHistoryIndex = -1;
    };

    void RequestLoad(Tab &tab, const std::string &url, bool pushHistory, int setHistoryIndex);
    void PerformLoad(Tab &tab, const std::string &url, bool pushHistory, int setHistoryIndex);
    void LoadHomePage(Tab &tab, bool pushHistory);

    INetworkClient &network_;
    ITextMeasurer &measurer_;
    float contentWidth_;
    std::vector<std::unique_ptr<Tab>> tabs_;
    int activeTab_ = -1;
    PendingNavigation pending_;
};

}  // namespace Core
