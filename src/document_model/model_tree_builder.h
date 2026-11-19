// The tree builder shared by model_tree_builder.cpp (html/head/body modes) and
// model_tree_table.cpp (the table insertion modes and foster parenting).
// Internal to the Document Model parser; see docs/DEVIATIONS.md D7.
#pragma once
#include "document_model/model_internal.h"
#include <algorithm>

namespace ruyijs::document_model::html::detail {
using domain::NodeId;
using Kind = Token::Kind;
inline bool Space(char16_t c) { return c == u'\t' || c == u'\n' || c == u'\f' || c == u' '; }
struct Builder {
  domain::DocumentAggregate& doc;
  TreeState& tree;
  const Budget& budget;
  NodeId Current() const { return tree.open.empty() ? tree.root : tree.open.back(); }
  // The parser's own change reports: one notice per inserted node, attribute
  // write and text growth, so a script that observes during a load sees the
  // same records DOM-API changes produce.
  // Nodes under an open template end up in its inert content: no notices.
  void Inserted(NodeId parent, const std::vector<NodeId>& added) const {
    if (tree.notice && tree.notice->inserted && tree.templates.empty())
      tree.notice->inserted(tree.notice->context, tree.notice->document, parent, added);
  }
  void Attribute(NodeId target, DomStringView name, const DomString* previous) const {
    if (tree.notice && tree.notice->attributes && tree.templates.empty())
      tree.notice->attributes(tree.notice->context, tree.notice->document, target, name, previous);
  }
  void Data(NodeId target, const DomString* previous) const {
    if (tree.notice && tree.notice->data && tree.templates.empty())
      tree.notice->data(tree.notice->context, tree.notice->document, target, previous);
  }
  // HTML elements only: the scope and end-tag rules never match a foreign name.
  DomStringView Name(NodeId id) const {
    const auto* element = doc.get_element(id);
    return element && element->tag_name().html() ? DomStringView(element->tag_name().local16()) : DomStringView();
  }
  TagSpace SpaceOf(NodeId id) const {
    const auto* element = doc.get_element(id);
    if (!element) return TagSpace::Html;
    if (id == tree.root) return tree.context_space;  // a fragment's context element
    const auto space = element->tag_name().namespace16();
    return space == SpaceUri(TagSpace::Svg) ? TagSpace::Svg : space == SpaceUri(TagSpace::MathMl) ? TagSpace::MathMl : TagSpace::Html;
  }
  // The tokenizer's flag: the current node is foreign and not an integration point.
  bool ForeignInput() const {
    const NodeId node = Current();
    const TagSpace space = SpaceOf(node);
    const auto* element = doc.get_element(node);
    return space != TagSpace::Html && !(element && node != tree.root &&
                                     HtmlIntegrationPoint(space, element->tag_name().local16()));
  }
  void InsertForeign(const Token& token, TagSpace space) {
    size_t units = token.name.size();
    for (const auto& attribute : token.attributes) units += attribute.name.size() + attribute.value.size();
    budget(1, units);
    const domain::TagName tag(ForeignTagName(space, token.name), SpaceUri(space), DomStringView(),
                              doc.uses_html_syntax());
    const NodeId id = doc.create_element(tag, false);
    auto* element = doc.get_element(id);
    for (const auto& attribute : token.attributes) {
      if (!domain::valid_attribute_local_name(attribute.name)) continue;
      DomString name;
      DomStringView space_uri;
      ForeignAttribute(space, attribute.name, &name, &space_uri);
      if (space_uri.empty()) element->set_attribute16(name, attribute.value);
      else element->set_attribute_ns16(space_uri, name, attribute.value);
      Attribute(id, name, nullptr);
    }
    // Insert() notes ids; foreign attributes are written directly.
    if (const auto ident = element->get_attribute16(u"id"))
      doc.note_element_id(id, {}, *ident);
    Place(id);
    if (!token.self_closing) tree.open.push_back(id);
  }
  // "Parsing tokens in foreign content"; false hands the token to the HTML rules.
  bool Foreign(const Token& token) {
    if (tree.open.empty() || token.kind == Kind::End) return false;
    const TagSpace space = SpaceOf(Current());
    if (space == TagSpace::Html) return false;
    // An integration point takes start tags and text by the HTML rules.
    if (!ForeignInput() && (token.kind == Kind::StartTag || token.kind == Kind::Text)) return false;
    if (token.kind == Kind::Text) { Text(token.data); return true; }
    if (token.kind == Kind::Comment) { Comment(token, Current()); return true; }
    if (token.kind == Kind::Doctype) return true;
    if (BreaksOutOfForeign(token)) {
      while (tree.open.size() > 1 && SpaceOf(Current()) != TagSpace::Html && ForeignInput()) tree.open.pop_back();
      return false;
    }
    if (token.kind == Kind::StartTag) { InsertForeign(token, space); return true; }
    // End tag: the nearest foreign element of that (ASCII case-insensitive) name.
    for (size_t i = tree.open.size(); i-- > 1;) {
      const NodeId id = tree.open[i];
      if (SpaceOf(id) == TagSpace::Html) return false;
      const auto* element = doc.get_element(id);
      if (element && Folded(element->tag_name().local16()) == token.name) { tree.open.resize(i); return true; }
    }
    return true;
  }
  // SVG foreignObject/desc/title and the MathML text points bound scopes and are special.
  bool Integration(NodeId id) const {
    const TagSpace space = SpaceOf(id);
    const auto* element = doc.get_element(id);
    return space != TagSpace::Html && element && id != tree.root &&
        HtmlIntegrationPoint(space, element->tag_name().local16());
  }
  static DomString Folded(DomStringView name) {
    DomString out(name);
    for (auto& c : out) if (c >= u'A' && c <= u'Z') c = char16_t(c + 32);
    return out;
  }
  NodeId Insert(const Token& token, bool push) {
    size_t units = token.name.size();
    for (const auto& attribute : token.attributes) units += attribute.name.size() + attribute.value.size();
    budget(1, units);
    NodeId id = doc.create_element16(token.name);
    auto* element = doc.get_element(id);
    if (element->tag_name().is_script()) {  // A script in template contents never runs.
      const bool runs = tree.scripts && tree.templates.empty();
      element->script_state().parser_inserted = runs;
      element->script_state().force_async = false;
      element->script_state().started = !runs;
    }
    for (const auto& attribute : token.attributes)  // Names the DOM rejects are dropped.
      if (domain::valid_attribute_local_name(attribute.name)) {
        element->set_attribute16(attribute.name, attribute.value);
        if (auto ident = element->get_attribute16(u"id"))
          doc.note_element_id(id, {}, *ident);
        Attribute(id, attribute.name, nullptr);
      }
    Place(id);
    if (push && !element->tag_name().is_void()) tree.open.push_back(id);
    if (push && token.name == u"template") tree.templates.push_back(id);
    tree.raw = push && ContentFor(token.name) != Content::Data;
    return id;
  }
  // The first raw text of a parser-created script records where it starts in
  // the document, so the script compiles with Firefox's line/column numbers.
  void ScriptSource(const Token& token) {
    auto* element = doc.get_element(Current());
    if (tree.source.empty() || token.data.empty() || !element || !element->tag_name().is_script() ||
        element->script_state().line) return;
    const size_t end = token.offset < tree.source.size() ? token.offset : tree.source.size();
    // document.write text is not part of the document's own lines.
    auto written = [&](size_t at) {
      for (const auto& range : tree.inserted)
        if (at >= range.first && at < range.first + range.second) return true;
      return false;
    };
    for (size_t at = tree.cursor < end ? tree.cursor : end; at < end; ++at)
      if (tree.source[at] == u'\n' && !written(at)) { ++tree.line; tree.line_start = at + 1; }
    if (end > tree.cursor) tree.cursor = end;
    // Ranges never overlap (Session::Write merges), so subtracting each one's
    // share of [line_start, end) leaves the document's own columns.
    size_t column = end - tree.line_start;
    for (const auto& range : tree.inserted) {
      const size_t from = range.first > tree.line_start ? range.first : tree.line_start;
      const size_t to = range.first + range.second < end ? range.first + range.second : end;
      if (to > from) column -= to - from;
    }
    element->script_state().line = tree.line;
    element->script_state().column = uint32_t(column + 1);
  }
  void Text(DomStringView data) {
    if (tree.skip_newline && !data.empty() && data[0] == u'\n') data.remove_prefix(1);
    tree.skip_newline = false;
    if (data.empty()) return;
    NodeId parent = Current(), before = 0;
    Location(&parent, &before);
    // Adjacent character tokens share one Text node: the node just before the
    // insertion point (the last child, or the fostered table's previous sibling).
    const auto& children = doc.require(parent).child_ids();
    NodeId previous = 0;
    if (!before) previous = children.empty() ? 0 : children.back();
    else for (size_t i = 0; i < children.size(); ++i) if (children[i] == before) { previous = i ? children[i - 1] : 0; break; }
    if (previous && doc.require(previous).node_type() == domain::NodeType::Text) {
      auto& last = doc.require(previous);
      budget(0, data.size());
      const DomString old = last.text_content16();
      last.set_node_value16(old + DomString(data));
      Data(previous, &old);
      return;
    }
    budget(1, data.size());
    const NodeId id = doc.create_text_node16(data);
    if (before) doc.insert_before(parent, id, before); else doc.append_child(parent, id);
    Inserted(parent, {id});
  }
  // The appropriate place for inserting a node: the current node, or the
  // foster-parent position while a table-mode token takes the body rules.
  void Location(NodeId* parent, NodeId* before) const { LocationFor(Current(), parent, before); }
  void LocationFor(NodeId target, NodeId* parent, NodeId* before) const;
  void Place(NodeId id) {
    NodeId parent = Current(), before = 0;
    Location(&parent, &before);
    if (before) doc.insert_before(parent, id, before); else doc.append_child(parent, id);
    Inserted(parent, {id});
  }
  // Table modes (model_tree_table.cpp). Table() handles one token in a table
  // mode; false hands it to the body rules (already foster-flagged if needed).
  bool InTableScope(std::initializer_list<const char16_t*> names) const;
  void ClearBackTo(std::initializer_list<const char16_t*> names);
  void ResetMode();
  bool Table(Token& token);
  bool BodyTableTag(Token& token);
  // Active formatting elements (model_tree_formatting.cpp).
  void Reconstruct();
  void PushFormatting(NodeId id, const Token& token);
  void Marker() { tree.formatting.push_back({}); }
  void ClearToMarker();
  // The adoption agency algorithm for an end tag; false: the "any other end tag" rules apply.
  bool Adoption(DomStringView subject);
  bool FormattingStart(Token& token);
  void Comment(const Token& token, NodeId parent) {
    budget(1, token.data.size());
    const NodeId id = doc.create_comment16(token.data);
    doc.append_child(parent, id);
    Inserted(parent, {id});
  }
  void Merge(NodeId target, const Token& token) {  // Repeated <html>/<body> add new attributes only.
    auto* element = doc.get_element(target);
    for (const auto& attribute : token.attributes)
      if (element && domain::valid_attribute_local_name(attribute.name) && !element->get_attribute16(attribute.name)) {
        budget(0, attribute.name.size() + attribute.value.size());
        const auto previous_id = element->get_attribute16(u"id");
        element->set_attribute16(attribute.name, attribute.value);
        const auto next_id = element->get_attribute16(u"id");
        // A repeated <html>/<body> can add an id ahead of an element that
        // already holds it. That element is already in the tree.
        if (previous_id != next_id)
          doc.note_element_id(target, previous_id ? DomStringView(*previous_id) : DomStringView(),
                              next_id ? DomStringView(*next_id) : DomStringView());
        Attribute(target, attribute.name, nullptr);
      }
  }
  bool InScope(DomStringView target, bool button) const {
    for (auto it = tree.open.rbegin(); it != tree.open.rend(); ++it) {
      auto name = Name(*it);
      if (name == target) return true;
      if (Boundary(name) || Integration(*it) || (button && name == u"button")) return false;
    }
    return false;
  }
  void PopUntil(DomStringView target) {
    for (bool done = false; !done && !tree.open.empty(); tree.open.pop_back()) done = Name(tree.open.back()) == target;
  }
  void Close(DomStringView target) {  // Generate implied end tags, then pop through target.
    DomString found(target);
    while (Name(Current()) != found && Implied(Name(Current()))) tree.open.pop_back();
    PopUntil(found);
  }
  void CloseP() { if (InScope(u"p", true)) Close(u"p"); }
  void CloseItem(std::initializer_list<const char16_t*> names) {  // li, or dd/dt
    for (auto it = tree.open.rbegin(); it != tree.open.rend(); ++it) {
      auto name = Name(*it);
      if (In(name, names)) { DomString found(name); PopUntil(found); return; }
      if ((Special(name) || Integration(*it)) && !In(name, {u"address", u"div", u"p"})) return;
    }
  }
  void EndOther(DomStringView target) {
    for (auto it = tree.open.rbegin(); it != tree.open.rend(); ++it) {
      auto name = Name(*it);
      if (name == target) return Close(target);
      if (Special(name) || Integration(*it)) return;  // Unmatched end tags are ignored.
    }
  }
  // Leading whitespace goes to the current node (keep) or is dropped.
  bool Leading(Token& token, bool keep) {
    size_t n = 0;
    while (n < token.data.size() && Space(token.data[n])) ++n;
    if (keep) Text(DomStringView(token.data).substr(0, n));
    token.data.erase(0, n);
    return !token.data.empty();
  }
  void Open(DomStringView name, Mode next, NodeId* slot) {  // Implied html/head/body.
    Token token{Kind::StartTag, DomString(name)};
    *slot = Insert(token, true);
    tree.mode = next;
  }
  bool BeforeBody(Token& token) {  // true: reprocess in the new mode
    const Mode mode = tree.mode;
    const bool head = mode == Mode::InHead || mode == Mode::AfterHead;
    if (token.kind == Kind::Doctype) {
      if (mode == Mode::BeforeHtml && doc.require(tree.root).node_type() == domain::NodeType::Document &&
          !doc.parsed_doctype().node) {
        budget(1, token.name.size() + token.public_id.size() + token.system_id.size());
        const NodeId id = doc.create_document_type16(token.name, token.doctype_cloneable, token.public_id, token.system_id);
        doc.append_child(tree.root, id);
        doc.set_parsed_doctype({0, id});
        Inserted(tree.root, {id});
      }
      return false;
    }
    if (token.kind == Kind::Comment) { Comment(token, Current()); return false; }
    if (token.kind == Kind::Text && !Leading(token, head)) return false;
    const bool start = token.kind == Kind::StartTag, end = token.kind == Kind::EndTag;
    const DomStringView name = token.name;
    if (mode == Mode::InHead && !tree.templates.empty()) {  // "in template" (head content, else body rules)
      if (token.kind == Kind::End) {  // End of input closes the templates, then the head.
        tree.open.erase(std::find(tree.open.begin(), tree.open.end(), tree.templates.front()), tree.open.end());
      } else {
        if (end && name == u"template") EndOther(name);
        else if (start && HeadContent(name)) Insert(token, !HeadVoid(name));
        else if (!(end && In(name, {u"head", u"body", u"html"}))) InBody(token);
        return false;
      }
    }
    if (start && name == u"html") {
      if (mode != Mode::BeforeHtml) { Merge(tree.html, token); return false; }
      tree.html = Insert(token, true);
      tree.mode = Mode::BeforeHead;
      return false;
    }
    if (end && !In(name, {u"head", u"body", u"html", u"br"}) && (mode != Mode::InHead || name != u"template"))
      return false;
    if (mode == Mode::BeforeHtml) { Open(u"html", Mode::BeforeHead, &tree.html); return true; }
    if (start && name == u"head") {  // Explicit tags keep their attributes.
      if (mode == Mode::BeforeHead) { tree.head = Insert(token, true); tree.mode = Mode::InHead; }
      return false;
    }
    if (mode == Mode::BeforeHead) { Open(u"head", Mode::InHead, &tree.head); return true; }
    if (start && HeadContent(name)) {
      if (mode == Mode::AfterHead) tree.open.push_back(tree.head);
      Insert(token, !HeadVoid(name));
      if (mode == Mode::AfterHead) std::erase(tree.open, tree.head);
      return false;
    }
    if (mode == Mode::InHead) {
      if (end && name == u"template") { EndOther(name); return false; }
      if (Name(Current()) == u"head") tree.open.pop_back();
      tree.mode = Mode::AfterHead;
      return !(end && name == u"head");
    }
    if (start && name == u"body") { tree.body = Insert(token, true); tree.mode = Mode::InBody; return false; }
    Open(u"body", Mode::InBody, &tree.body);
    return true;
  }
  void InBody(Token& token) {
    if (token.kind == Kind::StartTag && token.name == u"image") token.name = u"img";
    const DomStringView name = token.name;
    if (token.kind == Kind::Text) { Reconstruct(); Text(token.data); return; }
    if (token.kind == Kind::Comment) { Comment(token, Current()); return; }
    if (token.kind == Kind::StartTag) {
      if (BodyTableTag(token)) return;
      if (FormattingStart(token)) return;
      if (name == u"html" || name == u"body") { Merge(name == u"html" ? tree.html : tree.body, token); return; }
      if (name == u"head") return;
      if (name == u"svg" || name == u"math") { InsertForeign(token, name == u"svg" ? TagSpace::Svg : TagSpace::MathMl); return; }
      if (ClosesP(name)) CloseP();
      if (Heading(name) && Heading(Name(Current()))) tree.open.pop_back();
      if (name == u"li") { CloseItem({u"li"}); CloseP(); }
      if (name == u"dd" || name == u"dt") { CloseItem({u"dd", u"dt"}); CloseP(); }
      if (name == u"button" && InScope(name, false)) PopUntil(name);
      // Block-level and list starts do not reconstruct the formatting elements.
      if (!ClosesP(name) && !Heading(name) && !HeadContent(name) &&
          !In(name, {u"li", u"dd", u"dt", u"form", u"textarea", u"iframe", u"noembed", u"noscript", u"template"}))
        Reconstruct();
      Insert(token, true);
      if (In(name, {u"applet", u"marquee", u"object", u"template"})) Marker();
      tree.skip_newline = In(name, {u"pre", u"listing", u"textarea"});
      return;
    }
    if (token.kind != Kind::EndTag) return;
    if (name == u"body" || name == u"html") { if (InScope(u"body", false)) tree.mode = Mode::AfterBody; return; }
    if (name == u"br") { token.kind = Kind::StartTag; Insert(token, false); return; }
    if (name == u"p" && !InScope(name, true)) { Token p{Kind::StartTag, u"p"}; Insert(p, true); }
    if (Heading(name)) {
      for (auto it = tree.open.rbegin(); it != tree.open.rend() && !Boundary(Name(*it)); ++it)
        if (Heading(Name(*it))) { DomString found(Name(*it)); PopUntil(found); return; }
      return;
    }
    if (Adoption(name)) return;
    // Block, list and form end tags close their element in scope; others use the generic rule.
    if (Special(name)) {
      if (InScope(name, false)) {
        Close(name);
        if (In(name, {u"applet", u"marquee", u"object", u"template"})) ClearToMarker();
      }
      return;
    }
    EndOther(name);
  }
};
}  // namespace ruyijs::document_model::html::detail
