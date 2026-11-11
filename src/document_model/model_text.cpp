#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::SetNodeValue(NodeKey key, DomStringView value) noexcept {
  return Protect([&] {
    auto& node = State().owner(key).require(key.node);
    if (node.node_type() == domain::NodeType::Text || node.node_type() == domain::NodeType::Comment ||
      node.node_type() == domain::NodeType::ProcessingInstruction) {
      const DomString old = node.text_content16();
      State().budget(0,value.size(),node.storage_units()); node.set_node_value16(DomString(value));
      State().NoteData(key, &old);
      State().Touch();
    }
  });
}
Status Model::SubstringData(NodeKey key, uint32_t offset, uint32_t count, DomString* result) const noexcept {
  return Protect([&] { auto value = State().data(key).substring(offset,count); result->swap(value); });
}
Status Model::ReplaceData(NodeKey key, uint32_t offset, uint32_t count, DomStringView value) noexcept {
  return Protect([&] {
    auto& data = State().data(key);
    if (offset > data.length()) throw domain::DomError(domain::DomErrorCode::IndexSize,"CharacterData offset exceeds length");
    const DomString old = data.text_content16();
    State().budget(0,value.size(),std::min<size_t>(count,data.length()-offset));
    data.replace(offset,count,value);
    State().NoteData(key, &old);
    State().Touch();
  });
}
Status Model::TextContent(NodeKey key, std::optional<DomString>* result) const noexcept {
  return Protect([&] {
    auto& owner = State().owner(key); std::optional<DomString> value;
    if (owner.require(key.node).node_type() != domain::NodeType::Document) value = owner.text_content16(key.node);
    result->swap(value);
  });
}
Status Model::SetTextContent(NodeKey key, DomStringView value) noexcept {
  return Protect([&] {
    auto& owner = State().owner(key); auto& node = owner.require(key.node);
    if (node.node_type() == domain::NodeType::Document || node.node_type() == domain::NodeType::DocumentType) return;
    bool element = node.node_type() == domain::NodeType::Element;
    State().budget(element && !value.empty() ? 1 : 0,value.size(),element ? 0 : node.storage_units());
    if (element) {
      // textContent on an Element replaces the children wholesale: one record
      // with both sides, exactly like innerHTML.
      const auto old = node.child_ids();
      owner.set_text_content16(key.node,value);
      State().NoteReplace(key, owner.require(key.node).child_ids(), old);
    } else {
      const DomString old = node.text_content16();
      owner.set_text_content16(key.node,value);
      State().NoteData(key, &old);
    }
    State().Touch();
  });
}
}
