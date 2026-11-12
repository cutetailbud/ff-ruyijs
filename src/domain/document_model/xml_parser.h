#ifndef RUYIJS_DOMAIN_DOCUMENT_MODEL_XML_PARSER_H
#define RUYIJS_DOMAIN_DOCUMENT_MODEL_XML_PARSER_H
// The XML 1.0 + Namespaces parser DOMParser uses for its XML media types
// (docs/owners/DOCUMENT_MODEL.md, 2026-10-22), replacing the HTML-syntax
// stand-in recorded as D23. It builds the tree of one DocumentAggregate; a
// failure is an XmlError carrying expat's message and position, which the
// caller turns into Firefox's parsererror document (xml_parsererror.h).
// Not represented (no node type yet, D138): CDATA becomes a Text node.
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "domain/document_model/document.h"
#include "domain/document_model/xml_markup.h"
#include "domain/document_model/xml_namespaces.h"

namespace ruyijs::domain::document_model {
class XmlParser {
 public:
  using Budget = std::function<void(size_t nodes, size_t units)>;
  XmlParser(DocumentAggregate& document, std::u16string_view input, Budget budget)
      : doc_(document), r_(input), markup_(r_), budget_(std::move(budget)) {}
  void Parse() {
    if (r_.starts(u"<?xml") && Declaration(5)) Skip(u"?>");
    while (!r_.done()) {
      const char16_t c = r_.peek();
      if (c == u'<') Markup();
      else if (c == u'&' && !open_.empty()) markup_.Reference(&text_, false);
      else Character(c);
    }
    if (!root_ || !open_.empty()) XmlReader::Fail(u"no root element found", r_.pos);
  }
  const std::u16string& text() const noexcept { return r_.text(); }

