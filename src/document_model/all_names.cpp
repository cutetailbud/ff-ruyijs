#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Object.h"
#include "js/String.h"

namespace ruyijs::document_model {
bool AllNameValue(State& state, CollectionRecord& record, DomStringView name,
                   JS::MutableHandleValue result) {
  std::vector<NodeKey> nodes;
  if (!state.Check(state.model.AllNamed(record.scope, &name, &nodes, nullptr))) return false;
  if (nodes.size() < 2) return state.Wrap(nodes.empty() ? NodeKey{} : nodes[0], result);
  auto* collection = state.FindCollection(CollectionKind::HTMLCollection, record.scope,
                                           CollectionFilter::AllNamed, name);
  if (!collection) collection = state.NewCollection(CollectionKind::HTMLCollection, record.scope,
                                                     CollectionFilter::AllNamed, name, true);
  if (!collection) return false;
  result.setObject(*collection->object); return true;
}
bool LazyCollection(const CollectionRecord& record) {
  if (!record.live || (record.kind != CollectionKind::NodeList && record.kind != CollectionKind::HTMLCollection))
    return false;
  switch (record.filter) {
    case CollectionFilter::FormControls: case CollectionFilter::RadioGroup:
    case CollectionFilter::SelectOptions: case CollectionFilter::WindowNamed: return false;
    default: return true;
  }
}
bool StaleCollection(State& state, JS::HandleObject object, CollectionRecord& record) {
  if (record.stale) return true;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, object);
  JS::ObjectOpResult removed;
  for (size_t index = 0; index < record.materialized; ++index)
    if (!JS_DeleteElement(cx, object, uint32_t(index), removed) || !removed.ok()) return false;
  JS::RootedValue value(cx);
  JS::RootedId id(cx);
  for (const auto& name : record.strings) {
    if (!Value(cx, DomStringView(name), &value) || !JS_ValueToId(cx, value, &id) ||
        !JS_DeletePropertyById(cx, object, id, removed) || !removed.ok()) return false;
  }
  record.materialized = 0;
  record.strings.clear();
  record.stale = true;
  return true;
}
bool FreshCollection(State& state, CollectionRecord& record) {
  if (!record.stale || !record.cache || !record.cache->object) return true;
  record.stale = false;  // first: materializing defines properties on the object
  JS::RootedObject object(state.cx, record.cache->object);
  if (!RefreshCollection(state, object, record)) return false;
  record.revision = state.model.StructureRevision();
  return true;
}
// A stale collection is collected again before a property lookup misses or
// its keys are enumerated, so script never sees the removed properties.
bool CollectionEnumerate(JSContext* cx, JS::HandleObject object) {
  const JS::Value slot = JS::GetReservedSlot(object, 0);
  auto* record = slot.isUndefined() ? nullptr : static_cast<CollectionRecord*>(slot.toPrivate());
  if (!record || !record->dataOwner) return true;
  State& owner = *record->dataOwner;
  if (!record->live && record->materialized < CollectionCount(*record)) {
    JSAutoRealm realm(cx, owner.page);
    JS::RootedObject rooted(cx, object);
    return MaterializeCollection(owner, rooted, *record);
  }
  if (!record->stale) return true;
  JSAutoRealm realm(cx, owner.page);
  return FreshCollection(owner, *record);
}
bool CollectionResolve(JSContext* cx, JS::HandleObject object, JS::HandleId id, bool* resolved) {
  *resolved = false;
  const JS::Value slot = JS::GetReservedSlot(object, 0);
  auto* record = slot.isUndefined() ? nullptr : static_cast<CollectionRecord*>(slot.toPrivate());
  if (!record || !record->dataOwner) return true;
  if (!record->live && id.isInt()) {
    const int32_t index = id.toInt();
    if (index >= 0 && size_t(index) < CollectionCount(*record)) {
      bool present = false;
      if (!JS_AlreadyHasOwnPropertyById(cx, object, id, &present)) return false;
      if (present) { *resolved = true; return true; }
      JS::RootedValue value(cx);
      if (!CollectionIndex(*record->dataOwner, *record, size_t(index), &value)) return false;
      if (!JS_DefineElement(cx, object, uint32_t(index), value, JSPROP_ENUMERATE | JSPROP_READONLY))
        return false;
      *resolved = true;
      return true;
    }
  }
  if (!record->stale) return true;
  return CollectionEnumerate(cx, object) && JS_AlreadyHasOwnPropertyById(cx, object, id, resolved);
}
bool CollectionMayResolve(const JSAtomState&, jsid, JSObject*) { return true; }
bool MaterializeAllNames(State& state, JS::HandleObject object, CollectionRecord& record) {
  JSContext* cx = state.cx;
  JS::RootedValue value(cx);
  JS::RootedId id(cx);
  for (const auto& name : record.strings) {
    if (!Value(cx, DomStringView(name), &value) || !JS_ValueToId(cx, value, &id)) return false;
    JS::ObjectOpResult removed;
    if (!JS_DeletePropertyById(cx, object, id, removed) || !removed.ok()) return false;
  }
  std::vector<DomString> names;
  if (!state.Check(state.model.AllNamed(record.scope, nullptr, nullptr, &names))) return false;
  record.strings.clear();
  JS::RootedObject prototype(cx, state.Find("HTMLAllCollection")->prototype);
  for (const auto& name : names) {
    if (!Value(cx, DomStringView(name), &value) || !JS_ValueToId(cx, value, &id)) return false;
    bool found = false;
    if (id.isInt()) continue;
    if (!JS_HasPropertyById(cx, prototype, id, &found)) return false;
    if (found) continue;
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> own(cx);
    if (!JS_GetOwnPropertyDescriptorById(cx, object, id, &own)) return false;
    if (own.isSome()) continue;
    if (!AllNameValue(state, record, name, &value) ||
        !JS_DefinePropertyById(cx, object, id, value, JSPROP_READONLY)) return false;
    record.strings.push_back(name);
  }
  return true;
}
}
