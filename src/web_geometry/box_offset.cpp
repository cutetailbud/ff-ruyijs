#include "web_geometry/box_input.h"
#include <algorithm>

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
}
