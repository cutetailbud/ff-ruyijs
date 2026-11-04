#include "domain/styling/box_layout_internal.h"

namespace ruyijs::domain::styling {
bool BoxFlow::Place(size_t index, const BoxRect& containing, bool definite_height,
                    double x, double y, size_t positioned, unsigned depth) {
  if (depth > 256 || index >= nodes.size()) return false;
  ++placements;
  const auto& node = nodes[index]; auto& box = boxes[index];
  const auto& style = node.style;
  // Place reads display and position several times. Get's hint makes a repeat
  // one compare, but the string still has to be fetched; hold the reference.
  const auto& display = style.Slot(BoxKnown::Display);
  const auto& position = style.Slot(BoxKnown::Position);
  if (display == "none") return true;
  if (node.text) {
    box.present = node.text_width > 0;
    box.border = box.content = {x, y, node.text_width, node.text_height};
    box.line = node.line_height >= 0 ? node.line_height : node.text_height;
    return true;
  }
  box.present = true;
  BoxRect cb = containing;
  const bool absolute = !node.text && (position == "absolute" || position == "fixed");
  const bool fixed = position == "fixed";
  if (absolute) {
    if (positioned == kNoBox || fixed) {
      cb = {fixed ? viewport.scroll_x : 0, fixed ? viewport.scroll_y : 0,
            viewport.ClientWidth(), viewport.ClientHeight()};
      definite_height = true;
    } else {
      const auto& ancestor = boxes[positioned];
      cb = {ancestor.border[0] + ancestor.edge[3], ancestor.border[1] + ancestor.edge[0],
            ancestor.border[2] - ancestor.edge[1] - ancestor.edge[3],
            ancestor.border[3] - ancestor.edge[0] - ancestor.edge[2]};
      definite_height = true;
    }
  }
  BoxEdges(node, cb[2], &box);
  const double extra_w = box.padding[1]+box.padding[3]+box.edge[1]+box.edge[3];
  const double extra_h = box.padding[0]+box.padding[2]+box.edge[0]+box.edge[2];
  const double percent_h = definite_height ? cb[3] : BoxAuto();
  double width = BoxLengthOf(style, style.Slot(BoxKnown::Width), cb[2], BoxAuto());
  double height = BoxLengthOf(style, style.Slot(BoxKnown::Height), percent_h, BoxAuto());
  BoxViewportLength(style.Slot(BoxKnown::Width), viewport.width, viewport.height, &width);
  BoxViewportLength(style.Slot(BoxKnown::Height), viewport.width, viewport.height, &height);
  // nscoord_MAX is also reflow's NS_UNCONSTRAINEDSIZE block-size sentinel.
  if (height >= kBoxCoordMax / 60) height = BoxAuto();
  // width="100%" / height="100%" attributes map to the dimension properties
  // below the author's own width/height (HTML presentational hints).
  if (!std::isfinite(width) && node.attribute_width_percent >= 0)
    width = BoxRound(node.attribute_width_percent * cb[2] / 100);
  if (!std::isfinite(height) && node.attribute_height_percent >= 0 && std::isfinite(percent_h))
    height = BoxRound(node.attribute_height_percent * percent_h / 100);
  const bool border_box = style.Slot(BoxKnown::BoxSizing) == "border-box";
  if (border_box) { width -= extra_w; height -= extra_h; }
  const double left = BoxLengthOf(style, style.Slot(BoxKnown::Left), cb[2], BoxAuto());
  const double right = BoxLengthOf(style, style.Slot(BoxKnown::Right), cb[2], BoxAuto());
  const double top = BoxLengthOf(style, style.Slot(BoxKnown::Top), cb[3], BoxAuto());
  const double bottom = BoxLengthOf(style, style.Slot(BoxKnown::Bottom), cb[3], BoxAuto());
  // A flex item is blockified; its container resolved its size (box_layout_flex.cpp).
  const bool flex_item = index < forced_width.size() && std::isfinite(forced_width[index]);
  const bool inline_box = display == "inline" && !BoxReplaced(node) && !absolute && !flex_item;
  if (inline_box) width = height = BoxAuto();
  if (BoxReplaced(node)) {
    if (!std::isfinite(width)) width = node.intrinsic_width >= 0 ? node.intrinsic_width : 300;
    if (!std::isfinite(height)) height = node.intrinsic_height >= 0 ? node.intrinsic_height : 150;
  }
  if (absolute && !std::isfinite(width) && std::isfinite(left) && std::isfinite(right))
    width = cb[2]-left-right-extra_w-box.margin[1]-box.margin[3];
  if (absolute && !std::isfinite(height) && std::isfinite(top) && std::isfinite(bottom))
    height = cb[3]-top-bottom-extra_h-box.margin[0]-box.margin[2];
  bool auto_width = !std::isfinite(width);
  if (auto_width) width = cb[2]-extra_w-box.margin[1]-box.margin[3];
  // An auto-width inline-flex container is shrink-to-fit around its items.
  if (auto_width && !absolute && display == "inline-flex")
    width = std::min(width, Intrinsic(index, false, depth));
  width = BoxLimit(style, true, cb[2], std::max(0.0, width), extra_w);
  if (flex_item) { width = forced_width[index]; auto_width = false; }
  if (index < forced_height.size() && std::isfinite(forced_height[index])) height = forced_height[index];
  box.definite_height = std::isfinite(height);
  if (box.definite_height && !(index < forced_height.size() && std::isfinite(forced_height[index])))
    height = BoxLimit(style, false, percent_h, std::max(0.0, height), extra_h);
  if (!absolute && !auto_width && !flex_item) {
    const double free = std::max(0.0, cb[2]-width-extra_w-box.margin[1]-box.margin[3]);
    const bool auto_l = style.Slot(BoxKnown::MarginLeft) == "auto", auto_r = style.Slot(BoxKnown::MarginRight) == "auto";
    if (auto_l) box.margin[3] = BoxRound(free / (auto_r ? 2 : 1));
    if (auto_r) box.margin[1] = BoxRound(free / (auto_l ? 2 : 1));
  }
  double bx = x + box.margin[3], by = y + box.margin[0];
  if (absolute) {
    if (std::isfinite(left)) bx = cb[0]+left+box.margin[3];
    else if (std::isfinite(right)) bx = cb[0]+cb[2]-right-width-extra_w-box.margin[1];
    if (std::isfinite(top)) by = cb[1]+top+box.margin[0];
    else if (std::isfinite(bottom) && box.definite_height) by = cb[1]+cb[3]-bottom-height-extra_h-box.margin[2];
  } else if (position == "relative") {
    bx += std::isfinite(left) ? left : std::isfinite(right) ? -right : 0;
    by += std::isfinite(top) ? top : std::isfinite(bottom) ? -bottom : 0;
  }
  box.border = {BoxRound(bx), BoxRound(by), BoxRound(width+extra_w), BoxRound((box.definite_height?height:0)+extra_h)};
  box.content = {box.border[0]+box.edge[3]+box.padding[3], box.border[1]+box.edge[0]+box.padding[0],
                 width, box.definite_height ? height : 0};
  const bool establishes = position != "static" || style.Slot(BoxKnown::Transform) != "none";
  const size_t child_positioned = establishes ? index : positioned;
  // A scroll container's scrollbars take their room from its content box
  // (overflow:scroll always; auto once the content overflows, below).
  const bool scroller = !inline_box && BoxScrollContainer(nodes, index);
  const auto overflow_x = style.Slot(BoxKnown::OverflowX), overflow_y = style.Slot(BoxKnown::OverflowY);
  box.scrollbar = {scroller && overflow_y == "scroll" ? kBoxScrollbar : 0,
                   scroller && overflow_x == "scroll" ? kBoxScrollbar : 0};
  box.content[2] = std::max(0.0, box.content[2] - box.scrollbar[0]);
  if (box.definite_height) box.content[3] = std::max(0.0, box.content[3] - box.scrollbar[1]);
  double children_height = 0, children_width = 0;
  if (!BoxReplaced(node) && !Children(index, child_positioned, depth, &children_height, &children_width)) return false;
  if (scroller && !BoxReplaced(node)) {
    bool again = false;
    if (overflow_y == "auto" && box.scrollbar[0] == 0 && box.definite_height &&
        children_height > box.content[3]) {
      box.scrollbar[0] = kBoxScrollbar;
      box.content[2] = std::max(0.0, box.content[2] - kBoxScrollbar);
      again = true;
    }
    if (overflow_x == "auto" && box.scrollbar[1] == 0 && children_width > box.content[2]) {
      box.scrollbar[1] = kBoxScrollbar;
      if (box.definite_height) box.content[3] = std::max(0.0, box.content[3] - kBoxScrollbar);
    }
    if (again && !Children(index, child_positioned, depth, &children_height, &children_width)) return false;
  }
  if (inline_box || (auto_width && display == "inline-block")) {
    box.content[2] = children_width; box.border[2] = BoxRound(children_width+extra_w);
  }
  if (absolute && auto_width && !BoxReplaced(node)) {
    // Shrink-to-fit: min(max(min-content, available), max-content). Without
    // a wrap opportunity the min-content width is the unwrapped line itself.
    bool wraps = false;
    for (const auto child : node.children) wraps = wraps || nodes[child].breakable;
    const double minimum = wraps ? 0 : children_width;
    const double fit = BoxLimit(style, true, cb[2],
                                std::min(std::max(minimum, box.content[2]), children_width), extra_w);
    box.content[2] = fit; box.border[2] = BoxRound(fit+extra_w);
  }
  if (inline_box) {
    // A non-replaced inline box is as tall as its font's content area; its
    // line-height contribution is what its contents add to the line box.
    box.line = children_height;
    double area = 0;
    for (const auto child : node.children) {
      const auto& item = nodes[child];
      if (item.text) area = std::max(area, item.text_height);
      else if (!BoxAbsolute(item) && item.style.Slot(BoxKnown::Display) == "inline" && !BoxReplaced(item))
        area = std::max(area, boxes[child].content[3]);
    }
    if (area > 0) children_height = area;
  }
  if (!box.definite_height) {
    box.content[3] = BoxLimit(style, false, percent_h, children_height, extra_h);
    box.border[3] = BoxRound(box.content[3]+extra_h+box.scrollbar[1]);
  }
  // Margins of the first/last in-flow children that adjoin this box's own
  // (box_layout_flow.cpp) collapse with them: the box moves down by what its
  // top margin gains, and its children with it.
  self_collapse[index] = 0;
  if (!BoxReplaced(node) && !inline_box) {
    const double top = BoxCollapse(box.margin[0], leading[index]);
    const double shift = top - box.margin[0];
    box.margin[0] = top;
    box.margin[2] = BoxCollapse(box.margin[2], trailing[index]);
    if (shift != 0) {
      ShiftBoxes(index, 0, shift);
      for (const auto child : node.children)
        if (BoxAbsolute(nodes[child]) && std::isfinite(static_y[child])) static_y[child] += shift;
    }
    self_collapse[index] = empty_flow[index] && box.border[3] == 0 &&
                           AdjoinsChildren(index, true) && AdjoinsChildren(index, false);
  }
  for (const auto child : node.children)
    if (BoxAbsolute(nodes[child]) && !Place(child, box.content, box.definite_height,
        std::isfinite(static_x[child]) ? static_x[child] : box.content[0],
        std::isfinite(static_y[child]) ? static_y[child] : box.content[1],
        child_positioned, depth+1)) return false;
  return true;
}
namespace {
// The overflow that applies to the viewport (CSS Overflow 3 §3.3): the root's,
// or the body's when the root's is visible on both axes.
size_t ViewportOverflowSource(const std::vector<BoxNode>& nodes) {
  const auto visible = [&](size_t i) {
    for (const int index : {BoxKnown::OverflowX, BoxKnown::OverflowY}) {
      const auto& value = nodes[i].style.Slot(index);
      if (!value.empty() && value != "visible") return false;
    }
    return true;
  };
  if (!visible(0)) return 0;
  for (const auto child : nodes[0].children)
    if (!nodes[child].text && nodes[child].tag == "body") return child;
  return 0;
}
bool LayoutPass(const std::vector<BoxNode>& nodes, const BoxViewport& viewport,
                std::vector<BoxLayout>* result);
}
bool BoxScrollContainer(const std::vector<BoxNode>& nodes, size_t index) {
  const auto& node = nodes[index];
  if (node.text || node.parent == kNoBox || BoxReplaced(node)) return false;
  if (node.tag == "body" && nodes[node.parent].parent == kNoBox && ViewportOverflowSource(nodes) == index)
    return false;
  for (const int index : {BoxKnown::OverflowX, BoxKnown::OverflowY}) {
    const auto& value = node.style.Slot(index);
    if (value == "hidden" || value == "scroll" || value == "auto") return true;
  }
  return false;
}
void BoxOverflowExtent(const std::vector<BoxNode>& nodes, const std::vector<BoxLayout>& boxes,
                       size_t index, double* right, double* bottom) {
  std::vector<std::pair<size_t, unsigned>> stack{{index, 0}};
  while (!stack.empty()) {
    const auto [current, depth] = stack.back(); stack.pop_back();
    if (depth > 256) continue;
    for (const auto child : nodes[current].children) {
      const auto& box = boxes[child];
      if (!box.present) continue;
      *right = std::max(*right, box.border[0] + box.border[2] + box.margin[1]);
      *bottom = std::max(*bottom, box.border[1] + box.border[3] + box.margin[2]);
      if (!nodes[child].text && !BoxScrollContainer(nodes, child)) stack.push_back({child, depth + 1});
    }
  }
}
bool ComputeBoxLayout(const std::vector<BoxNode>& nodes, const BoxViewport& viewport,
                       std::vector<BoxLayout>* result, BoxScrollport* scrollport) {
  if (nodes.empty() || nodes.size() > 16384) return false;
  // nsHTMLScrollFrame::ReflowContents for the viewport: lay out without
  // scrollbars, then with each auto scrollbar the scrollable overflow needs.
  const size_t source = ViewportOverflowSource(nodes);
  const auto overflow_x = nodes[source].style.Slot(BoxKnown::OverflowX), overflow_y = nodes[source].style.Slot(BoxKnown::OverflowY);
  const auto may = [](const std::string& value) { return value != "hidden" && value != "clip"; };
  bool vertical = overflow_y == "scroll", horizontal = overflow_x == "scroll";
  BoxViewport pass = viewport;
  double right = 0, bottom = 0;
  for (int attempt = 0; attempt < 4; ++attempt) {
    pass.client_width = std::max(0.0, viewport.width - (vertical ? kBoxScrollbar : 0));
    pass.client_height = std::max(0.0, viewport.height - (horizontal ? kBoxScrollbar : 0));
    if (!LayoutPass(nodes, pass, result)) return false;
    const auto& root = (*result)[0];
    right = root.border[0] + root.border[2] + root.margin[1];
    bottom = root.border[1] + root.border[3] + root.margin[2];
    BoxOverflowExtent(nodes, *result, 0, &right, &bottom);
    const bool want_vertical = overflow_y == "scroll" ||
        (may(overflow_y) && BoxRound(bottom) > pass.client_height);
    const bool want_horizontal = overflow_x == "scroll" ||
        (may(overflow_x) && BoxRound(right) > std::max(0.0, viewport.width - (want_vertical ? kBoxScrollbar : 0)));
    if (want_vertical == vertical && want_horizontal == horizontal) break;
    // Scrollbars only get added within one reflow (no oscillation).
    vertical = vertical || want_vertical; horizontal = horizontal || want_horizontal;
  }
  if (scrollport) {
    scrollport->client_width = pass.ClientWidth(); scrollport->client_height = pass.ClientHeight();
    scrollport->scroll_width = std::max(scrollport->client_width, BoxRound(right));
    scrollport->scroll_height = std::max(scrollport->client_height, BoxRound(bottom));
  }
  return true;
}
namespace {
bool LayoutPass(const std::vector<BoxNode>& nodes, const BoxViewport& viewport,
                std::vector<BoxLayout>* result) {
  result->assign(nodes.size(), {});
  BoxFlow flow{nodes, viewport, *result};
  flow.forced_width.assign(nodes.size(), BoxAuto());
  flow.forced_height.assign(nodes.size(), BoxAuto());
  flow.max_content.assign(nodes.size(), BoxAuto());
  flow.min_content.assign(nodes.size(), BoxAuto());
  flow.static_x.assign(nodes.size(), BoxAuto());
  flow.static_y.assign(nodes.size(), BoxAuto());
  flow.leading.assign(nodes.size(), 0);
  flow.trailing.assign(nodes.size(), 0);
  flow.empty_flow.assign(nodes.size(), 0);
  flow.self_collapse.assign(nodes.size(), 0);
  return flow.Place(0, {0, 0, viewport.ClientWidth(), viewport.ClientHeight()}, true, 0, 0, kNoBox, 0);
}
}
}
