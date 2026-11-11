#pragma once
#include <cstdint>
#include <vector>
#include <utility>
#include <string>
#include <memory>
#include <set>
#include "js/RootingAPI.h"

namespace ruyijs::web_styling {
struct State;
struct ComputedStyle;
// What an attribute change can reach beyond its own element and its
// descendants/following siblings: an attribute selector on `style`, and the
// classes, ids and attribute names read inside :has() arguments (a :has()
// subject's match depends on its descendants or siblings).
// fromEnd: a structural pseudo-class counted from the end or over a type
// (:last-*, :nth-last-*, :only-*, *-of-type), so any change of a parent's
// child list can change every child's match; empty: :empty/:blank, so a change
// of an element's own children can change its match. Inside :has() arguments,
// a compound with a pseudo-class also depends on its siblings (hasSibling*),
// and one with no class, id or type key on any tree change (hasStructural).
struct SelectorDependencies {
  bool styleAttribute = false, fromEnd = false, empty = false, hasStructural = false, siblingCombinator = false;
  std::set<std::string> hasClasses, hasIds, hasAttributes, hasTags;
  std::set<std::string> hasSiblingClasses, hasSiblingIds, hasSiblingTags;
  void Merge(const SelectorDependencies& other) {
    styleAttribute |= other.styleAttribute; fromEnd |= other.fromEnd; empty |= other.empty;
    hasStructural |= other.hasStructural; siblingCombinator |= other.siblingCombinator;
    for (const auto& pair : {std::make_pair(&hasClasses, &other.hasClasses), std::make_pair(&hasIds, &other.hasIds),
                       std::make_pair(&hasAttributes, &other.hasAttributes), std::make_pair(&hasTags, &other.hasTags),
                       std::make_pair(&hasSiblingClasses, &other.hasSiblingClasses),
                       std::make_pair(&hasSiblingIds, &other.hasSiblingIds),
                       std::make_pair(&hasSiblingTags, &other.hasSiblingTags)})
      pair.first->insert(pair.second->begin(), pair.second->end());
  }
  bool HasKeys() const {
    return hasStructural || !hasClasses.empty() || !hasIds.empty() || !hasAttributes.empty() || !hasTags.empty();
  }
};
// Adds what one selector (list) reads to `out` (selector_dependencies.cpp).
void CollectSelectorDependencies(const std::string& selector, SelectorDependencies* out);
class ComputedStyleReader final {
 public:
  explicit ComputedStyleReader(State&);
  ~ComputedStyleReader();
  bool Read(JS::HandleObject, ComputedStyle*);
  // Firefox getDefaultComputedStyle: the same cascade with user-agent rules
  // only (no author sheets and no inline style on the element or its ancestors).
  bool ReadDefault(JS::HandleObject, ComputedStyle*);
  // The cascade scopes (document or shadow root) this reader resolved.
  size_t Scopes() const;
  // The selector dependencies of the scopes read so far (Web Geometry decides
  // from them whether an attribute change allows reusing other styles).
  const SelectorDependencies& Dependencies() const;
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
// Call-scoped projection of the unique cascade, never an additional style store.
struct ComputedStyle {
  std::vector<std::pair<std::string, std::string>> values;
  // The serialized custom-property suffix of the declaration block ("--x: v;…").
  // Custom properties share a copy-on-write map across every element that
  // inherits them, so ReadComputed serializes this once per map identity and
  // reuses the text, instead of copying hundreds of entries per element.
  std::string customsText;
  double fontSize = 16;
  double rootFontSize = 16;
  unsigned specifiedMarginMask = 0, specifiedBorderMask = 0, specifiedPaddingMask = 0;
  // What this style passes to children's cascade
  // (StyleResolutionService::child_inputs), and whether it `inherit`s a
  // property that does not inherit, so reads its parent's other values too.
  uint64_t childInputs = 0;
  bool inheritsReset = false;
  std::string Get(const char* name) const {
    for (const auto& entry : values) if (entry.first == name) return entry.second;
    return {};
  }
};
}
