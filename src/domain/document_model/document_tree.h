#pragma once
#include "domain/document_model/document.h"

namespace ruyijs::domain::document_model {
inline void DocumentAggregate::validate_parent(NodeId parent, NodeId child) const {
  const auto& p = require(parent);
  if (p.node_type() != NodeType::Document && p.node_type() != NodeType::Element &&
      p.node_type() != NodeType::DocumentFragment)
    throw DomError(DomErrorCode::HierarchyRequest, "Invalid parent node type");
  for (NodeId ancestor = parent; ancestor;
       ancestor = require(ancestor).parent_id() ? require(ancestor).parent_id() : require(ancestor).shadow_host_id())
    if (ancestor == child) throw DomError(DomErrorCode::HierarchyRequest, "Insertion would create a cycle");
}
inline void DocumentAggregate::validate_insert(NodeId parent, NodeId child, NodeId replaced) const {
  const auto& p = require(parent); const auto& c = require(child);
  if (c.node_type() == NodeType::Document) throw DomError(DomErrorCode::HierarchyRequest, "Document is not an allowed child");
  if (p.node_type() == NodeType::Document) {
    if (c.node_type() == NodeType::Text) throw DomError(DomErrorCode::HierarchyRequest, "Document cannot contain Text children");
    if (c.node_type() == NodeType::DocumentType)
      for (NodeId key : p.child_ids())
        if (key != replaced && (require(key).node_type() == NodeType::Element || require(key).node_type() == NodeType::DocumentType))
          throw DomError(DomErrorCode::HierarchyRequest, "Document already has an element or document type");
    if (c.node_type() == NodeType::Element)
      for (NodeId key : p.child_ids())
        if (key != replaced && require(key).node_type() == NodeType::Element)
          throw DomError(DomErrorCode::HierarchyRequest, "Document already has an element");
  }
}
// Pre-insertion of a whole DocumentFragment: every child moves to one position
// and the fragment ends up empty. Validation runs for the whole set before the
// first mutation, so a rejecting insert leaves the tree and the fragment intact.
inline void DocumentAggregate::insert_fragment(NodeId parent, NodeId fragment, NodeId before, NodeId replaced) {
  auto& p = require(parent);
  validate_parent(parent, fragment);
  if (before && p.index_of_child(before) < 0) throw DomError(DomErrorCode::NotFound, "Reference is not a child of parent");
  if (replaced && p.index_of_child(replaced) < 0) throw DomError(DomErrorCode::NotFound, "Replaced node is not a child of parent");
  const std::vector<NodeId> moving = require(fragment).child_ids();
  size_t elements = 0, doctypes = 0;
  for (NodeId node : moving) {
    const auto& c = require(node);
    validate_insert(parent, node, replaced);
    if (c.node_type() == NodeType::Document) throw DomError(DomErrorCode::HierarchyRequest, "Document is not an allowed child");
    if (p.node_type() == NodeType::Document) {
      if (c.node_type() == NodeType::Text) throw DomError(DomErrorCode::HierarchyRequest, "Document cannot contain Text children");
      if (c.node_type() == NodeType::Element) ++elements;
      if (c.node_type() == NodeType::DocumentType) ++doctypes;
    }
  }
  if (p.node_type() == NodeType::Document) {
    if (doctypes > 1 || (doctypes && elements))
      throw DomError(DomErrorCode::HierarchyRequest, "Document type conflicts with document contents");
    size_t existing = 0;
    for (NodeId key : p.child_ids())
      if (key != replaced && require(key).node_type() == NodeType::Element) ++existing;
    if (existing + elements > 1) throw DomError(DomErrorCode::HierarchyRequest, "Document already has an element");
  }
  std::vector<NodeId> next = p.child_ids();
  if (replaced) next.erase(std::find(next.begin(), next.end(), replaced));
  std::vector<NodeId> empty;
  require(fragment).replace_children(empty);
  for (NodeId node : moving) require(node).set_parent(0);
  auto position = before ? std::find(next.begin(), next.end(), before) : next.end();
  next.insert(position, moving.begin(), moving.end());
  if (replaced) require(replaced).set_parent(0);
  p.replace_children(next);
  for (NodeId node : moving) require(node).set_parent(parent);
}
inline void DocumentAggregate::insert_before(NodeId parent, NodeId child, NodeId before) {
  if (require(child).node_type() == NodeType::DocumentFragment) { insert_fragment(parent, child, before, 0); return; }
  auto& p = require(parent); auto& c = require(child);
  validate_parent(parent, child);
  if (before && p.index_of_child(before) < 0) throw DomError(DomErrorCode::NotFound, "Reference is not a child of parent");
  validate_insert(parent, child, 0);
  if (before == child) return;
  // appendChild of a node that is not in a tree. Copying the parent's child
  // vector is quadratic when a page appends thousands of siblings; push_back
  // leaves the same parent, the same order, and the same parent id.
  if (!before && !c.parent_id()) {
    p.append_child_id(child);
    c.set_parent(parent);
    return;
  }
  std::vector<NodeId> next = p.child_ids(), old;
  next.erase(std::remove(next.begin(), next.end(), child), next.end());
  auto position = before ? std::find(next.begin(), next.end(), before) : next.end();
  next.insert(position, child);
  Node* previous = c.parent_id() && c.parent_id() != parent ? &require(c.parent_id()) : nullptr;
  if (previous) { old = previous->child_ids(); old.erase(std::remove(old.begin(), old.end(), child), old.end()); }
  if (previous) previous->replace_children(old);
  p.replace_children(next); c.set_parent(parent);
}
inline void DocumentAggregate::replace_child(NodeId parent, NodeId child, NodeId old_child) {
  if (require(child).node_type() == NodeType::DocumentFragment) {
    auto& p = require(parent);
    if (p.index_of_child(old_child) < 0) throw DomError(DomErrorCode::NotFound, "Replaced node is not a child of parent");
    const auto siblings = p.child_ids();
    const size_t index = size_t(p.index_of_child(old_child));
    insert_fragment(parent, child, index + 1 < siblings.size() ? siblings[index + 1] : 0, old_child);
    return;
  }
  auto& p = require(parent); auto& c = require(child); auto& removed = require(old_child);
  validate_parent(parent, child);
  if (p.index_of_child(old_child) < 0) throw DomError(DomErrorCode::NotFound, "Replaced node is not a child of parent");
  validate_insert(parent, child, old_child);
  if (child == old_child) return;
  std::vector<NodeId> next = p.child_ids(), old;
  next.erase(std::remove(next.begin(), next.end(), child), next.end());
  *std::find(next.begin(), next.end(), old_child) = child;
  Node* previous = c.parent_id() && c.parent_id() != parent ? &require(c.parent_id()) : nullptr;
  if (previous) { old = previous->child_ids(); old.erase(std::remove(old.begin(), old.end(), child), old.end()); }
  if (previous) previous->replace_children(old);
  p.replace_children(next); c.set_parent(parent); removed.set_parent(0);
}
inline void DocumentAggregate::remove_child(NodeId parent, NodeId child) {
  auto& p = require(parent); auto& c = require(child);
  if (c.parent_id() != parent || p.index_of_child(child) < 0)
    throw DomError(DomErrorCode::NotFound, "Node is not a child of parent");
  p.remove_child_id(child); c.set_parent(0);
}
inline void DocumentAggregate::remove_node(NodeId key) noexcept {
  Node* root = get_node(key); if (!root) return;
  const NodeId boundary = key;
  if (auto* parent = get_node(root->parent_id())) parent->remove_child_id(key);
  if (auto* host = get_element(root->shadow_host_id())) host->set_shadow_root(0);
  root->set_parent(0);
  while (auto* node = get_node(key)) {
    if (node->shadow_root_id()) { key = node->shadow_root_id(); continue; }
    if (node->has_children()) { key = node->first_child_id(); continue; }
    NodeId parent = node->parent_id() ? node->parent_id() : node->shadow_host_id();
    if (auto* p = get_node(parent)) p->remove_child_id(key);
    if (auto* host = get_element(node->shadow_host_id())) host->set_shadow_root(0);
    nodes_.erase(key);
    if (key == boundary) break;
    key = parent;
  }
}
}
