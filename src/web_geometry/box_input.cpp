#include "web_geometry/box_input.h"
#include "domain/styling/css_numeric_value.h"
#include "web_geometry/font_config.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <array>
#include <cstdlib>
#include <map>
#include <set>
#include <memory>

namespace ruyijs::web_geometry {
void ApplyBoxAttributes(const std::vector<document_model::AttributeSnapshot>& attrs,
                        box::BoxNode* node) {
  node->input_type.clear();
  node->no_frame_border = false;
  node->intrinsic_width = node->intrinsic_height = -1;
  node->attribute_width_percent = node->attribute_height_percent = -1;
  for (const auto& attr : attrs) {
    if (attr.name == u"type") node->input_type.assign(attr.value.begin(), attr.value.end());
    if (attr.name == u"frameborder" && node->tag == "iframe" &&
        (attr.value == u"0" || attr.value == u"no" || attr.value == u"NO"))
      node->no_frame_border = true;
    if (attr.name != u"width" && attr.name != u"height") continue;
    const std::string value(attr.value.begin(), attr.value.end());
    const auto parsed = box::CssNumericParse(value);
    if (parsed.kind == box::CssNumber && parsed.number >= 0)
      (attr.name == u"width" ? node->intrinsic_width : node->intrinsic_height) = parsed.number;
    else if (!value.empty() && value.back() == '%') {
      char* end = nullptr;
      const double pct = std::strtod(value.c_str(), &end);
      if (end == value.c_str() + value.size() - 1 && pct >= 0)
        (attr.name == u"width" ? node->attribute_width_percent : node->attribute_height_percent) = pct;
    }
  }
}
bool BoxSameStyle(const box::BoxStyle& a, const box::BoxStyle& b) {
  return a.font_size==b.font_size && a.root_font_size==b.root_font_size &&
         a.specified_margin_mask==b.specified_margin_mask && a.specified_border_mask==b.specified_border_mask &&
         a.specified_padding_mask==b.specified_padding_mask &&
         (a.values==b.values || (a.values && b.values && *a.values==*b.values));
}
bool BoxSameLayout(const box::BoxStyle& a, const box::BoxStyle& b) {
  static constexpr const char* kExtraLayoutProps[] = {
      "-x-lang", "align-content", "align-items", "align-self", "column-gap", "flex-basis",
      "flex-direction", "flex-grow", "flex-shrink", "flex-wrap", "font-family", "font-style",
      "font-weight", "grid-auto-rows", "grid-column", "grid-column-end", "grid-column-start",
      "grid-row", "grid-row-end", "grid-row-start", "grid-template-columns", "grid-template-rows",
      "justify-content", "line-height", "order", "overflow", "row-gap", "transform-origin",
      "white-space",
  };
  if (a.font_size != b.font_size || a.root_font_size != b.root_font_size ||
      a.specified_margin_mask != b.specified_margin_mask ||
      a.specified_border_mask != b.specified_border_mask ||
      a.specified_padding_mask != b.specified_padding_mask)
    return false;
  if (a.values == b.values) return true;
  for (int k = 0; k < box::BoxKnown::kCount; ++k)
    if (a.Slot(k) != b.Slot(k)) return false;
  for (const char* name : kExtraLayoutProps)
    if (a.Get(name) != b.Get(name)) return false;
  return true;
}
bool BoxSameNode(const box::BoxNode& a, const box::BoxNode& b) {
  return a.text == b.text && a.breakable == b.breakable && a.no_frame_border == b.no_frame_border &&
         a.text_width == b.text_width && a.text_height == b.text_height &&
         a.line_height == b.line_height && a.intrinsic_width == b.intrinsic_width &&
         a.intrinsic_height == b.intrinsic_height &&
         a.attribute_width_percent == b.attribute_width_percent &&
         a.attribute_height_percent == b.attribute_height_percent &&
         a.tag == b.tag && a.input_type == b.input_type && BoxSameLayout(a.style, b.style);
}
namespace {
bool Read(State& s, JS::HandleObject object, document_model::LayoutNodeSource* node,
           JS::MutableHandleObject parent, JS::MutableHandleObjectVector children) {
  JS::RootedValue value(s.cx,JS::ObjectValue(*object));
  return s.boxPorts.node(s.boxPorts.context,value,node,parent,children);
}
// One layout pass. `previous` is the last layout of the same root (same
// cascade, viewport and focus). An element keeps its previous computed style
// when nothing its selectors read changed and what its parent passes to it is
// unchanged (inherited and custom properties, display, font sizes), as Firefox
// restyles only what a change can reach: a change to a parent's reset
// properties alone (padding, width, ...) restyles a child only when the child
// `inherit`s a reset property. An element is
// dirty when it is new, moved, or before/after a changed position among its
// siblings, or its attributes changed; dirtiness reaches its descendants and
// following siblings (descendant and sibling combinators). Selectors counted
// from the end make a changed child list dirty all children; :empty makes an
// element with a changed child list dirty. A change a :has() argument may read
// lays the tree out again without reuse (`restart`). `same` marks a computed
// style equal to the previous one, `inputs` one passing its children the same
// inputs as before; `reached` the previous nodes laid out again.
struct Pass {
  LayoutCache& cache;
  size_t found = size_t(-1);
  web_styling::ComputedStyleReader& styles;
  const LayoutCache* previous = nullptr;
  std::vector<size_t> before_index;
  std::vector<char> same, inputs, dirty, reached, same_node, structural_node;
  bool restart = false, structural = false;
};
std::set<std::string> Tokens(const std::u16string& value);
// The keys of one element against a set of class, id, tag and attribute keys.
bool KeyMatch(const std::string& tag, const LayoutAttributes& attributes, const std::set<std::string>& classes,
              const std::set<std::string>& ids, const std::set<std::string>& tags, const std::set<std::string>* names) {
  if (tags.count(tag)) return true;
  for (const auto& [name, value] : attributes) {
    if (names && names->count(std::string(name.begin(), name.end()))) return true;
    if (name == u"id" && ids.count(std::string(value.begin(), value.end()))) return true;
    if (name == u"class" && !classes.empty())
      for (const auto& token : Tokens(value)) if (classes.count(token)) return true;
  }
  return false;
}
bool HasKeyMatch(const web_styling::SelectorDependencies& d, const std::string& tag,
                 const LayoutAttributes& attributes) {
  return KeyMatch(tag, attributes, d.hasClasses, d.hasIds, d.hasTags, &d.hasAttributes) ||
         KeyMatch(tag, attributes, d.hasSiblingClasses, d.hasSiblingIds, d.hasSiblingTags, nullptr);
}
std::set<std::string> Tokens(const std::u16string& value) {
  std::set<std::string> out;
  std::string token;
  for (char16_t c : value) {
    if (c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\f') {
      if (!token.empty()) out.insert(token);
      token.clear();
    } else token.push_back(c < 0x80 ? char(c) : '?');
  }
  if (!token.empty()) out.insert(token);
  return out;
}
// Whether the change from `before` to `after` attributes can change a :has()
// argument's match (and so a subject anywhere above or before the element).
bool HasReach(const web_styling::SelectorDependencies& has, const LayoutAttributes& before,
              const std::vector<document_model::AttributeSnapshot>& after) {
  if (has.hasClasses.empty() && has.hasIds.empty() && has.hasAttributes.empty()) return false;
  std::map<std::u16string, std::u16string> old, now;
  for (const auto& [name, value] : before) old[name] = value;
  for (const auto& attr : after) now[attr.name] = attr.value;
  std::set<std::u16string> names;
  for (const auto& entry : old) names.insert(entry.first);
  for (const auto& entry : now) names.insert(entry.first);
  for (const auto& name : names) {
    const auto o = old.find(name), n = now.find(name);
    const std::u16string a = o == old.end() ? u"" : o->second, b = n == now.end() ? u"" : n->second;
    if ((o == old.end()) == (n == now.end()) && a == b) continue;
    const std::string ascii(name.begin(), name.end());
    if (has.hasAttributes.count(ascii)) return true;
    if (name == u"class") {
      const auto x = Tokens(a), y = Tokens(b);
      for (const auto& t : x) if (!y.count(t) && has.hasClasses.count(t)) return true;
      for (const auto& t : y) if (!x.count(t) && has.hasClasses.count(t)) return true;
    }
    if (name == u"id") {
      for (const auto* v : {&a, &b})
        if (has.hasIds.count(std::string(v->begin(), v->end()))) return true;
    }
  }
  return false;
}
uint64_t AttributeHash(const document_model::LayoutNodeSource& source) {
  uint64_t hash = 1469598103934665603ull;
  for (const auto& attr : source.attributes) {
    if (attr.name == u"style") continue;
    for (char16_t c : attr.name) { hash ^= c; hash *= 1099511628211ull; }
    hash ^= 0x3d; hash *= 1099511628211ull;
    for (char16_t c : attr.value) { hash ^= c; hash *= 1099511628211ull; }
    hash ^= 0x1f; hash *= 1099511628211ull;
  }
  return hash;
}
bool Visit(State& s, JS::HandleObject object, JS::HandleObject target, size_t parent_index,
            Pass& pass, unsigned depth, bool inherited_dirty, size_t parent_before, size_t position) {
  auto* nodes=&pass.cache.nodes;
  auto& meta=*pass.cache.meta;
  auto& styles=pass.styles;
  if(pass.restart) return true;  // this pass is discarded: lay out again without reuse
  if(depth>256||nodes->size()>=16384) return WebKit::Fail(s.cx,"Element geometry tree budget exceeded");
  document_model::LayoutNodeSource source;
  JS::RootedObject parent(s.cx);
  JS::RootedObjectVector children(s.cx);
  if(!Read(s,object,&source,&parent,&children)) return false;
  const size_t index=nodes->size();
  box::BoxNode node;
  node.tag.assign(source.tag.begin(),source.tag.end()); node.text=source.text; node.parent=parent_index;
  std::u16string inline_style;
  for(const auto& attr:source.attributes) if(attr.name==u"style") inline_style=attr.value;
  const uint64_t attributes=AttributeHash(source);
  const bool parent_same=parent_index==size_t(-1) || pass.same[parent_index];
  const bool parent_inputs=parent_index==size_t(-1) || pass.inputs[parent_index];
  const LayoutCache* previous=pass.previous;
  size_t before=size_t(-1);
  if(previous) {
    const auto hit=previous->meta->index.find({source.key.document,source.key.node});
    if(hit!=previous->meta->index.end() && previous->nodes[hit->second].tag==node.tag) before=hit->second;
  }
  LayoutAttributes list;
  list.reserve(source.attributes.size());
  for(const auto& attr:source.attributes) list.emplace_back(attr.name,attr.value);
  // Moved, or a sibling before it inserted or removed: its position changed.
  bool moved=false;
  if(before!=size_t(-1)) {
    const size_t old_parent=previous->nodes[before].parent;
    moved=old_parent!=parent_before ||
          (old_parent!=size_t(-1) && (position>=previous->nodes[old_parent].children.size() ||
                                      previous->nodes[old_parent].children[position]!=before));
    pass.reached[before]=1;
  }
  const bool node_structural=previous && (before==size_t(-1) || moved);
  if(node_structural) {
    pass.structural=true;
    if(source.element && styles.Scopes() && HasKeyMatch(styles.Dependencies(),node.tag,list)) pass.restart=true;
  }
  // Its own child list changed (:empty reads it).
  const bool children_changed=before!=size_t(-1) && previous->nodes[before].children.size()!=children.length();
  const bool known=styles.Scopes()!=0;
  const bool own_children=children_changed && (!known || styles.Dependencies().empty);
  bool same=false, inputs=false, inherits_reset=false;
  uint64_t child_inputs=0;
  const bool changed=before!=size_t(-1) && previous->meta->attributes[before]!=attributes;
  const bool restyled=before!=size_t(-1) && previous->inline_styles[before]!=inline_style;
  // With a selector on the style attribute, an inline-style change reaches
  // other elements like any attribute change (unknown before the first read).
  const bool style_reach=restyled && (!styles.Scopes() || styles.Dependencies().styleAttribute);
  const bool dirty=inherited_dirty || before==size_t(-1) || changed || style_reach || moved || own_children;
  if(source.element && styles.Scopes()) {
    const auto& dependencies=styles.Dependencies();
    if(changed && HasReach(dependencies,previous->meta->attribute_lists[before],source.attributes)) pass.restart=true;
    if(restyled && dependencies.hasAttributes.count("style")) pass.restart=true;
  }
  if(source.element) {
    // A child that inherits a reset property reads all of its parent's style.
    if(!dirty && styles.Scopes() && !restyled && parent_inputs &&
       (parent_same || !previous->inherits_reset[before])) {
      node.style=previous->nodes[before].style;
      child_inputs=previous->child_inputs[before]; inherits_reset=previous->inherits_reset[before];
      same=inputs=true;
    } else {
      web_styling::ComputedStyle style;
      if(!styles.Read(object,&style)) return false;
      // Layout reads no custom property; dropping them keeps BoxStyle::Get short.
      std::erase_if(style.values,[](const auto& value){return value.first.starts_with("--");});
      child_inputs=style.childInputs; inherits_reset=style.inheritsReset;
      node.style=box::BoxStyle::Build(std::move(style.values),style.fontSize,style.rootFontSize,
                  style.specifiedMarginMask,style.specifiedBorderMask,style.specifiedPaddingMask);
      // The custom properties and the inherited line-height value are not in
      // BoxStyle: equal styles also pass their children the same inputs.
      inputs=before!=size_t(-1) && previous->child_inputs[before]==child_inputs;
      same=inputs && BoxSameStyle(node.style,previous->nodes[before].style);
    }
  } else {
    if(parent_index!=size_t(-1)) node.style=(*nodes)[parent_index].style;
    same=parent_same; inputs=parent_inputs;
  }
  // A text box keeps its measurement when its text and everything BoxText
  // reads are unchanged: its element's inherited font properties, white-space,
  // line-height and root font size (what that element passes its children),
  // under the same font configuration.
  if(source.text) {
    if(before!=size_t(-1) && parent_inputs && previous->fonts==FontConfigGeneration() &&
       previous->meta->texts[before]==source.data) {
      const auto& measured=previous->nodes[before];
      node.text_width=measured.text_width; node.text_height=measured.text_height;
      node.line_height=measured.line_height; node.breakable=measured.breakable;
    } else if(!BoxText(s,source,&node)) return false;
  }
  ApplyBoxAttributes(source.attributes, &node);
  if(js::CheckedUnwrapStatic(object)==js::CheckedUnwrapStatic(target)) pass.found=index;
  const bool hidden=node.style.Slot(box::BoxKnown::Display)=="none";
  const bool node_same=before!=size_t(-1) && BoxSameNode(node,previous->nodes[before]);
  nodes->push_back(std::move(node));
  meta.keys.push_back(source.key);
  meta.is_element.push_back(source.element ? 1 : 0);
  meta.attributes.push_back(attributes);
  meta.attribute_lists.push_back(std::move(list));
  meta.texts.push_back(source.text ? std::move(source.data) : std::u16string());
  pass.cache.inline_styles.push_back(std::move(inline_style));
  pass.cache.child_inputs.push_back(child_inputs);
  pass.cache.inherits_reset.push_back(inherits_reset);
  pass.before_index.push_back(before);
  pass.same.push_back(same);
  pass.inputs.push_back(inputs);
  pass.dirty.push_back(dirty);
  pass.same_node.push_back(node_same ? 1 : 0);
  pass.structural_node.push_back(node_structural ? 1 : 0);
  if(parent_index!=size_t(-1)) (*nodes)[parent_index].children.push_back(index);
  if(hidden) {
    if(!children.empty()) meta.has_hidden_children=true;
    return true;
  }
  // A changed child list reaches every child through selectors counted from the end.
  const bool all_children=children_changed && (!styles.Scopes() || styles.Dependencies().fromEnd);
  if(children_changed) pass.structural=true;
  bool sibling_dirty=false;  // a preceding sibling's change reaches later siblings (+, ~)
  for(size_t i=0;i<children.length();++i) {
    JS::RootedObject child(s.cx,children[i]);
    const size_t child_index=nodes->size();
    if(!Visit(s,child,target,index,pass,depth+1,dirty||sibling_dirty||all_children,before,i)) return false;
    if(child_index<pass.dirty.size() && pass.dirty[child_index] &&
       (pass.structural_node[child_index] || !styles.Scopes() || styles.Dependencies().siblingCombinator))
      sibling_dirty=true;
  }
  return true;
}
bool Same(const document_model::NodeKey& a, const document_model::NodeKey& b) {
  return a.document==b.document&&a.node==b.node;
}
}
namespace {
bool Result(const State& s, const LayoutCache& cache, size_t found, box::BoxViewport viewport, bool content,
            std::array<double,4>* out, bool* present) {
  if(!cache.layout[found].present) return true;
  *present=true;
  // A scrolled container moves its descendants like the viewport moves all.
  const auto scrolled=AncestorScroll(s,cache,found);
  viewport.scroll_x+=scrolled[0]; viewport.scroll_y+=scrolled[1];
  *out=content?cache.layout[found].content:box::ProjectBoxRect(cache.nodes,cache.layout,viewport,found);
  return true;
}
}
void Realm::SetBoxPorts(const BoxPorts& ports) { state_->boxPorts=ports; state_->layoutCache.reset(); }
bool Realm::MeasureElement(JS::HandleObject element, std::array<double,4>* out, bool* present, bool content) {
  *out={}; *present=false;
  std::shared_ptr<const LayoutCache> layout; size_t found=size_t(-1); box::BoxViewport viewport;
  if(!LayoutFor(*state_,element,&layout,&found,&viewport)) return false;
  return !layout||found==size_t(-1)||Result(*state_,*layout,found,viewport,content,out,present);
}
bool LayoutFor(State& s, JS::HandleObject element, std::shared_ptr<const LayoutCache>* out, size_t* index,
               box::BoxViewport* viewport_out) {
  out->reset(); *index=size_t(-1);
  if(!s.boxPorts.node||!s.boxPorts.viewport||!s.boxPorts.styles) return WebKit::Fail(s.cx,"Element layout source ports are not installed");
  JSAutoRealm realm(s.cx,s.kit.page);
  JS::RootedObject target(s.cx,element);
  if(!JS_WrapObject(s.cx,&target)) return false;
  JS::RootedValue targetVal(s.cx,JS::ObjectValue(*target));
  JS::RootedObject root(s.cx,target);
  document_model::NodeKey targetKey, rootKey, focused;
  uint64_t revision=0, structure=0, tree=0;
  if(s.boxPorts.head) {
    document_model::LayoutHeadSource head;
    if(!s.boxPorts.head(s.boxPorts.context,targetVal,&head,&root)) return false;
    if(!head.renderedDocument) return true;
    if(head.ancestorBudgetExceeded) return WebKit::Fail(s.cx,"Element geometry ancestor budget exceeded");
    if(root && !JS_WrapObject(s.cx,&root)) return false;
    targetKey=head.key; rootKey=head.root; focused=head.focused;
    revision=head.revision; structure=head.structure; tree=head.tree;
  } else {
    JS::RootedObject parent(s.cx);
    JS::RootedObjectVector children(s.cx);
    document_model::LayoutNodeSource source;
    if(!Read(s,target,&source,&parent,&children)) return false;
    if(!source.renderedDocument) return true;
    targetKey=source.key; focused=source.focused;
    revision=source.revision; structure=source.structure; tree=source.tree;
    for(unsigned depth=0;parent;++depth) {
      if(depth>256) return WebKit::Fail(s.cx,"Element geometry ancestor budget exceeded");
      root=parent;
      if(!Read(s,root,&source,&parent,&children)) return false;
    }
    rootKey=source.key;
  }
  box::BoxViewport& viewport=*viewport_out;
  if(!s.boxPorts.viewport(s.boxPorts.context,&viewport.width,&viewport.height,&viewport.scroll_x,&viewport.scroll_y)) return false;
  const std::array<double,4> view{viewport.width,viewport.height,viewport.scroll_x,viewport.scroll_y};
  uint64_t stamp=0;
  bool stamped=s.boxPorts.stamp&&s.boxPorts.stamp(s.boxPorts.context,target,&stamp);
  if(!stamped) JS_ClearPendingException(s.cx);  // Uncached: the full path below reports real errors.
  const auto& cached=s.layoutCache;
  if(stamped&&cached&&cached->styles==stamp&&cached->viewport==view&&
     Same(cached->root,rootKey)&&Same(cached->focused,focused)) {
    if(cached->revision==revision) {
      const auto hit=cached->meta->index.find({targetKey.document,targetKey.node});
      if(hit!=cached->meta->index.end()) { *out=cached; *index=hit->second; }
      return true;
    }
    if(cached->tree==tree) {
      bool handled=false;
      if(!TryTreeUnchangedLayout(s,targetVal,targetKey,*cached,revision,structure,viewport,out,index,&handled))
        return false;
      if(handled) return true;
    }
  }
  auto cache=std::make_shared<LayoutCache>();
  auto styles=s.boxPorts.styles(s.boxPorts.context);
  if(!styles) return false;
  Pass pass{*cache,size_t(-1),*styles};
  if(stamped&&cached&&cached->styles==stamp&&cached->viewport==view&&
     Same(cached->root,rootKey)&&Same(cached->focused,focused)) {
    pass.previous=cached.get();
    pass.reached.assign(cached->nodes.size(),0);
  }
  if(!Visit(s,root,target,size_t(-1),pass,0,false,size_t(-1),0)) return false;
  if(pass.previous && styles->Scopes()) {
    const auto& dependencies=styles->Dependencies();
    if(dependencies.HasKeys()) {
      if(pass.structural && dependencies.hasStructural) pass.restart=true;
      // Mutations inside an unvisited display:none subtree are absent from
      // previous->meta->keys; re-walk cleanly when :has() may observe them.
      if(pass.previous->meta->has_hidden_children &&
         (pass.previous->structure!=structure || dependencies.hasAttributes.count("style")))
        pass.restart=true;
      // A removed (or no longer laid out) element a :has() argument may read.
      const auto& old=*pass.previous;
      for(size_t i=0;i<old.nodes.size() && !pass.restart;++i)
        if(!pass.reached[i] && !old.nodes[i].text && HasKeyMatch(dependencies,old.nodes[i].tag,old.meta->attribute_lists[i]))
          pass.restart=true;
    }
  }
  if(pass.restart) {  // a :has() argument may have changed: lay out without reuse
    *cache=LayoutCache();
    pass.previous=nullptr; pass.restart=false; pass.found=size_t(-1);
    pass.before_index.clear();
    pass.same.clear(); pass.inputs.clear(); pass.dirty.clear(); pass.reached.clear();
    pass.same_node.clear(); pass.structural_node.clear();
    if(!Visit(s,root,target,size_t(-1),pass,0,false,size_t(-1),0)) return false;
  }
  const size_t found=pass.found;
  box::BoxReuseInput reuse;
  const box::BoxReuseInput* reuse_ptr=nullptr;
  if(pass.previous) {
    reuse={&pass.previous->nodes,&pass.previous->box_cache,&pass.before_index,&pass.same_node};
    reuse_ptr=&reuse;
  }
  if(!box::ComputeBoxLayout(cache->nodes,viewport,&cache->layout,&cache->scrollport,reuse_ptr,&cache->box_cache))
    return WebKit::Fail(s.cx,"Element box layout budget exceeded");
  // One cascade scope only: a shadow scope's sheets are outside the fingerprint.
  if(stamped&&styles->Scopes()<=1) {
    cache->root=rootKey; cache->focused=focused; cache->revision=revision; cache->styles=stamp;
    cache->fonts=FontConfigGeneration();
    cache->structure=structure; cache->tree=tree;
    cache->viewport=view;
    const bool unknown=styles->Scopes()==0;
    const auto& d=styles->Dependencies();
    cache->has_style_dep=unknown || d.styleAttribute || d.hasAttributes.count("style");
    cache->has_key_dep=unknown || d.HasKeys();
    cache->has_sibling_dep=unknown || d.siblingCombinator;
    const auto& keys=cache->meta->keys;
    cache->meta->index.reserve(keys.size());
    for(size_t i=0;i<keys.size();++i) cache->meta->index.emplace(std::make_pair(keys[i].document,keys[i].node),i);
    s.layoutCache=cache;
  }
  *out=cache; *index=found;
  return true;
}
bool Realm::MeasureOffset(JS::HandleObject element, document_model::OffsetGeometry* out) {
  *out={};
  std::shared_ptr<const LayoutCache> layout; size_t found=size_t(-1); box::BoxViewport viewport;
  if(!LayoutFor(*state_,element,&layout,&found,&viewport)) return false;
  if(layout&&found!=size_t(-1)) OffsetFrom(*layout,found,out);
  return true;
}
bool ElementProjection(State& s, JS::HandleValue value, bool list, JS::MutableHandleValue result) {
  if(!s.boxPorts.node) return WebKit::Fail(s.cx,"Element layout source port is not installed");
  void* layout_owner=nullptr;
  void* geom_factory=nullptr;
  if(s.boxPorts.head) {
    document_model::LayoutHeadSource head;
    JS::RootedObject root(s.cx);
    if(!s.boxPorts.head(s.boxPorts.context,value,&head,&root)) return false;
    layout_owner=head.layoutOwner;
    geom_factory=head.factory;
  } else {
    document_model::LayoutNodeSource node;
    JS::RootedObject parent(s.cx);
    JS::RootedObjectVector children(s.cx);
    if(!s.boxPorts.node(s.boxPorts.context,value,&node,&parent,&children)) return false;
    layout_owner=node.layoutOwner;
    geom_factory=node.factory;
  }
  auto* owner=static_cast<Realm*>(layout_owner);
  auto* factory=static_cast<Realm*>(geom_factory);
  if(!owner||!factory) return WebKit::Fail(s.cx,"Element original geometry factory is not installed");
  JS::RootedObject target(s.cx,&value.toObject());
  std::array<double,4> rect{}; bool present=false;
  if(!owner->MeasureElement(target,&rect,&present)) return false;
  return factory->NewElementRects(rect,present,list,result);
}
}
