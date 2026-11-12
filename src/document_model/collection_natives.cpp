#include "document_model/collections.h"

#include "document_model/state.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include "js/Realm.h"
#include <cstring>

namespace ruyijs::document_model {
enum CollectionOperation { Item, NamedItem, HTMLItem };

bool CollectionLength(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  CollectionRecord* record = CollectionReceiver(cx, args.thisv(), CollectionKind(FunctionSlot(args)));
  if (!record) return false;
  args.rval().setNumber(double(CollectionCount(*record)));
  return true;
}

// HTMLCollection named access, in collection order: the first element whose id
// is `name`, or whose name attribute is `name`. An earlier name beats a later
// id. A unique id is used only when FastNamedItemId can prove that element is
// first in the collection, so the scan cannot find an earlier name.
static bool Named(State& state, CollectionRecord& record, DomStringView name, NodeKey* found) {
  if (name.empty()) { *found = {}; return true; }
  bool taken = false;
  if (!FastNamedItemId(state, record, name, found, &taken)) return false;
  if (taken) return true;
  const char16_t* attributes[] = {u"id", u"name"};
  for (NodeKey key : record.items) {
    for (const char16_t* attribute : attributes) {
      std::optional<DomString> value;
      if (!state.Check(state.model.GetAttribute(key, attribute, &value))) return false;
      if (value && DomStringView(*value) == name) {
        *found = key;
        return true;
      }
    }
  }
  *found = {};
  return true;
}

static bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto operation = CollectionOperation(FunctionSlot(args));
  CollectionRecord* record = CollectionReceiver(cx, args.thisv(), operation == Item
      ? CollectionKind::NodeList : CollectionKind::HTMLCollection);
  if (!record) return false;
  if (record->filter == CollectionFilter::RadioGroup && argc == 0) {
    args.rval().setNull(); return true;
  }
  if (!argc) return TypeError(cx, "Collection lookup requires an argument");
  if (operation == NamedItem) {
    DomString name;
    if (!String(cx, args.get(0), &name)) return false;
    State& state = *record->dataOwner;
    NodeKey found;
    {
      JSAutoRealm realm(cx, state.page);
      if (!Named(state, *record, name, &found)) return false;
    }
    return state.Wrap(found, args.rval());
  }
  uint32_t index = 0;
  if (!JS::ToUint32(cx, args.get(0), &index)) return false;
  if (index >= CollectionCount(*record)) {
    args.rval().setNull();
    return true;
  }
  return CollectionIndex(*record->dataOwner, *record, index, args.rval());
}

bool CollectionForEach(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  CollectionRecord* record = CollectionReceiver(cx, args.thisv(), CollectionKind(FunctionSlot(args)));
  if (!record) return false;
  if (args.length() < 1 || !args[0].isObject() || !JS::IsCallable(&args[0].toObject()))
    return TypeError(cx, "Collection forEach requires a callable argument");
  if (record->filter == CollectionFilter::RadioGroup) { args.rval().setUndefined(); return true; }
  JS::RootedValue callback(cx, args[0]);
  JS::RootedValue thisArg(cx, args.get(1));
  JS::RootedObject list(cx, &args.thisv().toObject());
  const size_t count = CollectionCount(*record);
  for (size_t index = 0; index < count; ++index) {
    if (index >= CollectionCount(*record)) continue;
    JS::RootedValue item(cx);
    if (!CollectionIndex(*record->dataOwner, *record, index, &item)) return false;
    JS::RootedValueVector values(cx);
    if (!values.append(item) || !values.append(JS::NumberValue(double(index))) ||
        !values.append(JS::ObjectValue(*list)))
      return false;
    JS::RootedValue ignored(cx);
    if (!JS::Call(cx, thisArg, callback, values, &ignored)) return false;
  }
  args.rval().setUndefined();
  return true;
}

static bool OptionsIterate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  if (!CollectionReceiver(cx, args.thisv(), CollectionKind::HTMLCollection)) return false;
  JS::RootedValue values(cx, JS::ObjectValue(*state.optionsIteratorValues));
  return JS::Call(cx, args.thisv(), values, JS::HandleValueArray::empty(), args.rval());
}

bool InstallCollectionIterator(State& state, const char* iface, int32_t kind, unsigned flags) {
  JS::RootedObject target(state.cx, state.Find(iface)->prototype);
  JS::RootedObject array(state.cx, JS::GetRealmArrayPrototype(state.cx));
  JS::RootedValue method(state.cx);
  const char* name = kind == int32_t(CollectionIteration::Keys) ? "keys"
      : kind == int32_t(CollectionIteration::Entries) ? "entries" : "values";
  JS::RootedId id(state.cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(state.cx, JS::SymbolCode::iterator)));
  if (!array || !JS_GetProperty(state.cx, array, name, &method)) return false;
  if (std::strcmp(iface, "HTMLOptionsCollection") == 0) {
    state.optionsIteratorValues = &method.toObject();
    JS::RootedObject entry(state.cx, state.Function(OptionsIterate, "[Symbol.iterator]", 0, kind));
    if (!entry) return false;
    method.setObject(*entry);
  }
  return JS_DefinePropertyById(state.cx, target, id, method, flags);
}

bool InstallCollectionMethods(State& state, const char* iface) {
  JS::RootedObject array(state.cx, JS::GetRealmArrayPrototype(state.cx));
  JS::RootedObject target(state.cx, state.Find(iface)->prototype);
  JS::RootedValue method(state.cx);
  if (!array) return false;
  for (const char* name : {"keys", "values", "entries"})
    if (!JS_GetProperty(state.cx, array, name, &method) ||
        !JS_DefineProperty(state.cx, target, name, method, JSPROP_ENUMERATE)) return false;
  return InstallCollectionIterator(state, iface, int32_t(CollectionIteration::Values));
}

bool InstallCollections(State& state) {
  return state.Accessor("NodeList", "length", CollectionLength, int32_t(CollectionKind::NodeList)) &&
      state.Method("NodeList", "item", Call, 1, Item) &&
      state.Method("NodeList", "forEach", CollectionForEach, 1, int32_t(CollectionKind::NodeList)) &&
      InstallCollectionMethods(state, "NodeList") &&
      state.Accessor("HTMLCollection", "length", CollectionLength, int32_t(CollectionKind::HTMLCollection)) &&
      state.Method("HTMLCollection", "item", Call, 1, HTMLItem) &&
      state.Method("HTMLCollection", "namedItem", Call, 1, NamedItem) &&
      InstallCollectionIterator(state, "HTMLCollection", int32_t(CollectionIteration::Values)) &&
      InstallAllCollection(state);
}
}  // namespace ruyijs::document_model
