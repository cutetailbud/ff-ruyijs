// Metadata and retirement over the existing source port and sheet identity.
#include "web_styling/cssom.h"
#include "js/Realm.h"
#include <algorithm>

namespace ruyijs::web_styling {
bool SameSheetSource(const SheetCache& cache, const document_model::StyleSheetSource& source) {
  const bool sameText = cache.external ||
      (cache.sourceText && source.sharedText && cache.sourceText.get() == source.sharedText.get()) ||
      cache.text == source.Text();
  return cache.document == source.document && cache.key == source.key &&
      cache.scopeDocument == source.scopeDocument && cache.scopeNode == source.scopeNode &&
      cache.mainDocument == source.mainDocument &&
      cache.external == source.external && sameText &&
      cache.href == source.href && cache.sourceRel == source.rel &&
      cache.sourceCors == source.crossOrigin && cache.hasCors == source.hasCrossOrigin &&
      cache.sourceTitle == source.titleAttribute &&
      cache.sourceMedia == source.media && cache.sourceType == source.typeAttribute;
}
bool RetireSheet(State& state, const std::shared_ptr<SheetCache>& cache) {
  JS::RootedObject sheet(state.cx, cache->object);
  if (sheet) {
    CancelExternalSheet(sheet);
    JS::RootedValue node(state.cx, JS::GetReservedSlot(sheet, SheetOwnerNode));
    if (node.isObject() && state.associationPort) {
      JS::RootedObject ownerNode(state.cx, &node.toObject());
      JS::RootedValue expected(state.cx, JS::ObjectValue(*sheet)), replacement(state.cx, JS::NullValue());
      if (!state.associationPort(state.sourceContext, ownerNode, expected, replacement)) return false;
    }
    JS::SetReservedSlot(sheet, SheetOwnerNode, JS::NullValue());
  }
  cache->retired = true;
  cache->object = nullptr;
  return true;
}
bool RefreshSheetSource(JSContext* cx, JS::HandleObject sheet) {
  const auto& cache = SheetDataOf(sheet);
  if (cache.constructed || cache.retired) return true;
  auto& state = SheetOwnerOf(sheet);
  JSAutoRealm realm(cx, state.page);
  std::vector<document_model::StyleSheetSource> sources;
  if (cache.mainDocument) return ReadSheetSources(state, &sources);
  if (state.sourcePort &&
      !state.sourcePort(state.sourceContext, cache.document, cache.key, &sources)) return false;
  if (std::any_of(sources.begin(), sources.end(), [&](const auto& source) {
        return SameSheetSource(cache, source);
      })) return true;
  for (auto it = state.sheets.begin(); it != state.sheets.end(); ++it) {
    if (it->get() != &cache) continue;
    if (!RetireSheet(state, *it)) return false;
    state.sheets.erase(it);
    break;
  }
  return true;
}
}
