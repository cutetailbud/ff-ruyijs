#ifndef RUYIJS_DOCUMENT_COLLECTIONS_H
#define RUYIJS_DOCUMENT_COLLECTIONS_H
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "js/CallArgs.h"
#include "js/Class.h"
#include "js/RootingAPI.h"
#include "js/PropertyDescriptor.h"
#include "document_model/model.h"

namespace ruyijs::document_model {
struct State;
struct Collection;

// The one native class behind every collection object, and its iterator.
extern const JSClass kCollectionClass;
extern const JSClass kAllCollectionClass;
struct State;
struct CollectionRecord;
// Live element collections (document.all, getElementsBy*, children, table and
// link lists, document.all named lists) are refreshed when read, not at every
// structural change: a change removes their own index and name properties
// (StaleCollection, once per change run) and the next read through their
// members (CollectionReceiver), resolve or enumerate hooks collects and
// materializes them again (FreshCollection). Form controls, radio groups,
// select options, window named items, attribute maps and token views keep the
// eager refresh: other owners read their records directly.
bool LazyCollection(const CollectionRecord&);
bool StaleCollection(State&, JS::HandleObject, CollectionRecord&);
bool FreshCollection(State&, CollectionRecord&);
// The class hooks of a lazily refreshed collection object.
bool CollectionEnumerate(JSContext*, JS::HandleObject);
bool CollectionResolve(JSContext*, JS::HandleObject, JS::HandleId, bool*);
bool CollectionMayResolve(const JSAtomState&, jsid, JSObject*);
// Iteration shape shared by keys()/values()/entries()/Symbol.iterator.
enum class CollectionIteration : int32_t { Values = 0, Keys = 1, Entries = 2 };

// A record is the only collection state. Keys are re-derived from the
// DocumentAggregate whenever the owner revision moves; the record stores the
// revision its materialized index properties came from, never a second tree.
enum class CollectionKind : uint8_t { NodeList, HTMLCollection, TokenList, AttributeMap };
enum class CollectionFilter : uint8_t { None, Children, TagName, ClassNames, Name, TagNameNS,
                                        LinkHref, AnchorName, TableRows, TableBodies, TableCells,
                                        SelectOptions, FormControls, RadioGroup, All, AllNamed, WindowNamed };

struct CollectionRecord {
  CollectionKind kind = CollectionKind::NodeList;
  CollectionFilter filter = CollectionFilter::None;
  NodeKey scope;                    // Element or Document the collection belongs to.
  DomString argument;               // Tag name, class list or name filter.
  bool live = true;                 // false is a static querySelectorAll NodeList.
  uint64_t revision = 0;            // Owner revision the indices came from.
  // A lazily refreshed collection a structural change left with its own
  // properties removed; collected again when script next reads it.
  bool stale = false;
  size_t materialized = 0;          // Own index properties currently defined.
  std::vector<NodeKey> items;       // NodeList / HTMLCollection keys, tree order.
  // Static node lists hold original objects instead of Model keys. The class
  // trace hook owns these edges, which follow adoption and moving GC.
  std::vector<JS::Heap<JSObject*>> objects;
  std::vector<DomString> strings;   // TokenList tokens / AttributeMap names.
  // Object-owned live views are weakly registered for the existing sync path.
  State* weakOwner = nullptr;
  State* dataOwner = nullptr;  // Current Model owner; independent of the caller/factory realm.
  JSObject* weakObject = nullptr;
  CollectionRecord* weakPrevious = nullptr;
  CollectionRecord* weakNext = nullptr;
  Collection* cache = nullptr;  // The original native identity cache, not a GC root.
};

inline uint64_t TargetCollectionRevision(const Model& model, const CollectionRecord& record) noexcept {
  switch (record.kind) {
    case CollectionKind::NodeList:
      if (record.filter != CollectionFilter::RadioGroup && record.filter != CollectionFilter::Name)
        return model.TreeRevision();
      return model.StructureRevision();
    case CollectionKind::HTMLCollection:
      switch (record.filter) {
        case CollectionFilter::Children:
        case CollectionFilter::TagName:
        case CollectionFilter::TagNameNS:
        case CollectionFilter::SelectOptions:
        case CollectionFilter::TableRows:
        case CollectionFilter::TableBodies:
        case CollectionFilter::TableCells:
          return model.TreeRevision();
        case CollectionFilter::ClassNames:
          return model.ClassRevision();
        default:
          return model.StructureRevision();
      }
    case CollectionKind::TokenList:
      if (record.argument.empty() || record.argument == u"class")
        return model.ClassRevision();
      return model.StructureRevision();
    case CollectionKind::AttributeMap:
      return model.StructureRevision();
  }
  return model.StructureRevision();
}

// One cached JS object per live collection. The scope Wrapper traces this
// object, which in turn traces that wrapper through reserved slot 2. The cache
// is weak: trace refreshes its address; finalize clears it for safe-point reap.
struct Collection {
  JSObject* object = nullptr;
  std::unique_ptr<CollectionRecord> record;
  std::unique_ptr<Collection> next;
  explicit Collection(JSObject* value) : object(value) {}
};
void FinalizeCollection(JS::GCContext*, JSObject*);
void TraceCollection(JSTracer*, JSObject*);

// Checked native brand and interface kind, independent of the callee realm.
CollectionRecord* CollectionReceiver(JSContext*, JS::HandleValue, CollectionKind);
// Current element/token count, from the record the caller already validated.
size_t CollectionCount(const CollectionRecord&);
// Materializes index `index` of a record as a JS value; false only on OOM.
bool CollectionIndex(State&, CollectionRecord&, size_t index, JS::MutableHandleValue);
// Attribute objects are built by the NamedNodeMap owner (attr_map.cpp). The
// record argument carries namespace, local name and qualified name, joined by
// U+001F, so attributes that share a qualified name stay distinct.
bool AttributeValue(State&, NodeKey element, const DomString& argument, JS::MutableHandleValue);
// document.createAttribute: an Attr with no owner element. Its value lives on the
// object until it is attached; the element-backed Attr above stays a live view.
bool DetachedAttribute(State&, const DomString& name, JS::MutableHandleValue);
// createAttribute's name step: HTML documents fold the name, then the domain's
// attribute-name production decides validity. False reports InvalidCharacterError.
bool AttributeName(bool html, DomString* name);
DomString AttributeArgument(DomStringView namespace_uri, DomStringView local, DomStringView name);
void SplitAttributeArgument(DomStringView argument, DomStringView* namespace_uri,
                            DomStringView* local, DomStringView* name);
// getElementsByTagNameNS keeps its two arguments in the one record argument,
// joined by U+001F, which no namespace or local name may contain.
DomString NamespaceArgument(DomStringView namespace_uri, DomStringView local);
void SplitNamespaceArgument(DomStringView argument, DomStringView* namespace_uri,
                            DomStringView* local);
// Shared prototype members: length getter and the from-record iterator.
bool CollectionLength(JSContext*, unsigned, JS::Value*);
bool CollectionForEach(JSContext*, unsigned, JS::Value*);
bool InstallCollectionMethods(State&, const char* iface);
bool InstallCollectionIterator(State&, const char* iface, int32_t kind, unsigned flags = 0);

// Element's attribute-node members exchange Attr objects with their owner
// (attr_object.cpp), the only code allowed to touch an Attr slot.
struct AttributeState {
  DomString namespace_uri, local_name, name, value;
  NodeKey owner;  // Null while the Attr has no owner element.
  State* dataOwner = nullptr;  // NodeKey identity is scoped to this source Model.
  bool detached = false;
};
// Fills the view from a native Attr's current source owner, including foreign realms.
bool SetAttributeNode(State&, NodeKey element, JS::HandleValue, JS::MutableHandleValue);
bool FindAttributeByIdentity(State&, NodeKey, DomStringView, DomStringView, AttributeSnapshot*);
bool AttrStateOf(State&, JS::HandleValue, AttributeState*);
// Rebinds a detached Attr to `element` so the same object becomes that
// attribute's live view; a no-op for any other value.
bool AttrBind(State&, JS::HandleValue, NodeKey);
// Detaches a removed Attr, keeping the value it last reported.
void AttrDetach(JS::HandleValue, DomStringView);
// A detached Attr built from a snapshot, for setAttributeNode's return value.
bool DetachedAttributeFor(State&, const AttributeSnapshot&, JS::MutableHandleValue);
bool InstallAttr(State&);
bool InstallAttrNode(State&);
bool InstallCollections(State&);
bool InstallAllCollection(State&);
bool MaterializeAllNames(State&, JS::HandleObject, CollectionRecord&);
bool AllNameValue(State&, CollectionRecord&, DomStringView, JS::MutableHandleValue);
bool InstallTokenList(State&);
bool InstallPart(State&);
enum class TokenOperation { Contains, Add, Remove, Toggle, Replace, Supports, TokenItem };
bool PartTokenCall(State&, CollectionRecord&, const JS::CallArgs&, TokenOperation);
bool PartTokenSet(State&, CollectionRecord&, const JS::CallArgs&);
void UnlinkTokenView(CollectionRecord&);
void TraceTokenView(JSObject*);
bool RefreshCollection(State&, JS::HandleObject, CollectionRecord&);
bool MaterializeCollection(State&, JS::HandleObject, CollectionRecord&);
bool InstallAttributeMap(State&);
// NamedNodeMap named properties, rebuilt by MaterializeCollection (attr_map.cpp).
bool MaterializeAttributeNames(State&, JS::HandleObject, CollectionRecord&);
bool InstallQuery(State&);
}  // namespace ruyijs::document_model
#endif
