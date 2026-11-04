// The unique native node wrapper factory: data owner and allocation realm are explicit.
#include "document_model/state.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <new>
namespace ruyijs::document_model {
bool State::WrapLocal(State& factory, NodeKey key, JS::MutableHandleValue result, const JS::CallArgs* args) {
  if (Same(key, {})) { result.setNull(); return true; }
  // The one safe point of the wrapper lifetime: a wrapper the engine collected
  // is reclaimed here, before this realm hands out identity again.
  if (reclaimPending) { JSAutoRealm realm(cx, page); ReclaimPending(); }
  if (auto* entry = Find(key)) { result.setObject(*entry->object); return true; }
  NodeSnapshot snapshot;
  if (model.Inspect(key, &snapshot).code != ErrorCode::None) {
    // A released identity (T41): neither the tree nor a wrapper names the node,
    // so there is no object to hand out. Callers read null as "no such node".
    result.setNull(); return true;
  }
  Interface* iface = factory.Find(snapshot.interface_name);
  if (!iface) return TypeError(cx, "DOM interface has no installed factory");
  JS::RootedObject prototype(cx, iface->prototype);
  if (args) {
    JS::RootedObject target(cx, &args->newTarget().toObject());
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, target, "prototype", &value)) return false;
    if (value.isObject()) prototype = &value.toObject();
  }
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kNodeClass, prototype));
  if (!object) return false;
  // Both slots are written before any call that can collect: the finalizer and
  // the trace hook run for this object from the moment it exists, and each of
  // them reads slot 0, so an unwritten slot must never be observable (T41).
  JS::SetReservedSlot(object, 0, JS::PrivateValue(nullptr));
  JS::SetReservedSlot(object, 1, JS::PrivateValue(nullptr));
  std::unique_ptr<Wrapper> entry(new (std::nothrow) Wrapper(cx, key, object.get()));
  if (!entry) { JS_ReportOutOfMemory(cx); return false; }
  entry->owner = this;
  entry->LinkFactory(factory);
  JS::SetReservedSlot(object, 0, JS::PrivateValue(entry.get()));
  if (snapshot.kind == NodeKind::Document) {
    if (!JS_DefineProperty(cx, object, "location", documentLocationGetter,
                          documentLocationSetter,
                          JSPROP_ENUMERATE | JSPROP_PERMANENT)) {
      JS::SetReservedSlot(object, 0, JS::PrivateValue(nullptr));
      return false;
    }
  }
  if (!factory.helpers.Object(iface->name, object)) {
    JS::SetReservedSlot(object, 0, JS::PrivateValue(nullptr));
    return false;
  }
  Wrapper* raw = entry.get();
  entry->next = std::move(wrappers);
  wrappers = std::move(entry);
  raw->listed = true;
  wrapperIndex[key] = raw;
  // The node's wrapper is a borrowed Interaction target: that record must never
  // extend this wrapper's lifetime (T41).
  void* record = nullptr;
  if (!factory.interaction.AttachBorrowed(object, &record)) {
    wrapperIndex.erase(key);
    JS::SetReservedSlot(object, 0, JS::PrivateValue(nullptr));
    raw->listed = false;
    auto failed = std::move(wrappers);
    wrappers = std::move(failed->next);
    return false;
  }
  JS::SetReservedSlot(object, 1, JS::PrivateValue(record));
  // A connected node belongs to the unique tree, so its wrapper is rooted and
  // keeps its identity and listeners for as long as the node is in the tree. A
  // detached node's wrapper lives only while script references it, and the
  // subtree becomes a reclaim candidate.
  raw->Root(cx, snapshot.is_connected);
  if (!snapshot.is_connected) detached[key] = true;
  if (!CacheShadowNode(*this, snapshot, object)) return false;
  if (!CacheParsedDocumentType(*this, key, snapshot.kind, object)) return false;
  result.setObject(*object);
  return true;
}
bool State::Wrap(NodeKey key, JS::MutableHandleValue result, const JS::CallArgs* args, State* factory) {
  State& allocation = factory ? *factory : *this;
  JS::RootedValue value(cx);
  {
    JSAutoRealm realm(cx, allocation.page);
    if (!WrapLocal(allocation, key, &value, args)) return false;
  }
  if (!JS_WrapValue(cx, &value)) return false;
  result.set(value); return true;
}
bool State::Commit(Draft draft, JS::MutableHandleValue result, const JS::CallArgs* args, State* factory) {
  if (!Wrap(draft.root, result, args, factory)) { model.Rollback(draft.token); return false; }
  model.Publish(draft.token); return true;
}
}
