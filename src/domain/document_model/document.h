#pragma once
#include "domain/shared/aggregate_root.h"
#include "domain/document_model/element.h"
#include "domain/document_model/document_metadata.h"
#include "domain/document_model/document_type.h"
#include "domain/document_model/shadow_root.h"
#include <functional>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace ruyijs::domain::document_model {
enum class ReadyState { Loading, Interactive, Complete };
inline const char* to_string(ReadyState state) {
  switch (state) { case ReadyState::Loading: return "loading"; case ReadyState::Interactive: return "interactive";
    case ReadyState::Complete: return "complete"; } return "unknown";
}
class DocumentAggregate : public shared::AggregateRoot<NodeId> {
 public:
  explicit DocumentAggregate(NodeId id = 1, DocumentMode mode = DocumentMode::Html,
                             bool presented = false)
      : AggregateRoot<NodeId>(id), id_counter_(id), mode_(mode), presented_(presented) {
    nodes_.emplace(id, std::make_unique<Node>(id, NodeType::Document));
  }
  DocumentMode mode() const noexcept { return mode_; }
  bool uses_html_syntax() const noexcept { return mode_ == DocumentMode::Html || metadata_.parsed_html_syntax(); }
  const DocumentMetadata& metadata() const noexcept { return metadata_; }
  DocumentMetadata& metadata() noexcept { return metadata_; }
  // Presentation belongs to this document, never inferred from URI or HTML mode.
  // New and cloned documents have no browsing context. Focus remains a separate capability.
  bool hidden() const noexcept { return !presented_; }
  NodeId active_element_id() const noexcept;
  // The focused element of this document; focused_id() is 0 once it left the
  // document's tree, and active_element_id() then falls back to the body.
  void set_focused(NodeId id) noexcept { focused_id_ = id; }
  NodeId focused_id() const noexcept;
  // Only the public createDocument factory and cloning stored names bypass
  // validation. Ordinary element creation retains its original name contract.
  NodeId create_element(const TagName& tag, bool validate = true) {
    if (validate && !valid_element_local_name(tag.local16())) throw DomError(DomErrorCode::InvalidCharacter, "Invalid element name");
    TagName stored = tag;
    stored.set_document_html(uses_html_syntax());
    NodeId key = next_id(); nodes_.emplace(key, std::make_unique<Element>(key, std::move(stored))); return key;
  }
  NodeId create_element16(DomStringView name) {
    return create_element(TagName(DomString(name), uses_html_syntax(), metadata_.parsed_html_syntax()));
  }
  NodeId create_text_node16(DomStringView data) {
    NodeId key = next_id(); nodes_.emplace(key, std::make_unique<TextNode>(key, DomString(data))); return key;
  }
  NodeId create_comment16(DomStringView data) {
    NodeId key = next_id(); nodes_.emplace(key, std::make_unique<CommentNode>(key, DomString(data))); return key;
  }
  NodeId create_document_type16(DomStringView name, bool cloneable,
                                DomStringView public_id = {}, DomStringView system_id = {}) {
    NodeId key = next_id(); nodes_.emplace(key, std::make_unique<DocumentTypeNode>(key, name, cloneable, public_id, system_id)); return key;
  }
  NodeId create_processing_instruction16(DomStringView target, DomStringView data) {
    NodeId key = next_id(); nodes_.emplace(key, std::make_unique<ProcessingInstructionNode>(key, DomString(target), DomString(data))); return key;
  }
  ParsedDoctypeRef parsed_doctype() const noexcept { return parsed_doctype_; }
  void set_parsed_doctype(ParsedDoctypeRef value) noexcept { parsed_doctype_ = value; }
  // DocumentFragment has no children of its own identity rules; it is only a
  // temporary insertion container, so the factory adds no validation.
  NodeId create_fragment() {
    NodeId key = next_id(); nodes_.emplace(key, std::make_unique<Node>(key, NodeType::DocumentFragment)); return key;
  }
  NodeId attach_shadow_root(NodeId host, ShadowOptions options);
  const ShadowRootNode* shadow_node(NodeId id) const {
    const auto* node = get_node(id);
    return node && node->shadow_host_id() ? static_cast<const ShadowRootNode*>(node) : nullptr;
  }
  NodeId composed_root_id(NodeId id) const;
  std::vector<NodeId> owned_descendants(NodeId root, bool include_root = false) const;
  NodeId create_text_node(const std::string& data) { return create_text_node16(from_utf8(data)); }
  NodeId create_comment(const std::string& data) { return create_comment16(from_utf8(data)); }
  Node* get_node(NodeId id) { auto it = nodes_.find(id); return it == nodes_.end() ? nullptr : it->second.get(); }
  const Node* get_node(NodeId id) const { auto it = nodes_.find(id); return it == nodes_.end() ? nullptr : it->second.get(); }
  Element* get_element(NodeId id) {
    Node* n = get_node(id); return n && n->node_type() == NodeType::Element ? static_cast<Element*>(n) : nullptr;
  }
  const Element* get_element(NodeId id) const {
    const Node* n = get_node(id); return n && n->node_type() == NodeType::Element ? static_cast<const Element*>(n) : nullptr;
  }
  Node& require(NodeId id) { auto* n = get_node(id); if (!n) throw DomError(DomErrorCode::NotFound, "Node does not exist"); return *n; }
  const Node& require(NodeId id) const { auto* n = get_node(id); if (!n) throw DomError(DomErrorCode::NotFound, "Node does not exist"); return *n; }
  void append_child(NodeId parent, NodeId child) { insert_before(parent, child, 0); }
  void insert_before(NodeId parent, NodeId child, NodeId before);
  void replace_child(NodeId parent, NodeId child, NodeId old_child);
  void remove_child(NodeId parent, NodeId child);
  void ensure_append_validity(NodeId parent, const DocumentAggregate& source, NodeId child) const;
  void commit_child_list(NodeId parent, NodeId child, std::vector<NodeId>& children);
  void remove_node(NodeId id) noexcept;
  NodeId root_id(NodeId node) const {
    const Node* current = &require(node); while (current->parent_id()) current = &require(current->parent_id()); return current->id();
  }
  std::vector<NodeId> descendants(NodeId root, bool include_root = false) const;
  NodeId get_element_by_id(const std::string& value) const;
  std::vector<NodeId> get_elements_by_tag(const TagName& tag) const;
  std::vector<NodeId> get_elements_by_class(const std::string& name) const;
  std::vector<NodeId> query16(NodeId root, unsigned kind, DomStringView value) const;
  // getElementById for this document. `previous` is the node's old id.
  void note_element_id(NodeId node, DomStringView previous, DomStringView next);
  // True when the index knows the answer. A duplicate id is not indexed.
  bool lookup_element_id(DomStringView value, NodeId* hit) const;
  // Namespaced tag query: "*" matches any namespace and any local name, an
  // empty namespace matches elements outside the XHTML namespace.
  std::vector<NodeId> query_ns16(NodeId root, DomStringView namespace_uri, DomStringView local) const;
  DomString text_content16(NodeId node) const;
  void set_text_content16(NodeId node, DomStringView value);
  ReadyState ready_state() const noexcept { return ready_state_; }
  void set_ready_state(ReadyState state) noexcept { ready_state_ = state; }
  // Main URL uses an external port; cloned documents own immutable URI snapshots.
  // Legacy cookie metadata is retained but is not a production cookie owner.
  // document.title: the child text content of the head's first title element.
  // HTML documents only; the tree is the only source, so set_title16 creates the
  // element (and a head, when the html element exists) if it is missing.
  DomString title16() const;
  void set_title16(DomStringView value);
  std::string url() const { return url_ ? to_utf8(*url_) : std::string{}; }
  void set_url(std::string value) { url_ = from_utf8(value); }
  const std::optional<DomString>& url16() const noexcept { return url_; }
  void set_url16(DomStringView value) { url_ = DomString(value); }
  void use_external_url() noexcept { url_.reset(); }
  std::string cookie() const { return to_utf8(cookie_); }
  void set_cookie(std::string value) { cookie_ = from_utf8(value); }
  NodeId document_element_id() const noexcept;
  NodeId head_id() const noexcept;
  NodeId body_id() const noexcept;
  void set_document_element(NodeId node) {
    auto old = document_element_id(); if (old) replace_child(id(), node, old); else append_child(id(), node);
  }
  void set_head(NodeId node) { append_child(document_element_id(), node); }
  void set_body(NodeId node) { append_child(document_element_id(), node); }
  NodeId current_script_id() const noexcept { return current_script_id_; }
  void set_current_script(NodeId node) noexcept { current_script_id_ = node; }
  size_t node_count() const noexcept { return nodes_.size(); }
  void template_links(std::vector<std::pair<NodeId, TemplateContentRef>>* out) const {
    for (const auto& [id, node] : nodes_) {
      if (node->node_type() != NodeType::Element) continue;
      const auto ref = static_cast<const Element*>(node.get())->template_content();
      if (ref.node) out->emplace_back(id, ref);
    }
  }
  void rekey_template_content(TemplateContentRef from, TemplateContentRef to) noexcept {
    for (auto& [id, node] : nodes_) {
      if (node->node_type() != NodeType::Element) continue;
      auto* element = static_cast<Element*>(node.get());
      if (element->template_content() == from) element->set_template_content(to);
    }
  }
  size_t storage_units() const noexcept;
 private:
  void validate_parent(NodeId parent, NodeId child) const;
  void validate_insert(NodeId parent, NodeId child, NodeId replaced) const;
  // Pre-insertion of every child of a DocumentFragment at one position, with
  // one atomic validation pass; the fragment is emptied but never destroyed.
  void insert_fragment(NodeId parent, NodeId fragment, NodeId before, NodeId replaced);
  NodeId next_id() {
    if (id_counter_ == std::numeric_limits<NodeId>::max()) throw DomError(DomErrorCode::LimitExceeded, "Node identity limit exceeded");
    return ++id_counter_;
  }
  NodeId html_child(const char16_t* tag) const noexcept;
  NodeId id_counter_, current_script_id_ = 0, focused_id_ = 0;
  DocumentMode mode_;
  const bool presented_;
  ReadyState ready_state_ = ReadyState::Loading;
  DomString cookie_;
  DocumentMetadata metadata_;
  ParsedDoctypeRef parsed_doctype_;
  std::optional<DomString> url_{DomString{}};
  std::unordered_map<NodeId, std::unique_ptr<Node>> nodes_;
  std::unordered_map<DomString, NodeId> element_ids_;
  std::unordered_set<DomString> ambiguous_ids_;
};
inline void DocumentAggregate::note_element_id(NodeId node, DomStringView previous, DomStringView next) {
  if (!previous.empty()) {
    auto found = element_ids_.find(DomString(previous));
    if (found != element_ids_.end() && found->second == node) element_ids_.erase(found);
    else if (found != element_ids_.end()) ambiguous_ids_.insert(DomString(previous));
  }
  if (next.empty()) return;
  auto found = element_ids_.find(DomString(next));
  if (found == element_ids_.end()) { element_ids_.emplace(DomString(next), node); return; }
  if (found->second == node) return;
  element_ids_.erase(found);
  ambiguous_ids_.insert(DomString(next));
}
inline bool DocumentAggregate::lookup_element_id(DomStringView value, NodeId* hit) const {
  if (value.empty() || ambiguous_ids_.count(DomString(value))) return false;
  auto found = element_ids_.find(DomString(value));
  if (found == element_ids_.end()) return false;
  const auto* element = get_element(found->second);
  if (!element) return false;
  const auto id = element->get_attribute16(u"id");
  if (!id || DomStringView(*id) != value) return false;
  *hit = found->second;
  return true;
}
}
#include "domain/document_model/document_tree.h"
#include "domain/document_model/document_replace.h"
#include "domain/document_model/document_shadow.h"
#include "domain/document_model/document_query.h"
#include "domain/document_model/document_text.h"
