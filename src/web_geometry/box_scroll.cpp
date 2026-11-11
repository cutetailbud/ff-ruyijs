// Element scroll positions (CSSOM View §7 scrollTop/scrollLeft, §5 scroll
// operations) of scroll containers other than the viewport. The position is
// layout (frame) state, as nsHTMLScrollFrame keeps it in Firefox: it lives
// here, clamps to the container's scrollable overflow, and shifts the projected
// boxes of its descendants. The viewport keeps its Browser Environment owner.
#include "web_geometry/box_input.h"
#include <cmath>

namespace ruyijs::web_geometry {
namespace {
constexpr size_t kMaxScrolledElements = 65536;
std::pair<uint64_t, uint64_t> Key(const document_model::NodeKey& key) { return {key.document, key.node}; }
}  // namespace

std::array<double, 2> ScrollOffset(const State& s, const document_model::NodeKey& key) {
  const auto found = s.elementScroll.find(Key(key));
  return found == s.elementScroll.end() ? std::array<double, 2>{0, 0} : found->second;
}

// The sum of the scroll offsets of |index|'s ancestors (its own does not move it).
std::array<double, 2> AncestorScroll(const State& s, const LayoutCache& cache, size_t index) {
  std::array<double, 2> total{0, 0};
  if (s.elementScroll.empty()) return total;
  for (size_t i = cache.nodes[index].parent; i != size_t(-1); i = cache.nodes[i].parent) {
    const auto offset = ScrollOffset(s, cache.meta->keys[i]);
    total[0] += offset[0]; total[1] += offset[1];
  }
  return total;
}

bool Realm::ElementScroll(JS::HandleObject element, bool write, double* x, double* y, bool* changed) {
  *changed = false;
  std::shared_ptr<const LayoutCache> layout; size_t found = size_t(-1); box::BoxViewport viewport;
  if (!LayoutFor(*state_, element, &layout, &found, &viewport)) return false;
  if (!layout || found == size_t(-1) || !layout->layout[found].present || !box::BoxScrollContainer(layout->nodes, found)) {
    if (!write) *x = *y = 0;
    return true;
  }
  const auto key = Key(layout->meta->keys[found]);
  auto current = ScrollOffset(*state_, layout->meta->keys[found]);
  if (!write) { *x = current[0]; *y = current[1]; return true; }
  document_model::OffsetGeometry geometry;
  OffsetFrom(*layout, found, &geometry);
  // Positions are app units (1/60 CSS px), read back through a float as
  // Firefox's Element::ScrollTop does, clamped to [0, scrollMax].
  const auto clamp = [](double value, double limit) {
    if (!std::isfinite(value)) value = 0;
    const double units = std::clamp(std::round(value * 60), 0.0, std::max(0.0, std::round(limit) * 60));
    return double(float(units / 60));
  };
  const std::array<double, 2> next{clamp(*x, geometry.scroll_width - geometry.client_width),
                                   clamp(*y, geometry.scroll_height - geometry.client_height)};
  if (next == current) return true;
  if (next[0] == 0 && next[1] == 0) state_->elementScroll.erase(key);
  else if (state_->elementScroll.count(key) || state_->elementScroll.size() < kMaxScrolledElements)
    state_->elementScroll[key] = next;
  else return true;
  *changed = true;
  return true;
}
}  // namespace ruyijs::web_geometry
