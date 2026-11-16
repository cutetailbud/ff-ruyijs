#pragma once
#include <array>

namespace ruyijs::web_styling {
// getComputedStyle resolved (used) values for a rendered element (#42): the
// content width and height, then padding, margin, border widths and insets as
// top, right, bottom, left. Supplied by the geometry owner through
// Realm::SetUsedBoxPort; false leaves the computed value as it is.
struct UsedBox {
  double width = 0, height = 0;
  std::array<double, 4> padding{}, margin{}, border{}, inset{};
};
}  // namespace ruyijs::web_styling
