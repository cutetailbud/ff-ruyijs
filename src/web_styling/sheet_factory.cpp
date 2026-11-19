#include "web_styling/cssom.h"
#include "js/Array.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <algorithm>
#include <new>

namespace ruyijs::web_styling {
namespace {
void FinalizeSheet(JS::GCContext*, JSObject* object) {
  const auto& value = JS::GetReservedSlot(object, SheetData);
  if (value.isUndefined()) return;
  auto* holder = static_cast<std::shared_ptr<SheetCache>*>(value.toPrivate());
  if ((*holder)->object == object) (*holder)->object = nullptr;
  delete holder;
}
void TraceSheet(JSTracer*, JSObject* object) {
  const auto& value = JS::GetReservedSlot(object, SheetData);
  if (value.isUndefined()) return;
  auto& cache = **static_cast<std::shared_ptr<SheetCache>*>(value.toPrivate());
  // Do not publish a partially built object or revive a retired identity.
  if (cache.object && !cache.retired) cache.object = object;
}
const JSClassOps sheetOps = {nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, FinalizeSheet, nullptr, nullptr, TraceSheet};
}
const JSClass kSheetClass = {"CSSStyleSheet",
    JSCLASS_HAS_RESERVED_SLOTS(10) | JSCLASS_FOREGROUND_FINALIZE, &sheetOps};

bool CssReceiver(JSContext* cx, JS::HandleValue value, const JSClass* clasp,
                  JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (!out || JS::GetClass(out) != clasp)
    return WebKit::Fail(cx, "CSSOM member called on an incompatible receiver");
  return true;
}

std::shared_ptr<SheetCache> State::Materialize(const document_model::StyleSheetSource& source) {
  for (auto it = sheets.begin(); it != sheets.end(); ++it) {
    if ((*it)->document != source.document || (*it)->key != source.key) continue;
    if (SameSheetSource(**it, source)) return *it;
    if (!RetireSheet(*this, *it)) return nullptr;
    sheets.erase(it);
    break;
  }
  auto cache = std::make_shared<SheetCache>(cx);
  cache->document = source.document;
  cache->key = source.key;
  cache->scopeDocument = source.scopeDocument;
  cache->scopeNode = source.scopeNode;
  cache->mainDocument = source.mainDocument;
  if (source.sharedText) cache->sourceText = source.sharedText;
  else cache->sourceText = std::make_shared<const std::string>(source.text);
  cache->text = *cache->sourceText;
  cache->href = source.href;
  cache->title = source.title;
  cache->sourceTitle = source.titleAttribute;
  cache->sourceMedia = source.media;
  cache->sourceType = source.typeAttribute;
  cache->external = source.external;cache->ready = !source.external;
  cache->loadDelivered = !source.external;
  cache->sourceRel = source.rel;cache->sourceCors = source.crossOrigin;
  cache->hasCors = source.hasCrossOrigin;cache->documentURL = source.documentURL;
  cache->origin = source.origin;cache->resourceOrigin = source.resourceOrigin;
  cache->media.set(source.media);
  cache->disabled = source.disabled ||
      sheetSets[{source.scopeDocument, source.scopeNode}].disabled(source.title);
  if (!cache->external) ParseSheet(*cache, source);
  else {
    cache->resource=externalResources.Find(cache->href,cache->documentURL,cache->origin,ExternalCorsMode(*cache));
    if (cache->resource) {
      cache->ready=true;cache->originClean=cache->resource->originClean;
      document_model::StyleSheetSource loaded;
      loaded.href=cache->href;loaded.text=cache->resource->text;
      ParseSheet(*cache,loaded);
    }
  }
  sheets.push_back(cache);
  return cache;
}
bool State::RetireSheets(const std::vector<document_model::StyleSheetSource>& sources,
                          const document_model::StyleSheetScope* scope) {
  for (auto it = sheets.begin(); it != sheets.end();) {
    if (scope ? (*it)->scopeDocument != scope->document || (*it)->scopeNode != scope->key
              : !(*it)->mainDocument) { ++it; continue; }
    bool active = std::any_of(sources.begin(), sources.end(), [&](const auto& source) {
      return SameSheetSource(**it, source);
    });
    if (active) { ++it; continue; }
    if (!RetireSheet(*this, *it)) return false;
    it = sheets.erase(it);
  }
  return true;
}
bool ReadSheetSources(State& state, std::vector<document_model::StyleSheetSource>* out) {
  if (state.sheetsPort && !state.sheetsPort(state.sheetsContext, out)) return false;
  return PrepareSheetSources(state, out);
}
JSObject* SheetObject(State& state, const std::shared_ptr<SheetCache>& cache,
                      JS::MutableHandleObject out, JS::HandleObject prototype) {
  if (cache->object) { out.set(cache->object); return out; }
  auto holder = std::unique_ptr<std::shared_ptr<SheetCache>>(
      new (std::nothrow) std::shared_ptr<SheetCache>(cache));
  if (!holder) { JS_ReportOutOfMemory(state.cx); return nullptr; }
  JS::RootedObject actualPrototype(state.cx, prototype ? prototype.get() : state.sheet.prototype.get());
  JS::RootedObject object(state.cx, JS_NewObjectWithGivenProto(state.cx, &kSheetClass, actualPrototype));
  if (!object) return nullptr;
  JS::SetReservedSlot(object, SheetData, JS::PrivateValue(holder.release()));
  JS::SetReservedSlot(object, SheetOwner, JS::PrivateValue(&state));
  JS::RootedValue node(state.cx, JS::NullValue());
  if (!cache->constructed && state.nodePort &&
      !state.nodePort(state.nodeContext, cache->document, cache->key, &node)) return nullptr;
  JS::SetReservedSlot(object, SheetOwnerNode, node);
  JS::SetReservedSlot(object, SheetConstructorDocument,
      cache->constructed ? JS::ObjectValue(*state.documentObject) : JS::NullValue());
  JS::RootedObject wrappers(state.cx, JS::NewArrayObject(state.cx, cache->rules.size()));
  if (!wrappers || !JS_SetPrototype(state.cx, wrappers, nullptr)) return nullptr;
  JS::SetReservedSlot(object, SheetWrappers, JS::ObjectValue(*wrappers));
  JS::RootedObject list(state.cx);
  if (!NewCssList(state, object, &list)) return nullptr;
  JS::SetReservedSlot(object, SheetList, JS::ObjectValue(*list));
  JS::RootedObject media(state.cx);
  if (!NewCssList(state, object, &media, MediaCollection)) return nullptr;
  JS::SetReservedSlot(object, SheetMedia, JS::ObjectValue(*media));
  // The original node's GC slot preserves DOM identity; no native strong root.
  if (node.isObject() && state.associationPort) {
    JS::RootedObject ownerNode(state.cx, &node.toObject());
    JS::RootedValue expected(state.cx), replacement(state.cx, JS::ObjectValue(*object));
    if (!state.associationPort(state.sourceContext, ownerNode, expected, replacement)) return nullptr;
  }
  cache->object = object;
  out.set(object);
  return out;
}
bool SheetAt(State& state, uint32_t index, JS::MutableHandleObject out, JS::HandleObject document) {
  std::vector<document_model::StyleSheetSource> sources;
  if (!(document ? ReadDocumentSheetSources(state, document, &sources) :
                   ReadSheetSources(state, &sources))) return false;
  if (index >= sources.size()) { out.set(nullptr); return true; }
  auto cache = state.Materialize(sources[index]);
  return cache && bool(SheetObject(state, cache, out));
}
}  // namespace ruyijs::web_styling
