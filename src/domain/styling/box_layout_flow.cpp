#include "domain/styling/box_layout_internal.h"

namespace ruyijs::domain::styling {
void BoxFlow::ShiftBoxes(size_t index, double dx, double dy) {
  if (dx == 0 && dy == 0) return;
  auto& box = boxes[index];
  box.border[0] += dx; box.border[1] += dy;
  box.content[0] += dx; box.content[1] += dy;
  for (const auto child : nodes[index].children) ShiftBoxes(child, dx, dy);
}
bool BoxFlow::AdjoinsChildren(size_t index, bool top) const {
  const auto& node = nodes[index];
  const auto& box = boxes[index];
  // The root, replaced and out-of-flow boxes, and every box that establishes a
  // new block formatting context keep their children's margins inside.
  if (node.text || node.parent == kNoBox || BoxReplaced(node) || BoxAbsolute(node)) return false;
  const auto display = node.style.Slot(BoxKnown::Display);
  if (display != "block" && display != "list-item") return false;
  const auto container = nodes[node.parent].style.Slot(BoxKnown::Display);
  if (container == "flex" || container == "inline-flex" || container == "grid" ||
      container == "inline-grid") return false;
  const auto floating = node.style.Slot(BoxKnown::Float);
  if (!floating.empty() && floating != "none") return false;
  for (const int index : {BoxKnown::OverflowX, BoxKnown::OverflowY}) {
    const auto& value = node.style.Slot(index);
    if (!value.empty() && value != "visible" && value != "clip") return false;
  }
  if (top) return box.edge[0] == 0 && box.padding[0] == 0;
  if (box.edge[2] != 0 || box.padding[2] != 0 || box.definite_height) return false;
  const auto minimum = node.style.Slot(BoxKnown::MinHeight);
  return minimum.empty() || minimum == "auto" || minimum == "0" || minimum == "0px";
}
bool BoxFlow::Children(size_t index, size_t positioned, unsigned depth, double* height, double* width) {
  leading[index] = trailing[index] = 0;
  empty_flow[index] = 1;
  const auto display = nodes[index].style.Slot(BoxKnown::Display);
  if (display == "flex" || display == "inline-flex") {
    empty_flow[index] = 0;
    return FlexChildren(index, positioned, depth, height, width);
  }
  if (display == "grid" || display == "inline-grid") {
    empty_flow[index] = 0;
    return GridChildren(index, positioned, depth, height, width);
  }
  const auto& parent = boxes[index];
  double cursor_y = 0, line_x = 0, line_height = 0, extent_x = 0;
  // The adjoining margins not yet resolved: the largest positive and the most
  // negative (CSS 2 §8.3.1). While |leading| holds, they adjoin the parent's
  // top margin and collapse through it instead of moving the first box.
  double positive = 0, negative = 0;
  bool open = AdjoinsChildren(index, true);
  const auto add = [&](double margin) {
    positive = std::max(positive, margin); negative = std::min(negative, margin);
  };
  // Content (a block's border box or a line box) ends the run of adjoining margins.
  const auto resolve = [&]() {
    if (open) leading[index] = positive + negative;
    else cursor_y += positive + negative;
    open = false; positive = negative = 0;
  };
  for (const auto child : nodes[index].children) {
    const auto& node = nodes[child];
    const auto display = node.style.Slot(BoxKnown::Display);
    if (display == "none") continue;
    if (BoxAbsolute(node)) {
      // CSS static position: the position this child would have occupied in
      // normal flow. Do not consume space; resolve it after the parent size.
      const bool inline_position = display == "inline" || display == "inline-block";
      static_x[child] = parent.content[0] + (inline_position ? line_x : 0);
      static_y[child] = parent.content[1] + cursor_y +
          (inline_position ? 0 : line_height + (open ? 0 : positive + negative));
      continue;
    }
    const bool in_line = node.text || display == "inline" || display == "inline-block";
    if (!in_line) { cursor_y += line_height; line_x = line_height = 0; }
    // A line box with content separates the margins before it from those after.
    if (in_line && line_x == 0 && line_height == 0 && (!node.text || node.text_width > 0)) {
      resolve();
      empty_flow[index] = 0;
    }
    const double x = parent.content[0]+(in_line ? line_x : 0);
    // Anonymous text boxes inherit font properties, but element margins,
    // padding, and borders do not apply to their inline content. Place
    // computes element edges itself, for this same content-box width, so
    // doing it here only to overwrite it was a second pass per child.
    if (node.text) {
      boxes[child].margin = {};
      boxes[child].padding = {};
      boxes[child].edge = {};
    }
    const double y = parent.content[1]+cursor_y;
    if (!Place(child, parent.content, parent.definite_height, x, y, positioned, depth+1)) return false;
    const auto& box = boxes[child];
    if (!box.present) continue;
    const double outer_width = box.border[2]+box.margin[1]+box.margin[3];
    if (in_line) {
      const double outer_height = box.border[3]+box.margin[0]+box.margin[2];
      line_x += outer_width;
      // An inline replaced element is baseline-aligned. Firefox's line box
      // keeps the normal-font descent below an iframe/image baseline; without
      // it the containing block is 5.5px too short for the Turnstile iframe.
      const double baseline_descent = BoxReplaced(node) ? 5.5 : 0.0;
      const bool inline_flow = node.text || (display == "inline" && !BoxReplaced(node));
      line_height = std::max(line_height, inline_flow ? box.line : outer_height + baseline_descent);
      extent_x = std::max(extent_x,line_x);
      continue;
    }
    extent_x = std::max(extent_x,outer_width);
    // Place put the border box at y + its (collapsed) top margin; move it to
    // where the pending margins put it, keeping any relative offset.
    const double placed = y + box.margin[0];
    add(box.margin[0]);
    if (self_collapse[child]) {
      // A self-collapsing box's own margins collapse through it with its
      // neighbours'; its border box sits at its collapsed top margin.
      ShiftBoxes(child, 0, parent.content[1] + cursor_y + (open ? 0 : positive + negative) - placed);
      add(box.margin[2]);
      continue;
    }
    resolve();
    empty_flow[index] = 0;
    ShiftBoxes(child, 0, parent.content[1] + cursor_y - placed);
    cursor_y += box.border[3];
    add(box.margin[2]);
  }
  cursor_y += line_height;
  if (open) {
    // No in-flow content: every margin met collapses through the top edge.
    leading[index] = positive + negative;
  } else if (AdjoinsChildren(index, false)) {
    trailing[index] = positive + negative;
  } else {
    cursor_y += positive + negative;
  }
  *height = BoxRound(cursor_y); *width = BoxRound(extent_x);
  return true;
}

}
