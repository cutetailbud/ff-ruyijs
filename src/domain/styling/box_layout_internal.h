#pragma once
#include "domain/styling/box_layout.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ruyijs::domain::styling {
inline constexpr size_t kNoBox = size_t(-1);
inline constexpr double kBoxCoordMax = double((1 << 30) - 1);
inline double BoxRound(double value) {
  // Firefox nscoord / NSToCoordRoundWithClamp: limit before integer conversion.
  return std::floor(std::clamp(value * 60, -kBoxCoordMax, kBoxCoordMax) + .5) / 60;
}
inline double BoxAuto() { return std::numeric_limits<double>::quiet_NaN(); }
double BoxLength(const BoxStyle&, const char*, double percent, double fallback);
double BoxLengthOf(const BoxStyle&, const std::string&, double percent, double fallback);
// A single viewport-percentage length (vw, vh, vmin, vmax) against the layout
// viewport; *out is unset (false) for any other value.
bool BoxViewportLength(const std::string& text, double width, double height, double* out);
double BoxLimit(const BoxStyle&, bool width, double available, double value, double extras);
bool BoxReplaced(const BoxNode&);
bool BoxAbsolute(const BoxNode&);
void BoxEdges(const BoxNode&, double width, BoxLayout*);
struct BoxFlow {
  const std::vector<BoxNode>& nodes;
  const BoxViewport& viewport;
  std::vector<BoxLayout>& boxes;
  bool Place(size_t, const BoxRect& containing, bool definite_height, double x, double y,
             size_t positioned, unsigned depth);
  bool Children(size_t, size_t positioned, unsigned depth, double* height, double* width);
  bool FlexChildren(size_t, size_t positioned, unsigned depth, double* height, double* width);
  bool GridChildren(size_t, size_t positioned, unsigned depth, double* height, double* width);  // box_layout_grid.cpp
  // Content-box min-/max-content width (percentages as zero), and with the
  // node's own padding, border and margins (box_layout_intrinsic.cpp).
  double Intrinsic(size_t, bool minimum, unsigned depth);
  double IntrinsicOuter(size_t, bool minimum, unsigned depth);
  double IntrinsicContent(size_t, bool minimum, unsigned depth);  // Ignoring its own width.
  void ShiftBoxes(size_t, double dx, double dy);
  // Per node for one ComputeBoxLayout call: a flex item's resolved content-box
  // width/height (NaN: from its style), and memoized intrinsic widths.
  std::vector<double> forced_width{}, forced_height{}, max_content{}, min_content{};
  // Hypothetical positions for out-of-flow children, in the current layout pass.
  std::vector<double> static_x{}, static_y{};
  // Place calls in this layout; flex second placements stop at a multiple of
  // the node count, since nested flex containers would otherwise re-lay out
  // exponentially (box_layout_flex.cpp).
  size_t placements = 0;
  // Whether a flex item's descendants depend on its resolved size; when not
  // (or past the budget) the item box is resized in place instead.
  bool Relayout(size_t, bool height);
  void Resize(size_t, bool height, double content_size);
  // CSS 2 §8.3.1 margin collapsing in block flow: whether a block container's
  // top/bottom margin adjoins its in-flow children's (no border, padding, or
  // new block formatting context between them).
  bool AdjoinsChildren(size_t, bool top) const;
  // Per node: the collapsed margins its block flow passed up through its top
  // and bottom edges, and whether it has no in-flow content (self-collapsing).
  std::vector<double> leading{}, trailing{};
  std::vector<char> empty_flow{}, self_collapse{};
};
// The collapsed result of two adjoining margins (CSS 2 §8.3.1).
inline double BoxCollapse(double a, double b) {
  return std::max({a, b, 0.0}) + std::min({a, b, 0.0});
}
}
