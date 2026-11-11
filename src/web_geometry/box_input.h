#pragma once
#include "web_geometry/internal.h"
#include "domain/styling/box_layout.h"
#include "document_model/layout_source.h"
#include "web_styling/computed_source.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ruyijs::web_geometry {
namespace box = ruyijs::domain::styling;
struct LayoutNodeKeyHash {
  size_t operator()(const std::pair<uint64_t, uint64_t>& key) const noexcept {
    return size_t(key.first * 0x9E3779B97F4A7C15ull ^
                  (key.second + 0x9E3779B97F4A7C15ull + (key.first << 6) + (key.first >> 2)));
  }
};
using LayoutAttributes = std::vector<std::pair<std::u16string, std::u16string>>;
// Tree and non-style attribute metadata shared across inline-style-only updates.
struct LayoutTreeMeta {
  std::vector<document_model::NodeKey> keys;  // Per node, in layout order.
  std::vector<char> is_element;
  std::vector<uint64_t> attributes;  // Per node: hash of its other attributes.
  std::vector<LayoutAttributes> attribute_lists;  // Per node: its attributes (for :has() reach).
  std::vector<std::u16string> texts;  // Per node: its text (empty for elements).
  std::unordered_map<std::pair<uint64_t, uint64_t>, size_t, LayoutNodeKeyHash> index;
  bool has_hidden_children = false;
};
// One document's last full layout and the inputs it was computed from. A
// measurement whose root, Model revision, focused element, cascade fingerprint
// and viewport all match reads it instead of laying the tree out again.
struct LayoutCache {
  document_model::NodeKey root, focused;
  uint64_t revision = 0, styles = 0, structure = 0, tree = 0, fonts = 0;
  std::array<double, 4> viewport{};
  std::vector<box::BoxNode> nodes;
  std::vector<box::BoxLayout> layout;
  box::BoxScrollport scrollport;  // The viewport scroll frame of this layout.
  box::BoxLayoutCache box_cache;
  std::shared_ptr<LayoutTreeMeta> meta = std::make_shared<LayoutTreeMeta>();
  std::vector<std::u16string> inline_styles;  // Per node: its style attribute (empty if none).
  // Per element: what its computed style passes to its children's cascade,
  // and whether it inherits a reset property (ComputedStyle::childInputs,
  // inheritsReset); 0 for text.
  std::vector<uint64_t> child_inputs;
  std::vector<char> inherits_reset;
  bool has_style_dep = false, has_key_dep = false, has_sibling_dep = false;
};
void ApplyBoxAttributes(const std::vector<document_model::AttributeSnapshot>&, box::BoxNode*);
bool BoxSameStyle(const box::BoxStyle&, const box::BoxStyle&);
bool BoxSameLayout(const box::BoxStyle&, const box::BoxStyle&);
bool BoxSameNode(const box::BoxNode&, const box::BoxNode&);
bool TryTreeUnchangedLayout(State&, JS::HandleValue target_value,
                            const document_model::NodeKey& target_key, const LayoutCache& previous,
                            uint64_t revision, uint64_t structure, const box::BoxViewport& viewport,
                            std::shared_ptr<const LayoutCache>* out, size_t* found_out, bool* handled);
// HTMLElement offset* from one layout: border box without transforms, relative
// to the offset parent's padding edge (or the initial containing block).
void OffsetFrom(const LayoutCache&, size_t index, document_model::OffsetGeometry*);
// Lays out (or reuses) the rendered tree containing `element`; `index` is the
// element's node, or size_t(-1) with no layout for an unrendered element.
bool LayoutFor(State&, JS::HandleObject element, std::shared_ptr<const LayoutCache>*, size_t* index,
               box::BoxViewport*);
bool BoxText(State&, const document_model::LayoutNodeSource&, box::BoxNode*);
// The summed scroll offsets of a node's scroll-container ancestors (box_scroll.cpp).
std::array<double, 2> AncestorScroll(const State&, const LayoutCache&, size_t index);
bool ElementProjection(State&, JS::HandleValue, bool list, JS::MutableHandleValue);
}
