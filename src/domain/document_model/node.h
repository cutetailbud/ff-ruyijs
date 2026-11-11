#pragma once
#include "domain/shared/entity.h"
#include "domain/document_model/node_type.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ruyijs::domain::document_model {
class Node : public shared::Entity<NodeId> {
 public:
  Node(NodeId id, NodeType type) : Entity<NodeId>(id), node_type_(type) {}
  ~Node() override = default;
  NodeType node_type() const noexcept { return node_type_; }
  virtual NodeId shadow_host_id() const noexcept { return 0; }
  virtual NodeId shadow_root_id() const noexcept { return 0; }
  virtual size_t storage_units() const noexcept { return 0; }
  virtual DomString node_name16() const {
    switch (node_type_) {
      case NodeType::Text: return u"#text"; case NodeType::Comment: return u"#comment";
      case NodeType::Document: return u"#document"; case NodeType::DocumentFragment: return u"#document-fragment";
      default: return {};
    }
  }
  virtual std::optional<DomString> node_value16() const { return std::nullopt; }
  virtual void set_node_value16(DomString) {}
  virtual DomString text_content16() const { return node_value16().value_or(DomString{}); }
  virtual std::string node_name() const { return to_utf8(node_name16()); }
  virtual std::string node_value() const { return to_utf8(node_value16().value_or(DomString{})); }
  virtual void set_node_value(const std::string& value) { set_node_value16(from_utf8(value)); }
  virtual std::string text_content() const { return to_utf8(text_content16()); }
  NodeId parent_id() const noexcept { return parent_id_; }
  bool has_parent() const noexcept { return parent_id_ != 0; }
  void set_parent(NodeId id) noexcept { parent_id_ = id; }
  const std::vector<NodeId>& child_ids() const noexcept { return children_; }
  size_t child_count() const noexcept { return children_.size(); }
  bool has_children() const noexcept { return !children_.empty(); }
  NodeId first_child_id() const noexcept { return children_.empty() ? 0 : children_.front(); }
  NodeId last_child_id() const noexcept { return children_.empty() ? 0 : children_.back(); }
  // Internal owner operations. Production callers mutate through DocumentAggregate.
  void append_child_id(NodeId id) {
    if (child_index_) (*child_index_)[id] = children_.size();
    children_.push_back(id);
  }
  void insert_child_id_at(NodeId id, size_t index) {
    const size_t pos = std::min(index, children_.size());
    if (pos == children_.size()) {
      append_child_id(id);
      return;
    }
    child_index_.reset();
    children_.insert(children_.begin() + pos, id);
  }
  bool remove_child_id(NodeId id) {
    if (child_index_) {
      auto found = child_index_->find(id);
      if (found == child_index_->end()) return false;
      const size_t pos = found->second;
      if (pos + 1 == children_.size()) {
        child_index_->erase(found);
        children_.pop_back();
        return true;
      }
      child_index_.reset();
      children_.erase(children_.begin() + pos);
      return true;
    }
    auto it = std::find(children_.begin(), children_.end(), id);
    if (it == children_.end()) return false;
    children_.erase(it);
    return true;
  }
  void clear_children() noexcept {
    children_.clear();
    child_index_.reset();
  }
  void replace_children(std::vector<NodeId>& value) noexcept {
    children_.swap(value);
    child_index_.reset();
  }
  int index_of_child(NodeId id) const {
    if (children_.size() < kChildIndexThreshold) {
      auto it = std::find(children_.begin(), children_.end(), id);
      return it == children_.end() ? -1 : int(it - children_.begin());
    }
    if (!child_index_) {
      auto map = std::make_unique<std::unordered_map<NodeId, size_t>>();
      map->reserve(children_.size());
      for (size_t i = 0; i < children_.size(); ++i) (*map)[children_[i]] = i;
      child_index_ = std::move(map);
    }
    auto found = child_index_->find(id);
    return found == child_index_->end() ? -1 : int(found->second);
  }
 protected:
  static constexpr size_t kChildIndexThreshold = 64;
  Node(const Node& other)
      : Entity<NodeId>(other), node_type_(other.node_type_), parent_id_(other.parent_id_), children_(other.children_) {}
  Node& operator=(const Node& other) {
    if (this != &other) {
      Entity<NodeId>::operator=(other);
      node_type_ = other.node_type_;
      parent_id_ = other.parent_id_;
      children_ = other.children_;
      child_index_.reset();
    }
    return *this;
  }
  NodeType node_type_;
  NodeId parent_id_ = 0;
  std::vector<NodeId> children_;
  mutable std::unique_ptr<std::unordered_map<NodeId, size_t>> child_index_;
};
}
