#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <litehtml/document_container.h>

#include "Core/INetworkClient.h"
#include "Core/Tab.h"

namespace Core {

// Owns every tab and the fetch -> parse -> render pipeline. This is the one
// class Platform/N3DS's UI code talks to; it never touches libcurl directly
// (that's behind INetworkClient) or litehtml's HTML/CSS internals directly
// (layout is litehtml's job -- BrowserApp just calls document::render()).
//
// `container` is litehtml's abstract document_container, not a concrete
// Platform type, so this stays free of any 3DS/citro2d dependency; the one
// thing litehtml can't hand back through that interface is the page title,
// which is why `takeTitle` is injected separately (Platform's container
// implementation captures it from set_caption() and hands it over here).
class BrowserApp {
public:
    BrowserApp(INetworkClient &network, litehtml::document_container &container,
               std::function<std::string()> takeTitle, float contentWidth);

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
    litehtml::document_container &container_;
    std::function<std::string()> takeTitle_;
    float contentWidth_;
    std::vector<std::unique_ptr<Tab>> tabs_;
    int activeTab_ = -1;
    PendingNavigation pending_;
};

}  // namespace Core
