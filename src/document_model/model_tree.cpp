#include "document_model/model_transfer_values.h"

namespace ruyijs::document_model {
namespace {
// Model-layer key equality; the realm's Same() lives behind state.h.
bool SameKey(NodeKey a, NodeKey b) { return a.document == b.document && a.node == b.node; }
}  // namespace
Status Model::Insert(NodeKey parent, NodeKey child, NodeKey before,
                     std::vector<std::pair<NodeKey, NodeKey>>* rekey) noexcept {
  return Protect([&] {
    State().owner(parent); State().owner(child);
    if (parent.document == child.document && !Null(before) && SameKey(before, child)) {
      auto& owner = State().owner(parent);
      if (owner.require(child.node).parent_id() == parent.node) {
        const auto& siblings = owner.require(parent.node).child_ids();
        const auto found = std::find(siblings.begin(), siblings.end(), child.node);
        before = found != siblings.end() && std::next(found) != siblings.end()
            ? Key(parent.document, *std::next(found)) : NodeKey{};
      }
    }
    if (!Null(before)) {
      State().owner(before);
      if (before.document != parent.document) throw domain::DomError(domain::DomErrorCode::NotFound,"Reference belongs to another document");
    }
    if (parent.document != child.document) {
      // Firefox adopts the node into the parent's document instead of failing.
      IdentityTransferScope publication(State().identityTransfer);
      if (State().owner(child).require(child.node).node_type() == domain::NodeType::DocumentFragment) {
        const auto moved = MoveFragmentIn(State().document_key(parent.document), child, parent,
                                          before, {}, rekey);
        std::vector<domain::NodeId> added;
        added.reserve(moved.size());
        for (NodeKey key : moved) added.push_back(key.node);
        if (!added.empty()) {
          const int first_idx = State().owner(parent).require(parent.node).index_of_child(added.front());
          if (first_idx >= 0) State().NoteRangeInsert(parent, uint32_t(first_idx), uint32_t(added.size()));
        }
        State().NoteInserted(parent, added);
        State().Touch();
        return;
      }
      const NodeKey moved = MoveIn(State().document_key(parent.document), child, parent, before, {}, rekey);
      const int ins_idx = State().owner(parent).require(parent.node).index_of_child(moved.node);
      if (ins_idx >= 0) State().NoteRangeInsert(parent, uint32_t(ins_idx), 1);
      State().NoteInserted(parent, {moved.node});
      State().Touch();
      return;
    }
    auto& owner = State().owner(parent);
    const bool fragment = owner.require(child.node).node_type() == domain::NodeType::DocumentFragment;
    const auto added = fragment ? owner.require(child.node).child_ids()
                                : std::vector<domain::NodeId>{child.node};
    const NodeKey old_parent = fragment ? child : Key(child.document, owner.require(child.node).parent_id());
    const int old_idx = (!fragment && !Null(old_parent))
        ? owner.require(old_parent.node).index_of_child(child.node) : -1;
    NodeKey previous, next;
    if (!fragment && !Null(old_parent)) State().Neighbours(old_parent, child.node, &previous, &next);
    owner.insert_before(parent.node,child.node,before.node);
    if (fragment) {
      for (domain::NodeId id : added) State().NoteRangeRemove(child, id, 0);
      if (!added.empty()) {
        const int first_idx = owner.require(parent.node).index_of_child(added.front());
        if (first_idx >= 0) State().NoteRangeInsert(parent, uint32_t(first_idx), uint32_t(added.size()));
      }
    } else {
      if (!Null(old_parent) && old_idx >= 0)
        State().NoteRangeRemove(old_parent, child.node, uint32_t(old_idx));
      const int ins_idx = owner.require(parent.node).index_of_child(child.node);
      if (ins_idx >= 0) State().NoteRangeInsert(parent, uint32_t(ins_idx), 1);
    }
    if (fragment && !added.empty()) State().NoteRemoved(child, added, {}, {});
    else if (!Null(old_parent)) State().NoteRemoved(old_parent, {child.node}, previous, next);
    State().NoteInserted(parent, added);
    State().Touch();
  });
}
Status Model::Replace(NodeKey parent, NodeKey child, NodeKey old_child,
                      std::vector<std::pair<NodeKey, NodeKey>>* rekey) noexcept {
  return Protect([&] {
    State().owner(parent); State().owner(child); State().owner(old_child);
    if (parent.document != old_child.document) throw domain::DomError(domain::DomErrorCode::NotFound,"Replaced node belongs to another document");
    if (SameKey(child, old_child)) {
      auto& owner = State().owner(parent);
      const int self_idx = owner.require(parent.node).index_of_child(old_child.node);
      owner.replace_child(parent.node, child.node, old_child.node);
      if (self_idx >= 0) {
        State().NoteRangeRemove(parent, child.node, uint32_t(self_idx));
        State().NoteRangeInsert(parent, uint32_t(self_idx), 1);
      }
      NodeKey previous, next;
      State().Neighbours(parent, child.node, &previous, &next);
      State().NoteRemoved(parent, {child.node}, previous, next);
      State().NoteInserted(parent, {child.node});
      State().Touch();
      return;
    }
    if (parent.document != child.document) {
      IdentityTransferScope publication(State().identityTransfer);
      const int rep_idx = State().owner(parent).require(parent.node).index_of_child(old_child.node);
      if (State().owner(child).require(child.node).node_type() == domain::NodeType::DocumentFragment) {
        const auto moved = MoveFragmentIn(State().document_key(parent.document), child, parent,
                                          {}, old_child, rekey);
        std::vector<domain::NodeId> added;
        added.reserve(moved.size());
        for (NodeKey key : moved) added.push_back(key.node);
        if (rep_idx >= 0) {
          State().NoteRangeRemove(parent, old_child.node, uint32_t(rep_idx));
          if (!added.empty()) State().NoteRangeInsert(parent, uint32_t(rep_idx), uint32_t(added.size()));
        }
        State().NoteReplace(parent, added, {old_child.node});
        State().Touch();
        return;
      }
      const NodeKey moved = MoveIn(State().document_key(parent.document), child, parent, {}, old_child, rekey);
      if (rep_idx >= 0) {
        State().NoteRangeRemove(parent, old_child.node, uint32_t(rep_idx));
        State().NoteRangeInsert(parent, uint32_t(rep_idx), 1);
      }
      State().NoteReplace(parent, {moved.node}, {old_child.node});
      State().Touch();
      return;
    }
    auto& owner = State().owner(parent);
    const bool fragment = owner.require(child.node).node_type() == domain::NodeType::DocumentFragment;
    const auto added = fragment ? owner.require(child.node).child_ids()
                                : std::vector<domain::NodeId>{child.node};
    const NodeKey old_parent = fragment ? child : Key(child.document, owner.require(child.node).parent_id());
    const int child_old_idx = (!fragment && !Null(old_parent))
        ? owner.require(old_parent.node).index_of_child(child.node) : -1;
    const int raw_rep_idx = owner.require(parent.node).index_of_child(old_child.node);
    NodeKey previous, next;
    if (!fragment && !Null(old_parent)) State().Neighbours(old_parent, child.node, &previous, &next);
    owner.replace_child(parent.node,child.node,old_child.node);
    uint32_t effective_rep_idx = raw_rep_idx >= 0 ? uint32_t(raw_rep_idx) : 0u;
    if (fragment) {
      for (domain::NodeId id : added) State().NoteRangeRemove(child, id, 0);
    } else if (!Null(old_parent) && child_old_idx >= 0) {
      State().NoteRangeRemove(old_parent, child.node, uint32_t(child_old_idx));
      if (SameKey(old_parent, parent) && child_old_idx < raw_rep_idx)
        --effective_rep_idx;
    }
    if (raw_rep_idx >= 0) {
      State().NoteRangeRemove(parent, old_child.node, effective_rep_idx);
      if (!added.empty()) State().NoteRangeInsert(parent, effective_rep_idx, uint32_t(added.size()));
    }
    if (fragment && !added.empty()) State().NoteRemoved(child, added, {}, {});
    else if (!Null(old_parent)) State().NoteRemoved(old_parent, {child.node}, previous, next);
    State().NoteReplace(parent, added, {old_child.node});
    State().Touch();
  });
}
NodeKey Model::MoveIn(NodeKey document, NodeKey source, NodeKey parent, NodeKey before,
                      NodeKey replaced, std::vector<std::pair<NodeKey, NodeKey>>* rekey) {
  auto& state = State();
  auto& from = state.owner(source);
  auto& to = state.document(document);
  if (from.require(source.node).node_type() == domain::NodeType::Document)
    throw domain::DomError(domain::DomErrorCode::NotSupported, "A Document cannot be adopted");
  const auto written = from.owned_descendants(source.node, true);
  size_t units = 0;
  for (domain::NodeId id : written) units += from.require(id).storage_units();
  state.budget(written.size(), units);
  const domain::NodeId copy = domain::DomTreeService::clone_into(from, source.node, to, true, true);
  // Read the copy's shape before placing it: inserting a DocumentFragment moves
  // its children out and empties it, so the post-insertion traversal would no
  // longer line up with the source's. The copy is shape-identical to the source
  // while still detached.
  const auto made = to.owned_descendants(copy, true);
  try {
    PreserveElementState(from, to, written, made);
    // The target validates the placement through the same domain entry points a
    // same-document edit uses; a rejection deletes the copy again, so the
    // target and (because the source is still untouched) the source too are
    // exactly as they were.
    if (Null(replaced)) to.insert_before(parent.node, copy, before.node);
    else to.replace_child(parent.node, copy, replaced.node);
    PairIdentities(written, made, source.document, document.document, rekey);
  } catch (...) {
    if (!Null(replaced)) { try { to.replace_child(parent.node, replaced.node, copy); } catch (...) {} }
    to.remove_node(copy);
    throw;
  }
  CommitTransfer(source, Key(document.document, copy));
  return Key(document.document, copy);
}
std::vector<NodeKey> Model::MoveFragmentIn(NodeKey document, NodeKey source, NodeKey parent,
                                           NodeKey before, NodeKey replaced,
                                           std::vector<std::pair<NodeKey, NodeKey>>* rekey) {
  auto& state = State();
  auto& from = state.owner(source);
  auto& to = state.document(document);
  const auto children = from.require(source.node).child_ids();
  const auto written = from.owned_descendants(source.node, false);
  size_t units = 0;
  for (domain::NodeId id : written) units += from.require(id).storage_units();
  // The copied children plus the temporary container that carries them.
  state.budget(written.size() + 1, units);
  const domain::NodeId copy = domain::DomTreeService::clone_into(from, source.node, to, true, true);
  // Read the shape while the copy still holds its children (see MoveIn).
  const auto made = to.owned_descendants(copy, false);
  const auto copies = to.require(copy).child_ids();
  try {
    PreserveElementState(from, to, written, made);
    // The target's fragment insertion moves the children out of the container
    // and validates them like a same-document fragment insertion.
    if (Null(replaced)) to.insert_before(parent.node, copy, before.node);
    else to.replace_child(parent.node, copy, replaced.node);
    PairIdentities(written, made, source.document, document.document, rekey);
  } catch (...) {
    to.remove_node(copy);
    throw;
  }
  to.remove_node(copy);  // The emptied temporary container.
  std::vector<NodeKey> moved;
  moved.reserve(copies.size());
  for (domain::NodeId id : copies) moved.push_back(Key(document.document, id));
  // The source fragment keeps its identity and is left empty, like a
  // same-document fragment insertion; its log names the moved identities.
  for (domain::NodeId id : children) {
    state.NoteRangeRemove(source, id, 0);
    from.remove_node(id);
  }
  if (!moved.empty()) state.NoteRemovedKeys(source, moved, {}, {});
  state.Touch();
  return moved;
}
void Model::CommitTransfer(NodeKey source, NodeKey moved) {
  auto& state = State();
  auto& from = state.owner(source);
  const domain::NodeId old_parent = from.require(source.node).parent_id();
  const int old_idx = old_parent ? from.require(old_parent).index_of_child(source.node) : -1;
  NodeKey previous, next;
  if (old_parent) state.Neighbours(Key(source.document, old_parent), source.node, &previous, &next);
  if (old_parent && old_idx >= 0)
    state.NoteRangeRemove(Key(source.document, old_parent), source.node, uint32_t(old_idx));
  from.remove_node(source.node);
  // One entry in the source document's own change log, exactly like the
  // removeChild that the adoption performs there. The removed identity is
  // `moved`, the node's key in its new document, so the delivered record names
  // the object the caller still holds.
  if (old_parent) state.NoteRemovedKeys(Key(source.document, old_parent), {moved}, previous, next);
  state.Touch();
}
Status Model::Remove(NodeKey parent, NodeKey child) noexcept {
  return Protect([&] {
    auto& owner = State().owner(parent); State().owner(child);
    if (parent.document != child.document) throw domain::DomError(domain::DomErrorCode::NotFound,"Node belongs to another document");
    const int old_idx = owner.require(parent.node).index_of_child(child.node);
    NodeKey previous, next;
    State().Neighbours(parent, child.node, &previous, &next);
    owner.remove_child(parent.node,child.node);
    if (old_idx >= 0) State().NoteRangeRemove(parent, child.node, uint32_t(old_idx));
    State().NoteRemoved(parent, {child.node}, previous, next);
    State().Touch();
  });
}
Status Model::AdoptNode(NodeKey document, NodeKey source, Draft* result,
                        std::vector<std::pair<NodeKey, NodeKey>>* rekey) noexcept {
  return Protect([&] {
    auto& from = State().owner(source);
    auto& to = State().document(document);
    const auto& node = from.require(source.node);
    if (node.node_type() == domain::NodeType::Document)
      throw domain::DomError(domain::DomErrorCode::NotSupported, "A Document cannot be adopted");
    if (node.shadow_host_id())
      throw domain::DomError(domain::DomErrorCode::HierarchyRequest, "A ShadowRoot cannot be adopted");
    // Same document: adoption keeps the node where it is and returns it as is.
    if (source.document == document.document) { *result = Draft{0, source}; return; }
    IdentityTransferScope publication(State().identityTransfer);
    const auto written = from.owned_descendants(source.node, true);
    size_t units = 0;
    for (domain::NodeId id : written) units += from.require(id).storage_units();
    State().budget(written.size(), units);
    const domain::NodeId root = domain::DomTreeService::clone_into(from, source.node, to, true, true);
    try {
      const auto made = to.owned_descendants(root, true);
      PreserveElementState(from, to, written, made);
      PairIdentities(written, made, source.document, document.document, rekey);
    } catch (...) { to.remove_node(root); throw; }
    CommitTransfer(source, Key(document.document, root));
    *result = Draft{0, Key(document.document, root)};
  });
}
Status Model::Clone(NodeKey key, bool deep, Draft* result,
                    std::optional<DomStringView> source_document_url) noexcept {
  return Protect([&] {
    auto& source = State().owner(key); const auto& node = source.require(key.node);
    if (node.shadow_host_id()) throw domain::DomError(domain::DomErrorCode::NotSupported, "A ShadowRoot cannot be cloned");
    auto nodes = domain::DomTreeService::cloned_nodes(source, key.node, deep);
    size_t units = 0; for (auto id : nodes) units += source.require(id).storage_units();
    std::optional<DomString> url;
    if (node.node_type() == domain::NodeType::Document) {
      url = source.url16();
      if (!url) {
        if (!source_document_url) throw domain::DomError(domain::DomErrorCode::InvalidState,"Document clone requires an external URL snapshot");
        url = DomString(*source_document_url);
      }
      units += url->size();
    }
    State().budget(nodes.size(),units);
    if (node.node_type() == domain::NodeType::Document) {
      DocumentId id = State().allocate_document();
      auto copy = std::make_unique<domain::DocumentAggregate>(1,source.mode());
      copy->metadata().set_xml_media_type(source.metadata().xml_media_type());
      copy->metadata().set_xml_parsed(source.metadata().xml_parsed());
      copy->metadata().set_parsed_html_syntax(source.metadata().parsed_html_syntax());
      copy->set_ready_state(domain::ReadyState::Complete);
      copy->set_url16(*url);
      if (deep) for (auto child : node.child_ids()) {
        auto cloned = domain::DomTreeService::clone_into(source,child,*copy);
        copy->append_child(1,cloned);
        const auto type = ParsedType(key.document, source);
        if (!Null(type) && type.document == key.document) {
          const auto written = source.descendants(child, true), made = copy->descendants(cloned, true);
          for (size_t i = 0; i < written.size(); ++i)
            if (written[i] == type.node) copy->set_parsed_doctype({0, made[i]});
        }
      }
      State().documents.emplace(id,std::move(copy));
      *result = State().remember({id,1},true);
    } else {
      auto root = domain::DomTreeService::clone_into(source,key.node,source,deep);
      try { if (deep) State().CloneTemplateContents(State(), key, Key(key.document,root)); }
      catch (...) { source.remove_node(root); throw; }
      *result = State().remember(Key(key.document,root),false);
    }
  });
}
}
