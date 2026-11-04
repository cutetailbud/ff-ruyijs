#include "document_model/collections.h"

#include "document_model/state.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
enum ElementField { Children, ChildCount, FirstElement, LastElement, NextElement,
                    PreviousElement, ClassList, Attributes };
enum QueryOperation { QueryOne, QueryAll, ByTag, ByClass, ByName, ByTagNS };

static bool FragmentId(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = NodeReceiver(cx, args.thisv(), NodeKind::DocumentFragment);
  if (!entry) return false;
  if (!argc || !args[0].isString()) return TypeError(cx, "getElementById requires a string");
  DomString value;
  std::vector<NodeKey> found;
  if (!String(cx, args[0], &value)) return false;
  State& state = *entry->owner;
  if (!state.Check(state.model.Query(entry->key, QueryKind::Id, value, &found))) return false;
  return state.Wrap(found.empty() ? NodeKey{} : found.front(), args.rval());
}
static bool DocumentNamespace(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  NodeKind kind = NodeKind::Document;
  if (!Receiver(state, args, &kind)) return false;
  if (argc < 2) return TypeError(cx, "getElementsByTagNameNS requires two arguments");
  JS::RootedObject list(cx);
  if (!state.NodeListOwned({}, &list, CollectionKind::HTMLCollection)) return false;
  args.rval().setObject(*list); return true;
}

static bool Empty(NodeKey key) { return key.document == 0 && key.node == 0; }

// Nearest following/preceding sibling that is an Element, walked over owner
// snapshots; the model keeps sibling order, this adapter only classifies.
static bool Sibling(State& state, NodeKey node, bool forward, NodeKey* result) {
  NodeSnapshot snapshot;
  if (!state.Check(state.model.Inspect(node, &snapshot))) return false;
  NodeKey next = forward ? snapshot.next_sibling : snapshot.previous_sibling;
  while (!Empty(next)) {
    NodeSnapshot value;
    if (!state.Check(state.model.Inspect(next, &value))) return false;
    if (value.kind == NodeKind::Element) {
      *result = next;
      return true;
    }
    next = forward ? value.next_sibling : value.previous_sibling;
  }
  *result = {};
  return true;
}

// Cached live collection of one scope+filter, refreshed from the owner.
static bool CollectionObject(State& state, CollectionKind kind, NodeKey scope,
                             CollectionFilter filter, DomStringView argument,
                             JS::MutableHandleValue result) {
  JS::RootedValue value(state.cx);
  {
    JSAutoRealm realm(state.cx, state.page);
    Collection* entry = state.FindCollection(kind, scope, filter, argument);
    if (!entry) entry = state.NewCollection(kind, scope, filter, argument, true);
    if (!entry || !state.SyncCollections()) return false;
    value.setObject(*entry->object);
  }
  if (!JS_WrapValue(state.cx, &value)) return false;
  result.set(value); return true;
}

static bool NodeGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv());
  if (!entry) return false;
  State& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    if (!CollectionObject(state, CollectionKind::NodeList, entry->key, CollectionFilter::None, u"", &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}

static bool ElementGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = FunctionSlot(args);
  NodeKind kind = slot >= 100 ? NodeKind::Text : NodeKind::Element;
  Wrapper* entry = NodeReceiver(cx, args.thisv(), kind);
  if (!entry) return false;
  State& state = *entry->owner;
  const ElementField field = ElementField(slot % 100);
  if (field == Children)
    return CollectionObject(state, CollectionKind::HTMLCollection, entry->key,
                            CollectionFilter::Children, u"", args.rval());
  if (field == ClassList)
    return CollectionObject(state, CollectionKind::TokenList, entry->key,
                            CollectionFilter::None, u"", args.rval());
  if (field == Attributes)
    return CollectionObject(state, CollectionKind::AttributeMap, entry->key,
                            CollectionFilter::None, u"", args.rval());
  if (field == NextElement || field == PreviousElement) {
    NodeKey found;
    if (!Sibling(state, entry->key, field == NextElement, &found)) return false;
    return state.Wrap(found, args.rval());
  }
  std::vector<NodeKey> values;
  if (!state.Check(state.model.ChildrenElements(entry->key, &values))) return false;
  if (field == ChildCount) {
    args.rval().setNumber(double(values.size()));
    return true;
  }
  NodeKey found = values.empty() ? NodeKey{} : (field == FirstElement ? values.front() : values.back());
  return state.Wrap(found, args.rval());
}

