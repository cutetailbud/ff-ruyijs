#include "web_geometry/box_input.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ruyijs::web_geometry {
namespace {
bool Positioned(const box::BoxNode& node) {
  const auto position = node.style.Get("position"), transform = node.style.Get("transform");
  return (!position.empty() && position != "static") || (!transform.empty() && transform != "none");
}
}
// nsGenericHTMLElement::GetOffsetRect: body, the root and fixed boxes have no
// offset parent; a static body parent measures from the initial containing block.
namespace {
bool Clips(const box::BoxNode& node) {
  for (const char* name : {"overflow", "overflow-x", "overflow-y"}) {
    const auto value = node.style.Get(name);
    if (value.find("hidden") != value.npos || value.find("scroll") != value.npos ||
        value.find("auto") != value.npos || value.find("clip") != value.npos) return true;
  }
  return false;
}
// The right/bottom edge of |index|'s descendants' border boxes, including the
// overflow of every descendant that does not clip its own.
void Extent(const LayoutCache& cache, size_t index, unsigned depth, double* right, double* bottom) {
  if (depth > 256) return;
  for (const auto child : cache.nodes[index].children) {
    const auto& box = cache.layout[child];
    if (!box.present) continue;
    *right = std::max(*right, box.border[0] + box.border[2] + box.margin[1]);
    *bottom = std::max(*bottom, box.border[1] + box.border[3] + box.margin[2]);
    if (!cache.nodes[child].text && !Clips(cache.nodes[child])) Extent(cache, child, depth + 1, right, bottom);
  }
}
}  // namespace
void OffsetFrom(const LayoutCache& cache, size_t index, document_model::OffsetGeometry* out) {
  const auto& node = cache.nodes[index];
  const auto& box = cache.layout[index];
  if (!box.present) return;
  out->present = true;
  out->width = box.border[2]; out->height = box.border[3];
  out->root = node.parent == size_t(-1);
  const bool replaced = node.tag == "iframe" || node.tag == "img" || node.tag == "canvas" ||
                        node.tag == "video" || node.tag == "object" || node.tag == "embed";
  if (replaced || node.style.Get("display") != "inline") {
    // The padding box less the scrollbars (nsIFrame::GetPaddingRect less the
    // scrollbar gutter), and the left/top border widths.
    out->client_left = box.edge[3]; out->client_top = box.edge[0];
    out->scroller = box::BoxScrollContainer(cache.nodes, index);
    out->client_width = std::max(0.0, box.border[2] - box.edge[1] - box.edge[3] - box.scrollbar[0]);
    out->client_height = std::max(0.0, box.border[3] - box.edge[0] - box.edge[2] - box.scrollbar[1]);
    // Content overflow is inflated by the end padding (Firefox 158).
    const double left = box.border[0] + box.edge[3], top = box.border[1] + box.edge[0];
    double right = left + out->client_width - box.padding[1], bottom = top + out->client_height - box.padding[2];
    Extent(cache, index, 0, &right, &bottom);
    out->scroll_width = std::max(out->client_width, right + box.padding[1] - left);
    out->scroll_height = std::max(out->client_height, bottom + box.padding[2] - top);
  }
  out->own_scroll_width = out->scroll_width; out->own_scroll_height = out->scroll_height;
  if (out->root) {
    // The root element's client and scroll sizes are the viewport scroll
    // frame's: the scrollport and the scrollable overflow.
    out->client_left = out->client_top = 0;
    out->client_width = cache.scrollport.client_width;
    out->client_height = cache.scrollport.client_height;
    out->scroll_width = cache.scrollport.scroll_width;
    out->scroll_height = cache.scrollport.scroll_height;
  }
  if (node.tag == "body") return;
  out->left = box.border[0]; out->top = box.border[1];
  if (node.parent == size_t(-1) || node.style.Get("position") == "fixed") return;
  const bool statically = !Positioned(node);
  size_t parent = node.parent;
  for (unsigned depth = 0; parent != size_t(-1) && depth <= 256; ++depth) {
    const auto& ancestor = cache.nodes[parent];
    if (ancestor.tag == "body" || Positioned(ancestor) || (statically &&
        (ancestor.tag == "td" || ancestor.tag == "th" || ancestor.tag == "table"))) break;
    parent = ancestor.parent;
  }
  if (parent == size_t(-1) || parent >= cache.meta->keys.size()) return;
  out->parent = true; out->parentKey = cache.meta->keys[parent];
  const auto& origin = cache.layout[parent];
  if (cache.nodes[parent].tag == "body" && !Positioned(cache.nodes[parent])) return;
  out->left -= origin.border[0] + origin.edge[3];
  out->top -= origin.border[1] + origin.edge[0];
}
namespace {
size_t NextContainingAncestor(const LayoutCache& cache, size_t cur) {
  const auto pos = cache.nodes[cur].style.Get("position");
  if (pos == "fixed") {
    for (size_t p = cache.nodes[cur].parent; p != size_t(-1); p = cache.nodes[p].parent) {
      const auto tf = cache.nodes[p].style.Get("transform");
      if (!tf.empty() && tf != "none") return p;
    }
    return size_t(-1);
  }
  if (pos == "absolute") {
    for (size_t p = cache.nodes[cur].parent; p != size_t(-1); p = cache.nodes[p].parent)
      if (Positioned(cache.nodes[p])) return p;
    return size_t(-1);
  }
  return cache.nodes[cur].parent;
}

bool ParseZIndexInt(const box::BoxNode& node, int64_t* z_out) {
  std::string raw = node.style.Get("z-index");
  size_t b = 0, e = raw.size();
  while (b < e && (raw[b] == ' ' || raw[b] == '\t')) ++b;
  while (e > b && (raw[e - 1] == ' ' || raw[e - 1] == '\t')) --e;
  if (b == e) return false;
  std::string s = raw.substr(b, e - b);
  if (s == "auto") return false;
  char* end = nullptr;
  const long long v = std::strtoll(s.c_str(), &end, 10);
  if (!end || end == s.c_str() || *end != '\0') return false;
  *z_out = int64_t(v);
  return true;
}

bool FormsStackingContext(const LayoutCache& cache, size_t idx, int64_t* z_val) {
  *z_val = 0;
  if (idx >= cache.nodes.size() || cache.nodes[idx].text || !cache.layout[idx].present) return false;
  const auto& node = cache.nodes[idx];
  int64_t z = 0;
  const bool has_z = ParseZIndexInt(node, &z);
  if (has_z) *z_val = z;
  if (idx == 0) return true;
  const auto pos = node.style.Get("position");
  const bool is_pos = !pos.empty() && pos != "static";
  if (is_pos && (has_z || pos == "fixed" || pos == "sticky")) return true;
  if (has_z && node.parent != size_t(-1)) {
    const auto pdisp = cache.nodes[node.parent].style.Get("display");
    if (pdisp == "flex" || pdisp == "inline-flex" || pdisp == "grid" || pdisp == "inline-grid")
      return true;
  }
  const auto op = node.style.Get("opacity");
  if (!op.empty() && op != "1") {
    char* end = nullptr;
    const double v = std::strtod(op.c_str(), &end);
    if (end && end != op.c_str() && v < 1.0) return true;
  }
  for (const char* prop : {"transform", "filter", "backdrop-filter", "clip-path", "mask-image", "perspective"}) {
    const auto v = node.style.Get(prop);
    if (!v.empty() && v != "none") return true;
  }
  if (node.style.Get("isolation") == "isolate") return true;
  const auto mbm = node.style.Get("mix-blend-mode");
  if (!mbm.empty() && mbm != "normal") return true;
  return false;
}

struct PaintOrderBuilder {
  const LayoutCache& cache;
  std::vector<char> is_sc;
  std::vector<char> is_pos;
  std::vector<int64_t> sc_z;
  std::vector<size_t> order;

