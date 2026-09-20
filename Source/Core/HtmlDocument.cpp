#include "Core/HtmlDocument.h"

#include "lexbor/html/interfaces/document.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interface.h"

namespace Core {

HtmlDocument::HtmlDocument() = default;

HtmlDocument::~HtmlDocument() { Reset(); }

HtmlDocument::HtmlDocument(HtmlDocument &&other) noexcept : doc_(other.doc_) {
    other.doc_ = nullptr;
}

HtmlDocument &HtmlDocument::operator=(HtmlDocument &&other) noexcept {
    if (this != &other) {
        Reset();
        doc_ = other.doc_;
        other.doc_ = nullptr;
    }
    return *this;
}

void HtmlDocument::Reset() {
    if (doc_ != nullptr) {
        lxb_html_document_destroy(doc_);
        doc_ = nullptr;
    }
}

bool HtmlDocument::Parse(const std::string &html) {
    Reset();

    doc_ = lxb_html_document_create();
    if (doc_ == nullptr) {
        return false;
    }

    lxb_status_t status = lxb_html_document_parse(
        doc_, reinterpret_cast<const lxb_char_t *>(html.data()), html.size());
    if (status != LXB_STATUS_OK) {
        Reset();
        return false;
    }

    return true;
}

std::string HtmlDocument::Title() const {
    if (doc_ == nullptr) {
        return std::string();
    }

    size_t len = 0;
    const lxb_char_t *title = lxb_html_document_title(doc_, &len);
    if (title == nullptr) {
        return std::string();
    }

    return std::string(reinterpret_cast<const char *>(title), len);
}

lxb_dom_node *HtmlDocument::BodyNode() const {
    if (doc_ == nullptr) {
        return nullptr;
    }

    if (doc_->body != nullptr) {
        return lxb_dom_interface_node(doc_->body);
    }

    lxb_dom_element_t *root = lxb_dom_document_element(&doc_->dom_document);
    return root != nullptr ? lxb_dom_interface_node(root) : nullptr;
}

}  // namespace Core
