#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::Inspect(NodeKey key, NodeSnapshot* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key); const auto& node = owner.require(key.node);
    NodeSnapshot value; value.key = key; value.kind = NodeKind(node.node_type());
    value.document_mode = owner.mode() == domain::DocumentMode::Html ? DocumentMode::Html : DocumentMode::Xml;
    value.node_name = node.node_name16(); value.node_value = node.node_value16();
    value.owner_document = node.node_type() == domain::NodeType::Document ? NodeKey{} : Key(key.document,owner.id());
    value.parent = Key(key.document,node.parent_id());
    value.parent_element = owner.get_element(node.parent_id()) ? value.parent : NodeKey{};
    value.first_child = Key(key.document,node.first_child_id()); value.last_child = Key(key.document,node.last_child_id());
    value.child_count = node.child_count(); value.root = Key(key.document,owner.root_id(key.node));
    value.composed_root = Key(key.document, owner.composed_root_id(key.node));
    value.is_connected = value.composed_root.node == owner.id();
    value.shadow_host = Key(key.document, node.shadow_host_id());
    value.shadow_root = Key(key.document, node.shadow_root_id());
    if (node.parent_id()) {
      const auto& parent = owner.require(node.parent_id()); size_t index = parent.index_of_child(key.node);
      if (index) value.previous_sibling = Key(key.document,parent.child_ids()[index-1]);
      if (index+1 < parent.child_count()) value.next_sibling = Key(key.document,parent.child_ids()[index+1]);
    }
    if (const auto* element = owner.get_element(key.node)) {
      value.local_name = element->tag_name().local16(); value.is_html_element = element->tag_name().html();
      if (!element->tag_name().namespace16().empty()) value.namespace_uri = DomString(element->tag_name().namespace16());
      if (!element->tag_name().prefix16().empty()) value.prefix = DomString(element->tag_name().prefix16());
      value.interface_name = domain::ElementFactory::interface_name_static(element->tag_name());
    } else if (value.kind == NodeKind::Document) {
      value.interface_name = value.document_mode == DocumentMode::Html ? "HTMLDocument"
          : owner.metadata().xml_parsed() ? "XMLDocument" : "Document";
    } else if (value.kind == NodeKind::DocumentFragment) {
      value.interface_name = node.shadow_host_id() ? "ShadowRoot" : "DocumentFragment";
    } else if (value.kind == NodeKind::DocumentType) {
      value.interface_name = "DocumentType";
      value.cloneable = static_cast<const domain::DocumentTypeNode&>(node).cloneable();
      value.public_id = static_cast<const domain::DocumentTypeNode&>(node).public_id();
      value.system_id = static_cast<const domain::DocumentTypeNode&>(node).system_id();
    } else if (value.kind == NodeKind::ProcessingInstruction) {
      value.interface_name = "ProcessingInstruction";
      value.data_length = static_cast<const domain::CharacterData&>(node).length();
    } else {
      value.interface_name = value.kind == NodeKind::Text ? "Text" : "Comment";
      value.data_length = static_cast<const domain::CharacterData&>(node).length();
    }
    *result = std::move(value);
  });
}
Status Model::InspectLayout(NodeKey key, NodeSnapshot* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    const auto& node = owner.require(key.node);
    NodeSnapshot value;
    value.key = key;
    value.kind = NodeKind(node.node_type());
    value.parent = Key(key.document, node.parent_id());
    value.composed_root = Key(key.document, owner.composed_root_id(key.node));
    value.is_connected = value.composed_root.node == owner.id();
    value.shadow_host = Key(key.document, node.shadow_host_id());
    value.shadow_root = Key(key.document, node.shadow_root_id());
    if (const auto* element = owner.get_element(key.node)) {
      value.local_name = element->tag_name().local16();
    } else if (value.kind == NodeKind::Text || value.kind == NodeKind::Comment ||
               value.kind == NodeKind::ProcessingInstruction) {
      value.node_value = node.node_value16();
    }
    *result = std::move(value);
  });
}
Status Model::InspectDocument(NodeKey key, DocumentSnapshot* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().document(key); DocumentSnapshot value;
    value.key = key; value.mode = owner.mode() == domain::DocumentMode::Html ? DocumentMode::Html : DocumentMode::Xml;
    value.document_element = Key(key.document,owner.document_element_id());
    value.content_type = owner.metadata().content_type(value.mode == DocumentMode::Html);
    value.head = Key(key.document,owner.head_id()); value.body = Key(key.document,owner.body_id());
    value.active_element = Key(key.document,owner.active_element_id()); value.hidden = owner.hidden();
    value.current_script = Key(key.document,owner.current_script_id());
    value.ready_state = ReadyState(owner.ready_state()); value.url = owner.url16();
    value.xml_parsed = owner.metadata().xml_parsed(); *result = std::move(value);
  });
}
Status Model::Children(NodeKey key, std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    std::vector<NodeKey> values;
    for (auto child : State().owner(key).require(key.node).child_ids()) values.push_back(Key(key.document,child));
    result->swap(values);
  });
}
Status Model::AncestorPath(NodeKey key, std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    auto& owner = State().owner(key); std::vector<NodeKey> path;
    for (auto node = key.node; node; node = owner.require(node).parent_id()) path.push_back(Key(key.document,node));
    result->swap(path);
  });
}
Status Model::Contains(NodeKey key, NodeKey other, bool* result) const noexcept {
  return Protect([&] {
    auto& owner = State().owner(key);
    bool found = false;
    if (!Null(other)) {
      State().owner(other);
      if (key.document == other.document)
        for (auto node = other.node; node; node = owner.require(node).parent_id()) if (node == key.node) { found = true; break; }
    }
    *result = found;
  });
}
Status Model::IsEqual(NodeKey key, NodeKey other, bool* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    *result = !Null(other) && domain::DomTreeService::is_equal(owner,key.node,State().owner(other),other.node);
  });
}
Status Model::Query(NodeKey key, QueryKind kind, DomStringView value, std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    std::vector<NodeKey> output;
    for (auto node : State().owner(key).query16(key.node,unsigned(kind),value)) output.push_back(Key(key.document,node));
    result->swap(output);
  });
}
}