  explicit PaintOrderBuilder(const LayoutCache& c)
      : cache(c), is_sc(c.nodes.size(), 0), is_pos(c.nodes.size(), 0), sc_z(c.nodes.size(), 0) {
    order.reserve(c.nodes.size());
    for (size_t i = 0; i < c.nodes.size(); ++i) {
      if (c.nodes[i].text || !c.layout[i].present) continue;
      const auto pos = c.nodes[i].style.Get("position");
      is_pos[i] = (!pos.empty() && pos != "static") ? 1 : 0;
      int64_t z = 0;
      if (FormsStackingContext(c, i, &z)) {
        is_sc[i] = 1;
        sc_z[i] = z;
      }
    }
  }

  void EmitLocalSubtree(size_t root, unsigned depth, std::vector<size_t>* out) {
    if (depth > 256) return;
    out->push_back(root);
    std::vector<size_t> blocks, floats, inlines;
    CollectLocalFlow(root, depth + 1, &blocks, &floats, &inlines);
    out->insert(out->end(), blocks.begin(), blocks.end());
    out->insert(out->end(), floats.begin(), floats.end());
    out->insert(out->end(), inlines.begin(), inlines.end());
  }

  void CollectLocalFlow(size_t box_root, unsigned depth, std::vector<size_t>* blocks,
                        std::vector<size_t>* floats, std::vector<size_t>* inlines) {
    if (depth > 256) return;
    for (const size_t c : cache.nodes[box_root].children) {
      if (cache.nodes[c].text || !cache.layout[c].present) continue;
      if (is_sc[c] || is_pos[c]) continue;
      const auto fl = cache.nodes[c].style.Get("float");
      const auto disp = cache.nodes[c].style.Get("display");
      if (!fl.empty() && fl != "none") {
        EmitLocalSubtree(c, depth + 1, floats);
      } else if (disp == "inline" || disp == "inline-block" || disp == "inline-flex" ||
                 disp == "inline-grid" || disp == "inline-table") {
        EmitLocalSubtree(c, depth + 1, inlines);
      } else {
        blocks->push_back(c);
        CollectLocalFlow(c, depth + 1, blocks, floats, inlines);
      }
    }
  }

