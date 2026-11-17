#pragma once
#include "document_model/model.h"
#include "document_model/model_html.h"
#include "domain/document_model/dom_tree_service.h"
#include <deque>
#include <map>
#include <new>
#include <cstdio>

namespace ruyijs::document_model {
namespace domain = ruyijs::domain::document_model;
template<class Work> Status Protect(Work&& work) noexcept {
  try { work(); return {}; }
  catch (const domain::DomError& error) {
    using D = domain::DomErrorCode;
    switch (error.code) {
      case D::NotFound: return {ErrorCode::NotFound,error.what()};
      case D::HierarchyRequest: return {ErrorCode::HierarchyRequest,error.what()};
      case D::InvalidCharacter: return {ErrorCode::InvalidCharacter,error.what()};
      case D::IndexSize: return {ErrorCode::IndexSize,error.what()};
      case D::NotSupported: return {ErrorCode::NotSupported,error.what()};
      case D::Namespace: return {ErrorCode::Namespace,error.what()};
      case D::NoModification: return {ErrorCode::NoModification,error.what()};
      case D::Syntax: return {ErrorCode::Syntax,error.what()};
      case D::InvalidState: return {ErrorCode::InvalidState,error.what()};
      case D::LimitExceeded: return {ErrorCode::LimitExceeded,error.what()};
    }
  } catch (const std::bad_alloc&) { return {ErrorCode::OutOfMemory,"DOM allocation failed"}; }
  catch (const std::length_error&) { return {ErrorCode::LimitExceeded,"DOM value size exceeded"}; }
  catch (...) { return {ErrorCode::InvalidState,"DOM owner operation failed"}; }
  return {ErrorCode::InvalidState,"DOM owner operation failed"};
}
inline bool Null(NodeKey key) { return key.document == 0 && key.node == 0; }
// Publication-only guard: realm identity adapters update keys after this call.
class IdentityTransferScope final {
 public:
  explicit IdentityTransferScope(bool& value, bool enable=true)
      : value_(value), previous_(value) { value_=value || enable; }
  ~IdentityTransferScope() { value_=previous_; }
 private:
  bool& value_;
  bool previous_;
};
// Attribute qualified names: the prefix (possibly empty) and local name. The
// domain folds ASCII case only for the null namespace on an HTML element, so
// the same split must be used to find an existing attribute before a write.
inline void SplitQualified(DomStringView qualified, DomString* prefix, DomString* local) {
  size_t colon = qualified.find(u':');
  if (colon == DomStringView::npos) { prefix->clear(); local->assign(qualified); return; }
  prefix->assign(qualified.substr(0, colon)); local->assign(qualified.substr(colon + 1));
}
namespace html {
// Charges nodes and UTF-16 units before a creation or text growth; throws on excess.
using Budget = std::function<void(size_t nodes, size_t units)>;
// Applies one token to the DocumentAggregate. Returns the parser-inserted script
// element that the token closed when tree.scripts is set, otherwise 0.
domain::NodeId Build(domain::DocumentAggregate& document, TreeState& tree,
                     Token& token, const Budget& budget);
}
// The HTML fragment entry shared by innerHTML, outerHTML and insertAdjacentHTML:
// one tokenizer/tree-builder path into a fresh unpublished <html> root of
// `owner`, which throws on failure. `tag` is the insertion context element's
// local name and selects the content model. The caller moves the children out
// and then removes the emptied root.
domain::NodeId ParseFragmentRoot(const html::Budget& charge, const Limits& limits,
                                 domain::DocumentAggregate& owner, DomStringView tag,
                                 DomStringView markup, const html::TemplateSink& templates,
                                 html::TagSpace space = html::TagSpace::Html);
// Namespace of a fragment context element (Html for anything else).
html::TagSpace ContextSpace(const domain::DocumentAggregate& owner, domain::NodeId node);
// Local name of the fragment context element; a Document or any other
// non-element context falls back to "html".
DomString ContextTag(const domain::DocumentAggregate& owner, domain::NodeId node);
inline NodeKey Key(DocumentId document, domain::NodeId node) { return node ? NodeKey{document,node} : NodeKey{}; }
inline NodeKey ParsedType(DocumentId document, const domain::DocumentAggregate& owner) {
  const auto ref = owner.parsed_doctype();
  return Key(ref.document ? ref.document : document, ref.node);
}
struct Model::Impl {
  struct Pending { NodeKey root; bool document; };
  Limits limits;
  DocumentId next_document = 0;
  DraftId next_draft = 0;
  uint64_t revision = 1;
  // Bumped by every mutation except an in-place update to an existing style
  // attribute or a no-op attribute write/remove: adding or removing style still
  // changes Element.attributes (NamedNodeMap) membership (StructureRevision).
  uint64_t structure = 1;
  // Bumped by tree changes only (nodes inserted, removed or moved, text and
  // other character data); attribute writes leave it (TreeRevision).
  uint64_t tree = 1;
  // Bumped by tree changes and class-attribute changes (ClassRevision).
  uint64_t classes = 1;
  void Touch() noexcept { ++revision; ++structure; ++tree; ++classes; }
  void TouchClass() noexcept { ++revision; ++structure; ++classes; }
  void TouchAttribute() noexcept { ++revision; ++structure; }
  void TouchStyle() noexcept { ++revision; }
  // Real-change log; see Model::SetMutationRecording. Bounded, oldest first.
  static constexpr size_t kMutationLogLimit = 4096;
  std::deque<MutationNotice> mutations;
  uint64_t mutationSequence = 0, mutationDropped = 0;
  bool mutationRecording = false;
  bool identityTransfer = false;
  void (*mutationSink)(void*) = nullptr;
  void* mutationSinkContext = nullptr;
  void (*scriptSink)(void*, NodeKey, bool) = nullptr;
  void* scriptSinkContext = nullptr;
  void NoteScript(NodeKey key, bool subtree=false) {
    if(scriptSink)scriptSink(scriptSinkContext,key,subtree);
  }
  // Structure-change port; runs on every successful tree edit, recording or
  // not, so a consumer never depends on the observer log being enabled.
  void (*structureSink)(void*, DocumentId, const std::vector<NodeKey>&) = nullptr;
  void* structureSinkContext = nullptr;
  // Wrapper-lifetime port: the same committed change point, carrying both the
  // keys that entered and the keys that left the tree, so a realm can root or
  // unroot one node wrapper without re-deriving connectivity (T41).
  void (*treeSink)(void*, DocumentId, const std::vector<NodeKey>&,
                   const std::vector<NodeKey>&) = nullptr;
  void* treeSinkContext = nullptr;
  // Engine-collection request from the node cap; the owning realm runs one
  // collection and its wrapper reclaim, then the cap is re-checked (T41).
  void (*reclaim)(void*) = nullptr;
  void* reclaimContext = nullptr;
  mutable bool reclaiming = false;
  void NotifyStructure(DocumentId document, const std::vector<NodeKey>& inserted,
                       const std::vector<NodeKey>& removed) {
    if (structureSink) structureSink(structureSinkContext, document, inserted);
    if (treeSink) treeSink(treeSinkContext, document, inserted, removed);
  }
  // Attribute-change port; runs on every committed attribute write, recording
  // or not, so the runtime frame owner never depends on the observer log.
  void (*attributeSink)(void*, DocumentId, NodeKey, DomStringView, DomStringView, const DomString*) = nullptr;
  void* attributeSinkContext = nullptr;
  void (*styleSink)(void*, NodeKey, MutationKind, DomStringView, DomStringView,
                     const DomString*) = nullptr;
  void* styleSinkContext = nullptr;
  void NoteStyle(NodeKey key, MutationKind kind, DomStringView name = {},
                 DomStringView space = {}, const DomString* old = nullptr) {
    if (styleSink) styleSink(styleSinkContext, key, kind, name, space, old);
  }
  // Appends one notice while recording is on; the sink runs after the append.
  void Note(MutationNotice notice);
  // Sibling keys of `node` inside `parent` as the tree is right now. A null
  // out-parameter means the caller does not need that side.
  void Neighbours(const NodeKey& parent, domain::NodeId node,
                  NodeKey* previous, NodeKey* next) const;
  // A successful insert, removal or wholesale replacement. The removal form
  // takes the neighbours the caller read before the node left the tree.
  void NoteInserted(const NodeKey& parent, const std::vector<domain::NodeId>& added);
  void NoteRemoved(const NodeKey& parent, const std::vector<domain::NodeId>& removed,
                   const NodeKey& previous, const NodeKey& next);
  // Same notice with node keys already resolved in the document that owned the
  // removal; used when a cross-document adoption removes a node whose identity
  // now lives in the target document.
  void NoteRemovedKeys(const NodeKey& parent, const std::vector<NodeKey>& removed,
                       const NodeKey& previous, const NodeKey& next);
  void NoteReplace(const NodeKey& parent, const std::vector<domain::NodeId>& added,
                   const std::vector<domain::NodeId>& removed);
  // One attribute write or one CharacterData write, with the previous value.
  void InputTypeChanged(NodeKey key, const DomString* old_type);
  void NoteAttributes(const NodeKey& element, DomStringView name,
                      DomStringView namespace_uri, const DomString* old_value);
  void NoteData(const NodeKey& node, const DomString* old_value);