enum DocumentField { Forms, Images, Links, Scripts, Embeds, Anchors };

// Document's live HTMLCollections (forms/images/links/scripts/embeds/anchors).
// Each is the same owner-backed collection machinery, only the filter differs;
// links and anchors additionally require an href/name attribute, evaluated from
// the element's own attribute set in Model::Links/Anchors.
static bool DocumentGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  NodeKind kind = NodeKind::Document;
  Wrapper* entry = Receiver(state, args, &kind);
  if (!entry) return false;
  const CollectionFilter tag = CollectionFilter::TagName;
  switch (DocumentField(FunctionSlot(args))) {
    case Forms:
      return CollectionObject(state, CollectionKind::HTMLCollection, entry->key, tag, u"form", args.rval());
    case Images:
      return CollectionObject(state, CollectionKind::HTMLCollection, entry->key, tag, u"img", args.rval());
    case Scripts:
      return CollectionObject(state, CollectionKind::HTMLCollection, entry->key, tag, u"script", args.rval());
    case Embeds:
      return CollectionObject(state, CollectionKind::HTMLCollection, entry->key, tag, u"embed", args.rval());
    case Links:
      return CollectionObject(state, CollectionKind::HTMLCollection, entry->key,
                              CollectionFilter::LinkHref, u"", args.rval());
    case Anchors:
      return CollectionObject(state, CollectionKind::HTMLCollection, entry->key,
                              CollectionFilter::AnchorName, u"", args.rval());
  }
  return false;
}

// ParentNode mixin read for DocumentFragment: the fragment is a query scope
// like a Document, but its own children are the only element source it owns.
static bool FragmentGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = FunctionSlot(args);
  NodeKind kind = slot >= 100 ? NodeKind::Document : NodeKind::DocumentFragment;
  Wrapper* entry = NodeReceiver(cx, args.thisv(), kind);
  if (!entry) return false;
  State& state = *entry->owner;
  const ElementField field = ElementField(slot % 100);
  if (field == Children)
    return CollectionObject(state, CollectionKind::HTMLCollection, entry->key,
                            CollectionFilter::Children, u"", args.rval());
  std::vector<NodeKey> values;
  if (!state.Check(state.model.ChildrenElements(entry->key, &values))) return false;
  if (field == ChildCount) {
    args.rval().setNumber(double(values.size()));
    return true;
  }
  NodeKey found = values.empty() ? NodeKey{} : (field == FirstElement ? values.front() : values.back());
  return state.Wrap(found, args.rval());
}

static bool QueryCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv());
  if (!entry) return false;
  NodeSnapshot self;
  if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &self))) return false;
  const bool document = self.kind == NodeKind::Document;
  const bool parent = document || self.kind == NodeKind::Element ||
      self.kind == NodeKind::DocumentFragment;
  if (!parent) return TypeError(cx, "DOM query receiver is incompatible");
  const QueryOperation op = QueryOperation(FunctionSlot(args));
  if (op == ByName && !document) return TypeError(cx, "getElementsByName is a Document member");
  if (op == ByTagNS) {
    DomString namespace_uri, local;
    if (argc < 2) return TypeError(cx, "getElementsByTagNameNS has too few arguments");
    if (!String(cx, args[0], &namespace_uri, true) || !String(cx, args[1], &local, true)) return false;
    State& state = *entry->owner;
    return CollectionObject(state, CollectionKind::HTMLCollection, entry->key,
                            CollectionFilter::TagNameNS,
                            NamespaceArgument(namespace_uri, local), args.rval());
  }
  DomString text;
  if (!String(cx, args.get(0), &text)) return false;
  State& state = *entry->owner;
  if (op == QueryOne || op == QueryAll) {
    std::vector<NodeKey> found;
    if (!state.Check(state.model.QuerySelector(entry->key, text, &found))) return false;
    if (op == QueryOne) return state.Wrap(found.empty() ? NodeKey{} : found.front(), args.rval());
    JS::RootedObject list(cx);
    {
      JSAutoRealm realm(cx, state.page);
      if (!state.NodeListOwned(found, &list, CollectionKind::NodeList, nullptr, true)) return false;
    }
    JS::RootedValue result(cx, JS::ObjectValue(*list));
    if (!JS_WrapValue(cx, &result)) return false;
    args.rval().set(result); return true;
  }
  const CollectionFilter filter = op == ByTag ? CollectionFilter::TagName
      : op == ByClass ? CollectionFilter::ClassNames : CollectionFilter::Name;
  // getElementsByName returns a live NodeList (HTML Document interface); the
  // tag and class queries return HTMLCollections.
  const CollectionKind kind = filter == CollectionFilter::Name ? CollectionKind::NodeList
                                                               : CollectionKind::HTMLCollection;
  return CollectionObject(state, kind, entry->key, filter, text, args.rval());
}

