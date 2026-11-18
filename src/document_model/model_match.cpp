#include "document_model/model_internal.h"
#include "document_model/model_selector.h"

namespace ruyijs::document_model {
Status Model::Matches(NodeKey node, DomStringView selector, bool* result) const noexcept {
  Selector parsed;
  Status status = Selector::Parse(selector,&parsed);
  if (status.code != ErrorCode::None) return status;
  return Protect([&] {
    const auto& owner = State().owner(node);
    if (!owner.get_element(node.node))
      throw domain::DomError(domain::DomErrorCode::InvalidState,"matches needs an Element identity");
    *result = parsed.Matches(owner,node.node,node.node);
  });
}
Status Model::Closest(NodeKey node, DomStringView selector, NodeKey* result) const noexcept {
  Selector parsed;
  Status status = Selector::Parse(selector,&parsed);
  if (status.code != ErrorCode::None) return status;
  return Protect([&] {
    const auto& owner = State().owner(node);
    for (auto current = node.node; current; current = owner.require(current).parent_id())
      if (owner.get_element(current) && parsed.Matches(owner,current,node.node)) {
        *result = Key(node.document,current); return;
      }
    *result = {};
  });
}
Status Model::ElementsByTagNameNS(NodeKey scope, DomStringView namespace_uri, DomStringView local,
                                  std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    std::vector<NodeKey> values;
    for (auto node : State().owner(scope).query_ns16(scope.node,namespace_uri,local))
      values.push_back(Key(scope.document,node));
    result->swap(values);
  });
}
Status Model::Normalize(NodeKey node) noexcept {
  return Protect([&] {
    auto& doc = State().owner(node);
    doc.require(node.node);
    // Tree-order walk matching WHATWG DOM §4.4 Node.prototype.normalize():
    // only exclusive Text descendants are removed (when empty) or merged into
    // their preceding Text sibling, updating live Ranges and MutationNotices.
    std::vector<domain::NodeId> stack{node.node};
    while (!stack.empty()) {
      const domain::NodeId cur_parent = stack.back();
      stack.pop_back();
      const NodeKey parent_key = Key(node.document, cur_parent);
      const std::vector<domain::NodeId> children = doc.require(cur_parent).child_ids();
      std::vector<domain::NodeId> recurse;
      domain::NodeId kept = 0;
      for (domain::NodeId child : children) {
        if (!doc.get_node(child)) continue;
        const auto type = doc.require(child).node_type();
        if (type != domain::NodeType::Text) {
          kept = 0;
          if (doc.require(child).has_children()) recurse.push_back(child);
          continue;
        }
        auto& text = static_cast<domain::CharacterData&>(doc.require(child));
        const int cur_idx = doc.require(cur_parent).index_of_child(child);
        if (cur_idx < 0) continue;
        if (text.data16().empty()) {
          NodeKey prev_sib, next_sib;
          if (State().mutationRecording) State().Neighbours(parent_key, child, &prev_sib, &next_sib);
          doc.remove_child(cur_parent, child);
          State().NoteRangeRemove(parent_key, child, uint32_t(cur_idx));
          State().NoteRemoved(parent_key, {child}, prev_sib, next_sib);
          continue;
        }
        if (!kept) {
          kept = child;
          continue;
        }
        auto& kept_node = static_cast<domain::CharacterData&>(doc.require(kept));
        const DomString old_kept = kept_node.data16();
        const DomString extra = text.data16();
        const uint32_t kept_len = uint32_t(old_kept.size());
        const NodeKey kept_key = Key(node.document, kept);
        const NodeKey merged_key = Key(node.document, child);
        kept_node.set_node_value16(old_kept + extra);
        State().NoteRangeData(kept_key, kept_len, 0, uint32_t(extra.size()));
        State().NoteData(kept_key, &old_kept);
        State().NoteRangeMerge(kept_key, kept_len, merged_key, parent_key, uint32_t(cur_idx));
        NodeKey prev_sib, next_sib;
        if (State().mutationRecording) State().Neighbours(parent_key, child, &prev_sib, &next_sib);
        doc.remove_child(cur_parent, child);
        State().NoteRangeRemove(parent_key, child, uint32_t(cur_idx));
        State().NoteRemoved(parent_key, {child}, prev_sib, next_sib);
      }
      for (auto it = recurse.rbegin(); it != recurse.rend(); ++it) stack.push_back(*it);
    }
    State().Touch();
  });
}
Status Model::CompareDocumentPosition(NodeKey node, NodeKey other, uint32_t* result) const noexcept {
  return Protect([&] {
    if (Null(other)) throw domain::DomError(domain::DomErrorCode::InvalidState,"compareDocumentPosition needs a Node");
    *result = node.document == other.document
        ? domain::DomTreeService::compare_document_position(State().owner(node),node.node,other.node)
        : uint32_t(1 | 32 | 4);  // Disconnected, implementation-specific, following.
  });
}
}
