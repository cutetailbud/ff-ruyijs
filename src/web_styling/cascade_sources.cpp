// Resolve original scope, use its Styling factory/cache and original cascade.
#include "web_styling/adopted.h"
#include "web_styling/condition_environment.h"
#include "web_styling/computed_source.h"
#include "domain/styling/stylesheet.h"
#include "js/Array.h"
#include "js/Realm.h"

namespace ruyijs::web_styling {
namespace {
void Dependencies(const std::vector<std::shared_ptr<ruyijs::domain::styling::CssomRuleValue>>& rows,
                  SelectorDependencies* out) {
  for (const auto& row : rows) {
    if (!row->selector.empty()) CollectSelectorDependencies(row->selector, out);
    Dependencies(row->children, out);
  }
}
bool HasMedia(const std::vector<std::shared_ptr<ruyijs::domain::styling::CssomRuleValue>>& rows) {
  for(const auto& row:rows) if(row->kind==4||HasMedia(row->children)) return true;
  return false;
}
bool CurrentRules(State& state,const SheetCache& cache,
                  ruyijs::domain::styling::CssMediaEnvironment* environment,bool* initialized,
                  std::vector<ruyijs::domain::styling::StyleSheetAggregate>* out) {
  if(cache.disabled||cache.rules.empty()) return true;
  if(!*initialized&&(!cache.media.text().empty()||HasMedia(cache.rules))) {
    if(!ReadConditionEnvironment(state,environment)) return false;
    *initialized=true;
  }
  if(!ruyijs::domain::styling::CssMediaCondition(cache.media.text(),*environment)) return true;
  out->emplace_back(cache.key?cache.key:1,cache.href);
  out->back().append_rule_values(cache.rules,*environment);
  return true;
}
// Visits the sheets of `element`'s cascade scope in cascade order: the
// document's sheet sources, then its adopted sheets. One walk serves both the
// cascade input and its fingerprint, so the two cannot disagree.
template <class Visit>
bool VisitCascadeSheets(State& caller, JS::HandleObject element, Visit&& visit) {
  if (!caller.cascadeScope || !caller.adoptedResolve)
    return WebKit::Fail(caller.cx, "CSS cascade scope port is not installed");
  JSContext* cx = caller.cx;
  JS::RootedObject scope(cx);
  bool shadow;
  if (!caller.cascadeScope(caller.adoptedContext, element, &scope, &shadow)) return false;
  JS::RootedValue scopeValue(cx, JS::ObjectValue(*scope)), array(cx);
  if (!caller.adoptedResolve(caller.adoptedContext, scopeValue, &array, shadow)) return false;
  JS::RootedObject proxy(cx, js::CheckedUnwrapStatic(&array.toObject())), rows(cx);
  State& state = AdoptedOwnerOf(proxy);
  JSAutoRealm realm(cx, state.page);
  scope.set(&js::GetProxyReservedSlot(proxy, AdoptedScope).toObject());
  std::vector<document_model::StyleSheetSource> sources;
  if (!ReadDocumentSheetSources(state, scope, &sources)) return false;
  for (auto& source : sources) {
    auto cache = state.Materialize(source);
    if (!cache || !visit(state, *cache)) return false;
  }
  uint32_t count;
  if (!AdoptedBacking(cx, proxy, &rows) || !JS::GetArrayLength(cx, rows, &count)) return false;
  for (uint32_t i = 0; i < count; ++i) {
    JS::RootedValue value(cx);
    JS::RootedObject sheet(cx);
    if (!JS_GetElement(cx, rows, i, &value) || !CssReceiver(cx, value, &kSheetClass, &sheet) ||
        !visit(state, SheetDataOf(sheet))) return false;
  }
  return true;
}
void Mix(uint64_t* hash, const void* data, size_t size) {
  const auto* bytes = static_cast<const unsigned char*>(data);
  for (size_t i = 0; i < size; ++i) { *hash ^= bytes[i]; *hash *= 1099511628211ull; }
}
void Mix(uint64_t* hash, const std::string& text) {
  const uint64_t size = text.size();
  Mix(hash, &size, sizeof size);
  Mix(hash, text.data(), text.size());
}
void MixRules(uint64_t* hash, const std::vector<std::shared_ptr<ruyijs::domain::styling::CssomRuleValue>>& rows) {
  const uint64_t count = rows.size();
  Mix(hash, &count, sizeof count);
  for (const auto& row : rows) {
    Mix(hash, &row->kind, sizeof row->kind);
    Mix(hash, row->selector); Mix(hash, row->DeclarationText()); Mix(hash, row->condition);
    Mix(hash, row->media.text());
    MixRules(hash, row->children);
  }
}
}  // namespace

// Building the aggregates parses every selector and declaration of the scope's
// sheets, and a layout read builds them once per read: a page that measures an
// element after each inline-style change (font probes) re-parsed every sheet
// each time. The parsed input is reused while its content fingerprint (sheet
// identity, href, flags, media, every rule's text, and the media environment
// when a sheet depends on it) is unchanged.
bool ReadCascadeSources(State& caller, JS::HandleObject element,
    std::shared_ptr<const std::vector<ruyijs::domain::styling::StyleSheetAggregate>>* out,
    std::shared_ptr<const SelectorDependencies>* dependencies,
    std::shared_ptr<ruyijs::domain::styling::RuleIndexStore>* indexes) {
  ruyijs::domain::styling::CssMediaEnvironment environment;
  bool environment_initialized = false, media = false;
  auto selectors = std::make_shared<SelectorDependencies>();
  State* owner = nullptr;
  uint64_t hash = 1469598103934665603ull;
  if (!VisitCascadeSheets(caller, element, [&](State& state, const SheetCache& cache) {
        if (!owner) owner = &state;
        const unsigned char flags = (cache.disabled ? 1 : 0) | (cache.rules.empty() ? 2 : 0);
        Mix(&hash, &flags, 1);
        Mix(&hash, &cache.key, sizeof cache.key);
        Mix(&hash, cache.href);
        Mix(&hash, cache.media.text());
        MixRules(&hash, cache.rules);
        if (!cache.disabled && !cache.rules.empty() && !media)
          media = !cache.media.text().empty() || HasMedia(cache.rules);
        if (!cache.disabled) Dependencies(cache.rules, selectors.get());
        return true;
      })) return false;
  if (media) {
    if (!ReadConditionEnvironment(*owner, &environment)) return false;
    environment_initialized = true;
    const double numbers[] = {environment.width, environment.height, environment.deviceWidth,
                              environment.deviceHeight, environment.ratio, environment.color};
    Mix(&hash, numbers, sizeof numbers);
    Mix(&hash, &environment.touch, sizeof environment.touch);
    for (const auto& [name, value] : environment.profile) { Mix(&hash, name); Mix(&hash, value); }
  }
  auto& memo = caller.cascadeInputs;
  for (const auto& entry : memo)
    if (entry.hash == hash) {
      *out = entry.aggregates; *dependencies = entry.dependencies; *indexes = entry.indexes;
      return true;
    }
  auto built = std::make_shared<std::vector<ruyijs::domain::styling::StyleSheetAggregate>>();
  if (!VisitCascadeSheets(caller, element, [&](State& state, const SheetCache& cache) {
        return CurrentRules(state, cache, &environment, &environment_initialized, built.get());
      })) return false;
  if (memo.size() >= 8) memo.erase(memo.begin());
  auto store = std::make_shared<ruyijs::domain::styling::RuleIndexStore>();
  memo.push_back({hash, built, selectors, store});
  *dependencies = selectors;
  *indexes = std::move(store);
  *out = std::move(built);
  return true;
}
// The fingerprint of everything the cascade reads from `element`'s sheets:
// order, disabled flags, sheet media and every rule's text. Media results also
// depend on the environment, which the layout cache keys separately.
bool CascadeStamp(State& caller, JS::HandleObject element, uint64_t* stamp) {
  return CascadeStamp(caller, element, stamp, false);
}
bool CascadeStamp(State& caller, JS::HandleObject element, uint64_t* stamp,
                  bool includeEnvironment) {
  uint64_t hash = 1469598103934665603ull;
  bool media = false;
  State* owner = nullptr;
  const bool ok = VisitCascadeSheets(caller, element, [&](State& state, const SheetCache& cache) {
    if (!owner) owner = &state;
    const unsigned char flags = (cache.disabled ? 1 : 0) | (cache.rules.empty() ? 2 : 0);
    Mix(&hash, &flags, 1);
    Mix(&hash, cache.media.text());
    MixRules(&hash, cache.rules);
    if (includeEnvironment && !cache.disabled && !cache.rules.empty() && !media)
      media = !cache.media.text().empty() || HasMedia(cache.rules);
    return true;
  });
  if (ok && includeEnvironment && media && owner) {
    ruyijs::domain::styling::CssMediaEnvironment environment;
    if (!ReadConditionEnvironment(*owner, &environment)) return false;
    const double numbers[] = {environment.width, environment.height, environment.deviceWidth,
                              environment.deviceHeight, environment.ratio, environment.color};
    Mix(&hash, numbers, sizeof numbers);
    Mix(&hash, &environment.touch, sizeof environment.touch);
    for (const auto& [name, value] : environment.profile) { Mix(&hash, name); Mix(&hash, value); }
  }
  *stamp = hash;
  return ok;
}
}
