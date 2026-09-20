#include "Core/Layout.h"

#include <cctype>

#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/node.h"
#include "lexbor/dom/interfaces/text.h"
#include "lexbor/tag/tag.h"

namespace Core {
namespace {

constexpr float kMarginX = 8.0f;
constexpr float kMarginTop = 8.0f;
constexpr float kListIndent = 16.0f;
constexpr float kBlockGapFactor = 0.4f;  // extra breathing room before a block, as a fraction of a line

struct InlineStyle {
    float scale = 0.5f;
    bool bold = false;
    RunKind kind = RunKind::Text;
    std::string href;
};

// Elements whose entire subtree should be skipped: not "rendered", per the
// HTML spec (script/style), or handled separately (head/title).
bool IsSkippedSubtree(lxb_tag_id_t tag) {
    switch (tag) {
        case LXB_TAG_SCRIPT:
        case LXB_TAG_STYLE:
        case LXB_TAG_HEAD:
        case LXB_TAG_TITLE:
        case LXB_TAG_TEMPLATE:
        case LXB_TAG_NOSCRIPT:
            return true;
        default:
            return false;
    }
}

// Elements that start on their own line. This is a text-flow approximation
// of CSS `display: block`, not a real box model.
bool IsBlock(lxb_tag_id_t tag) {
    switch (tag) {
        case LXB_TAG_P:
        case LXB_TAG_DIV:
        case LXB_TAG_SECTION:
        case LXB_TAG_ARTICLE:
        case LXB_TAG_HEADER:
        case LXB_TAG_FOOTER:
        case LXB_TAG_NAV:
        case LXB_TAG_MAIN:
        case LXB_TAG_ASIDE:
        case LXB_TAG_UL:
        case LXB_TAG_OL:
        case LXB_TAG_LI:
        case LXB_TAG_BLOCKQUOTE:
        case LXB_TAG_PRE:
        case LXB_TAG_TABLE:
        case LXB_TAG_TR:
        case LXB_TAG_H1:
        case LXB_TAG_H2:
        case LXB_TAG_H3:
        case LXB_TAG_H4:
        case LXB_TAG_H5:
        case LXB_TAG_H6:
            return true;
        default:
            return false;
    }
}

// 0.0f means "not a heading".
float HeadingScale(lxb_tag_id_t tag) {
    switch (tag) {
        case LXB_TAG_H1: return 1.1f;
        case LXB_TAG_H2: return 0.95f;
        case LXB_TAG_H3: return 0.85f;
        case LXB_TAG_H4: return 0.75f;
        case LXB_TAG_H5: return 0.68f;
        case LXB_TAG_H6: return 0.6f;
        default: return 0.0f;
    }
}

std::string TextOf(lxb_dom_node *textNode) {
    lexbor_str_t &str = lxb_dom_interface_text(textNode)->char_data.data;
    return std::string(reinterpret_cast<const char *>(str.data), str.length);
}

std::string HrefOf(lxb_dom_node *anchorNode) {
    static const lxb_char_t kHref[] = "href";
    size_t len = 0;
    const lxb_char_t *value = lxb_dom_element_get_attribute(
        lxb_dom_interface_element(anchorNode), kHref, 4, &len);
    if (value == nullptr) {
        return std::string();
    }
    return std::string(reinterpret_cast<const char *>(value), len);
}

// Walks the DOM and produces a flat, wrapped stream of positioned words.
// This owns all the layout state (cursor position, current line height) so
// Layout.h's public surface can stay a single free function.
class FlowLayoutEngine {
public:
    FlowLayoutEngine(float contentWidth, ITextMeasurer &measurer)
        : contentWidth_(contentWidth), measurer_(measurer) {}

    LayoutResult Run(lxb_dom_node *root) {
        x_ = kMarginX;
        y_ = kMarginTop;
        if (root != nullptr) {
            WalkChildren(root, InlineStyle{}, 0);
        }
        AdvanceLineIfPending();
        result_.contentHeight = y_ + kMarginTop;
        return std::move(result_);
    }

private:
    float MarginForDepth(int listDepth) const { return kMarginX + listDepth * kListIndent; }

