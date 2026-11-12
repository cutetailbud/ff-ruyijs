// The list of active formatting elements and the adoption agency algorithm
// (WHATWG 13.2.4.3, 13.2.6.4.7 "in body" formatting tags).
#include "document_model/model_tree_builder.h"

namespace ruyijs::document_model::html::detail {
namespace {
bool FormattingTag(DomStringView n) {
  return In(n, {u"a", u"b", u"big", u"code", u"em", u"font", u"i", u"nobr", u"s", u"small", u"strike",
                u"strong", u"tt", u"u"});
}
bool OnStack(const TreeState& tree, NodeId id) {
  return std::find(tree.open.begin(), tree.open.end(), id) != tree.open.end();
}
long IndexIn(const TreeState& tree, NodeId id) {
  for (size_t i = 0; i < tree.formatting.size(); ++i) if (tree.formatting[i].id == id) return long(i);
  return -1;
}
bool SameAttributes(const Token& a, const Token& b) {
  if (a.attributes.size() != b.attributes.size()) return false;
  for (const auto& x : a.attributes) {
    bool found = false;
    for (const auto& y : b.attributes) if (x.name == y.name && x.value == y.value) { found = true; break; }
    if (!found) return false;
  }
  return true;
}
// An element for `token`, not yet inserted (the algorithm places it).
NodeId CreateDetached(Builder& b, const Token& token) {
  size_t units = token.name.size();
  for (const auto& attribute : token.attributes) units += attribute.name.size() + attribute.value.size();
  b.budget(1, units);
  const NodeId id = b.doc.create_element16(token.name);
  auto* element = b.doc.get_element(id);
  for (const auto& attribute : token.attributes)
    if (domain::valid_attribute_local_name(attribute.name)) {
      element->set_attribute16(attribute.name, attribute.value);
      b.Attribute(id, attribute.name, nullptr);
    }
  // Adoption creates this element without Insert(), which is what notes ids.
  if (const auto ident = element->get_attribute16(u"id"))
    b.doc.note_element_id(id, {}, *ident);
  return id;
}
void Move(Builder& b, NodeId parent, NodeId child, NodeId before = 0) {
  if (before) b.doc.insert_before(parent, child, before); else b.doc.append_child(parent, child);
  b.Inserted(parent, {child});
}
// "has an element in scope" for one particular element.
bool ElementInScope(const Builder& b, NodeId target) {
  for (auto it = b.tree.open.rbegin(); it != b.tree.open.rend(); ++it) {
    if (*it == target) return true;
    if (Boundary(b.Name(*it)) || b.Integration(*it)) return false;
  }
  return false;
}
}  // namespace

void Builder::Reconstruct() {
  auto& list = tree.formatting;
  if (list.empty() || !list.back().id || OnStack(tree, list.back().id)) return;
  size_t i = list.size() - 1;
  while (i > 0 && list[i - 1].id && !OnStack(tree, list[i - 1].id)) --i;
  for (; i < list.size(); ++i) {
    Token token = list[i].token;
    list[i].id = Insert(token, true);
  }
}
void Builder::PushFormatting(NodeId id, const Token& token) {
  // Noah's Ark: at most three equal entries after the last marker.
  auto& list = tree.formatting;
  size_t equal = 0, earliest = list.size();
  for (size_t i = list.size(); i-- > 0 && list[i].id;)
    if (list[i].token.name == token.name && SameAttributes(list[i].token, token)) { ++equal; earliest = i; }
  if (equal >= 3) list.erase(list.begin() + long(earliest));
  list.push_back({id, token});
}
void Builder::ClearToMarker() {
  while (!tree.formatting.empty()) {
    const bool marker = !tree.formatting.back().id;
    tree.formatting.pop_back();
    if (marker) return;
  }
}
bool Builder::Adoption(DomStringView subject) {
  if (!FormattingTag(subject)) return false;
  auto& list = tree.formatting;
  auto& open = tree.open;
  if (Name(Current()) == subject && IndexIn(tree, Current()) < 0) { open.pop_back(); return true; }
  for (int outer = 0; outer < 8; ++outer) {
    long fi = -1;
    for (long k = long(list.size()) - 1; k >= 0 && list[size_t(k)].id; --k)
      if (Name(list[size_t(k)].id) == subject) { fi = k; break; }
    if (fi < 0) { EndOther(subject); return true; }
    const NodeId formatting = list[size_t(fi)].id;
    const auto at = std::find(open.begin(), open.end(), formatting);
    if (at == open.end()) { list.erase(list.begin() + fi); return true; }
    if (!ElementInScope(*this, formatting)) return true;
    const size_t fePos = size_t(at - open.begin());
    size_t fb = open.size();
    for (size_t k = fePos + 1; k < open.size(); ++k) if (Special(Name(open[k]))) { fb = k; break; }
    if (fb == open.size()) { open.resize(fePos); list.erase(list.begin() + fi); return true; }
    const NodeId furthest = open[fb];
    const NodeId common = fePos ? open[fePos - 1] : tree.root;
    size_t bookmark = size_t(fi);
    NodeId last = furthest;
    size_t index = fb;
    for (int inner = 1;; ++inner) {
      --index;
      NodeId node = open[index];
      if (node == formatting) break;
      long ni = IndexIn(tree, node);
      if (inner > 3 && ni >= 0) { list.erase(list.begin() + ni); if (size_t(ni) < bookmark) --bookmark; ni = -1; }
      if (ni < 0) { open.erase(open.begin() + long(index)); continue; }
      const Token token = list[size_t(ni)].token;
      const NodeId fresh = CreateDetached(*this, token);
      list[size_t(ni)].id = fresh;
      open[index] = fresh;
      if (last == furthest) bookmark = size_t(ni) + 1;
      Move(*this, fresh, last);
      last = fresh;
    }
    NodeId parent = common, before = 0;
    LocationFor(common, &parent, &before);
    Move(*this, parent, last, before);
    fi = IndexIn(tree, formatting);
    const Token token = list[size_t(fi)].token;
    const NodeId fresh = CreateDetached(*this, token);
    const std::vector<NodeId> children = doc.require(furthest).child_ids();
    for (NodeId child : children) doc.append_child(fresh, child);
    Move(*this, furthest, fresh);
    list.erase(list.begin() + fi);
    if (bookmark > size_t(fi)) --bookmark;
    list.insert(list.begin() + long(std::min(bookmark, list.size())), {fresh, token});
    open.erase(std::find(open.begin(), open.end(), formatting));
    open.insert(std::find(open.begin(), open.end(), furthest) + 1, fresh);
  }
  return true;
}
bool Builder::FormattingStart(Token& token) {
  const DomStringView name = token.name;
  if (!FormattingTag(name)) return false;
  if (name == u"a") {
    for (size_t k = tree.formatting.size(); k-- > 0 && tree.formatting[k].id;) {
      if (Name(tree.formatting[k].id) != u"a") continue;
      const NodeId previous = tree.formatting[k].id;
      Adoption(u"a");
      const long left = IndexIn(tree, previous);
      if (left >= 0) tree.formatting.erase(tree.formatting.begin() + left);
      std::erase(tree.open, previous);
      break;
    }
  }
  Reconstruct();
  if (name == u"nobr" && InScope(u"nobr", false)) { Adoption(u"nobr"); Reconstruct(); }
  const NodeId id = Insert(token, true);
  PushFormatting(id, token);
  return true;
}
}  // namespace ruyijs::document_model::html::detail
