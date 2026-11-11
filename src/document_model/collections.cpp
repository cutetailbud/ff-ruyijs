#include "document_model/collections.h"
#include "runtime/weak_object.h"

#include <algorithm>
#include <new>
#include <utility>

#include "document_model/state.h"
#include "document_model/html_form.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "js/Realm.h"
#include "js/TracingAPI.h"

namespace ruyijs::document_model {
// An explicit-node NodeList (MutationRecord.addedNodes/removedNodes) owns its
// record. Cached live records are reaped only at the original owner safe point.
void FinalizeCollection(JS::GCContext*, JSObject* object) {
  const auto slot = JS::GetReservedSlot(object, 0);
  if (slot.isUndefined()) return;
  auto* record = static_cast<CollectionRecord*>(slot.toPrivate());
  if (!record) return;
  if (record->cache) {
    record->cache->object = nullptr;
    if (record->dataOwner) record->dataOwner->reclaimPending = true;
    return;
  }
  const auto owned = JS::GetReservedSlot(object, 1);
  if (!owned.isBoolean() || !owned.toBoolean()) return;
  UnlinkTokenView(*record);
  delete record;
}
void TraceCollection(JSTracer* tracer, JSObject* object) {
  const auto slot = JS::GetReservedSlot(object, 0);
  if (slot.isUndefined()) return;
  auto* record = static_cast<CollectionRecord*>(slot.toPrivate());
  if (record) {
    if (record->cache) record->cache->object = object;
    for (auto& value : record->objects)
      JS::TraceEdge(tracer, &value, "DocumentModel static NodeList node");
  }
  TraceTokenView(object);
}
namespace {
const JSClassOps kCollectionOps = {nullptr, nullptr, CollectionEnumerate, nullptr, CollectionResolve,
                                   CollectionMayResolve, FinalizeCollection, nullptr, nullptr, TraceCollection};
}  // namespace
const JSClass kCollectionClass = {"DOM Collection",
                                  JSCLASS_HAS_RESERVED_SLOTS(5) | JSCLASS_FOREGROUND_FINALIZE,
                                  &kCollectionOps};

size_t CollectionCount(const CollectionRecord& record) {
  if (record.kind != CollectionKind::NodeList && record.kind != CollectionKind::HTMLCollection)
    return record.strings.size();
  if (record.live || !record.items.empty()) return record.items.size();
  return record.objects.size();
}

DomString NamespaceArgument(DomStringView namespace_uri, DomStringView local) {
  DomString value(namespace_uri);
  value.push_back(u'\x1f');
  value += local;
  return value;
}
void SplitNamespaceArgument(DomStringView argument, DomStringView* namespace_uri,
                            DomStringView* local) {
  const size_t at = argument.find(u'\x1f');
  if (at == DomStringView::npos) { *namespace_uri = argument; *local = DomStringView(); return; }
  *namespace_uri = argument.substr(0, at); *local = argument.substr(at + 1);
}
DomString AttributeArgument(DomStringView namespace_uri, DomStringView local, DomStringView name) {
  DomString value(namespace_uri);
  value.push_back(u'\x1f'); value += local; value.push_back(u'\x1f'); value += name;
  return value;
}
void SplitAttributeArgument(DomStringView argument, DomStringView* namespace_uri,
                            DomStringView* local, DomStringView* name) {
  const size_t first = argument.find(u'\x1f');
  const size_t second = first == DomStringView::npos ? DomStringView::npos
                                                     : argument.find(u'\x1f', first + 1);
  if (second == DomStringView::npos) {
    *namespace_uri = argument; *local = DomStringView(); *name = DomStringView(); return;
  }
  *namespace_uri = argument.substr(0, first);
  *local = argument.substr(first + 1, second - first - 1);
  *name = argument.substr(second + 1);
}

bool CollectionIndex(State& state, CollectionRecord& record, size_t index,
                     JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  State& owner = *record.dataOwner;
  JS::RootedValue value(cx);
  {
    JSAutoRealm realm(cx, owner.page);
    switch (record.kind) {
      case CollectionKind::NodeList:
      case CollectionKind::HTMLCollection:
        if (!record.live && record.items.empty())
          value.setObject(*record.objects[index]);
        else if (!record.live && index < record.objects.size() && record.objects[index])
          value.setObject(*record.objects[index]);
        else if (!owner.Wrap(record.items[index], &value)) return false;
        else if (!record.live && value.isObject()) {
          if (record.objects.size() <= index) record.objects.resize(index + 1);
          record.objects[index] = &value.toObject();
        }
        break;
      case CollectionKind::TokenList:
        if (!Value(cx, DomStringView(record.strings[index]), &value)) return false;
        break;
      case CollectionKind::AttributeMap: {
        const DomString argument = record.strings[index];
        if (!AttributeValue(owner, record.scope, argument, &value)) return false;
        break;
      }
    }
  }
  if (!JS_WrapValue(cx, &value)) return false;
  result.set(value); return true;
}

namespace {
// Recomputes the record keys from the sole DocumentAggregate owner.
bool Collect(State& state, CollectionRecord& record) {
  Model& model = state.model;
  std::vector<NodeKey> nodes;
  std::vector<DomString> strings;
  switch (record.kind) {
    case CollectionKind::NodeList:
      if (record.filter == CollectionFilter::RadioGroup) {
        DomStringView name(record.argument);
        if (!state.Check(model.FormNodes(record.scope, &name, &nodes))) return false;
      } else if (record.filter == CollectionFilter::Name) {  // document.getElementsByName
        if (!state.Check(model.ElementsByName(record.scope, record.argument, &nodes))) return false;
      } else if (!state.Check(model.Children(record.scope, &nodes))) return false;
      break;
    case CollectionKind::HTMLCollection:
      if (record.filter == CollectionFilter::WindowNamed) {
        if (!state.Check(model.WindowNamed(record.scope, record.argument, &nodes))) return false;
      } else if (record.filter == CollectionFilter::AllNamed) {
        DomStringView name(record.argument);
        if (!state.Check(model.AllNamed(record.scope, &name, &nodes, nullptr))) return false;
      } else if (record.filter == CollectionFilter::All) {
        if (!state.Check(model.Query(record.scope, QueryKind::TagName, u"*", &nodes))) return false;
      } else if (record.filter == CollectionFilter::FormControls) {
        if (!state.Check(model.FormNodes(record.scope, nullptr, &nodes))) return false;
      } else if (record.filter == CollectionFilter::Children) {
        if (!state.Check(model.ChildrenElements(record.scope, &nodes))) return false;
      } else if (record.filter == CollectionFilter::TagName ||
                 record.filter == CollectionFilter::SelectOptions) {
        if (!state.Check(model.Query(record.scope, QueryKind::TagName, record.argument, &nodes)))
          return false;
      } else if (record.filter == CollectionFilter::ClassNames) {
        if (!state.Check(model.Query(record.scope, QueryKind::ClassNames, record.argument, &nodes)))
          return false;
      } else if (record.filter == CollectionFilter::Name) {
        if (!state.Check(model.ElementsByName(record.scope, record.argument, &nodes)))
          return false;
      } else if (record.filter == CollectionFilter::TagNameNS) {
        DomStringView namespace_uri, local;
        SplitNamespaceArgument(record.argument, &namespace_uri, &local);
        if (!state.Check(model.ElementsByTagNameNS(record.scope, namespace_uri, local, &nodes)))
          return false;
      } else if (record.filter == CollectionFilter::LinkHref) {
        if (!state.Check(model.Links(record.scope, &nodes))) return false;
      } else if (record.filter == CollectionFilter::AnchorName) {
        if (!state.Check(model.Anchors(record.scope, &nodes))) return false;
      } else if (record.filter == CollectionFilter::TableRows ||
                 record.filter == CollectionFilter::TableBodies ||
                 record.filter == CollectionFilter::TableCells) {
        auto kind = record.filter == CollectionFilter::TableRows ? TableList::Rows :
            record.filter == CollectionFilter::TableBodies ? TableList::Bodies : TableList::Cells;
        if (!state.Check(model.TableNodes(record.scope, kind, &nodes))) return false;
      }
      break;
    case CollectionKind::TokenList:
      if (!state.Check(model.AttributeTokens(record.scope,
          record.argument.empty() ? DomStringView(u"class") : DomStringView(record.argument), &strings))) return false;
      break;
    case CollectionKind::AttributeMap: {
      std::vector<AttributeSnapshot> values;
      if (!state.Check(model.Attributes(record.scope, &values))) return false;
      for (const AttributeSnapshot& value : values)
        strings.push_back(AttributeArgument(value.namespace_uri, value.local_name, value.name));
      break;
    }
  }
  record.items.swap(nodes);
  if (record.filter != CollectionFilter::All) record.strings.swap(strings);
  return true;
}

}  // namespace

// Rewrites the own index properties to equal the current items, with the
// reference descriptor {writable:false, enumerable:true, configurable:true}.
bool MaterializeCollection(State& state, JS::HandleObject object, CollectionRecord& record) {
  const size_t count = CollectionCount(record);
  JS::RootedValue value(state.cx);
  for (size_t index = 0; index < count; ++index) {
    if (!CollectionIndex(state, record, index, &value) ||
        !JS_DefineElement(state.cx, object, uint32_t(index), value,
                          JSPROP_ENUMERATE | JSPROP_READONLY))
      return false;
  }
  for (size_t index = count; index < record.materialized; ++index) {
    JS::ObjectOpResult removed;
    if (!JS_DeleteElement(state.cx, object, uint32_t(index), removed) || !removed.ok())
      return false;
  }
  record.materialized = count;
  if (record.filter == CollectionFilter::FormControls) return MaterializeForm(state, object, record);
  if (record.filter == CollectionFilter::All) return MaterializeAllNames(state, object, record);
  if (record.kind == CollectionKind::AttributeMap) return MaterializeAttributeNames(state, object, record);
  return true;
}

Collection* State::FindCollection(CollectionKind kind, NodeKey scope, CollectionFilter filter,
                                  DomStringView argument, bool live) const {
  // Only live views enter the realm cache; static lists are object-owned.
  for (Collection* entry = collections.get(); entry; entry = entry->next.get()) {
    if (!WeakPeek(entry->object)) continue;
    const CollectionRecord& record = *entry->record;
    if (record.live == live && record.kind == kind && record.filter == filter &&
        Same(record.scope, scope) && DomStringView(record.argument) == argument) {
      JS::ExposeObjectToActiveJS(entry->object);
      return entry;
    }
  }
  return nullptr;
}

Collection* State::NewCollection(CollectionKind kind, NodeKey scope, CollectionFilter filter,
                                 DomStringView argument, bool live) {
  JS::RootedValue sourceValue(cx);
  if (!Wrap(scope, &sourceValue) || !sourceValue.isObject()) return nullptr;
  Wrapper* source = Find(scope);
  if (!source) return nullptr;
  JS::RootedObject sourceObject(cx, source->object);
  State& factory = source->factory ? *source->factory : *this;
  JSAutoRealm realm(cx, factory.page);
  if (!JS_WrapObject(cx, &sourceObject)) return nullptr;
  const char* iface = kind == CollectionKind::NodeList ? "NodeList"
      : kind == CollectionKind::HTMLCollection
          ? (filter == CollectionFilter::SelectOptions ? "HTMLOptionsCollection" : "HTMLCollection")
      : kind == CollectionKind::TokenList ? "DOMTokenList" : "NamedNodeMap";
  if (filter == CollectionFilter::FormControls) iface = "HTMLFormControlsCollection";
  if (filter == CollectionFilter::RadioGroup) iface = "RadioNodeList";
  if (filter == CollectionFilter::All) iface = "HTMLAllCollection";
  Interface* installed = factory.Find(iface);
  if (!installed) {
    TypeError(cx, "Collection interface is not installed");
    return nullptr;
  }
  JS::RootedObject prototype(cx, installed->prototype);
  const JSClass* clasp = filter == CollectionFilter::All ? &kAllCollectionClass : &kCollectionClass;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, clasp, prototype));
  if (!object) return nullptr;
  JS::RootedObject cachedObject(cx, object);
  {
    JSAutoRealm scopeRealm(cx, source->object);
    if (!JS_WrapObject(cx, &cachedObject)) return nullptr;
  }
  std::unique_ptr<Collection> created(new (std::nothrow) Collection(object));
  std::unique_ptr<CollectionRecord> record = std::make_unique<CollectionRecord>();
  if (!object || !created || !record) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }
  record->kind = kind;
  record->filter = filter;
  record->scope = scope;
  record->dataOwner = this;
  record->live = live;
  record->argument.assign(argument);
  record->cache = created.get();
  created->record = std::move(record);
  JS::SetReservedSlot(object, 0, JS::PrivateValue(created->record.get()));
  JS::SetReservedSlot(object, 2, JS::ObjectValue(*sourceObject));
  source->collectionObjects.emplace_back(cachedObject.get());
  created->next = std::move(collections);
  collections = std::move(created);
  Collection* result = collections.get();
  if (live && !Collect(*this, *result->record)) return nullptr;
  if (!MaterializeCollection(*this, object, *result->record)) return nullptr;
  result->record->revision = TargetCollectionRevision(model, *result->record);
  return result;
}