bool InstallQuery(State& state) {
  return state.Accessor("Node", "childNodes", NodeGet, 0) &&
      state.Accessor("Element", "children", ElementGet, Children) &&
      state.Accessor("Element", "childElementCount", ElementGet, ChildCount) &&
      state.Accessor("Element", "firstElementChild", ElementGet, FirstElement) &&
      state.Accessor("Element", "lastElementChild", ElementGet, LastElement) &&
      state.Accessor("Element", "nextElementSibling", ElementGet, NextElement) &&
      state.Accessor("Element", "previousElementSibling", ElementGet, PreviousElement) &&
      state.Accessor("CharacterData", "nextElementSibling", ElementGet, 100 + NextElement) &&
      state.Accessor("CharacterData", "previousElementSibling", ElementGet, 100 + PreviousElement) &&
      state.Accessor("Document", "children", FragmentGet, 100 + Children) &&
      state.Accessor("Document", "childElementCount", FragmentGet, 100 + ChildCount) &&
      state.Accessor("Document", "firstElementChild", FragmentGet, 100 + FirstElement) &&
      state.Accessor("Document", "lastElementChild", FragmentGet, 100 + LastElement) &&
      state.Accessor("Element", "classList", ElementGet, ClassList) &&
      state.Accessor("Element", "attributes", ElementGet, Attributes) &&
      state.Method("Element", "querySelector", QueryCall, 1, QueryOne) &&
      state.Method("Element", "querySelectorAll", QueryCall, 1, QueryAll) &&
      state.Method("Element", "getElementsByTagName", QueryCall, 1, ByTag) &&
      state.Method("Element", "getElementsByClassName", QueryCall, 1, ByClass) &&
      state.Method("Element", "getElementsByTagNameNS", QueryCall, 2, ByTagNS) &&
      state.Method("Document", "querySelector", QueryCall, 1, QueryOne) &&
      state.Method("Document", "querySelectorAll", QueryCall, 1, QueryAll) &&
      state.Method("Document", "getElementsByTagName", QueryCall, 1, ByTag) &&
      state.Method("Document", "getElementsByClassName", QueryCall, 1, ByClass) &&
      state.Method("Document", "getElementsByName", QueryCall, 1, ByName) &&
      state.Method("Document", "getElementsByTagNameNS", DocumentNamespace, 2, 0) &&
      state.Method("DocumentFragment", "getElementById", FragmentId, 1, 0) &&
      state.Accessor("DocumentFragment", "children", FragmentGet, Children) &&
      state.Accessor("DocumentFragment", "childElementCount", FragmentGet, ChildCount) &&
      state.Accessor("DocumentFragment", "firstElementChild", FragmentGet, FirstElement) &&
      state.Accessor("DocumentFragment", "lastElementChild", FragmentGet, LastElement) &&
      state.Method("DocumentFragment", "querySelector", QueryCall, 1, QueryOne) &&
      state.Method("DocumentFragment", "querySelectorAll", QueryCall, 1, QueryAll) &&
      state.Accessor("Document", "forms", DocumentGet, Forms) &&
      state.Accessor("Document", "images", DocumentGet, Images) &&
      state.Accessor("Document", "links", DocumentGet, Links) &&
      state.Accessor("Document", "scripts", DocumentGet, Scripts) &&
      state.Accessor("Document", "embeds", DocumentGet, Embeds) &&
      state.Accessor("Document", "anchors", DocumentGet, Anchors);
}
}  // namespace ruyijs::document_model
