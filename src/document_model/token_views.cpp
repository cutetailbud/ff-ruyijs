// GC-owned views use the original collection class, materializer and DOM state.
#include "document_model/state.h"
#include "runtime/weak_object.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <new>

namespace ruyijs::document_model {
void UnlinkTokenView(CollectionRecord& record) {
  if (!record.weakOwner) return;
  if (record.weakPrevious) record.weakPrevious->weakNext = record.weakNext;
  else record.weakOwner->tokenViews = record.weakNext;
  if (record.weakNext) record.weakNext->weakPrevious = record.weakPrevious;
  record.weakOwner = nullptr;
  record.weakPrevious = record.weakNext = nullptr;
}
void TraceTokenView(JSObject* object) {
  auto* record = static_cast<CollectionRecord*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (record && record->weakOwner) record->weakObject = object;
}
bool State::TokenListOwned(Wrapper& entry, DomStringView attribute, JS::MutableHandleValue result) {
  State& owner = *entry.owner;
  State& factory = entry.factory ? *entry.factory : *this;
  JSAutoRealm realm(cx, factory.page);
  JS::RootedObject node(cx, entry.object);
  if (!JS_WrapObject(cx, &node)) return false;
  JS::RootedObject prototype(cx, factory.Find("DOMTokenList")->prototype);
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kCollectionClass, prototype));
  if (!object) return false;
  auto* record = new (std::nothrow) CollectionRecord();
  if (!record) { JS_ReportOutOfMemory(cx); return false; }
  record->kind = CollectionKind::TokenList;
  record->dataOwner = &owner;
  record->scope = entry.key;
  record->argument.assign(attribute);
  JS::SetReservedSlot(object, 0, JS::PrivateValue(record));
  JS::SetReservedSlot(object, 1, JS::BooleanValue(true));
  JS::SetReservedSlot(object, 2, JS::ObjectValue(*node));
  if (!RefreshCollection(owner, object, *record)) return false;
  record->revision = TargetCollectionRevision(owner.model, *record);
  record->weakOwner = &owner;
  record->weakObject = object;
  record->weakNext = owner.tokenViews;
  if (owner.tokenViews) owner.tokenViews->weakPrevious = record;
  owner.tokenViews = record;
  result.setObject(*object); return true;
}
bool State::SyncTokenViews(uint64_t) {
  for (auto* record = tokenViews; record; record = record->weakNext) {
    const uint64_t target = TargetCollectionRevision(model, *record);
    if (record->revision == target) continue;
    // Finalization may unlink later views during allocation. This view stays
    // rooted and its updated next link remains the sole iteration cursor.
    JS::RootedObject object(cx, WeakLive(record->weakObject));
    if (!object) continue;  // being finalized; the finalizer unlinks it
    JSAutoRealm realm(cx, object);
    if (!RefreshCollection(*this, object, *record)) return false;
    record->revision = target;
  }
  return true;
}
namespace {
bool Part(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& factory = *FunctionState(args);
  JS::RootedObject node(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* entry = node && JS::GetClass(node) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(node, 0).toPrivate()) : nullptr;
  NodeSnapshot snapshot;
  if (entry && entry->owner && !entry->dead) {
    JSAutoRealm realm(cx, entry->owner->page);
    if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &snapshot))) return false;
  } else return TypeError(cx, "Element part receiver is incompatible");
  if (snapshot.kind != NodeKind::Element) return TypeError(cx, "Element part receiver is incompatible");
  factory.Observe("Element", "part", int(ApiAccess::Read));
  JS::RootedValue result(cx, JS::GetReservedSlot(node, 5));
  if (result.isUndefined()) {
    if (!factory.TokenListOwned(*entry, u"part", &result)) return false;
    JSAutoRealm realm(cx, node);
    if (!JS_WrapValue(cx, &result)) return false;
    JS::SetReservedSlot(node, 5, result);
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
}
bool InstallPart(State& state) { return state.Accessor("Element", "part", Part, 0); }
}
