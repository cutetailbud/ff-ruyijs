#pragma once
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::styling {
using BoxRect = std::array<double, 4>;
struct BoxStyle {
  // Computed values sorted by name (Share), searched by Get, and shared: a
  // text box takes its element's style and an unchanged element keeps the
  // previous layout's by copying a pointer, not a few hundred strings.
  using Values = std::vector<std::pair<std::string, std::string>>;
  std::shared_ptr<const Values> values;
  double font_size = 16, root_font_size = 16;
  unsigned specified_margin_mask = 0, specified_border_mask = 0, specified_padding_mask = 0;
  const std::string& Get(const char* name) const;
  static std::shared_ptr<const Values> Share(Values values);
};
struct BoxNode {
  BoxStyle style;
  std::string tag, input_type;
  std::vector<size_t> children;
  size_t parent = size_t(-1);
  bool text = false;
  bool breakable = false;  // Text with a soft wrap opportunity (min-content < max-content).
  // text_height is the primary font content area (inline box height);
  // line_height is the text's contribution to its line box.
  double text_width = 0, text_height = 0, line_height = -1, intrinsic_width = -1, intrinsic_height = -1;
  // HTML width/height attribute percentages (presentational hints that apply
  // when the author's width/height is auto), and an iframe frameborder="0"/"no".
  double attribute_width_percent = -1, attribute_height_percent = -1;
  bool no_frame_border = false;
};
struct BoxViewport {
  double width = 0, height = 0, scroll_x = 0, scroll_y = 0;
  // The scrollport (initial and fixed containing block): the viewport less the
  // viewport scrollbars; set by ComputeBoxLayout, -1 for the full viewport.
  double client_width = -1, client_height = -1;
  double ClientWidth() const { return client_width >= 0 ? client_width : width; }
  double ClientHeight() const { return client_height >= 0 ? client_height : height; }
};
// Classic (non-overlay) Windows scrollbar thickness, which Firefox lays out for
// a scroll container with a scrollbar (nsIFrame scrollbar gutter, 17 CSS px).
inline constexpr double kBoxScrollbar = 17;
struct BoxLayout {
  BoxRect border{}, content{};
  std::array<double, 4> margin{}, padding{}, edge{};
  // A scroll container's scrollbars: the vertical bar's width and the
  // horizontal bar's height, taken from its content box.
  std::array<double, 2> scrollbar{};
  double line = 0;  // Line box contribution of text and non-replaced inline boxes.
  bool present = false, definite_height = false;
};
// The viewport scroll frame: the scrollport (client) size and the root's
// scrollable overflow (scroll) size, at least the scrollport.
struct BoxScrollport { double client_width = 0, client_height = 0, scroll_width = 0, scroll_height = 0; };
// Call-scoped projection: no node identity, retained tree, renderer or engine dependency.
bool ComputeBoxLayout(const std::vector<BoxNode>&, const BoxViewport&, std::vector<BoxLayout>*,
                      BoxScrollport* scrollport = nullptr);
// Whether a box is a scroll container (overflow other than visible/clip on an
// axis). The root and a body whose overflow propagates to the viewport are not.
bool BoxScrollContainer(const std::vector<BoxNode>&, size_t);
// The right/bottom edge of |index|'s descendants' margin boxes, through every
// descendant that is not itself a scroll container.
void BoxOverflowExtent(const std::vector<BoxNode>&, const std::vector<BoxLayout>&, size_t index,
                       double* right, double* bottom);
BoxRect ProjectBoxRect(const std::vector<BoxNode>&, const std::vector<BoxLayout>&,
                       const BoxViewport&, size_t target);
}
