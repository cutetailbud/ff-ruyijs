#pragma once
#include "domain/document_model/dom_string.h"
#include <cstdint>
#include <exception>
#include <memory>

namespace ruyijs::domain::document_model {
enum class NodeType : uint16_t {
  Element = 1, Attribute = 2, Text = 3, CDataSection = 4, ProcessingInstruction = 7,
  Comment = 8, Document = 9, DocumentType = 10, DocumentFragment = 11
};
inline const char* node_type_name(NodeType type) {
  switch (type) {
    case NodeType::Element: return "Element"; case NodeType::Attribute: return "Attribute";
    case NodeType::Text: return "Text"; case NodeType::CDataSection: return "CDataSection";
    case NodeType::ProcessingInstruction: return "ProcessingInstruction";
    case NodeType::Comment: return "Comment"; case NodeType::Document: return "Document";
    case NodeType::DocumentType: return "DocumentType"; case NodeType::DocumentFragment: return "DocumentFragment";
  }
  return "Unknown";
}
using NodeId = uint64_t;
enum class DocumentMode { Html, Xml };
enum class DomErrorCode { NotFound, HierarchyRequest, InvalidCharacter, IndexSize, NotSupported,
                          Namespace, NoModification, InvalidState, LimitExceeded, Syntax };
class DomError final : public std::exception {
 public:
  DomError(DomErrorCode code, const char* text) noexcept : code(code), text_(text) {}
  const char* what() const noexcept override { return text_; }
  DomErrorCode code;
 private:
  const char* text_;
};
// name is the qualified name as written (what serialization and
// getAttribute(qualified) use). namespace_uri/prefix/local_name carry the
// namespace identity that getAttributeNS and the Attr wrapper expose; an empty
// namespace_uri means the null namespace.
struct Attribute {
  DomString name, value;
  DomString namespace_uri, prefix, local_name;
  bool operator==(const Attribute& o) const {
    return name == o.name && value == o.value && namespace_uri == o.namespace_uri &&
           prefix == o.prefix && local_name == o.local_name;
  }
};
class TagName {
 public:
  TagName() = default;
  explicit TagName(std::string name) : TagName(from_utf8(name), true) {}
  explicit TagName(DomString name, bool html = true, bool generic_unknown = false)
      : name_(html ? ascii_lower(name) : std::move(name)), html_(html), generic_unknown_(generic_unknown) {}
  // Only qualified names need extra storage; legacy HTML nodes allocate none.
  struct Qualified { DomString namespace_uri, prefix; };
  TagName(DomString local, DomStringView namespace_uri, DomStringView prefix, bool html_document)
      : name_(std::move(local)), qualified_(qualified_value(namespace_uri, prefix)),
        html_(namespace_uri == u"http://www.w3.org/1999/xhtml"),
        html_document_(html_document) {}
  std::string value() const { return to_utf8(value16()); }
  DomString value16() const {
    if (!qualified_ || qualified_->prefix.empty())
      return html_ && html_document_ ? ascii_upper(DomStringView(name_)) : name_;
    auto name = qualified_->prefix + u":" + name_;
    return html_ && html_document_ ? ascii_upper(DomStringView(name)) : name;
  }
  DomString qualified16() const {
    return qualified_ && !qualified_->prefix.empty() ? qualified_->prefix + u":" + name_ : name_;
  }
  DomStringView namespace16() const noexcept {
    return qualified_ ? DomStringView(qualified_->namespace_uri)
        : html_ ? DomStringView(u"http://www.w3.org/1999/xhtml") : DomStringView();
  }
  DomStringView prefix16() const noexcept { return qualified_ ? DomStringView(qualified_->prefix) : DomStringView(); }
  bool folds_case() const noexcept { return html_ && (!qualified_ || html_document_); }
  void set_document_html(bool value) noexcept { html_document_ = value; }
  size_t storage_units() const noexcept { return name_.size() +
      (qualified_ ? qualified_->namespace_uri.size() + qualified_->prefix.size() : 0); }
  const DomString& local16() const noexcept { return name_; }
  bool html() const noexcept { return html_; }
  bool generic_unknown() const noexcept { return generic_unknown_; }
  bool empty() const noexcept { return name_.empty(); }
  bool operator==(const TagName& other) const { return namespace16() == other.namespace16() &&
      prefix16() == other.prefix16() && name_ == other.name_; }
  bool operator!=(const TagName& other) const { return !(*this == other); }
  bool operator<(const TagName& other) const { return name_ < other.name_; }
  bool is_void() const {
    if (!html_) return false;
    const char16_t* names[] = {u"area",u"base",u"br",u"col",u"embed",u"hr",u"img",u"input",u"link",u"meta",u"param",u"source",u"track",u"wbr"};
    for (auto* name : names) if (name_ == name) return true;
    return false;
  }
  bool is_script() const { return html_ && name_ == u"script"; }
  bool is_style() const { return html_ && name_ == u"style"; }
 private:
  static std::shared_ptr<const Qualified> qualified_value(DomStringView space, DomStringView prefix) {
    if (prefix.empty()) {
      static const std::shared_ptr<const Qualified> common[] = {
        std::make_shared<const Qualified>(Qualified{}),
        std::make_shared<const Qualified>(Qualified{u"http://www.w3.org/1999/xhtml", {}}),
        std::make_shared<const Qualified>(Qualified{u"http://www.w3.org/2000/svg", {}}),
        std::make_shared<const Qualified>(Qualified{u"http://www.w3.org/1998/Math/MathML", {}}),
        std::make_shared<const Qualified>(Qualified{u"http://www.w3.org/XML/1998/namespace", {}}),
        std::make_shared<const Qualified>(Qualified{u"http://www.w3.org/2000/xmlns/", {}})
      };
      for (const auto& value : common) if (value->namespace_uri == space) return value;
    }
    return std::make_shared<const Qualified>(Qualified{DomString(space), DomString(prefix)});
  }
  DomString name_;
  std::shared_ptr<const Qualified> qualified_;
  bool html_ = true;
  bool generic_unknown_ = false;  // Public createDocument root uses Element for unknown tags.
  bool html_document_ = true;
};
}