bool RefreshCollection(State& state, JS::HandleObject object, CollectionRecord& record) {
  {
    JSAutoRealm realm(state.cx, state.page);
    if (!Collect(state, record)) return false;
  }
  JSAutoRealm realm(state.cx, object);
  return MaterializeCollection(state, object, record);
}

bool State::SyncCollections() {
  const uint64_t revision = model.StructureRevision();
  if (revision == syncedRevision) return true;
  syncing = true;
  bool ok = true;
  for (Collection* entry = collections.get(); entry && ok; entry = entry->next.get()) {
    // A dying view is never written: its slots would point the store buffer at
    // an arena the sweep is about to free (T41).
    const uint64_t target = TargetCollectionRevision(model, *entry->record);
    if (!entry->record->live || entry->record->revision == target) continue;
    JS::RootedObject object(cx, WeakLive(entry->object));
    if (!object) continue;
    if (LazyCollection(*entry->record)) {
      ok = StaleCollection(*this, object, *entry->record);  // its revision stays: still stale
      continue;
    }
    ok = RefreshCollection(*this, object, *entry->record);
    if (ok) entry->record->revision = target;
  }
  if (ok) ok = SyncTokenViews(revision);
  syncing = false;
  if (ok) syncedRevision = revision;
  return ok;
}
}  // namespace ruyijs::document_model
