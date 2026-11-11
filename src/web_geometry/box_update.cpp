// Fast incremental layout update when Model::TreeRevision is unchanged
// (inline-style writes and non-style attribute writes on existing elements).
#include "web_geometry/box_input.h"
#include "web_geometry/font_config.h"
#include <unordered_map>

namespace ruyijs::web_geometry {
namespace {
box::BoxNode CloneWithoutChildren(const box::BoxNode& src) {
  box::BoxNode out;
  out.style = src.style; out.tag = src.tag; out.input_type = src.input_type;
  out.parent = src.parent; out.text = src.text; out.breakable = src.breakable;
  out.text_width = src.text_width; out.text_height = src.text_height;
  out.line_height = src.line_height; out.intrinsic_width = src.intrinsic_width;
  out.intrinsic_height = src.intrinsic_height;
  out.attribute_width_percent = src.attribute_width_percent;
  out.attribute_height_percent = src.attribute_height_percent;
  out.no_frame_border = src.no_frame_border;
  return out;
}
void AssignWithoutChildren(box::BoxNode&& src, box::BoxNode* dst) {
  auto kept = std::move(dst->children);
  *dst = std::move(src);
  dst->children = std::move(kept);
}
}  // namespace

bool TryTreeUnchangedLayout(State& s, JS::HandleValue target_value,
                            const document_model::NodeKey& target_key, const LayoutCache& previous,
                            uint64_t revision, uint64_t structure, const box::BoxViewport& viewport,
                            std::shared_ptr<const LayoutCache>* out, size_t* found_out, bool* handled) {
  *handled = false;
  const bool style_only = previous.structure == structure;
  if (!s.boxPorts.changedElements || !s.boxPorts.wrap) return true;
  if ((style_only && previous.has_style_dep) || (!style_only && previous.has_key_dep)) return true;
  std::vector<document_model::ChangedElement> changed;
  if (!s.boxPorts.changedElements(s.boxPorts.context, target_value, previous.meta->keys,
                                  previous.meta->is_element, previous.meta->attributes,
                                  previous.inline_styles, style_only, &changed))
    return false;
  if (previous.has_style_dep)
    for (const auto& entry : changed) if (entry.style_changed) return true;

  const uint64_t fonts = FontConfigGeneration();
  const auto hit = previous.meta->index.find({target_key.document, target_key.node});
  const size_t found = hit == previous.meta->index.end() ? size_t(-1) : hit->second;
  const auto writable = [&]() -> std::shared_ptr<LayoutCache> {
    return s.layoutCache.use_count() == 1 ? std::const_pointer_cast<LayoutCache>(s.layoutCache)
                                          : std::make_shared<LayoutCache>(previous);
  };
  if (changed.empty() && previous.fonts == fonts) {
    auto cache = writable();
    cache->revision = revision; cache->structure = structure;
    s.layoutCache = cache; *out = cache; *found_out = found; *handled = true;
    return true;
  }

  auto styles = s.boxPorts.styles(s.boxPorts.context);
  if (!styles) return false;
  const size_t n = previous.nodes.size();
  std::vector<char> restyled(n, 0), dirty(n, 0), same(n, 0), inputs(n, 0), same_node(n, 0);
  std::vector<size_t> attr_entry(n, 0);
  for (size_t k = 0; k < changed.size(); ++k) {
    const size_t idx = changed[k].index;
    if (changed[k].style_changed) restyled[idx] = 1;
    if (!changed[k].attributes_changed) continue;
    attr_entry[idx] = k + 1;
    dirty[idx] = 1;
    const size_t p = previous.nodes[idx].parent;
    if (!previous.has_sibling_dep || p == size_t(-1)) continue;
    bool after = false;
    for (const size_t sib : previous.nodes[p].children) {
      if (after) dirty[sib] = 1;
      else if (sib == idx) after = true;
    }
  }

  struct Patch { size_t index; box::BoxNode node; uint64_t child_inputs; char inherits_reset; bool element; };
  std::vector<Patch> patches;
  std::unordered_map<size_t, size_t> patch_map;
  bool all_same = true;
  JS::RootedObject obj(s.cx);
  for (size_t i = 0; i < n; ++i) {
    const size_t p = previous.nodes[i].parent;
    if (p != size_t(-1) && dirty[p]) dirty[i] = 1;
    const bool p_same = p == size_t(-1) || same[p], p_inputs = p == size_t(-1) || inputs[p];
    if (previous.meta->is_element[i]) {
      if (!restyled[i] && !dirty[i] && p_inputs && (p_same || !previous.inherits_reset[i])) {
        same[i] = inputs[i] = same_node[i] = 1;
        continue;
      }
      obj.set(nullptr);
      if (!s.boxPorts.wrap(s.boxPorts.context, target_value, previous.meta->keys[i], &obj)) return false;
      if (!obj || !JS_WrapObject(s.cx, &obj)) { JS_ClearPendingException(s.cx); return true; }
      web_styling::ComputedStyle style;
      if (!styles->Read(obj, &style)) return false;
      if (styles->Scopes() > 1 || (!style_only && styles->Dependencies().HasKeys())) return true;
      std::erase_if(style.values, [](const auto& v) { return v.first.starts_with("--"); });
      auto next = box::BoxStyle::Build(std::move(style.values), style.fontSize, style.rootFontSize,
                                       style.specifiedMarginMask, style.specifiedBorderMask,
                                       style.specifiedPaddingMask);
      if ((next.Slot(box::BoxKnown::Display) == "none") !=
          (previous.nodes[i].style.Slot(box::BoxKnown::Display) == "none"))
        return true;
      auto patched = CloneWithoutChildren(previous.nodes[i]);
      patched.style = std::move(next);
      if (attr_entry[i]) ApplyBoxAttributes(changed[attr_entry[i] - 1].attribute_list, &patched);
      inputs[i] = previous.child_inputs[i] == style.childInputs;
      same[i] = inputs[i] && BoxSameStyle(patched.style, previous.nodes[i].style);
      same_node[i] = BoxSameNode(patched, previous.nodes[i]);
      if (!same_node[i]) all_same = false;
      patch_map[i] = patches.size();
      patches.push_back({i, std::move(patched), style.childInputs, style.inheritsReset ? char(1) : char(0), true});
    } else if (p_same && p_inputs && previous.fonts == fonts) {
      same[i] = inputs[i] = same_node[i] = 1;
    } else {
      auto patched = CloneWithoutChildren(previous.nodes[i]);
      if (p != size_t(-1)) {
        const auto pm = patch_map.find(p);
        patched.style = pm == patch_map.end() ? previous.nodes[p].style : patches[pm->second].node.style;
      }
      if (!p_inputs || previous.fonts != fonts) {
        document_model::LayoutNodeSource src; src.text = true; src.data = previous.meta->texts[i];
        patched.text_width = patched.text_height = 0; patched.line_height = -1; patched.breakable = false;
        if (!BoxText(s, src, &patched)) return false;
      }
      inputs[i] = p_inputs; same[i] = p_same;
      same_node[i] = BoxSameNode(patched, previous.nodes[i]);
      if (!same_node[i]) all_same = false;
      patches.push_back({i, std::move(patched), 0, 0, false});
    }
  }

  auto cache = writable();
  cache->revision = revision; cache->structure = structure; cache->fonts = fonts;
  if (!style_only && cache->meta.use_count() > 1) cache->meta = std::make_shared<LayoutTreeMeta>(*cache->meta);
  for (auto& entry : changed) {
    if (entry.style_changed) cache->inline_styles[entry.index] = std::move(entry.inline_style);
    if (!entry.attributes_changed) continue;
    cache->meta->attributes[entry.index] = entry.attributes;
    auto& list = cache->meta->attribute_lists[entry.index];
    list.clear(); list.reserve(entry.attribute_list.size());
    for (const auto& a : entry.attribute_list) list.emplace_back(a.name, a.value);
  }
  for (auto& patch : patches) {
    if (patch.element) {
      cache->child_inputs[patch.index] = patch.child_inputs;
      cache->inherits_reset[patch.index] = patch.inherits_reset;
    }
    AssignWithoutChildren(std::move(patch.node), &cache->nodes[patch.index]);
  }
  if (!all_same) {
    std::vector<box::BoxLayout> next_layout;
    box::BoxLayoutCache next_boxes;
    box::BoxReuseInput reuse{nullptr, &cache->box_cache, nullptr, &same_node};
    if (!box::ComputeBoxLayout(cache->nodes, viewport, &next_layout, &cache->scrollport, &reuse, &next_boxes))
      return WebKit::Fail(s.cx, "Element box layout budget exceeded");
    cache->layout = std::move(next_layout);
    cache->box_cache = std::move(next_boxes);
  }
  s.layoutCache = cache; *out = cache; *found_out = found; *handled = true;
  return true;
}
}  // namespace ruyijs::web_geometry
