// Real-change log of the unique Document Model owner. A DOM mutation becomes an
// observer-visible notice here: the tree, attribute and character-data bridges
// call one Note* helper immediately after the owner applied the change, so no
// consumer can ever see a half-applied state or a re-derived difference. The
// observer owner (src/web_observers) reads the log through Model, turns notices
// into MutationRecord objects and keeps every policy decision: filters, subtree
// matching, batching and delivery. Nothing in this file is JS-aware, and there
// is no second tree, index or record list.
#include "document_model/model_internal.h"
#include "domain/document_model/input_radio_membership.h"
#include <utility>

namespace ruyijs::document_model {
void Model::SetStyleSink(void (*sink)(void*, NodeKey, MutationKind, DomStringView,
                                     DomStringView, const DomString*), void* context) noexcept {
  if (!impl_) return;
  impl_->styleSink = sink; impl_->styleSinkContext = context;
}
void Model::SetMutationRecording(bool enabled) noexcept {
  if (impl_) impl_->mutationRecording = enabled;
}
bool Model::MutationRecording() const noexcept { return impl_ && impl_->mutationRecording; }
bool Model::IdentityTransferPending() const noexcept { return impl_ && impl_->identityTransfer; }
uint64_t Model::MutationSequence() const noexcept { return impl_ ? impl_->mutationSequence : 0; }
uint64_t Model::DroppedMutations() const noexcept { return impl_ ? impl_->mutationDropped : 0; }
void Model::SetMutationSink(void (*sink)(void*), void* context) noexcept {
  if (!impl_) return;
  impl_->mutationSink = sink;
  impl_->mutationSinkContext = context;
}
void Model::SetStructureSink(void (*sink)(void*, DocumentId, const std::vector<NodeKey>&),
                             void* context) noexcept {
  if (!impl_) return;
  impl_->structureSink = sink;
  impl_->structureSinkContext = context;
}
void Model::SetTreeSink(void (*sink)(void*, DocumentId, const std::vector<NodeKey>&,
                                     const std::vector<NodeKey>&),
                        void* context) noexcept {
  if (!impl_) return;
  impl_->treeSink = sink;
  impl_->treeSinkContext = context;
}
void Model::SetRangeSink(const RangeSink& sink) noexcept {
  if (!impl_) return;
  impl_->rangeSink = sink;
}
void Model::SetReclaimSink(void (*sink)(void*), void* context) noexcept {
  if (!impl_) return;
  impl_->reclaim = sink;
  impl_->reclaimContext = context;
}
void Model::SetAttributeSink(void (*sink)(void*, DocumentId, NodeKey, DomStringView, DomStringView,
                                                 const DomString*),
                             void* context) noexcept {
  if (!impl_) return;
  impl_->attributeSink = sink;
  impl_->attributeSinkContext = context;
}
Status Model::MutationsSince(uint64_t sequence, std::vector<MutationNotice>* result) const noexcept {
  return Protect([&] {
    std::vector<MutationNotice> copied;
    for (const MutationNotice& notice : State().mutations)
      if (notice.sequence > sequence) copied.push_back(notice);
    result->swap(copied);
  });
}
Status Model::AncestorOrSelf(NodeKey ancestor, NodeKey node, bool* result) const noexcept {
  return Protect([&] {
    *result = false;
    if (Null(ancestor) || Null(node) || ancestor.document != node.document) return;
    auto& owner = State().owner(ancestor);
    State().owner(node);
    for (domain::NodeId current = node.node; current; current = owner.require(current).parent_id())
      if (current == ancestor.node) { *result = true; return; }
  });
}
void Model::Impl::Note(MutationNotice notice) {
  if (!mutationRecording) return;
  if (mutations.size() >= kMutationLogLimit) { mutations.pop_front(); ++mutationDropped; }
  notice.sequence = ++mutationSequence;
  mutations.push_back(std::move(notice));
  if (mutationSink) mutationSink(mutationSinkContext);
}
void Model::Impl::Neighbours(const NodeKey& parent, domain::NodeId node,
                             NodeKey* previous, NodeKey* next) const {
  // A caller that only needs one side passes null for the other.
  if (previous) *previous = NodeKey{};
  if (next) *next = NodeKey{};
  auto found = documents.find(parent.document);
  if (found == documents.end()) return;
  const auto children = found->second->require(parent.node).child_ids();
  for (size_t index = 0; index < children.size(); ++index) {
    if (children[index] != node) continue;
    if (previous && index > 0) *previous = Key(parent.document, children[index - 1]);
    if (next && index + 1 < children.size()) *next = Key(parent.document, children[index + 1]);
    return;
  }
}
void Model::Impl::NoteInserted(const NodeKey& parent, const std::vector<domain::NodeId>& added) {
  if (added.empty()) return;
  domain::input_radio_inserted(owner(parent), added);
  std::vector<NodeKey> keys;
  keys.reserve(added.size());
  for (domain::NodeId node : added) keys.push_back(Key(parent.document, node));
  // The structure port runs even when observer recording is off, because the
  // runtime frame lifecycle is driven from the same change point instead of
  // polling the tree. It never sees a half-applied state.
  NotifyStructure(parent.document, keys, {});
  NoteStyle(parent, MutationKind::ChildList);
  if (mutationRecording) {
  MutationNotice notice;
  notice.kind = MutationKind::ChildList;
  notice.target = parent;
  notice.added = std::move(keys);
  Neighbours(parent, added.front(), &notice.previous_sibling, nullptr);
  Neighbours(parent, added.back(), nullptr, &notice.next_sibling);
  Note(std::move(notice));
  }
  NoteScript(parent);
  for(auto id:added)NoteScript(Key(parent.document,id),true);
}
void Model::Impl::NoteRemoved(const NodeKey& parent, const std::vector<domain::NodeId>& removed,
                              const NodeKey& previous, const NodeKey& next) {
  std::vector<NodeKey> keys;
  keys.reserve(removed.size());
  for (domain::NodeId node : removed) keys.push_back(Key(parent.document, node));
  NoteRemovedKeys(parent, keys, previous, next);
}
void Model::Impl::NoteRemovedKeys(const NodeKey& parent, const std::vector<NodeKey>& removed,
                                  const NodeKey& previous, const NodeKey& next) {
  if (removed.empty()) return;
  // A removal can disconnect a frame whose context must be retired; the
  // inserted-scope list stays empty.
  NotifyStructure(parent.document, {}, removed);
  NoteStyle(parent, MutationKind::ChildList);
  if (!mutationRecording) return;
  MutationNotice notice;
  notice.kind = MutationKind::ChildList;
  notice.target = parent;
  notice.previous_sibling = previous;
  notice.next_sibling = next;
  notice.removed = removed;
  Note(std::move(notice));
}
void Model::Impl::NoteReplace(const NodeKey& parent, const std::vector<domain::NodeId>& added,
                              const std::vector<domain::NodeId>& removed) {
  if (added.empty() && removed.empty()) return;
  if (!added.empty()) domain::input_radio_inserted(owner(parent), added);
  std::vector<NodeKey> keys;
  keys.reserve(added.size());
  for (domain::NodeId node : added) keys.push_back(Key(parent.document, node));
  std::vector<NodeKey> gone;
  gone.reserve(removed.size());
  for (domain::NodeId node : removed) gone.push_back(Key(parent.document, node));
  NotifyStructure(parent.document, keys, gone);
  NoteStyle(parent, MutationKind::ChildList);
  if (mutationRecording) {
  MutationNotice notice;
  notice.kind = MutationKind::ChildList;
  notice.target = parent;
  notice.added = std::move(keys);
  notice.removed = std::move(gone);
  // The record's neighbours are the added nodes' after the replacement; a
  // wholesale replacement leaves them null.
  if (!added.empty()) {
    Neighbours(parent, added.front(), &notice.previous_sibling, nullptr);
    Neighbours(parent, added.back(), nullptr, &notice.next_sibling);
  }
  Note(std::move(notice));
  }
  NoteScript(parent);
  for(auto id:added)NoteScript(Key(parent.document,id),true);
}
void Model::Impl::NoteAttributes(const NodeKey& element, DomStringView name,
                                 DomStringView namespace_uri, const DomString* old_value) {
  domain::input_radio_attribute(owner(element), element.node, name, namespace_uri, old_value);
  if(namespace_uri.empty() && name==u"async")
    owner(element).get_element(element.node)->script_state().force_async=false;
  // The attribute port runs even when observer recording is off, because the
  // runtime frame owner turns a changed iframe `src` into a navigation at this
  // same change point. It never sees a half-applied state.
  if (attributeSink)
    attributeSink(attributeSinkContext, element.document, element, name, namespace_uri, old_value);
  NoteStyle(element, MutationKind::Attributes, name, namespace_uri, old_value);
  if (mutationRecording) {
  MutationNotice notice;
  notice.kind = MutationKind::Attributes;
  notice.target = element;
  notice.attribute_name.assign(name);
  notice.attribute_namespace.assign(namespace_uri);
  notice.has_old_value = old_value != nullptr;
  if (old_value) notice.old_value = *old_value;
  Note(std::move(notice));
  }
  if(namespace_uri.empty() && name==u"src")NoteScript(element);
  if (namespace_uri.empty() && domain::ascii_lower(name) == u"type") InputTypeChanged(element, old_value);
}
void Model::Impl::NoteData(const NodeKey& node, const DomString* old_value) {
  NoteStyle(node, MutationKind::CharacterData);
  if (mutationRecording) {
  MutationNotice notice;
  notice.kind = MutationKind::CharacterData;
  notice.target = node;
  notice.has_old_value = old_value != nullptr;
  if (old_value) notice.old_value = *old_value;
  Note(std::move(notice));
  }
  NoteScript(node);
}
}  // namespace ruyijs::document_model
