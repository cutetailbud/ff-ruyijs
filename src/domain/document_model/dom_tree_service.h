#pragma once
#include <functional>
#include "domain/document_model/document.h"
#include "domain/document_model/element_factory.h"
#include "domain/document_model/dom_ports.h"
#include "domain/document_model/xml_serialization.h"

namespace ruyijs::domain::document_model {
class DomTreeService {
 public:
  static DomString compute_text_content16(const DocumentAggregate& doc, NodeId node) { return doc.text_content16(node); }
  static std::string compute_text_content(const DocumentAggregate& doc, NodeId node) { return to_utf8(compute_text_content16(doc, node)); }
  // Finds the aggregate of a template's content document; without one a
  // template serializes its own children.
  using ContentResolver = std::function<const DocumentAggregate*(uint64_t document)>;
  static DomString serialize_outer16(const DocumentAggregate& root_doc, NodeId root,
                                     const ContentResolver* contents = nullptr) {
    if (!root_doc.uses_html_syntax()) return serialize_xml(root_doc, root);
    struct Entry { const DocumentAggregate* doc; NodeId key; bool close; };
    std::vector<Entry> stack{{&root_doc, root, false}}; DomString output;
    while (!stack.empty()) {
      auto item = stack.back(); stack.pop_back();
      const DocumentAggregate& doc = *item.doc; const auto& node = doc.require(item.key);
      if (item.close) { output += doc.get_element(item.key)->serialize_close_tag16(); continue; }
      if (node.node_type() == NodeType::Text) {
        const auto* parent = doc.get_element(node.parent_id()); bool raw = false;
        if (parent && parent->tag_name().html()) {
          const char16_t* names[] = {u"script",u"style",u"xmp",u"iframe",u"noembed",u"noframes",u"plaintext"};
          for (auto* name : names) if (parent->tag_name().local16() == name) raw = true;
        }
        output += raw ? node.text_content16() : escape_markup(node.text_content16(), false); continue;
      }
      if (node.node_type() == NodeType::Comment) { output += u"<!--" + node.text_content16() + u"-->"; continue; }
      if (node.node_type() == NodeType::ProcessingInstruction) {
        const auto& pi = static_cast<const ProcessingInstructionNode&>(node);
        output += u"<?" + pi.target() + u" " + pi.text_content16() + u">"; continue;  // HTML serialization
      }
      if (node.node_type() == NodeType::DocumentType) { output += u"<!DOCTYPE " + node.node_name16() + u">"; continue; }
      if (const auto* element = doc.get_element(item.key)) {
        output += element->serialize_open_tag16(); if (element->tag_name().is_void()) continue;
        stack.push_back({item.doc, item.key, true});
        // A template's markup is its content (HTML fragment serialization).
        const auto ref = element->template_content();
        const DocumentAggregate* inert = contents && ref.node && element->tag_name().html() &&
            element->tag_name().local16() == u"template" ? (*contents)(ref.document) : nullptr;
        if (inert) {
          const auto& children = inert->require(ref.node).child_ids();
          for (auto it = children.rbegin(); it != children.rend(); ++it) stack.push_back({inert, *it, false});
          continue;
        }
      }
      for (auto it = node.child_ids().rbegin(); it != node.child_ids().rend(); ++it) stack.push_back({item.doc, *it, false});
    }
    return output;
  }
  static DomString serialize_inner16(const DocumentAggregate& doc, NodeId root,
                                     const ContentResolver* contents = nullptr) {
    DomString result; for (auto key : doc.require(root).child_ids()) result += serialize_outer16(doc, key, contents); return result;
  }
  static std::string serialize_outer(const DocumentAggregate& doc, NodeId node) { return to_utf8(serialize_outer16(doc, node)); }
  static std::string serialize_inner(const DocumentAggregate& doc, NodeId node) { return to_utf8(serialize_inner16(doc, node)); }
  static std::vector<NodeId> query_all(const DocumentAggregate& doc, NodeId root,
                                      const std::function<bool(const Element&)>& predicate) {
    std::vector<NodeId> result;
    for (NodeId id : doc.descendants(root, true)) if (auto* e = doc.get_element(id); e && predicate(*e)) result.push_back(id);
    return result;
  }
  static std::vector<NodeId> cloned_nodes(const DocumentAggregate& source, NodeId root,
                                         bool deep, bool transfer = false) {
    struct Entry { NodeId key; bool children; };
    std::vector<Entry> pending{{root, deep}};
    std::vector<NodeId> result;
    while (!pending.empty()) {
      auto item = pending.back(); pending.pop_back(); result.push_back(item.key);
      const auto& node = source.require(item.key);
      const auto* shadow = source.shadow_node(node.shadow_root_id());
      if (shadow && (transfer || shadow->options().clonable))
        pending.push_back({shadow->id(), true});
      if (item.children) for (auto it = node.child_ids().rbegin(); it != node.child_ids().rend(); ++it)
        pending.push_back({*it, true});
    }
    return result;
  }
  static NodeId clone_into(const DocumentAggregate& source, NodeId root, DocumentAggregate& target,
                           bool deep = true, bool transfer = false) {
    NodeId result = clone_one(source, root, target);
    try {
      struct Entry { NodeId source, target; bool children; };
      std::vector<Entry> stack{{root, result, deep}};
      while (!stack.empty()) {
        auto pair = stack.back(); stack.pop_back();
        const auto& node = source.require(pair.source);
        const auto* shadow = source.shadow_node(node.shadow_root_id());
        if (shadow && (transfer || shadow->options().clonable)) {
          auto options = shadow->options();
          if (!transfer) options.serializable = false;  // Firefox157 cloning behavior.
          NodeId copy = target.attach_shadow_root(pair.target, options);
          stack.push_back({shadow->id(), copy, true});
        }
        const auto children = node.child_ids();
        if (pair.children) for (NodeId child : children) {
          NodeId copy = clone_one(source, child, target);
          try { target.append_child(pair.target, copy); }
          catch (...) { target.remove_node(copy); throw; }
          stack.push_back({child, copy, true});
        }
      }
      return result;
    } catch (...) { target.remove_node(result); throw; }
  }
  static NodeId clone_subtree(DocumentAggregate& doc, NodeId root) { return clone_into(doc, root, doc); }
  static bool is_equal(const DocumentAggregate& left, NodeId a, const DocumentAggregate& right, NodeId b) {
    std::vector<std::pair<NodeId, NodeId>> pending{{a, b}};
    while (!pending.empty()) {
      auto [x, y] = pending.back(); pending.pop_back();
      const auto& first = left.require(x); const auto& second = right.require(y);
      if (first.node_type() != second.node_type() || first.child_count() != second.child_count()) return false;
      if (first.node_type() == NodeType::DocumentType) {
        const auto& x = static_cast<const DocumentTypeNode&>(first);
        const auto& y = static_cast<const DocumentTypeNode&>(second);
        if (x.node_name16() != y.node_name16() || x.public_id() != y.public_id() || x.system_id() != y.system_id()) return false;
      }
      if (first.node_type() == NodeType::ProcessingInstruction) {
        const auto& x = static_cast<const ProcessingInstructionNode&>(first);
        const auto& y = static_cast<const ProcessingInstructionNode&>(second);
        if (x.target() != y.target()) return false;
      }
      if (auto* element = left.get_element(x)) {
        const auto* other = right.get_element(y);
        if (element->tag_name() != other->tag_name() || element->attribute_count() != other->attribute_count()) return false;
        for (const auto& attr : element->attributes16())
          if (other->get_attribute_ns16(attr.namespace_uri, attr.local_name) != attr.value) return false;
      } else if (first.node_value16() != second.node_value16()) return false;
      for (size_t i = 0; i < first.child_count(); ++i) pending.emplace_back(first.child_ids()[i], second.child_ids()[i]);
    }
    return true;
  }
  // Merges adjacent CharacterData siblings of the same node type and drops the
  // ones left empty, in tree order for the whole subtree. Text and Comment
  // merge only with their own kind, so a Text never absorbs a Comment.
  static void normalize(DocumentAggregate& doc, NodeId root) {
    std::vector<NodeId> stack{root};
    while (!stack.empty()) {
      const NodeId key = stack.back(); stack.pop_back();
      NodeId previous = 0;
      // A copy: merging or dropping a child mutates the parent's child list.
      const std::vector<NodeId> children = doc.require(key).child_ids();
      for (NodeId child : children) {
        const auto type = doc.require(child).node_type();
        if (type != NodeType::Text && type != NodeType::Comment) {
          previous = 0; stack.push_back(child); continue;
        }
        auto& data = static_cast<CharacterData&>(doc.require(child));
        if (previous && doc.require(previous).node_type() == type) {
          auto& kept = static_cast<CharacterData&>(doc.require(previous));
          kept.set_node_value16(kept.data16() + data.data16());
          doc.remove_node(child); continue;
        }
        if (data.data16().empty()) { doc.remove_node(child); continue; }
        previous = child;
      }
    }
  }
  // DOM compareDocumentPosition bitmask for other relative to node: 0 for the
  // same node, ContainedBy|Following (20) when other is a descendant of node,
  // Contains|Preceding (10) when other is an ancestor, Following (4) or
  // Preceding (2) in tree order for two siblings, and Disconnected|
  // ImplementationSpecific|Following (37) for two different trees.
  static uint32_t compare_document_position(const DocumentAggregate& doc, NodeId a, NodeId b) {
    if (a == b) return 0;
    const auto path = [&](NodeId node) {
      std::vector<NodeId> result;
      for (NodeId current = node; current; current = doc.require(current).parent_id())
        result.push_back(current);
      std::reverse(result.begin(), result.end());
      return result;
    };
    const std::vector<NodeId> first = path(a), second = path(b);
    if (first.front() != second.front()) return 1 | 32 | 4;
    size_t shared = 0;
    while (shared < first.size() && shared < second.size() && first[shared] == second[shared]) ++shared;
    if (shared == first.size()) return 16 | 4;   // Other is contained by node.
    if (shared == second.size()) return 8 | 2;    // Other is an ancestor of node.
    const Node& parent = doc.require(first[shared - 1]);
    return parent.index_of_child(first[shared]) < parent.index_of_child(second[shared]) ? 4 : 2;
  }
 private:
  static NodeId clone_one(const DocumentAggregate& source, NodeId key, DocumentAggregate& target) {
    const auto& node = source.require(key); NodeId copy;
    if (node.shadow_host_id()) throw DomError(DomErrorCode::NotSupported, "A ShadowRoot cannot be cloned directly");
    if (const auto* element = source.get_element(key)) {
      copy = target.create_element(element->tag_name(), false);
      target.get_element(copy)->set_is_value(element->is_value());
      target.get_element(copy)->script_state().started = element->script_state().started;
      try { for (const auto& a : element->attributes16())
              target.get_element(copy)->set_attribute_ns16(a.namespace_uri, a.name, a.value);
        // Attributes are copied directly, so an id on the clone is a new holder.
        if (const auto ident = target.get_element(copy)->get_attribute16(u"id"))
          target.note_element_id(copy, {}, *ident);
        if (element->tag_name().html() && element->tag_name().local16() == u"input") {
          target.get_element(copy)->input_current_value() = element->input_current_value();
          auto& state = target.get_element(copy)->input_check_state();
          state = element->input_check_state();
          if (!state.dirty) state.checked.reset();
        }
      }
      catch (...) { target.remove_node(copy); throw; }
    } else if (node.node_type() == NodeType::Text) copy = target.create_text_node16(node.text_content16());
    else if (node.node_type() == NodeType::Comment) copy = target.create_comment16(node.text_content16());
    else if (node.node_type() == NodeType::DocumentFragment) copy = target.create_fragment();
    else if (node.node_type() == NodeType::DocumentType) {
      const auto& type = static_cast<const DocumentTypeNode&>(node);
      copy = target.create_document_type16(type.node_name16(), type.cloneable(), type.public_id(), type.system_id());
    } else if (node.node_type() == NodeType::ProcessingInstruction) {
      const auto& pi = static_cast<const ProcessingInstructionNode&>(node);
      copy = target.create_processing_instruction16(pi.target(), pi.text_content16());
    }
    else throw DomError(DomErrorCode::NotSupported, "Node kind requires a document clone factory");
    return copy;
  }
};
}