  std::map<DocumentId,std::unique_ptr<domain::DocumentAggregate>> documents;
  std::map<DocumentId, DocumentId> templateDocuments;  // Original inert document relation.
  NodeKey template_document(NodeKey);
  // A template's content fragment, created on first use (model_template.cpp).
  NodeKey template_content(NodeKey element);
  // Parser step: the closed template's children move into its content.
  void MoveTemplateChildren(NodeKey element);
  // Clone/import step: every template under `from` (owned by `source`) gets a
  // copy of its content in the matching template under `to`.
  void CloneTemplateContents(const Impl& source, NodeKey from, NodeKey to);
  // Content-aware HTML serialization of `node` (outer) or its children.
  DomString serialize(NodeKey node, bool outer) const;
  std::map<DraftId,Pending> drafts;  // Publication lifecycle only, no node state.
  std::map<DocumentId,std::unique_ptr<html::Session>> parsers;  // Active document loads.
  explicit Impl(const Limits& value) : limits(value) {}
  domain::DocumentAggregate& owner(NodeKey key) const {
    auto found = documents.find(key.document);
    if (found == documents.end()) throw domain::DomError(domain::DomErrorCode::NotFound,"Document identity is missing");
    found->second->require(key.node); return *found->second;
  }
  domain::DocumentAggregate& document(NodeKey key) const {
    auto& value = owner(key);
    if (key.node != value.id()) throw domain::DomError(domain::DomErrorCode::InvalidState,"Expected a Document identity");
    return value;
  }
  // The document node's own key for a document identity: MoveIn and the other
  // cross-document entry points need the Document NodeKey, not just its id.
  NodeKey document_key(DocumentId id) const {
    auto found = documents.find(id);
    if (found == documents.end()) throw domain::DomError(domain::DomErrorCode::NotFound,"Document identity is missing");
    return {id, found->second->id()};
  }
  domain::Element& element(NodeKey key) const {
    auto* value = owner(key).get_element(key.node);
    if (!value) throw domain::DomError(domain::DomErrorCode::InvalidState,"Expected an Element identity");
    return *value;
  }
  domain::CharacterData& data(NodeKey key) const {
    auto& value = owner(key).require(key.node);
    if (value.node_type() != domain::NodeType::Text && value.node_type() != domain::NodeType::Comment &&
        value.node_type() != domain::NodeType::ProcessingInstruction)
      throw domain::DomError(domain::DomErrorCode::InvalidState,"Expected a CharacterData identity");
    return static_cast<domain::CharacterData&>(value);
  }
  // Free capacity of the unique tree, recomputed only when the bound below runs
  // out. Nothing but this exact scan raises the bound, so the bound can only
  // underestimate the capacity that is really free: a charge that fits under it
  // cannot exceed a cap. Only the exact scan reports a cap, so the error
  // boundary and the counts a page observes are unchanged, while the hot path
  // stays two subtractions instead of a walk over every live node.
  mutable bool slackValid = false;
  mutable size_t nodeSlack = 0, unitSlack = 0;
  bool Rescan(size_t added_nodes, size_t added_units, size_t removed_units) const {
    size_t nodes = 0, units = 0;
    for (const auto& [key, owner] : documents) { nodes += owner->node_count(); units += owner->storage_units(); }
    slackValid = true;
    nodeSlack = nodes <= limits.nodes ? limits.nodes - nodes : 0;
    unitSlack = units <= limits.text_units ? limits.text_units - units : 0;
    if (removed_units > units || nodes > limits.nodes || added_nodes > limits.nodes - nodes ||
        units - removed_units > limits.text_units ||
        added_units > limits.text_units - (units - removed_units)) return false;
    nodeSlack -= added_nodes;
    unitSlack -= added_units;
    return true;
  }
  // The caps bound live nodes, but a wrapper script dropped is only collected
  // when the engine runs: one collection - and the owner reclaim that follows -
  // runs before a cap is reported, so an unreachable node never counts (T41).
  void budget(size_t added_nodes, size_t added_units, size_t removed_units = 0) const {
    if (slackValid && added_nodes <= nodeSlack && added_units <= unitSlack) {
      nodeSlack -= added_nodes;
      unitSlack -= added_units;
      return;
    }
    if (Rescan(added_nodes, added_units, removed_units)) return;
    if (reclaim && !reclaiming) {
      reclaiming = true;
      reclaim(reclaimContext);
      reclaiming = false;
      if (Rescan(added_nodes, added_units, removed_units)) return;
    }
    throw domain::DomError(domain::DomErrorCode::LimitExceeded,"DOM owner resource limit exceeded");
  }
  DocumentId allocate_document() {
    if (documents.size() >= limits.documents || next_document == UINT64_MAX)
      throw domain::DomError(domain::DomErrorCode::LimitExceeded,"Document limit exceeded");
    return ++next_document;
  }
  Draft remember(NodeKey root, bool is_document) {
    try {
      if (next_draft == UINT64_MAX) throw domain::DomError(domain::DomErrorCode::LimitExceeded,"DOM draft limit exceeded");
      DraftId token = ++next_draft; drafts.emplace(token,Pending{root,is_document}); return {token,root};
    } catch (...) {
      if (is_document) documents.erase(root.document); else owner(root).remove_node(root.node);
      throw;
    }
  }
};
}
