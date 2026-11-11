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
Status Model::Kind(NodeKey key, NodeKind* result) const noexcept {
  return Protect([&] {
    *result = NodeKind(State().owner(key).require(key.node).node_type());
  });
}
Status Model::NodeName(NodeKey key, DomString* result) const noexcept {
  return Protect([&] {
    *result = State().owner(key).require(key.node).node_name16();
  });
}
Status Model::NodeValue(NodeKey key, std::optional<DomString>* result) const noexcept {
  return Protect([&] {
    *result = State().owner(key).require(key.node).node_value16();
  });
}
Status Model::NodeLink(NodeKey key, NodeRelation relation, NodeKey* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    const auto& node = owner.require(key.node);
    domain::NodeId found = 0;
    switch (relation) {
      case NodeRelation::OwnerDocument:
        found = node.node_type() == domain::NodeType::Document ? 0 : owner.id();
        break;
      case NodeRelation::BaseDocument:
        found = node.node_type() == domain::NodeType::Document ? key.node : owner.id();
        break;
      case NodeRelation::Parent:
        found = node.parent_id();
        break;
      case NodeRelation::ParentElement: {
        const auto parent = node.parent_id();
        found = parent && owner.get_element(parent) ? parent : 0;
        break;
      }
      case NodeRelation::FirstChild:
        found = node.first_child_id();
        break;
      case NodeRelation::LastChild:
        found = node.last_child_id();
        break;
      case NodeRelation::PreviousSibling:
      case NodeRelation::NextSibling:
        if (const auto parent_id = node.parent_id()) {
          const auto& parent = owner.require(parent_id);
          const int index = parent.index_of_child(key.node);
          if (index >= 0) {
            if (relation == NodeRelation::PreviousSibling && index > 0)
              found = parent.child_ids()[size_t(index - 1)];
            else if (relation == NodeRelation::NextSibling && size_t(index + 1) < parent.child_count())
              found = parent.child_ids()[size_t(index + 1)];
          }
        }
        break;
      case NodeRelation::Root:
        found = owner.root_id(key.node);
        break;
      case NodeRelation::ComposedRoot:
        found = owner.composed_root_id(key.node);
        break;
      case NodeRelation::FirstElementChild:
        for (auto child : node.child_ids())
          if (owner.get_element(child)) { found = child; break; }
        break;
      case NodeRelation::LastElementChild: {
        const auto& kids = node.child_ids();
        for (size_t i = kids.size(); i > 0; --i)
          if (owner.get_element(kids[i - 1])) { found = kids[i - 1]; break; }
        break;
      }
      case NodeRelation::PreviousElementSibling:
      case NodeRelation::NextElementSibling:
        if (const auto parent_id = node.parent_id()) {
          const auto& parent = owner.require(parent_id);
          const int index = parent.index_of_child(key.node);
          const auto& kids = parent.child_ids();
          if (index >= 0) {
            if (relation == NodeRelation::PreviousElementSibling) {
              for (size_t i = size_t(index); i > 0; --i)
                if (owner.get_element(kids[i - 1])) { found = kids[i - 1]; break; }
            } else {
              for (size_t i = size_t(index) + 1; i < kids.size(); ++i)
                if (owner.get_element(kids[i])) { found = kids[i]; break; }
            }
          }
        }
        break;
    }
    *result = Key(key.document, found);
  });
}
Status Model::IsConnected(NodeKey key, bool* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    owner.require(key.node);
    *result = owner.composed_root_id(key.node) == owner.id();
  });
}
Status Model::HasChildren(NodeKey key, bool* result) const noexcept {
  return Protect([&] {
    *result = State().owner(key).require(key.node).has_children();
  });
}
Status Model::ChildElementCount(NodeKey key, size_t* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    const auto& node = owner.require(key.node);
    size_t count = 0;
    for (auto child : node.child_ids())
      if (owner.get_element(child)) ++count;
    *result = count;
  });
}
Status Model::ElementTag(NodeKey key, ElementTagField field, std::optional<DomString>* result) const noexcept {
  return Protect([&] {
    const auto& tag = State().element(key).tag_name();
    switch (field) {
      case ElementTagField::TagName:
        *result = tag.value16();
        break;
      case ElementTagField::LocalName:
        *result = tag.local16();
        break;
      case ElementTagField::Prefix:
        *result = tag.prefix16().empty() ? std::nullopt : std::optional<DomString>(DomString(tag.prefix16()));
        break;
      case ElementTagField::NamespaceURI:
        *result = tag.namespace16().empty() ? std::nullopt : std::optional<DomString>(DomString(tag.namespace16()));
        break;
    }
  });
}
Status Model::HasAttributes(NodeKey key, bool* result) const noexcept {
  return Protect([&] {
    *result = !State().element(key).attributes16().empty();
  });
}
Status Model::DataLength(NodeKey key, size_t* result) const noexcept {
  return Protect([&] {
    const auto& node = State().owner(key).require(key.node);
    *result = static_cast<const domain::CharacterData&>(node).length();
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
Status Model::PeekElement(NodeKey key, NodeSnapshot* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    const auto& node = owner.require(key.node);
    NodeSnapshot value;
    value.key = key;
    value.kind = NodeKind(node.node_type());
    value.owner_document = node.node_type() == domain::NodeType::Document
        ? NodeKey{} : Key(key.document, owner.id());
    value.composed_root = Key(key.document, owner.composed_root_id(key.node));
    value.is_connected = value.composed_root.node == owner.id();
    if (const auto* element = owner.get_element(key.node)) {
      value.local_name = element->tag_name().local16();
      value.is_html_element = element->tag_name().html();
    }
    *result = std::move(value);
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
Status Model::SheetElements(NodeKey key, std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    std::vector<NodeKey> output;
    for (auto node : owner.descendants(key.node)) {
      const auto* element = owner.get_element(node);
      if (!element) continue;
      const auto local = element->tag_name().local16();
      if (local == u"style" || (element->tag_name().html() && local == u"link"))
        output.push_back(Key(key.document, node));
    }
    result->swap(output);
  });
}
Status Model::InspectLayoutRoot(NodeKey key, NodeKey* root, NodeKey* composed_root,
                                bool* connected, bool* budget_exceeded) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    owner.require(key.node);
    const auto composed = owner.composed_root_id(key.node);
    *composed_root = Key(key.document, composed);
    *connected = composed == owner.id();
    *budget_exceeded = false;
    auto current = key.node;
    for (unsigned depth = 0;; ++depth) {
      const auto parent_id = owner.require(current).parent_id();
      if (!parent_id) break;
      const auto& p = owner.require(parent_id);
      domain::NodeId next = 0;
      if (p.shadow_host_id()) next = p.shadow_host_id();
      else if (p.node_type() == domain::NodeType::Element) next = parent_id;
      if (!next) break;
      if (depth > 256) { *budget_exceeded = true; return; }
      current = next;
    }
    *root = Key(key.document, current);
  });
}
}