    void WalkChildren(lxb_dom_node *parent, const InlineStyle &style, int listDepth) {
        for (lxb_dom_node *child = parent->first_child; child != nullptr; child = child->next) {
            if (child->type == LXB_DOM_NODE_TYPE_TEXT) {
                EmitText(TextOf(child), style, listDepth);
                continue;
            }
            if (child->type != LXB_DOM_NODE_TYPE_ELEMENT) {
                continue;
            }

            auto tag = static_cast<lxb_tag_id_t>(child->local_name);
            if (IsSkippedSubtree(tag)) {
                continue;
            }
            if (tag == LXB_TAG_BR) {
                AdvanceLineIfPending();
                x_ = MarginForDepth(listDepth);
                continue;
            }

            InlineStyle childStyle = style;
            float headingScale = HeadingScale(tag);
            if (headingScale > 0.0f) {
                childStyle.scale = headingScale;
                childStyle.bold = true;
                childStyle.kind = RunKind::Heading;
            }
            if (tag == LXB_TAG_B || tag == LXB_TAG_STRONG) {
                childStyle.bold = true;
            }
            if (tag == LXB_TAG_A) {
                std::string href = HrefOf(child);
                if (!href.empty()) {
                    childStyle.kind = RunKind::Link;
                    childStyle.href = std::move(href);
                }
            }

            int childListDepth = listDepth;
            if (tag == LXB_TAG_UL || tag == LXB_TAG_OL) {
                childListDepth = listDepth + 1;
            }

            bool isBlock = IsBlock(tag);
            if (isBlock) {
                StartBlock(childStyle, listDepth);
            }
            if (tag == LXB_TAG_LI) {
                InlineStyle markerStyle = childStyle;
                markerStyle.kind = RunKind::ListMarker;
                EmitText("\xE2\x80\xA2", markerStyle, childListDepth);
            }

            WalkChildren(child, childStyle, childListDepth);
        }
    }

    // Flushes any pending line and adds a bit of paragraph spacing, then
    // resets the cursor to the left margin for `listDepth`. Called once per
    // block-level element, right before laying out its content.
    void StartBlock(const InlineStyle &style, int listDepth) {
        AdvanceLineIfPending();
        y_ += measurer_.LineHeight(TextStyle{style.scale, style.bold}) * kBlockGapFactor;
        x_ = MarginForDepth(listDepth);
    }

    void AdvanceLineIfPending() {
        if (x_ <= kMarginX && !lineHasContent_) {
            return;
        }
        y_ += currentLineHeight_ > 0.0f ? currentLineHeight_
                                         : measurer_.LineHeight(TextStyle{});
        currentLineHeight_ = 0.0f;
        lineHasContent_ = false;
    }

    void EmitText(const std::string &text, const InlineStyle &style, int listDepth) {
        const float marginX = MarginForDepth(listDepth);
        const TextStyle measureStyle{style.scale, style.bold};
        const float spaceWidth = measurer_.SpaceWidth(measureStyle);
        const float lineHeight = measurer_.LineHeight(measureStyle);

        size_t i = 0;
        while (i < text.size()) {
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
                ++i;
            }
            size_t wordStart = i;
            while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) {
                ++i;
            }
            if (wordStart == i) {
                break;
            }

            std::string word = text.substr(wordStart, i - wordStart);
            float wordWidth = measurer_.MeasureWord(word, measureStyle);
            bool needsSpace = lineHasContent_ && x_ > marginX;
            float advance = needsSpace ? spaceWidth : 0.0f;
            const float rightEdge = contentWidth_ - kMarginX;

            if (x_ + advance + wordWidth > rightEdge && x_ > marginX) {
                AdvanceLineIfPending();
                x_ = marginX;
                needsSpace = false;
                advance = 0.0f;
            }

            x_ += advance;

            LaidWord laid;
            laid.text = std::move(word);
            laid.x = x_;
            laid.y = y_;
            laid.width = wordWidth;
            laid.height = lineHeight;
            laid.scale = style.scale;
            laid.bold = style.bold;
            laid.kind = style.kind;
            laid.href = style.href;
            result_.words.push_back(std::move(laid));

            x_ += wordWidth;
            lineHasContent_ = true;
            currentLineHeight_ = currentLineHeight_ > lineHeight ? currentLineHeight_ : lineHeight;
        }
    }

    float contentWidth_;
    ITextMeasurer &measurer_;
    LayoutResult result_;

    float x_ = kMarginX;
    float y_ = kMarginTop;
    float currentLineHeight_ = 0.0f;
    bool lineHasContent_ = false;
};

}  // namespace

LayoutResult BuildLayout(const HtmlDocument &document, float contentWidth,
                          ITextMeasurer &measurer) {
    FlowLayoutEngine engine(contentWidth, measurer);
    return engine.Run(document.BodyNode());
}

std::string HitTestLink(const LayoutResult &layout, float x, float y) {
    for (const LaidWord &word : layout.words) {
        if (word.kind != RunKind::Link) {
            continue;
        }
        if (x >= word.x && x <= word.x + word.width && y >= word.y &&
            y <= word.y + word.height) {
            return word.href;
        }
    }
    return std::string();
}

}  // namespace Core
