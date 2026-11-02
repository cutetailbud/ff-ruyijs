#pragma once
#include "web_geometry/internal.h"
#include "domain/styling/box_layout.h"
#include "document_model/layout_source.h"
#include "web_styling/computed_source.h"
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ruyijs::web_geometry {
namespace box = ruyijs::domain::styling;
// One document's last full layout and the inputs it was computed from. A
// measurement whose root, Model revision, focused element, cascade fingerprint
// and viewport all match reads it instead of laying the tree out again.
struct LayoutCache {
  document_model::NodeKey root, focused;
  uint64_t revision=0, styles=0, structure=0, tree=0;
  std::array<double,4> viewport{};
  std::vector<box::BoxNode> nodes;
  std::vector<box::BoxLayout> layout;
  box::BoxScrollport scrollport;  // The viewport scroll frame of this layout.
  std::vector<document_model::NodeKey> keys;  // Per node, in layout order.
  std::vector<std::u16string> inline_styles;  // Per node: its style attribute (empty if none).
  std::vector<uint64_t> attributes;  // Per node: hash of its other attributes.
  using Attributes = std::vector<std::pair<std::u16string, std::u16string>>;
  std::vector<Attributes> attribute_lists;  // Per node: its attributes (for :has() reach).
  // Per element: what its computed style passes to its children's cascade,
  // and whether it inherits a reset property (ComputedStyle::childInputs,
  // inheritsReset); 0 for text.
  std::vector<uint64_t> child_inputs;
  std::vector<char> inherits_reset;
  std::vector<std::u16string> texts;  // Per node: its text (empty for elements).
  uint64_t fonts=0;  // The font configuration generation the text was measured under.
  std::map<std::pair<uint64_t,uint64_t>,size_t> index;
};
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
