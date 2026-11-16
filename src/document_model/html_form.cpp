#include "document_model/html_form.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Symbol.h"
#include <cstring>

namespace ruyijs::document_model {
namespace {
Wrapper* FormObject(JS::HandleObject object) {
  auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (!entry || entry->dead || !entry->owner || entry->owner->syncing) return nullptr;
  if (entry->owner->Find(entry->key) != entry) return nullptr;
  // Every property lookup that misses a node's own properties (nodeType, id,
  // ...) passes here: read the interface once per wrapper, never the siblings (#35).
  if (entry->isForm < 0) {
    NodeSnapshot node;
    if (entry->owner->model.Inspect(entry->key, &node, Model::kInspectNames).code != ErrorCode::None)
      return nullptr;
    entry->isForm = !std::strcmp(node.interface_name, "HTMLFormElement");
  }
  return entry->isForm ? entry : nullptr;
}
}
bool FormResolve(JSContext* cx, JS::HandleObject object, JS::HandleId id, bool* resolved) {
  *resolved = false;
  if (id.isSymbol()) return true;
  auto* entry = FormObject(object);
  if (!entry) return true;
  if (!FormCollection(*entry->owner, entry->key)) return false;
  // Inspecting this exact id must not recursively enter the resolve hook.
  entry->owner->syncing = true;
  bool ok = JS_HasOwnPropertyById(cx, object, id, resolved);
  entry->owner->syncing = false;
  return ok;
}
bool FormEnumerate(JSContext*, JS::HandleObject object, JS::MutableHandleIdVector, bool) {
  auto* entry = FormObject(object);
  return !entry || FormCollection(*entry->owner, entry->key);
}
Collection* FormCollection(State& state, NodeKey form, CollectionFilter filter, DomStringView name) {
  const auto kind = filter == CollectionFilter::RadioGroup ? CollectionKind::NodeList : CollectionKind::HTMLCollection;
  auto* list = state.FindCollection(kind, form, filter, name);
  bool previous = state.syncing;
  state.syncing = true;
  if (!list) list = state.NewCollection(kind, form, filter, name, true);
  if (list) {
    JS::RootedValue scope(state.cx);
    if (!state.Wrap(form, &scope)) list = nullptr;
    else JS::SetReservedSlot(list->object, 2, scope);
  }
  state.syncing = previous;
  if (list && !previous && !state.SyncCollections()) return nullptr;
  return list;
}
bool FormNamed(State& state, NodeKey form, DomStringView name, JS::MutableHandleValue out) {
  std::vector<NodeKey> nodes;
  if (!state.Check(state.model.FormNodes(form, &name, &nodes))) return false;
  if (nodes.empty()) { out.setNull(); return true; }
  if (nodes.size() == 1) return state.Wrap(nodes[0], out);
  auto* list = FormCollection(state, form, CollectionFilter::RadioGroup, name);
  if (!list) return false;
  out.setObject(*list->object); return true;
}
bool InstallFormElements(State& state) {
  if (!InstallFormMembers(state) || !InstallFormCollections(state)) return false;
  JS::RootedObject array(state.cx, JS::GetRealmArrayPrototype(state.cx));
  JS::RootedValue values(state.cx);
  JS::RootedObject prototype(state.cx, state.Find("HTMLFormElement")->prototype);
  JS::RootedId iterator(state.cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(state.cx, JS::SymbolCode::iterator)));
  return JS_GetProperty(state.cx, array, "values", &values) &&
      JS_DefinePropertyById(state.cx, prototype, iterator, values, 0);
}
}
