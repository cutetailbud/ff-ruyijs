#include <algorithm>

#include "document_model/model_internal.h"
#include "document_model/model_selector.h"

namespace ruyijs::document_model {
Status Model::QuerySelector(NodeKey key, DomStringView text,
                            std::vector<NodeKey>* result) const noexcept {
  Selector selector;
  Status parsed = Selector::Parse(text, &selector);
  if (parsed.code != ErrorCode::None) return parsed;
  return Protect([&] {
    const auto& owner = State().owner(key);
    std::string ident;
    // Only a Document root. Element, fragment, and shadow scopes keep the walk
    // so an id outside that subtree cannot be returned. A unique connected
    // holder is the only match; every other index answer falls through.
    if (key.node == owner.id() && selector.PureId(&ident)) {
      domain::NodeId hit = 0;
      if (owner.lookup_element_id(domain::from_utf8(ident), &hit) && hit) {
        *result = {Key(key.document, hit)};
        return;
      }
    }
    std::vector<uint64_t> found;
    selector.Select(owner, key.node, &found);
    std::vector<NodeKey> output;
    for (uint64_t node : found) output.push_back(Key(key.document, node));
    result->swap(output);
  });
}

namespace {
// One pass over a scope's descendants, keeping HTML elements whose local name is
// in `names` and that carry `attribute`. The attribute present with an empty
// value still counts, so the collection never reads folded attribute spelling.
std::vector<NodeKey> CollectTagged(const domain::DocumentAggregate& owner, NodeKey scope,
                                   std::initializer_list<DomStringView> names,
                                   DomStringView attribute) {
  std::vector<NodeKey> values;
  for (domain::NodeId id : owner.descendants(scope.node)) {
    const auto* element = owner.get_element(id);
    if (!element || !element->tag_name().html()) continue;
    bool named = false;
    for (DomStringView name : names)
      if (element->tag_name().local16() == name) { named = true; break; }
    if (!named || !element->get_attribute16(attribute)) continue;
    values.push_back(Key(scope.document, id));
  }
  return values;
}
}  // namespace

Status Model::Links(NodeKey scope, std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    std::vector<NodeKey> values = CollectTagged(State().owner(scope), scope, {u"a", u"area"}, u"href");
    result->swap(values);
  });
}
Status Model::Anchors(NodeKey scope, std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    std::vector<NodeKey> values = CollectTagged(State().owner(scope), scope, {u"a"}, u"name");
    result->swap(values);
  });
}
Status Model::ChildrenElements(NodeKey key, std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    std::vector<NodeKey> values;
    for (auto child : owner.require(key.node).child_ids())
      if (owner.get_element(child)) values.push_back(Key(key.document, child));
    result->swap(values);
  });
}

Status Model::LookupUniqueId(NodeKey scope, DomStringView name, NodeKey* hit, bool* unique) const noexcept {
  return Protect([&] {
    *unique = false;
    *hit = {};
    if (name.empty()) return;
    const auto& owner = State().owner(scope);
    domain::NodeId node = 0;
    if (!owner.lookup_element_id(name, &node) || !node) return;
    *unique = true;
    *hit = Key(scope.document, node);
  });
}
Status Model::ElementsByName(NodeKey key, DomStringView name,
                             std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    std::vector<NodeKey> values;
    for (auto node : owner.descendants(key.node)) {
      const auto* element = owner.get_element(node);
      if (!element) continue;
      auto value = element->get_attribute16(u"name");
      if (value && DomStringView(*value) == name) values.push_back(Key(key.document, node));
    }
    result->swap(values);
  });
}

}  // namespace ruyijs::document_model