 private:
  struct Open { NodeId id; std::u16string qname; size_t bindings; };
  bool Declaration(size_t at) const noexcept {
    const char16_t c = r_.peek(at);
    return XmlReader::Space(c) || c == u'?';
  }
  void Skip(std::u16string_view end) {
    const size_t found = r_.text().find(end, r_.pos);
    if (found == std::u16string::npos) XmlReader::Fail(u"unclosed token", r_.pos);
    r_.pos = found + end.size();
  }
  NodeId Parent() const noexcept { return open_.empty() ? doc_.id() : open_.back().id; }
  void Append(NodeId node, size_t units) {
    budget_(1, units);
    doc_.append_child(Parent(), node);
  }
  void Flush() {
    if (text_.empty()) return;
    Append(doc_.create_text_node16(text_), text_.size());
    text_.clear();
  }
  // Character data: inside the root it is text ("]]>" is not allowed);
  // outside it only whitespace may appear.
  void Character(char16_t c) {
    if (open_.empty()) {
      if (!XmlReader::Space(c))
        XmlReader::Fail(root_ ? u"junk after document element" : u"syntax error", r_.pos);
      ++r_.pos;
      return;
    }
    if (c == u']' && r_.starts(u"]]>")) XmlReader::Fail(u"not well-formed", r_.pos + 2);
    text_.push_back(c);
    ++r_.pos;
  }
  void Markup() {
    Flush();
    const size_t at = r_.pos;
    if (r_.starts(u"<!--")) {
      const size_t end = r_.text().find(u"-->", at + 4);
      if (end == std::u16string::npos) XmlReader::Fail(u"unclosed token", at);
      Append(doc_.create_comment16(std::u16string_view(r_.text()).substr(at + 4, end - at - 4)), end - at);
      r_.pos = end + 3;
    } else if (r_.starts(u"<![CDATA[")) {
      if (open_.empty()) XmlReader::Fail(root_ ? u"junk after document element" : u"syntax error", at);
      const size_t end = r_.text().find(u"]]>", at + 9);
      if (end == std::u16string::npos) XmlReader::Fail(u"unclosed CDATA section", at);
      Append(doc_.create_text_node16(std::u16string_view(r_.text()).substr(at + 9, end - at - 9)), end - at);
      r_.pos = end + 3;
    } else if (r_.starts(u"<!DOCTYPE")) {
      if (root_ || doctype_) XmlReader::Fail(u"syntax error", at);
      doctype_ = true;
      const XmlDoctype type = markup_.Doctype();
      const NodeId node = doc_.create_document_type16(type.name, true, type.public_id, type.system_id);
      Append(node, type.name.size());
      doc_.set_parsed_doctype({0, node});  // document.doctype, as the HTML tree builder records it
    } else if (r_.starts(u"<?")) {
      if ((r_.starts(u"<?xml") || r_.starts(u"<?XML")) && Declaration(5))
        XmlReader::Fail(u"XML or text declaration not at start of entity", at);
      r_.pos += 2;
      // A processing instruction: the target, then (after the separating
      // whitespace) the data up to "?>", trailing whitespace kept.
      const std::u16string target = r_.Name();
      r_.SkipSpace();
      const size_t data_start = r_.pos;
      Skip(u"?>");
      std::u16string data(r_.text().substr(data_start, r_.pos - data_start - 2));
      Append(doc_.create_processing_instruction16(target, data), target.size() + data.size());
    } else if (r_.starts(u"</")) {
      End();
    } else {
      if (root_ && open_.empty()) XmlReader::Fail(u"junk after document element", at);
      Start(at);
    }
  }
  void End() {
    r_.pos += 2;
    const size_t at = r_.pos;
    const std::u16string name = r_.Name();
    if (open_.empty()) XmlReader::Fail(u"not well-formed", at);
    if (name != open_.back().qname)
      XmlReader::Fail((u"mismatched tag. Expected: </" + open_.back().qname + u">.").c_str(), at);
    r_.SkipSpace();
    if (r_.peek() != u'>') XmlReader::Fail(u"not well-formed", r_.pos);
    ++r_.pos;
    bindings_.resize(open_.back().bindings);
    open_.pop_back();
  }
  struct Attribute { std::u16string name, value; };
  void Start(size_t at) {
    ++r_.pos;
    const std::u16string qname = r_.Name();
    std::vector<Attribute> attributes;
    for (;;) {
      const bool space = r_.SkipSpace();
      if (r_.starts(u"/>") || r_.peek() == u'>') break;
      if (r_.done()) XmlReader::Fail(u"unclosed token", at);
      if (!space) XmlReader::Fail(u"not well-formed", r_.pos);
      const size_t name_at = r_.pos;
      Attribute attribute{r_.Name(), {}};
      for (const auto& other : attributes)
        if (other.name == attribute.name) XmlReader::Fail(u"duplicate attribute", name_at);
      r_.SkipSpace();
      if (r_.peek() != u'=') XmlReader::Fail(u"not well-formed", r_.pos);
      ++r_.pos;
      r_.SkipSpace();
      attribute.value = markup_.AttributeValue();
      attributes.push_back(std::move(attribute));
    }
    const bool empty = r_.peek() == u'/';
    r_.pos += empty ? 2 : 1;
    const size_t scope = bindings_.size();
    for (const auto& a : attributes) {
      if (a.name == u"xmlns") bindings_.emplace_back(u"", a.value);
      else if (a.name.rfind(u"xmlns:", 0) == 0) {
        if (a.value.empty()) XmlReader::Fail(u"must not undeclare prefix", at);
        bindings_.emplace_back(a.name.substr(6), a.value);
      }
    }
    const NodeId element = CreateElement(qname, at);
    Element* node = doc_.get_element(element);
    size_t units = qname.size();
    for (const auto& a : attributes) {
      units += a.name.size() + a.value.size();
      std::u16string ns;
      if (a.name == u"xmlns" || a.name.rfind(u"xmlns:", 0) == 0) ns = DomString(kXmlnsNamespace);
      else if (a.name.find(u':') != std::u16string::npos) ns = Resolve(a.name.substr(0, a.name.find(u':')), at);
      node->set_attribute_ns16(ns, a.name, a.value);
    }
    // HTML Insert() notes ids; this parser writes attributes directly.
    if (const auto ident = node->get_attribute16(u"id"))
      doc_.note_element_id(element, {}, *ident);
    Append(element, units);
    root_ = true;
    if (empty) bindings_.resize(scope);
    else open_.push_back({element, qname, scope});
  }
  NodeId CreateElement(const std::u16string& qname, size_t at) {
    const size_t colon = qname.find(u':');
    const std::u16string prefix = colon == std::u16string::npos ? u"" : qname.substr(0, colon);
    const std::u16string local = colon == std::u16string::npos ? qname : qname.substr(colon + 1);
    const std::u16string ns = prefix.empty() ? Lookup(u"") : Resolve(prefix, at);
    return doc_.create_element(TagName(local, ns, prefix, false));
  }
  std::u16string Lookup(const std::u16string& prefix) const {
    if (prefix == u"xml") return DomString(kXmlNamespace);
    for (size_t i = bindings_.size(); i > 0; --i)
      if (bindings_[i - 1].first == prefix) return bindings_[i - 1].second;
    return {};
  }
  std::u16string Resolve(const std::u16string& prefix, size_t at) const {
    std::u16string ns = Lookup(prefix);
    if (ns.empty()) XmlReader::Fail(u"prefix not bound to a namespace", at);
    return ns;
  }
  DocumentAggregate& doc_;
  XmlReader r_;
  XmlMarkup markup_;
  Budget budget_;
  std::vector<Open> open_;
  std::vector<std::pair<std::u16string, std::u16string>> bindings_;
  std::u16string text_;
  bool root_ = false, doctype_ = false;
};
}  // namespace ruyijs::domain::document_model
#endif
