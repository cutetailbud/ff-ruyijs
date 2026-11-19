// DOM selector adapter (model_selector.h): the Styling owner's ParsedSelector
// over the Document Model tree. Each query reads only the elements it visits
// into ElementNode records owned by that one call; nothing outlives the call.
#include "document_model/model_selector.h"

#include <unordered_map>
#include <utility>

#include "document_model/model_internal.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_selector.h"
#include "domain/document_model/character_data.h"

namespace ruyijs::document_model {
namespace {
using domain::DocumentAggregate;
namespace styling = ruyijs::domain::styling;

Status Invalid() { return {ErrorCode::Syntax, "Invalid or unsupported CSS selector"}; }

// The relations the Styling matcher reads: '>' parent element, '-' previous
// element sibling, '+' next element sibling, 'v' first element child.
class TreeInput final : public styling::ElementRelations {
 public:
  explicit TreeInput(const DocumentAggregate& owner) : owner_(owner) {}
  const styling::ElementNode* Load(uint64_t id) const {
    if (!id) return nullptr;
    auto found = nodes_.find(id);
    if (found != nodes_.end()) return found->second.get();
    const auto* element = owner_.get_element(id);
    if (!element) return nullptr;
    auto node = std::make_unique<styling::ElementNode>();
    node->key = id;
    node->relations = this;
    node->html = element->tag_name().html() && owner_.mode() == domain::DocumentMode::Html;
    node->tag = domain::to_utf8(element->tag_name().local16());
    for (const auto& attribute : element->attributes16()) {
      if (!attribute.namespace_uri.empty()) continue;
      std::string name = domain::to_utf8(attribute.name), value = domain::to_utf8(attribute.value);
      if (name == "id") node->id = value;
      if (name == "class")
        for (size_t p = 0; p < value.size();) {
          while (p < value.size() && styling::CssSpace(value[p])) ++p;
          size_t end = p;
          while (end < value.size() && !styling::CssSpace(value[end])) ++end;
          if (end > p) node->classes.push_back(value.substr(p, end - p));
          p = end;
        }
      node->attributes.emplace(std::move(name), std::move(value));
    }
    node->empty = true;
    for (uint64_t child : element->child_ids()) {
      const auto& item = owner_.require(child);
      if (item.node_type() == domain::NodeType::Element ||
          ((item.node_type() == domain::NodeType::Text || item.node_type() == domain::NodeType::CDataSection) &&
           static_cast<const domain::CharacterData&>(item).length())) { node->empty = false; break; }
    }
    node->root = element->parent_id() == owner_.id();
    node->focused = owner_.focused_id() == id;
    return nodes_.emplace(id, std::move(node)).first->second.get();
  }
  const styling::ElementNode* Related(const styling::ElementNode& node, char relation) const override {
    const auto& self = owner_.require(node.key);
    if (relation == '>') return owner_.get_element(self.parent_id()) ? Load(self.parent_id()) : nullptr;
    if (relation == 'v') {
      for (uint64_t child : self.child_ids()) if (owner_.get_element(child)) return Load(child);
      return nullptr;
    }
    if (!self.parent_id()) return nullptr;
    const auto& siblings = owner_.require(self.parent_id()).child_ids();
    int index = owner_.require(self.parent_id()).index_of_child(node.key);
    if (index < 0) return nullptr;
    if (relation == '-') {
      for (int i = index - 1; i >= 0; --i) if (owner_.get_element(siblings[i])) return Load(siblings[i]);
    } else if (relation == '+') {
      for (size_t i = size_t(index) + 1; i < siblings.size(); ++i)
        if (owner_.get_element(siblings[i])) return Load(siblings[i]);
    }
    return nullptr;
  }

 private:
  const DocumentAggregate& owner_;
  mutable std::unordered_map<uint64_t, std::unique_ptr<styling::ElementNode>> nodes_;
};

// A scoping root that is not an element (Document, DocumentFragment): :scope
// compares by address, so this record matches no element of the tree.
const styling::ElementNode kNoElementScope{};
}  // namespace

Status Selector::Parse(DomStringView text, Selector* result) noexcept {
  try {
    std::vector<std::shared_ptr<const styling::ParsedSelector>> groups;
    for (const auto& group : styling::CssSplit(domain::to_utf8(text), ',')) {
      auto parsed = std::make_shared<styling::ParsedSelector>(styling::ParsedSelector::Parse(group));
      if (!parsed->valid()) return Invalid();
      groups.push_back(std::move(parsed));
    }
    if (groups.empty()) return Invalid();
    result->groups_.swap(groups);
    return {};
  } catch (const std::bad_alloc&) {
    return {ErrorCode::OutOfMemory, "Selector allocation failed"};
  } catch (...) {
    return {ErrorCode::InvalidState, "Selector parsing failed"};
  }
}

bool Selector::PureId(std::string* id) const {
  return groups_.size() == 1 && groups_.front() && groups_.front()->pure_id(id);
}
bool Selector::Matches(const DocumentAggregate& owner, uint64_t element, uint64_t scope) const {
  TreeInput input(owner);
  const styling::ElementNode* node = input.Load(element);
  if (!node) return false;
  const styling::ElementNode* scoping = scope ? input.Load(scope) : nullptr;
  if (!scoping) scoping = &kNoElementScope;
  for (const auto& group : groups_)
    if (!group->has_pseudo_element() && group->matches(*node, scoping)) return true;
  return false;
}

void Selector::Select(const DocumentAggregate& owner, uint64_t root,
                      std::vector<uint64_t>* result) const {
  TreeInput input(owner);
  const styling::ElementNode* scoping = input.Load(root);
  if (!scoping) scoping = &kNoElementScope;
  // The subject compound's first id / class / type is a necessary condition of
  // a match (ParsedSelector::IndexKey); it is read from the tree before an
  // element is loaded, so most elements of a large scope are never copied.
  struct Key { char kind; DomString value; };
  std::vector<Key> keys;
  bool filtered = true;
  for (const auto& group : groups_) {
    if (group->has_pseudo_element()) continue;
    char kind = 0; std::string key;
    group->IndexKey(&kind, &key);
    if (!kind) { filtered = false; break; }
    keys.push_back({kind, domain::from_utf8(key)});
  }
  auto candidate = [&](uint64_t id) {
    if (!filtered) return true;
    const auto* element = owner.get_element(id);
    if (!element) return false;
    for (const Key& key : keys) {
      if (key.kind == '#') { auto value = element->get_attribute16(u"id"); if (value && *value == key.value) return true; }
      else if (key.kind == '.') { if (element->has_class16(key.value)) return true; }
      else if (domain::ascii_lower(element->tag_name().local16()) == key.value) return true;
    }
    return false;
  };
  for (uint64_t id : owner.descendants(root)) {
    if (!candidate(id)) continue;
    const styling::ElementNode* node = input.Load(id);
    if (!node) continue;
    for (const auto& group : groups_)
      if (!group->has_pseudo_element() && group->matches(*node, scoping)) { result->push_back(id); break; }
  }
}
}  // namespace ruyijs::document_model