  void CollectSCItems(size_t cur, unsigned depth, std::vector<size_t>* neg_sc,
                      std::vector<size_t>* zero_auto, std::vector<size_t>* pos_sc) {
    if (depth > 256) return;
    for (const size_t c : cache.nodes[cur].children) {
      if (cache.nodes[c].text || !cache.layout[c].present) continue;
      if (is_sc[c]) {
        if (sc_z[c] < 0) neg_sc->push_back(c);
        else if (sc_z[c] > 0) pos_sc->push_back(c);
        else zero_auto->push_back(c);
      } else {
        if (is_pos[c]) zero_auto->push_back(c);
        CollectSCItems(c, depth + 1, neg_sc, zero_auto, pos_sc);
      }
    }
  }

  void PaintSC(size_t sc_root, unsigned depth) {
    if (depth > 256) return;
    std::vector<size_t> neg_sc, zero_auto, pos_sc;
    CollectSCItems(sc_root, depth, &neg_sc, &zero_auto, &pos_sc);
    std::stable_sort(neg_sc.begin(), neg_sc.end(), [&](size_t a, size_t b) { return sc_z[a] < sc_z[b]; });
    std::stable_sort(pos_sc.begin(), pos_sc.end(), [&](size_t a, size_t b) { return sc_z[a] < sc_z[b]; });

    order.push_back(sc_root);
    for (const size_t child_sc : neg_sc) PaintSC(child_sc, depth + 1);

    std::vector<size_t> blocks, floats, inlines;
    CollectLocalFlow(sc_root, depth, &blocks, &floats, &inlines);
    order.insert(order.end(), blocks.begin(), blocks.end());
    order.insert(order.end(), floats.begin(), floats.end());
    order.insert(order.end(), inlines.begin(), inlines.end());

    for (const size_t item : zero_auto) {
      if (is_sc[item]) PaintSC(item, depth + 1);
      else EmitLocalSubtree(item, depth + 1, &order);
    }
    for (const size_t child_sc : pos_sc) PaintSC(child_sc, depth + 1);
  }
};
}  // namespace

bool Realm::HitTestPoint(JS::HandleObject root_element, double x, double y, bool all,
                         std::vector<document_model::NodeKey>* out) {
  out->clear();
  if (!std::isfinite(x) || !std::isfinite(y)) return true;
  std::shared_ptr<const LayoutCache> layout;
  size_t found = size_t(-1);
  box::BoxViewport viewport;
  if (!LayoutFor(*state_, root_element, &layout, &found, &viewport)) return false;
  if (!layout || layout->nodes.empty() || !layout->layout[0].present) return true;

  const double vp_w = layout->scrollport.client_width > 0 ? layout->scrollport.client_width : viewport.ClientWidth();
  const double vp_h = layout->scrollport.client_height > 0 ? layout->scrollport.client_height : viewport.ClientHeight();
  if (x < 0.0 || y < 0.0 || x >= vp_w || y >= vp_h) return true;

  PaintOrderBuilder builder(*layout);
  builder.PaintSC(0, 0);

  for (size_t k = builder.order.size(); k-- > 0;) {
    const size_t idx = builder.order[k];
    if (layout->nodes[idx].text || idx >= layout->meta->is_element.size() || !layout->meta->is_element[idx])
      continue;
    const auto vis = layout->nodes[idx].style.Get("visibility");
    if (!vis.empty() && vis != "visible") continue;
    const auto pe = layout->nodes[idx].style.Get("pointer-events");
    if (pe == "none") continue;

    if (idx > 0) {
      auto tview = viewport;
      const auto tscroll = AncestorScroll(*state_, *layout, idx);
      tview.scroll_x += tscroll[0];
      tview.scroll_y += tscroll[1];
      if (!box::HitBoxPoint(layout->nodes, layout->layout, tview, idx, x, y)) continue;

      bool clipped_out = false;
      for (size_t cur = idx, anc = NextContainingAncestor(*layout, cur);
           anc != size_t(-1) && anc != 0;
           cur = anc, anc = NextContainingAncestor(*layout, cur)) {
        if (!Clips(layout->nodes[anc])) continue;
        if (layout->nodes[anc].tag == "body" && layout->nodes[anc].parent == 0 &&
            !box::BoxScrollContainer(layout->nodes, anc)) continue;
        auto aview = viewport;
        const auto ascroll = AncestorScroll(*state_, *layout, anc);
        aview.scroll_x += ascroll[0];
        aview.scroll_y += ascroll[1];
        const auto aborder = box::ProjectBoxRect(layout->nodes, layout->layout, aview, anc);
        document_model::OffsetGeometry ag;
        OffsetFrom(*layout, anc, &ag);
        const double cl = aborder[0] + ag.client_left, ct = aborder[1] + ag.client_top;
        if (!(x >= cl && x < cl + ag.client_width && y >= ct && y < ct + ag.client_height)) {
          clipped_out = true;
          break;
        }
      }
      if (clipped_out) continue;
    }

    if (idx < layout->meta->keys.size() && layout->meta->keys[idx].node) {
      out->push_back(layout->meta->keys[idx]);
      if (!all) break;
    }
  }
  return true;
}
}
