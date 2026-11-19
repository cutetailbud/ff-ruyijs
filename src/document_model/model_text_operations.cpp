#include "document_model/model_internal.h"
#include "domain/document_model/text_operations.h"

namespace ruyijs::document_model {
Status Model::WholeText(NodeKey key, DomString* result) const noexcept {
  return Protect([&] {
    auto value = domain::whole_text(State().owner(key), key.node);
    result->swap(value);
  });
}
Status Model::SplitTextInto(NodeKey source, NodeKey destination, uint32_t offset) noexcept {
  return Protect([&] {
    if (source.document != destination.document)
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Split nodes must share their owner");
    auto& owner = State().owner(source);
    const auto& node = domain::require_text(owner, source.node);
    const DomString old = node.data16();
    const NodeKey parent = Key(source.document, node.parent_id());
    uint32_t parent_index_after = 0;
    if (!Null(parent)) {
      const int idx = owner.require(parent.node).index_of_child(source.node);
      if (idx >= 0) parent_index_after = uint32_t(idx) + 1;
    }
    domain::split_text_into(owner, source.node, destination.node, offset);
    // Total UTF-16 storage is unchanged. Create already charged the new node.
    State().NoteRangeSplit(source, destination, parent, parent_index_after, offset);
    State().Touch();
    if (!Null(parent)) State().NoteInserted(parent, {destination.node});
    State().NoteData(source, &old);
  });
}
Status Model::ClearFragment(NodeKey key) noexcept {
  return Protect([&] {
    auto& owner = State().owner(key);
    const auto& node = owner.require(key.node);
    if (node.node_type() != domain::NodeType::DocumentFragment)
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Expected a DocumentFragment");
    const auto removed = node.child_ids();
    if (removed.empty()) return;
    owner.set_text_content16(key.node, u"");
    for (auto child : removed) State().NoteRangeRemove(key, child, 0);
    State().Touch();
    State().NoteReplace(key, {}, removed);
  });
}
}
