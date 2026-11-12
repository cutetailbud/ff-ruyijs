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

namespace {
bool SameKey(NodeKey a, NodeKey b) { return a.document == b.document && a.node == b.node; }
DomString Lower(DomStringView value) {
  DomString out(value);
  for (auto& c : out) if (c >= u'A' && c <= u'Z') c = char16_t(c + 32);
  return out;
}
bool HasToken(DomStringView list, DomStringView token) {
  for (size_t i = 0; i < list.size();) {
    while (i < list.size() && (list[i] == u' ' || list[i] == u'\t' || list[i] == u'\n' ||
                               list[i] == u'\f' || list[i] == u'\r')) ++i;
    size_t begin = i;
    while (i < list.size() && list[i] != u' ' && list[i] != u'\t' && list[i] != u'\n' &&
           list[i] != u'\f' && list[i] != u'\r') ++i;
    if (i != begin && DomStringView(list).substr(begin, i - begin) == token) return true;
  }
  return false;
}
// True when every previous sibling on the way up to `scope` is not an element,
// so `node` is the first element in that subtree. False when it is not, or
// when it is not under `scope`.
bool FirstElement(State& state, NodeKey scope, NodeKey node, bool* first) {
  *first = false;
  if (!node.node || SameKey(node, scope)) return true;
  for (NodeKey current = node; !SameKey(current, scope);) {
    NodeSnapshot self;
    if (!state.Check(state.model.Inspect(current, &self))) return false;
    for (NodeKey sib = self.previous_sibling; sib.node;) {
      NodeSnapshot prev;
      if (!state.Check(state.model.Inspect(sib, &prev))) return false;
      if (prev.kind == NodeKind::Element) return true;
      sib = prev.previous_sibling;
    }
    if (!self.parent.node) return true;
    current = self.parent;
  }
  *first = true;
  return true;
}
bool CollectionMember(State& state, const CollectionRecord& record, NodeKey node, bool* member) {
  *member = false;
  NodeSnapshot self;
  if (!state.Check(state.model.Inspect(node, &self))) return false;
  if (self.kind != NodeKind::Element || SameKey(node, record.scope)) return true;
  const DomString local = self.local_name ? *self.local_name : DomString();
  const bool fold = self.is_html_element && self.document_mode == DocumentMode::Html;
  auto tag = [&](DomStringView expected) {
    if (expected == u"*") return true;
    const DomString folded = fold ? Lower(expected) : DomString(expected);
    return local == folded;
  };
  switch (record.filter) {
    case CollectionFilter::Children:
      *member = SameKey(self.parent, record.scope);
      return true;
    case CollectionFilter::TagName:
      *member = tag(record.argument);
      return true;
    case CollectionFilter::All:
      *member = true;
      return true;
    case CollectionFilter::ClassNames: {
      std::optional<DomString> value;
      if (!state.Check(state.model.GetAttribute(node, u"class", &value))) return false;
      const DomStringView present = value ? DomStringView(*value) : DomStringView();
      bool any = false;
      for (size_t i = 0; i < record.argument.size();) {
        while (i < record.argument.size() && (record.argument[i] == u' ' || record.argument[i] == u'\t' ||
               record.argument[i] == u'\n' || record.argument[i] == u'\f' || record.argument[i] == u'\r')) ++i;
        size_t begin = i;
        while (i < record.argument.size() && record.argument[i] != u' ' && record.argument[i] != u'\t' &&
               record.argument[i] != u'\n' && record.argument[i] != u'\f' && record.argument[i] != u'\r') ++i;
        if (i == begin) continue;
        any = true;
        if (!HasToken(present, DomStringView(record.argument).substr(begin, i - begin))) return true;
      }
      *member = any;
      return true;
    }
    case CollectionFilter::TagNameNS: {
      DomStringView ns, loc;
      SplitNamespaceArgument(record.argument, &ns, &loc);
      const DomStringView space = self.namespace_uri ? DomStringView(*self.namespace_uri) : DomStringView();
      if (ns != u"*" && space != ns) return true;
      if (loc == u"*") { *member = true; return true; }
      const DomString expected = fold ? Lower(loc) : DomString(loc);
      *member = local == expected;
      return true;
    }
    case CollectionFilter::LinkHref:
    case CollectionFilter::AnchorName: {
      if (!self.is_html_element) return true;
      const bool link = record.filter == CollectionFilter::LinkHref;
      if (local != u"a" && !(link && local == u"area")) return true;
      std::optional<DomString> attr;
      if (!state.Check(state.model.GetAttribute(node, link ? u"href" : u"name", &attr))) return false;
      *member = attr.has_value();
      return true;
    }
    default:
      return true;
  }
}
}  // namespace

bool FastNamedItemId(State& state, const CollectionRecord& record, DomStringView name,
                     NodeKey* hit, bool* taken) {
  *taken = false;
  if (name.empty() || record.kind != CollectionKind::HTMLCollection) return true;
  switch (record.filter) {
    case CollectionFilter::Children:
    case CollectionFilter::TagName:
    case CollectionFilter::ClassNames:
    case CollectionFilter::TagNameNS:
    case CollectionFilter::LinkHref:
    case CollectionFilter::AnchorName:
    case CollectionFilter::All:
      break;
    default:
      // Rows, form controls, options, and window/all name lists are not a
      // plain tree-order element filter. An earlier name match could still
      // win, so the scan stays the answer.
      return true;
  }
  bool unique = false;
  NodeKey holder;
  if (!state.Check(state.model.LookupUniqueId(record.scope, name, &holder, &unique))) return false;
  if (!unique) return true;
  bool first = false, member = false;
  if (!FirstElement(state, record.scope, holder, &first)) return false;
  if (!first) return true;
  if (!CollectionMember(state, record, holder, &member)) return false;
  if (!member) return true;
  *hit = holder;
  *taken = true;
  return true;
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
