// Prepared append/replace-all edits over the original domain tree. Only the
// variadic conversion and replaceChildren use these complete change records.
#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::CheckChildInsertion(NodeKey parent, const Model& source, NodeKey node) const noexcept {
  return Protect([&] {
    State().owner(parent).ensure_append_validity(parent.node, source.State().owner(node), node.node);
  });
}

Status Model::PrepareChildListChange(NodeKey parent, NodeKey node, bool replace,
                                     ChildListChange* result) noexcept {
  return Protect([&] {
    auto& owner = State().owner(parent);
    if (parent.document != node.document)
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Prepared child belongs to another document");
    owner.ensure_append_validity(parent.node, owner, node.node);
    const auto& source = owner.require(node.node);
    ChildListChange value;
    value.parent = parent; value.node = node; value.replace = replace;
    value.fragment = source.node_type() == domain::NodeType::DocumentFragment;
    value.added = value.fragment ? source.child_ids() : std::vector<uint64_t>{node.node};
    if (replace) value.removed = owner.require(parent.node).child_ids();
    else value.children = owner.require(parent.node).child_ids();
    if (!value.fragment) {
      value.source_parent = Key(node.document, source.parent_id());
      if (source.parent_id())
        State().Neighbours(value.source_parent, node.node, &value.previous, &value.next);
      value.children.erase(std::remove(value.children.begin(), value.children.end(), node.node),
                           value.children.end());
    }
    value.children.insert(value.children.end(), value.added.begin(), value.added.end());
    *result = std::move(value);
  });
}

Status Model::CommitChildListChange(ChildListChange& value) noexcept {
  return Protect([&] {
    auto& owner = State().owner(value.parent);
    const int src_old_idx = (!value.fragment && value.source_parent.node)
        ? owner.require(value.source_parent.node).index_of_child(value.node.node) : -1;
    const uint32_t old_parent_count = uint32_t(owner.require(value.parent.node).child_count());
    owner.commit_child_list(value.parent.node, value.node.node, value.children);
    if (value.replace) {
      if (value.fragment) {
        for (auto id : value.added) State().NoteRangeRemove(value.node, id, 0);
      } else if (value.source_parent.node && value.source_parent.node != value.parent.node && src_old_idx >= 0) {
        State().NoteRangeRemove(value.source_parent, value.node.node, uint32_t(src_old_idx));
      }
      for (auto id : value.removed) State().NoteRangeRemove(value.parent, id, 0);
      if (!value.added.empty()) State().NoteRangeInsert(value.parent, 0, uint32_t(value.added.size()));
    } else {
      if (value.fragment) {
        for (auto id : value.added) State().NoteRangeRemove(value.node, id, 0);
        if (!value.added.empty())
          State().NoteRangeInsert(value.parent, old_parent_count, uint32_t(value.added.size()));
      } else {
        if (value.source_parent.node && src_old_idx >= 0)
          State().NoteRangeRemove(value.source_parent, value.node.node, uint32_t(src_old_idx));
        const int ins_idx = owner.require(value.parent.node).index_of_child(value.node.node);
        if (ins_idx >= 0) State().NoteRangeInsert(value.parent, uint32_t(ins_idx), 1);
      }
    }
    State().Touch();
    if (value.fragment && !value.added.empty())
      State().NoteRemoved(value.node, value.added, {}, {});
    else if (value.source_parent.node && (!value.replace || value.source_parent.node != value.parent.node))
      State().NoteRemoved(value.source_parent, {value.node.node}, value.previous, value.next);
    if (value.replace) State().NoteReplace(value.parent, value.added, value.removed);
    else State().NoteInserted(value.parent, value.added);
  });
}

Status Model::DetachTransferred(NodeKey source,
    const std::vector<std::pair<NodeKey, NodeKey>>& identities, bool fragment) noexcept {
  return Protect([&] {
    auto& owner = State().owner(source);
    auto& node = owner.require(source.node);
    const NodeKey parent = fragment ? source : Key(source.document, node.parent_id());
    if (!parent.node) return;
    const auto removed = fragment ? node.child_ids() : std::vector<uint64_t>{source.node};
    std::vector<NodeKey> moved;
    moved.reserve(removed.size());
    for (auto id : removed) {
      const auto found = std::find_if(identities.begin(), identities.end(), [&](const auto& pair) {
        return pair.first.document == source.document && pair.first.node == id;
      });
      if (found == identities.end())
        throw domain::DomError(domain::DomErrorCode::InvalidState, "Missing transfer identity");
      moved.push_back(found->second);
    }
    NodeKey previous, next;
    if (!fragment) State().Neighbours(parent, source.node, &previous, &next);
    for (auto id : removed) {
      const int old_idx = owner.require(parent.node).index_of_child(id);
      owner.remove_child(parent.node, id);
      if (old_idx >= 0) State().NoteRangeRemove(parent, id, uint32_t(old_idx));
    }
    State().Touch();
    State().NoteRemovedKeys(parent, moved, previous, next);
  });
}
}
