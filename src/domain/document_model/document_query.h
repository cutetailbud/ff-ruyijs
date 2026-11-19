#pragma once
#include "domain/document_model/document.h"

namespace ruyijs::domain::document_model {
inline DomString normalize_title_text(DomString value) {
  size_t written = 0;
  bool pending_space = false;
  for (char16_t code : value) {
    const bool space = code == u'\t' || code == u'\n' || code == u'\f' ||
                       code == u'\r' || code == u' ';
    if (space) {
      if (written) pending_space = true;
      continue;
    }
    if (pending_space) value[written++] = u' ';
    pending_space = false;
    value[written++] = code;
  }
  value.resize(written);
  return value;
}

inline std::vector<NodeId> DocumentAggregate::descendants(NodeId root, bool include_root) const {
  require(root); std::vector<NodeId> result, stack{root};
  while (!stack.empty()) {
    NodeId key = stack.back(); stack.pop_back();
    if (include_root || key != root) result.push_back(key);
    const auto& children = require(key).child_ids();
    stack.insert(stack.end(), children.rbegin(), children.rend());
  }
  return result;
}
inline std::vector<NodeId> DocumentAggregate::query16(NodeId root, unsigned kind, DomStringView value) const {
  std::vector<NodeId> result;
  auto classes = kind == 2 ? tokens(value) : std::vector<DomString>{};
  if ((kind == 0 && value.empty()) || (kind == 2 && classes.empty())) return result;
  // One connected holder answers immediately. Duplicates and detached holders
  // fall through to the tree walk, which returns the first match in tree order.
  if (kind == 0 && root == id()) {
    NodeId hit = 0;
    if (lookup_element_id(value, &hit)) return hit ? std::vector<NodeId>{hit} : result;
  }
  for (NodeId key : descendants(root)) {
    const auto* element = get_element(key); if (!element) continue;
    bool match = false;
    if (kind == 0) match = element->get_attribute16(u"id") == value;
    if (kind == 1) match = value == u"*" || element->tag_name().local16() ==
        (element->tag_name().html() && uses_html_syntax() ? ascii_lower(value) : DomString(value));
    if (kind == 2) match = std::all_of(classes.begin(), classes.end(), [&](const auto& name) { return element->has_class16(name); });
    if (match) { result.push_back(key); if (kind == 0) break; }
  }
  return result;
}
inline std::vector<NodeId> DocumentAggregate::query_ns16(NodeId root, DomStringView namespace_uri,
                                                        DomStringView local) const {
  std::vector<NodeId> result;
  const bool any_namespace = namespace_uri == u"*";
  for (NodeId key : descendants(root)) {
    const auto* element = get_element(key); if (!element) continue;
    const DomStringView space = element->tag_name().namespace16();
    if (!any_namespace && space != namespace_uri) continue;
    if (local == u"*") { result.push_back(key); continue; }
    const DomString expected = element->tag_name().folds_case() ? ascii_lower(local) : DomString(local);
    if (element->tag_name().local16() == expected) result.push_back(key);
  }
  return result;
}
inline NodeId DocumentAggregate::get_element_by_id(const std::string& value) const {
  auto result = query16(id(), 0, from_utf8(value)); return result.empty() ? 0 : result.front();
}
inline std::vector<NodeId> DocumentAggregate::get_elements_by_tag(const TagName& tag) const { return query16(id(), 1, tag.local16()); }
inline std::vector<NodeId> DocumentAggregate::get_elements_by_class(const std::string& name) const { return query16(id(), 2, from_utf8(name)); }
inline NodeId DocumentAggregate::document_element_id() const noexcept {
  const Node* root = get_node(id()); if (!root) return 0;
  for (NodeId child : root->child_ids()) if (get_element(child)) return child;
  return 0;
}
inline NodeId DocumentAggregate::html_child(const char16_t* tag) const noexcept {
  const auto* root = get_element(document_element_id());
  if (!root || !root->tag_name().html() || root->tag_name().local16() != u"html") return 0;
  for (NodeId child : root->child_ids()) {
    const auto* element = get_element(child);
    if (element && element->tag_name().html() && element->tag_name().local16() == tag) return child;
  }
  return 0;
}
inline NodeId DocumentAggregate::head_id() const noexcept { return html_child(u"head"); }
inline NodeId DocumentAggregate::body_id() const noexcept {
  const auto* root = get_element(document_element_id());
  if (!root || !root->tag_name().html() || root->tag_name().local16() != u"html") return 0;
  for (NodeId child : root->child_ids()) {
    const auto* element = get_element(child);
    if (element && element->tag_name().html() &&
        (element->tag_name().local16() == u"body" || element->tag_name().local16() == u"frameset")) return child;
  }
  return 0;
}
inline NodeId DocumentAggregate::focused_id() const noexcept {
  const Node* node = focused_id_ ? get_node(focused_id_) : nullptr;
  while (node && node->parent_id()) node = get_node(node->parent_id());
  return node && node->id() == id() ? focused_id_ : 0;
}
inline NodeId DocumentAggregate::active_element_id() const noexcept {
  if (const NodeId focused = focused_id()) return focused;
  return mode_ == DocumentMode::Html ? body_id() : document_element_id();
}
inline DomString DocumentAggregate::title16() const {
  if (!uses_html_syntax()) return {};
  const NodeId head = head_id();
  if (!head) return {};
  for (NodeId child : require(head).child_ids()) {
    const auto* element = get_element(child);
    if (element && element->tag_name().html() && element->tag_name().local16() == u"title")
      return normalize_title_text(text_content16(child));
  }
  return {};
}
inline void DocumentAggregate::set_title16(DomStringView value) {
  if (mode_ != DocumentMode::Html) return;
  NodeId head = head_id();
  if (!head) {
    const NodeId root = document_element_id();
    if (!root) return;
    head = create_element16(u"head");
    insert_before(root, head, require(root).first_child_id());
  }
  for (NodeId child : require(head).child_ids()) {
    const auto* element = get_element(child);
    if (element && element->tag_name().html() && element->tag_name().local16() == u"title") {
      set_text_content16(child, value);
      return;
    }
  }
  const NodeId title = create_element16(u"title");
  try {
    append_child(head, title);
    set_text_content16(title, value);
  } catch (...) {
    remove_node(title);
    throw;
  }
}
inline size_t DocumentAggregate::storage_units() const noexcept {
  size_t count = (url_ ? url_->size() : 0) + cookie_.size() + metadata_.storage_units();
  for (const auto& [key, node] : nodes_) count += node->storage_units();
  return count;
}
}
