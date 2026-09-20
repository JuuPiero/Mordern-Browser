#pragma once

#include <memory>
#include <string>
#include <vector>

#include <litehtml/document.h>

namespace Core {

// One browser tab's state: where it is in its own history, what's currently
// loaded, and its litehtml document (layout + CSS box tree already
// computed). Tab does not know how to fetch a page or how to paint one --
// BrowserApp owns the fetch pipeline and pushes results in here, and
// Platform/N3DS paints document() -- which keeps Tab a plain, easily
// inspected piece of state.
class Tab {
public:
    const std::string &Url() const { return url_; }
    const std::string &Title() const { return title_.empty() ? url_ : title_; }
    const litehtml::document::ptr &Document() const { return document_; }
    float ContentHeight() const { return contentHeight_; }
    bool IsLoading() const { return loading_; }
    bool HasError() const { return !error_.empty(); }
    const std::string &Error() const { return error_; }

    // Bumped every time SetLoaded()/SetError() replace the page, so the
    // platform's renderer knows when it needs to re-render vs. redraw.
    int Generation() const { return generation_; }

    float ScrollY() const { return scrollY_; }
    void SetScrollY(float y) { scrollY_ = y; }

    bool CanGoBack() const { return historyIndex_ > 0; }
    bool CanGoForward() const { return historyIndex_ + 1 < static_cast<int>(history_.size()); }
    const std::string &HistoryUrlAt(int index) const { return history_[index]; }
    int HistoryIndex() const { return historyIndex_; }
    void SetHistoryIndex(int index) { historyIndex_ = index; }

    // Appends `url`, discarding any forward history past the current point
    // (the usual "navigating from the middle of history" behaviour).
    void PushHistory(const std::string &url) {
        history_.resize(historyIndex_ + 1);
        history_.push_back(url);
        historyIndex_ = static_cast<int>(history_.size()) - 1;
    }

    void SetLoading(const std::string &url) {
        loading_ = true;
        error_.clear();
        url_ = url;
    }

    void SetLoaded(const std::string &url, std::string title, litehtml::document::ptr document,
                    float contentHeight) {
        loading_ = false;
        error_.clear();
        url_ = url;
        title_ = std::move(title);
        document_ = std::move(document);
        contentHeight_ = contentHeight;
        scrollY_ = 0.0f;
        ++generation_;
    }

    void SetError(const std::string &url, std::string error) {
        loading_ = false;
        url_ = url;
        error_ = std::move(error);
        document_.reset();
        contentHeight_ = 0.0f;
        scrollY_ = 0.0f;
        ++generation_;
    }

private:
    std::string url_;
    std::string title_;
    litehtml::document::ptr document_;
    float contentHeight_ = 0.0f;
    float scrollY_ = 0.0f;
    bool loading_ = false;
    std::string error_;
    int generation_ = 0;

    std::vector<std::string> history_;
    int historyIndex_ = -1;
};

}  // namespace Core
