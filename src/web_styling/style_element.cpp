// Internal projection used by the unique HTMLStyleElement installer.
#include "web_styling/cssom.h"
#include "web_styling/realm.h"
#include "js/Realm.h"
#include <algorithm>

namespace ruyijs::web_styling {
bool StyleElementAction(State& state, uint64_t document, uint64_t key,
                         document_model::StyleElementAction action, bool disabled,
                         JS::MutableHandleValue out) {
  using Action = document_model::StyleElementAction;
  if (action == Action::Forget) {  // Not a stylesheet link: drop a sheet it may still hold.
    for (auto it = state.sheets.begin(); it != state.sheets.end(); ++it) {
      if ((*it)->document != document || (*it)->key != key) continue;
      if (!RetireSheet(state, *it)) return false;
      state.sheets.erase(it);
      break;
    }
    out.setUndefined();
    return true;
  }
  std::vector<document_model::StyleSheetSource> sources;
  if (state.sourcePort &&
      !state.sourcePort(state.sourceContext, document, key, &sources)) return false;
  if (!PrepareSheetSources(state, &sources, nullptr, false, false)) return false;
  auto source = std::find_if(sources.begin(), sources.end(), [&](const auto& value) {
    return value.document == document && value.key == key;
  });
  if (source == sources.end()) {
    for (auto it = state.sheets.begin(); it != state.sheets.end(); ++it) {
      if ((*it)->document != document || (*it)->key != key) continue;
      if (!RetireSheet(state, *it)) return false;
      state.sheets.erase(it);
      break;
    }
    if (action == Action::Sheet) out.setNull();
    else if (action == Action::ReadDisabled) out.setBoolean(false);
    else out.setUndefined();
    return true;
  }
  if (source->mainDocument && !state.RetireSheets(sources)) return false;
  auto cache = state.Materialize(*source);
  if (!cache) return false;
  if (!cache->ready) { out.setNull(); return true; }
  // Materialize the shared object also for disabled, preserving its association.
  JS::RootedObject sheet(state.cx);
  if (!SheetObject(state, cache, &sheet)) return false;
  if (action == Action::Sheet) out.setObject(*sheet);
  else if (action == Action::ReadDisabled) out.setBoolean(cache->disabled);
  else { cache->disabled = disabled; ++state.cssomGeneration; out.setUndefined(); }
  return true;
}

bool Realm::StyleElement(uint64_t document, uint64_t key,
                          document_model::StyleElementAction action, bool disabled,
                          JS::MutableHandleValue out) {
  State& state = *state_;
  JS::RootedValue result(state.cx);
  {
    JSAutoRealm realm(state.cx, state.page);
    if (!StyleElementAction(state, document, key, action, disabled, &result)) return false;
  }
  if (!JS_WrapValue(state.cx, &result)) return false;
  out.set(result);
  return true;
}
}
