#include "document_model/model_internal.h"
#include "domain/document_model/attribute_factory.h"

namespace ruyijs::document_model {
Status PublicAttributeName(DomStringView qualified, AttributeSnapshot* result) noexcept {
  return Protect([&] {
    *result = {};
    result->name = domain::public_attribute_name(qualified);
    result->local_name = domain::ascii_lower(result->name);
  });
}
bool AttributeName(bool html, DomString* name) {
  if (html) *name = domain::ascii_lower(*name);
  return domain::valid_attribute_local_name(*name);
}
Status Model::GetAttribute(NodeKey key, DomStringView name, std::optional<DomString>* result) const noexcept {
  return Protect([&] { auto value = State().element(key).get_attribute16(name); result->swap(value); });
}
Status Model::SetAttribute(NodeKey key, DomStringView name, DomStringView value) noexcept {
  return Protect([&] {
    auto& element = State().element(key); auto old = element.get_attribute16(name);
    const auto previous_id = element.get_attribute16(u"id");
    State().budget(0,value.size() + (old ? 0 : name.size()),old ? old->size() : 0);
    element.set_attribute16(name,value);
    const auto next_id = element.get_attribute16(u"id");
    if (previous_id != next_id)
      State().owner(key).note_element_id(key.node, previous_id ? DomStringView(*previous_id) : DomStringView(),
                                         next_id ? DomStringView(*next_id) : DomStringView());
    State().NoteAttributes(key, name, DomStringView(), old ? &*old : nullptr);
    // An unchanged value (classList.add of a present token) or an in-place update
    // to an existing style attribute is still a mutation record, but no collection
    // or NamedNodeMap membership can change; creating style adds an Attr entry.
    const bool is_style = element.tag_name().folds_case() ? domain::ascii_lower(DomString(name)) == u"style"
                                                          : name == u"style";
    if (old && (is_style || DomStringView(*old) == value)) State().TouchStyle();
    else if (domain::ascii_lower(DomString(name)) == u"class") State().TouchClass();
    else State().TouchAttribute();
  });
}
Status Model::RemoveAttribute(NodeKey key, DomStringView name) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    auto old = element.get_attribute16(name);
    const auto previous_id = element.get_attribute16(u"id");
    element.remove_attribute16(name);
    const auto next_id = element.get_attribute16(u"id");
    if (previous_id != next_id)
      State().owner(key).note_element_id(key.node, previous_id ? DomStringView(*previous_id) : DomStringView(),
                                         next_id ? DomStringView(*next_id) : DomStringView());
    // Removing an attribute that was not there is not a mutation.
    if (old) State().NoteAttributes(key, name, DomStringView(), &*old);
    if (!old) State().TouchStyle();
    else if (domain::ascii_lower(DomString(name)) == u"class") State().TouchClass();
    else State().TouchAttribute();
  });
}
Status Model::Attributes(NodeKey key, std::vector<AttributeSnapshot>* result) const noexcept {
  return Protect([&] {
    std::vector<AttributeSnapshot> values;
    for (const auto& attr : State().element(key).attributes16())
      values.push_back({attr.name,attr.value,attr.namespace_uri,attr.prefix,attr.local_name});
    result->swap(values);
  });
}
Status Model::ChangedElements(const std::vector<NodeKey>& keys, const std::vector<char>& elements,
                              const std::vector<uint64_t>& previous_attributes,
                              const std::vector<DomString>& previous_styles, bool style_only,
                              std::vector<ChangedElement>* changed) const noexcept {
  return Protect([&] {
    std::vector<ChangedElement> out;
    DocumentId cached_id = 0;
    const domain::DocumentAggregate* cached_doc = nullptr;
    for (size_t i = 0; i < keys.size(); ++i) {
      if (!elements[i]) continue;
      const auto& key = keys[i];
      if (!cached_doc || cached_id != key.document) {
        cached_doc = &State().owner(key);
        cached_id = key.document;
      }
      const auto* element = cached_doc->get_element(key.node);
      if (!element) continue;
      if (style_only) {
        const auto value = element->get_attribute16(u"style");
        const DomStringView current = value ? DomStringView(*value) : DomStringView{};
        if (current != previous_styles[i])
          out.push_back({i, previous_attributes[i], DomString(current), {}, false, true});
        continue;
      }
      uint64_t hash = 1469598103934665603ull;
      DomStringView style_view{};
      const auto& attrs = element->attributes16();
      for (const auto& attr : attrs) {
        if (attr.name == u"style") {
          style_view = attr.value;
          continue;
        }
        for (char16_t c : attr.name) { hash ^= c; hash *= 1099511628211ull; }
        hash ^= 0x3d; hash *= 1099511628211ull;
        for (char16_t c : attr.value) { hash ^= c; hash *= 1099511628211ull; }
        hash ^= 0x1f; hash *= 1099511628211ull;
      }
      const bool attr_diff = hash != previous_attributes[i];
      const bool style_diff = style_view != previous_styles[i];
      if (!attr_diff && !style_diff) continue;
      ChangedElement item{i, hash, DomString(style_view), {}, attr_diff, style_diff};
      if (attr_diff) {
        item.attribute_list.reserve(attrs.size());
        for (const auto& a : attrs)
          item.attribute_list.push_back({a.name, a.value, a.namespace_uri, a.prefix, a.local_name});
      }
      out.push_back(std::move(item));
    }
    changed->swap(out);
  });
}
Status Model::GetAttributeNS(NodeKey key, DomStringView namespace_uri, DomStringView local,
                             std::optional<DomString>* result) const noexcept {
  return Protect([&] {
    auto value = State().element(key).get_attribute_ns16(namespace_uri,local);
    result->swap(value);
  });
}
Status Model::HasAttributeNS(NodeKey key, DomStringView namespace_uri, DomStringView local,
                             bool* result) const noexcept {
  return Protect([&] { *result = State().element(key).has_attribute_ns16(namespace_uri,local); });
}
Status Model::SetAttributeNS(NodeKey key, DomStringView namespace_uri, DomStringView qualified,
                             DomStringView value) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    DomString prefix, local;
    SplitQualified(qualified, &prefix, &local);
    // The same identity the domain folds for a null namespace on an HTML element.
    const bool fold = namespace_uri.empty() && element.tag_name().folds_case();
    const DomString folded = fold ? domain::ascii_lower(local) : local;
    auto old = element.get_attribute_ns16(namespace_uri, folded);
    State().budget(0,value.size() + (old ? 0 : qualified.size()),old ? old->size() : 0);
    element.set_attribute_ns16(namespace_uri,qualified,value);
    State().NoteAttributes(key, folded, namespace_uri, old ? &*old : nullptr);
    if (old && (DomStringView(*old) == value || (namespace_uri.empty() && folded == u"style"))) State().TouchStyle();
    else if (namespace_uri.empty() && domain::ascii_lower(folded) == u"class") State().TouchClass();
    else State().TouchAttribute();
  });
}
Status Model::RemoveAttributeNS(NodeKey key, DomStringView namespace_uri, DomStringView local) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    auto old = element.get_attribute_ns16(namespace_uri,local);
    element.remove_attribute_ns16(namespace_uri,local);
    if (old) State().NoteAttributes(key, local, namespace_uri, &*old);
    if (!old) State().TouchStyle();
    else if (namespace_uri.empty() && domain::ascii_lower(DomString(local)) == u"class") State().TouchClass();
    else State().TouchAttribute();
  });
}
Status Model::ToggleAttribute(NodeKey key, DomStringView name, bool force, bool has_force,
                              bool* result) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    if (!domain::valid_attribute_local_name(name))
      throw domain::DomError(domain::DomErrorCode::InvalidCharacter, "Invalid attribute name");
    auto old = element.get_attribute16(name);
    const bool target = has_force ? force : !old.has_value();
    *result = target;
    if (target == old.has_value()) return;
    if (target) { State().budget(0,name.size()); element.set_attribute16(name,{}); }
    else { State().budget(0,0,old->size()); element.remove_attribute16(name); }
    State().NoteAttributes(key, name, DomStringView(), old ? &*old : nullptr);
    if (domain::ascii_lower(DomString(name)) == u"class") State().TouchClass();
    else State().TouchAttribute();
  });
}
}
