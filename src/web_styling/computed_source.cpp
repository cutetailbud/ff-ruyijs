#include <unordered_map>
#include "domain/styling/stylesheet.h"
#include "domain/styling/css_color_value.h"
#include "web_styling/internal.h"
#include "web_styling/adopted.h"
#include "web_styling/selector_input.h"
#include "web_styling/computed_source.h"
#include "web_styling/realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_styling {
struct ComputedStyleReader::Impl {
  struct Scope {
    JS::PersistentRootedObject scope;
    std::shared_ptr<const std::vector<ruyijs::domain::styling::StyleSheetAggregate>> aggregates;
    ruyijs::domain::styling::StyleResolutionContext computation;
    std::unordered_map<const ruyijs::domain::styling::StyleDeclaration*, ComputedStyle> projections;
    Scope(JSContext* cx, JS::HandleObject object) : scope(cx, object) {}
  };
  State& state;
  std::vector<std::unique_ptr<Scope>> sources;
  // Relation frames read once per session, shared by every element it reads.
  std::unique_ptr<SelectorInput> input;
  SelectorDependencies dependencies;
  explicit Impl(State& owner):state(owner) {}
};
namespace {
namespace css = ruyijs::domain::styling;
// Colour-valued properties serialize their computed colour as Firefox does.
bool ColorProperty(const std::string& name) {
  static const char suffix[] = "color";
  if (name.size() < 5 || name.compare(name.size() - 5, 5, suffix) != 0) return false;
  return name != "color-scheme" && name.rfind("color-", 0) != 0 && name != "print-color-adjust";
}
// The serialized custom-property suffix of a computed style. The map is shared
// (copy on write) by every element that inherits it, so the text is memoized by
// the map's identity and reused across elements of one getComputedStyle sweep.
std::string SerializeCustoms(const css::StyleDeclaration& computed) {
  const void* identity = computed.customs_identity();
  if (!identity) return {};
  static thread_local const void* cachedIdentity = nullptr;
  static thread_local std::string cachedText;
  if (identity != cachedIdentity) {
    std::string text;
    for (const auto& entry : computed.customs()) text += entry.first + ": " + entry.second.value + ";";
    cachedIdentity = identity;
    cachedText = std::move(text);
  }
  return cachedText;
}
void ComputeColors(ComputedStyle* style) {
  std::string current;
  for (const auto& [name, value] : style->values)
    if (name == "color") current = ruyijs::domain::styling::CssComputedColor(value, {});
  for (auto& [name, value] : style->values) {
    if (!ColorProperty(name)) continue;
    std::string computed = ruyijs::domain::styling::CssComputedColor(value, current);
    if (!computed.empty()) value = std::move(computed);
  }
}
}  // namespace
ComputedStyleReader::ComputedStyleReader(State& state):impl_(std::make_unique<Impl>(state)) {}
ComputedStyleReader::~ComputedStyleReader() = default;
bool ComputedStyleReader::Read(JS::HandleObject element, ComputedStyle* out) {
  namespace css = ruyijs::domain::styling;
  State& state=impl_->state;
  if (!state.chainPort || !state.sheetsPort)
    return WebKit::Fail(state.cx, "getComputedStyle is not available");
  bool isElement = false;
  // The relation reader below supplies the cascade's ancestors on demand.
  // Validate here without copying an unused chain for every layout element.
  if (!state.chainPort(state.chainContext, element, nullptr, &isElement)) return false;
  if (!isElement) return WebKit::Fail(state.cx, "getComputedStyle requires an Element");
  if (!impl_->input) impl_->input = std::make_unique<SelectorInput>(state, element);
  else impl_->input->Target(element);
  SelectorInput& input = *impl_->input;
  struct ClearGuard { SelectorInput& in; ~ClearGuard() { in.ClearTarget(); } } clearGuard{input};
  const auto* node = input.Load();
  if (!node) return false;
  JS::RootedObject scope(state.cx);
  bool shadow=false;
  if (!state.cascadeScope(state.adoptedContext,element,&scope,&shadow)) return false;
  scope=js::CheckedUnwrapStatic(scope);
  size_t index=0;
  for(;index<impl_->sources.size();++index) if(impl_->sources[index]->scope==scope) break;
  if(index==impl_->sources.size()) {
    auto entry=std::make_unique<Impl::Scope>(state.cx, scope);
    std::shared_ptr<const SelectorDependencies> dependencies;
    if (!ReadCascadeSources(state,element,&entry->aggregates,&dependencies,&entry->computation.store))
      return false;
    if (dependencies) impl_->dependencies.Merge(*dependencies);
    impl_->sources.push_back(std::move(entry));
  }
  auto& source=*impl_->sources[index];
  std::vector<const css::StyleSheetAggregate*> pointers;
  pointers.reserve(source.aggregates->size());
  for (const auto& aggregate : *source.aggregates) pointers.push_back(&aggregate);
  const auto& computed = css::StyleResolutionService::compute_style_ref(pointers, *node, source.computation);
  if (!input.ok()) return false;
  if (node->key) {
    const auto cached = source.projections.find(&computed);
    if (cached != source.projections.end()) {
      *out = cached->second;
      return true;
    }
  }
  ComputedStyle result;
  result.fontSize = computed.computed_font_size;
  result.rootFontSize = computed.root_font_size;
  result.specifiedMarginMask = computed.specified_margin_mask;
  result.specifiedBorderMask = computed.specified_border_mask;
  result.specifiedPaddingMask = computed.specified_padding_mask;
  const auto& initials = css::StyleResolutionService::initial_values();
  result.values.reserve(computed.all().size() + initials.size());
  for (const auto& entry : computed.all())
    result.values.emplace_back(entry.first, entry.first == "line-height"
        ? css::CssResolvedLineHeight(computed) : entry.second.value);
  for (const auto& entry : initials)
    if (!computed.all().count(entry.first)) result.values.emplace_back(entry.first, entry.second);
  result.customsText = SerializeCustoms(computed);
  result.childInputs = css::StyleResolutionService::child_inputs(computed);
  result.inheritsReset = computed.inherits_reset;
  ComputeColors(&result);
  if (node->key) source.projections.emplace(&computed, result);
  *out = std::move(result);
  return true;
}
bool ComputedStyleReader::ReadDefault(JS::HandleObject element, ComputedStyle* out) {
  namespace css = ruyijs::domain::styling;
  State& state=impl_->state;
  if (!state.chainPort) return WebKit::Fail(state.cx, "getDefaultComputedStyle is not available");
  bool isElement = false;
  if (!state.chainPort(state.chainContext, element, nullptr, &isElement)) return false;
  if (!isElement) return WebKit::Fail(state.cx, "getDefaultComputedStyle requires an Element");
  if (!impl_->input) impl_->input = std::make_unique<SelectorInput>(state, element);
  else impl_->input->Target(element);
  struct ClearGuard { SelectorInput& in; ~ClearGuard() { in.ClearTarget(); } } clearGuard{*impl_->input};
  const auto* node = impl_->input->Load();
  if (!node) return false;
  // A detached copy of the ancestor chain without inline declarations; keys are
  // cleared so the shared cascade cache never mixes it with author results.
  std::vector<std::unique_ptr<css::ElementNode>> chain;
  for (const css::ElementNode* cursor = node; cursor && chain.size() < 1024; cursor = cursor->related('>')) {
    auto copy = std::make_unique<css::ElementNode>(*cursor);
    copy->inline_style.clear();
    copy->key = 0;
    copy->parent = nullptr;
    copy->relations = nullptr;  // the copy is self-contained: parents only
    copy->ancestor_bloom_ready = false;
    chain.push_back(std::move(copy));
    if (!impl_->input->ok()) return false;
  }
  for (size_t i = 0; i + 1 < chain.size(); ++i) chain[i]->parent = chain[i + 1].get();
  if (!impl_->input->ok()) return false;
  const std::vector<const css::StyleSheetAggregate*> none;
  const auto computed = css::StyleResolutionService::compute_style(none, *chain.front());
  ComputedStyle result;
  result.fontSize = computed.computed_font_size;
  result.rootFontSize = computed.root_font_size;
  for (const auto& entry : computed.all())
    result.values.emplace_back(entry.first, entry.first == "line-height"
        ? css::CssResolvedLineHeight(computed) : entry.second.value);
  for (const auto& entry : css::StyleResolutionService::initial_values())
    if (!computed.all().count(entry.first)) result.values.emplace_back(entry.first, entry.second);
  result.customsText = SerializeCustoms(computed);
  ComputeColors(&result);
  *out = std::move(result);
  return true;
}
bool ReadComputed(State& state, JS::HandleObject element, ComputedStyle* out) {
  ComputedStyleReader reader(state);
  return reader.Read(element,out);
}
bool Realm::ReadComputed(JS::HandleObject element, ComputedStyle* out) {
  return web_styling::ReadComputed(*state_, element, out);
}
size_t ComputedStyleReader::Scopes() const { return impl_->sources.size(); }
const SelectorDependencies& ComputedStyleReader::Dependencies() const { return impl_->dependencies; }
bool Realm::CascadeStamp(JS::HandleObject element, uint64_t* stamp) {
  return web_styling::CascadeStamp(*state_, element, stamp);
}
std::unique_ptr<ComputedStyleReader> Realm::BeginComputedRead() {
  return std::make_unique<ComputedStyleReader>(*state_);
}
}
