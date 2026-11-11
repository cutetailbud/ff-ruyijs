// Flex layout (CSS Flexbox 1 §9): base and hypothetical main sizes, line
// collection, flexible length resolution with min/max freezing, main-axis
// auto margins and justify-content, cross sizing with stretch, align-self /
// align-items, align-content and wrap-reverse. Item boxes are produced by
// BoxFlow::Place with the resolved content-box sizes forced, so block sizing
// stays the single box owner.
#include "domain/styling/box_layout_internal.h"
#include <cstdlib>

namespace ruyijs::domain::styling {
namespace {
struct FlexItem {
  size_t node = 0;
  bool text = false, auto_start = false, auto_end = false, frozen = false;
  double base = 0, hypo = 0, target = 0, min = 0, max = 0, extra = 0, cross_extra = 0;
  double margin_start = 0, margin_end = 0, grow = 0, shrink = 1, measured = 0;
  long order = 0;
  std::string align;
  double Outer() const { return target + extra + margin_start + margin_end; }
};
struct FlexLine { std::vector<FlexItem*> items; double cross = 0, offset = 0, used = 0; };
bool AutoText(const std::string& v) { return v.empty() || v == "auto"; }
// The line box height of a text flex item, and the half-leading above its
// glyphs' content area.
double TextLine(const BoxNode& node) {
  return std::max(node.line_height >= 0 ? node.line_height : node.text_height, 0.0);
}
double Leading(const BoxNode& node) {
  return node.line_height >= 0 ? (node.line_height - node.text_height) / 2 : 0.0;
}
double Factor(const std::string& text, double fallback) {
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  return end != text.c_str() && std::isfinite(value) && value >= 0 ? value : fallback;
}
bool ScrollContainer(const BoxStyle& style) {
  for (const char* name : {"overflow", "overflow-x", "overflow-y"}) {
    const auto v = style.Get(name);
    if (v.find("hidden") != v.npos || v.find("scroll") != v.npos || v.find("auto") != v.npos) return true;
  }
  return false;
}
// §9.7 Resolving Flexible Lengths for one line.
void Resolve(std::vector<FlexItem*>& line, double available) {
  double hypothetical = 0;
  for (auto* item : line) { item->target = item->hypo; hypothetical += item->Outer(); }
  if (!std::isfinite(available)) return;
  const bool grow = hypothetical < available;
  for (auto* item : line)
    item->frozen = item->text || (grow ? item->grow == 0 : item->shrink == 0) ||
                   (grow ? item->base > item->hypo : item->base < item->hypo);
  const auto remaining = [&] {
    double used = 0;
    for (auto* item : line) used += (item->frozen ? item->target : item->base) + item->extra +
                                    item->margin_start + item->margin_end;
    return available - used;
  };
  const double initial = remaining();
  for (unsigned round = 0; round <= line.size(); ++round) {
    double free = remaining(), factors = 0, scaled = 0;
    for (auto* item : line) if (!item->frozen) {
      factors += grow ? item->grow : item->shrink;
      scaled += item->shrink * item->base;
    }
    if (factors == 0 && scaled == 0) break;
    if (factors < 1 && std::abs(initial * factors) < std::abs(free)) free = initial * factors;
    double violation = 0;
    std::vector<int> clamped(line.size(), 0);
    for (size_t i = 0; i < line.size(); ++i) {
      auto* item = line[i];
      if (item->frozen) continue;
      double size = item->base;
      if (grow && factors > 0) size += free * item->grow / factors;
      else if (!grow && scaled > 0) size += free * item->shrink * item->base / scaled;
      const double fixed = std::max(item->min, std::min(size, item->max));
      clamped[i] = fixed > size ? 1 : fixed < size ? -1 : 0;
      violation += fixed - size;
      item->target = fixed;
    }
    bool open = false;
    for (size_t i = 0; i < line.size(); ++i) {
      if (line[i]->frozen) continue;
      if (std::abs(violation) < 1e-9 || (violation > 0 && clamped[i] > 0) || (violation < 0 && clamped[i] < 0))
        line[i]->frozen = true;
      open = open || !line[i]->frozen;
    }
    if (!open) break;
  }
}
// Leading space and between-item space for justify-content / align-content.
void Distribute(const std::string& value, double free, size_t count, bool flipped, double* leading, double* between) {
  *leading = *between = 0;
  // Offsets run from the flex-relative start; start/end/left/right are physical
  // and swap sides on a reversed axis.
  bool end = value == "flex-end";
  if (value == "end" || value == "right") end = !flipped;
  if (value == "start" || value == "left") end = flipped;
  if (value == "center") *leading = free / 2;
  else if (end) *leading = free;
  else if (value == "space-between") { if (free > 0 && count > 1) *between = free / (count - 1); }
  else if (value == "space-around") { if (free > 0) { *between = free / count; *leading = *between / 2; } else *leading = free / 2; }
  else if (value == "space-evenly") { if (free > 0) { *between = free / (count + 1); *leading = *between; } else *leading = free / 2; }
}
// Every Place of the whole layout counts: a second placement re-lays out the
// item's subtree, so the cap is on total placements, not on relayouts.
constexpr size_t kPlacementsPerNode = 4;
bool PercentSize(const BoxStyle& style) {
  static constexpr const char* kNames[] = {"height", "min-height", "max-height", "flex-basis"};
  for (const char* name : kNames)
    if (style.Get(name).find('%') != std::string::npos) return true;
  return false;
}
}  // namespace

bool BoxFlow::Relayout(size_t index, bool height) {
  if (placements >= kPlacementsPerNode * nodes.size()) return false;
  const auto& node = nodes[index];
  const auto display = node.style.Get("display");
  bool needed = !height || display.find("flex") != std::string::npos || display.find("grid") != std::string::npos;
  for (const auto child : node.children) {
    if (needed) break;
    const auto& item = nodes[child];
    if (item.text) continue;
    needed = PercentSize(item.style) || (BoxAbsolute(item) && !AutoText(item.style.Get("bottom")));
  }
  return needed;
}
void BoxFlow::Resize(size_t index, bool height, double size) {
  auto& box = boxes[index];
  const double extra = height ? box.padding[0] + box.padding[2] + box.edge[0] + box.edge[2]
                              : box.padding[1] + box.padding[3] + box.edge[1] + box.edge[3];
  box.content[height ? 3 : 2] = size;
  box.border[height ? 3 : 2] = BoxRound(size + extra);
  if (height) box.definite_height = true;
  if (index < places.size()) places[index].valid = false;
}

bool BoxFlow::FlexChildren(size_t index, size_t positioned, unsigned depth, double* height, double* width) {
  const auto& style = nodes[index].style;
  const BoxRect content = boxes[index].content;
  const auto direction = style.Get("flex-direction"), wrap_mode = style.Get("flex-wrap");
  const bool column = direction.rfind("column", 0) == 0, reverse = direction.find("reverse") != direction.npos;
  const bool wrap = wrap_mode == "wrap" || wrap_mode == "wrap-reverse", wrap_reverse = wrap_mode == "wrap-reverse";
  const bool definite_height = boxes[index].definite_height;
  const double inner_main = column ? (definite_height ? content[3] : BoxAuto()) : content[2];
  const double inner_cross = column ? content[2] : (definite_height ? content[3] : BoxAuto());
  const auto gap = [&](const char* name, double basis) {
    const auto text = style.Get(name);
    return text.empty() || text == "normal" ? 0.0 : std::max(0.0, BoxLength(style, name, basis, 0));
  };
  const double main_gap = gap(column ? "row-gap" : "column-gap", inner_main);
  const double cross_gap = gap(column ? "column-gap" : "row-gap", inner_cross);
  const auto align_items = style.Get("align-items");
  std::vector<FlexItem> items;
  for (const auto child : nodes[index].children) {
    const auto& node = nodes[child];
    if (BoxAbsolute(node) || node.style.Get("display") == "none" || (node.text && node.text_width <= 0)) continue;
    FlexItem item;
    item.node = child; item.text = node.text;
    auto& box = boxes[child];
    if (node.text) { box.margin = {}; box.padding = {}; box.edge = {}; }
    else BoxEdges(node, content[2], &box);
    const auto& cs = node.style;
    const double pad_x = box.padding[1] + box.padding[3] + box.edge[1] + box.edge[3];
    const double pad_y = box.padding[0] + box.padding[2] + box.edge[0] + box.edge[2];
    item.extra = column ? pad_y : pad_x; item.cross_extra = column ? pad_x : pad_y;
    item.margin_start = column ? box.margin[0] : box.margin[3];
    item.margin_end = column ? box.margin[2] : box.margin[1];
    item.align = align_items;
    if (!node.text) {
      item.auto_start = cs.Get(column ? "margin-top" : "margin-left") == "auto";
      item.auto_end = cs.Get(column ? "margin-bottom" : "margin-right") == "auto";
      item.grow = Factor(cs.Get("flex-grow"), 0); item.shrink = Factor(cs.Get("flex-shrink"), 1);
      item.order = std::strtol(cs.Get("order").c_str(), nullptr, 10);
      const auto self = cs.Get("align-self");
      if (!AutoText(self)) item.align = self;
    }
    items.push_back(std::move(item));
  }
  std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.order < b.order; });
  for (auto& item : items) {
    const auto& node = nodes[item.node];
    const auto& cs = node.style;
    const bool border_box = cs.Get("box-sizing") == "border-box";
    const auto length = [&](const char* name, double percent, double extra) {
      const auto text = cs.Get(name);
      double value = text.find("content") != text.npos ? BoxAuto() : BoxLength(cs, name, percent, BoxAuto());
      BoxViewportLength(text, viewport.width, viewport.height, &value);
      return std::isfinite(value) && border_box ? std::max(0.0, value - extra) : value;
    };
    if (node.text) {
      // A text run is an anonymous block flex item holding one line box: its
      // block size is the line height (nsLineLayout), not the glyph extent.
      item.base = item.hypo = column ? TextLine(node) : node.text_width;
      item.min = column ? item.base : (node.breakable ? 0 : item.base);
      item.max = std::numeric_limits<double>::infinity();
      continue;
    }
    const char* main_name = column ? "height" : "width";
    if (column) {
      // The cross size is fixed first; the content height is measured with it.
      const auto& box = boxes[item.node];
      const double room = std::max(0.0, content[2] - box.margin[1] - box.margin[3] - item.cross_extra);
      double cross = length("width", content[2], item.cross_extra);
      const bool stretch = item.align.empty() || item.align == "normal" || item.align == "stretch";
      const bool auto_margins = cs.Get("margin-left") == "auto" || cs.Get("margin-right") == "auto";
      if (!std::isfinite(cross)) cross = stretch && !auto_margins ? room : std::min(room, Intrinsic(item.node, false, depth + 1));
      forced_width[item.node] = BoxLimit(cs, true, content[2], cross, item.cross_extra);
      forced_height[item.node] = BoxAuto();
      if (!Place(item.node, content, definite_height, content[0], content[1], positioned, depth + 1)) return false;
      item.measured = boxes[item.node].content[3];
    }
    double base = BoxAuto();
    const auto basis = cs.Get("flex-basis");
    if (AutoText(basis)) base = length(main_name, inner_main, item.extra);
    else base = length("flex-basis", inner_main, item.extra);
    const double content_size = column ? item.measured : Intrinsic(item.node, false, depth + 1);
    if (!std::isfinite(base)) base = content_size;
    const char* min_name = column ? "min-height" : "min-width";
    const char* max_name = column ? "max-height" : "max-width";
    const auto max_text = cs.Get(max_name);
    item.max = max_text.empty() || max_text == "none" ? std::numeric_limits<double>::infinity()
                                                       : length(max_name, inner_main, item.extra);
    if (!std::isfinite(item.max)) item.max = std::numeric_limits<double>::infinity();
    if (AutoText(cs.Get(min_name))) {
      // §4.5 automatic minimum: the content size suggestion, capped by a
      // definite specified size; zero for a scroll container.
      double minimum = ScrollContainer(cs) ? 0 : (column ? item.measured : IntrinsicContent(item.node, true, depth + 1));
      const double specified = length(main_name, inner_main, item.extra);
      if (std::isfinite(specified)) minimum = std::min(minimum, specified);
      item.min = std::min(minimum, item.max);
    } else {
      const double minimum = length(min_name, inner_main, item.extra);
      item.min = std::isfinite(minimum) ? minimum : 0;
    }
    item.base = base;
    item.hypo = std::max(item.min, std::min(base, item.max));
  }
  std::vector<FlexLine> lines(1);
  double run = 0;
  for (auto& item : items) {
    const double outer = item.hypo + item.extra + item.margin_start + item.margin_end;
    if (wrap && std::isfinite(inner_main) && !lines.back().items.empty() && run + main_gap + outer > inner_main + 1e-6) {
      lines.emplace_back();
      run = 0;
    }
    run += (lines.back().items.empty() ? 0 : main_gap) + outer;
    lines.back().items.push_back(&item);
  }
  double main_extent = 0;
  for (auto& line : lines) {
    const size_t count = line.items.size();
    Resolve(line.items, inner_main - main_gap * (count ? count - 1 : 0));
    double used = count ? main_gap * (count - 1) : 0;
    for (auto* item : line.items) used += item->Outer();
    line.used = used;
    main_extent = std::max(main_extent, used);
  }
  const double main_room = std::isfinite(inner_main) ? inner_main : main_extent;
  for (auto& line : lines) {
    const size_t count = line.items.size();
    double free = main_room - line.used, leading = 0, between = 0;
    size_t autos = 0;
    for (auto* item : line.items) autos += item->auto_start + item->auto_end;
    const double share = autos && free > 0 ? free / autos : 0;
    if (!autos || free <= 0) Distribute(style.Get("justify-content"), free, count, reverse, &leading, &between);
    double cursor = leading;
    for (auto* item : line.items) {
      auto& box = boxes[item->node];
      const double start = item->margin_start + (item->auto_start ? share : 0);
      const double border_main = item->target + item->extra;
      const double offset = cursor + start;
      const double position = reverse ? main_room - offset - border_main : offset;
      cursor = offset + border_main + item->margin_end + (item->auto_end ? share : 0) + between + main_gap;
      if (!item->text) {
        if (column) forced_height[item->node] = item->target;
        else { forced_width[item->node] = item->target; forced_height[item->node] = BoxAuto(); }
      }
      const double bx = column ? content[0] : content[0] + position;
      const double by = column ? content[1] + position : content[1];
      if (column && !item->text && (std::abs(item->target - item->measured) < 1e-6 || !Relayout(item->node, true))) {
        Resize(item->node, true, item->target);
        ShiftBoxes(item->node, BoxRound(bx) - box.border[0], BoxRound(by) - box.border[1]);
        continue;
      }
      if (!Place(item->node, content, definite_height, bx - box.margin[3], by - box.margin[0], positioned, depth + 1))
        return false;
      // The glyphs sit half the leading below the anonymous block's top.
      if (item->text) ShiftBoxes(item->node, BoxRound(bx) - box.border[0],
                                 BoxRound(by + Leading(nodes[item->node])) - box.border[1]);
    }
    for (auto* item : line.items) {
      const auto& box = boxes[item->node];
      const double block = item->text ? TextLine(nodes[item->node]) : box.border[3];
      line.cross = std::max(line.cross, column ? box.border[2] + box.margin[1] + box.margin[3]
                                               : block + box.margin[0] + box.margin[2]);
    }
  }
  if (!wrap && std::isfinite(inner_cross)) lines[0].cross = inner_cross;
  double cross_used = cross_gap * (lines.size() - 1);
  for (const auto& line : lines) cross_used += line.cross;
  double cross_leading = 0, cross_between = 0;
  if (wrap && std::isfinite(inner_cross)) {
    const double free = inner_cross - cross_used;
    const auto align_content = style.Get("align-content");
    if ((align_content.empty() || align_content == "normal" || align_content == "stretch") && free > 0) {
      for (auto& line : lines) line.cross += free / lines.size();
      cross_used = inner_cross;
    } else {
      Distribute(align_content, free, lines.size(), wrap_reverse, &cross_leading, &cross_between);
    }
  }
  const double cross_room = std::isfinite(inner_cross) ? inner_cross : cross_used;
  double cursor = cross_leading;
  for (auto& line : lines) {
    line.offset = wrap_reverse ? cross_room - cursor - line.cross : cursor;
    cursor += line.cross + cross_between + cross_gap;
    for (auto* item : line.items) {
      const auto& node = nodes[item->node];
      auto& box = boxes[item->node];
      auto align = item->align;
      if (align.empty() || align == "normal") align = "stretch";
      const char* start_margin = column ? "margin-left" : "margin-top";
      const char* end_margin = column ? "margin-right" : "margin-bottom";
      const bool auto_before = !node.text && node.style.Get(start_margin) == "auto";
      const bool auto_after = !node.text && node.style.Get(end_margin) == "auto";
      if (!node.text && align == "stretch" && !auto_before && !auto_after &&
          AutoText(node.style.Get(column ? "width" : "height"))) {
        const double margins = column ? box.margin[1] + box.margin[3] : box.margin[0] + box.margin[2];
        const double target = BoxLimit(node.style, column, column ? content[2] : inner_cross,
                                       std::max(0.0, line.cross - margins - item->cross_extra), item->cross_extra);
        const double current = column ? box.content[2] : box.content[3];
        if (std::abs(target - current) > 1e-6) {
          if (!Relayout(item->node, !column)) {
            Resize(item->node, !column, target);
          } else {
            if (column) forced_width[item->node] = target; else forced_height[item->node] = target;
            const double bx = box.border[0], by = box.border[1];
            if (!Place(item->node, content, definite_height, bx - box.margin[3], by - box.margin[0], positioned, depth + 1))
              return false;
          }
        }
      }
      const double outer = column ? box.border[2] + box.margin[1] + box.margin[3] : box.border[3] + box.margin[0] + box.margin[2];
      const double free = line.cross - outer;
      double offset = 0;
      if (auto_before || auto_after) offset = free <= 0 ? 0 : auto_before && auto_after ? free / 2 : auto_before ? free : 0;
      else if (align == "center") offset = free / 2;
      else if ((align == "flex-end" || align == "end" || align == "self-end") != wrap_reverse) offset = free;
      const double margin_before = column ? box.margin[3] : box.margin[0];
      const double cross_origin = (column ? content[0] : content[1]) + line.offset + margin_before + offset;
      const double current = column ? box.border[0] : box.border[1];
      if (column) ShiftBoxes(item->node, BoxRound(cross_origin) - current, 0);
      else ShiftBoxes(item->node, 0, BoxRound(cross_origin) - current);
    }
  }
  *height = BoxRound(std::max(0.0, column ? main_extent : cross_used));
  *width = BoxRound(std::max(0.0, column ? cross_used : main_extent));
  return true;
}
}  // namespace ruyijs::domain::styling
