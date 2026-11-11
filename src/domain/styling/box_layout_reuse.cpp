// Incremental subtree reuse across mutations and viewport scrollbar passes.
// Because BoxNode lists are in strict DFS preorder, every clean subtree sits in
// a contiguous slice [index, index + subtree_size) and can be copied + shifted
// in O(subtree) without re-running edge resolution or child flow when its
// containing-block and flex/grid forced constraints match.
#include "domain/styling/box_layout_internal.h"

namespace ruyijs::domain::styling {
namespace {
bool SameConstraint(double a, double b) {
  return (std::isnan(a) && std::isnan(b)) || a == b;
}
bool ValidCache(const BoxLayoutCache& c, const BoxViewport& vp, size_t expected) {
  const size_t m = c.boxes.size();
  return m != 0 && (expected == 0 || m == expected) && c.places.size() == m &&
         c.leading.size() == m && c.trailing.size() == m && c.empty_flow.size() == m &&
         c.self_collapse.size() == m && vp.width == c.viewport_width && vp.height == c.viewport_height;
}
}  // namespace

bool PrepareBoxReuse(const std::vector<BoxNode>& nodes, const BoxViewport& viewport,
                     size_t overflow_source, const BoxReuseInput* input, BoxFlow* flow,
                     BoxReuseState* out) {
  if (!input || !input->cache) return false;
  const size_t n = nodes.size();
  const bool identity = !input->previous_nodes && !input->before;
  if (n == 0 || (!identity && (!input->previous_nodes || !input->before || input->before->size() != n)) ||
      (input->same_node && input->same_node->size() != n))
    return false;
  const size_t m = identity ? n : input->previous_nodes->size();
  const auto& cache = *input->cache;
  if (!ValidCache(cache, viewport, m)) return false;

  std::vector<size_t> old_size;
  if (!identity) {
    const auto& old_nodes = *input->previous_nodes;
    old_size.assign(m, 1);
    for (size_t i = m; i-- > 0;) {
      size_t s = 1;
      for (const size_t c : old_nodes[i].children) s += old_size[c];
      old_size[i] = s;
    }
  }

  out->cache = &cache;
  out->before = input->before;
  if (input->fallback_cache && input->fallback_clean && input->fallback_clean->size() == n &&
      ValidCache(*input->fallback_cache, viewport, 0)) {
    out->fallback_cache = input->fallback_cache;
    out->fallback_before = input->fallback_before;
    out->fallback_clean = input->fallback_clean;
  }
  out->overflow_source = overflow_source;
  out->subtree_size.assign(n, 1);
  out->subtree_clean.assign(n, 0);
  out->has_unescaped_abs.assign(n, 0);
  out->has_fixed.assign(n, 0);

  for (size_t i = n; i-- > 0;) {
    const auto& node = nodes[i];
    const size_t b = identity ? i : (*input->before)[i];
    size_t s = 1;
    bool fixed = false, child_unescaped = false;
    bool clean = b < m && (!input->same_node || (*input->same_node)[i]) &&
                 (identity || node.children.size() == (*input->previous_nodes)[b].children.size());
    for (size_t k = 0; k < node.children.size(); ++k) {
      const size_t c = node.children[k];
      s += out->subtree_size[c];
      if (out->has_fixed[c]) fixed = true;
      if (out->has_unescaped_abs[c]) child_unescaped = true;
      const auto& child = nodes[c];
      if (!child.text) {
        const auto& pos = child.style.Slot(BoxKnown::Position);
        if (pos == "fixed") fixed = true;
        if (pos == "absolute") child_unescaped = true;
      }
      if (clean && (!out->subtree_clean[c] ||
                    (!identity && (*input->before)[c] != (*input->previous_nodes)[b].children[k])))
        clean = false;
    }
    if (clean && !identity && (s != old_size[b] || (s > 1 && (*input->before)[i + s - 1] != b + s - 1)))
      clean = false;
    out->subtree_size[i] = s;
    out->has_fixed[i] = fixed ? 1 : 0;
    const bool establishes = !node.text && (node.style.Slot(BoxKnown::Position) != "static" ||
                                            node.style.Slot(BoxKnown::Transform) != "none");
    out->has_unescaped_abs[i] = (!establishes && child_unescaped) ? 1 : 0;
    out->subtree_clean[i] = clean ? 1 : 0;
    if (clean && b < cache.min_content.size() && b < cache.max_content.size()) {
      flow->min_content[i] = cache.min_content[b];
      flow->max_content[i] = cache.max_content[b];
    }
  }
  return true;
}

bool BoxFlow::TryReuse(size_t index, const BoxRect& containing, bool definite_height,
                       double x, double y, bool parent_bfc) {
  if (!reuse || index == 0) return false;
  const auto& node = nodes[index];
  if (node.text || BoxAbsolute(node) || reuse->has_fixed[index] || reuse->has_unescaped_abs[index])
    return false;
  const double fw = index < forced_width.size() ? forced_width[index] : BoxAuto();
  const double fh = index < forced_height.size() ? forced_height[index] : BoxAuto();
  const auto attempt = [&](const BoxLayoutCache& src, size_t b) -> bool {
    if (b + reuse->subtree_size[index] > src.boxes.size()) return false;
    if (node.parent == 0 && ((reuse->overflow_source == index) != (src.overflow_source == b)))
      return false;
    const auto& rec = src.places[b];
    if (!rec.valid || rec.definite_height != definite_height || rec.parent_bfc != parent_bfc)
      return false;
    if (!SameConstraint(containing[2], rec.cb_w) || !SameConstraint(containing[3], rec.cb_h) ||
        !SameConstraint(fw, rec.forced_w) || !SameConstraint(fh, rec.forced_h))
      return false;
    const double dx = BoxRound(x + rec.rel_bx) - src.boxes[b].border[0];
    const double dy = BoxRound(y + rec.rel_by) + rec.collapse_shift - src.boxes[b].border[1];
    const size_t count = reuse->subtree_size[index];
    for (size_t k = 0; k < count; ++k) {
      BoxLayout copy = src.boxes[b + k];
      if (dx != 0 || dy != 0) {
        copy.border[0] += dx;
        copy.border[1] += dy;
        copy.content[0] += dx;
        copy.content[1] += dy;
      }
      boxes[index + k] = copy;
      leading[index + k] = src.leading[b + k];
      trailing[index + k] = src.trailing[b + k];
      empty_flow[index + k] = src.empty_flow[b + k];
      self_collapse[index + k] = src.self_collapse[b + k];
      places[index + k] = src.places[b + k];
    }
    return true;
  };
  if (reuse->subtree_clean[index] &&
      attempt(*reuse->cache, reuse->before ? (*reuse->before)[index] : index))
    return true;
  return reuse->fallback_cache && (*reuse->fallback_clean)[index] &&
         attempt(*reuse->fallback_cache,
                 reuse->fallback_before ? (*reuse->fallback_before)[index] : index);
}
}  // namespace ruyijs::domain::styling
