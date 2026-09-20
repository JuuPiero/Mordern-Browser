#pragma once

#include <memory>
#include <string>

struct lxb_html_document;
struct lxb_dom_node;

namespace Core {

// RAII owner of a parsed lexbor HTML document. Everything else in Core
// (Layout, and later a DOM-facing JS binding) borrows the raw lexbor handles
// from here rather than touching lexbor's lifetime rules directly.
class HtmlDocument {
public:
    HtmlDocument();
    ~HtmlDocument();

    HtmlDocument(const HtmlDocument &) = delete;
    HtmlDocument &operator=(const HtmlDocument &) = delete;
    HtmlDocument(HtmlDocument &&other) noexcept;
    HtmlDocument &operator=(HtmlDocument &&other) noexcept;

    // Parses `html` (already-decoded bytes; lexbor does its own charset
    // sniffing per the HTML5 spec). Returns false on allocation failure.
    bool Parse(const std::string &html);

    // The document's <title>, or empty if there isn't one.
    std::string Title() const;

    // The root node to start a layout/DOM walk from (<body>, falling back to
    // the document element if there's no body), or nullptr if nothing has
    // been parsed yet.
    lxb_dom_node *BodyNode() const;

    lxb_html_document *Raw() const { return doc_; }

private:
    void Reset();

    lxb_html_document *doc_ = nullptr;
};

}  // namespace Core
