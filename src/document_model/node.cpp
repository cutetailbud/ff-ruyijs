#include "document_model/state.h"
#include "document_model/foreign_adoption.h"
#include "browser_environment/environment.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
namespace ruyijs::document_model {
enum Field { Type, Name, Owner, Parent, ParentElement, First, Last, Previous, Next,
             Connected, NodeValue, Content, BaseURI };
enum Method { Append, Insert, Remove, Replace, Clone, Contains, Equal, SameNode, Root, HasChildren,
              Normalize, Position, LookupPrefix, LookupNamespace, DefaultNamespace,
              VariadicAppend, VariadicPrepend, Before, After, ReplaceWith, Detach };
static bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), &args);
  if (!entry) return false;
  State& state = *entry->owner;
  switch (FunctionSlot(args)) {
    case Type: {
      NodeKind kind = NodeKind::Document;
      if (!state.Check(state.model.Kind(entry->key, &kind))) return false;
      args.rval().setInt32(int32_t(kind)); return true;
    }
    case Name: {
      DomString name;
      if (!state.Check(state.model.NodeName(entry->key, &name))) return false;
      return Value(cx, DomStringView(name), args.rval());
    }
    case Owner:
    case Parent:
    case ParentElement:
    case First:
    case Last:
    case Previous:
    case Next: {
      const int32_t slot = FunctionSlot(args);
      const NodeRelation rel = slot == Owner ? NodeRelation::OwnerDocument
          : slot == Parent ? NodeRelation::Parent
          : slot == ParentElement ? NodeRelation::ParentElement
          : slot == First ? NodeRelation::FirstChild
          : slot == Last ? NodeRelation::LastChild
          : slot == Previous ? NodeRelation::PreviousSibling : NodeRelation::NextSibling;
      NodeKey target;
      if (!state.Check(state.model.NodeLink(entry->key, rel, &target))) return false;
      return state.Wrap(target, args.rval());
    }
    case Connected: {
      bool connected = false;
      if (!state.Check(state.model.IsConnected(entry->key, &connected))) return false;
      args.rval().setBoolean(connected); return true;
    }
    case NodeValue: {
      std::optional<DomString> val;
      if (!state.Check(state.model.NodeValue(entry->key, &val))) return false;
      return Value(cx, val, args.rval());
    }
    case Content: {
      std::optional<DomString> content;
      return state.Check(state.model.TextContent(entry->key, &content)) && Value(cx, content, args.rval());
    }
    case BaseURI: {
      // baseURI is the document base; a document without an owned URL reads the
      // realm's environment port, so a detached node still reports it.
      NodeKey doc_key;
      DocumentSnapshot document;
      if (!state.Check(state.model.NodeLink(entry->key, NodeRelation::BaseDocument, &doc_key)) ||
          !state.Check(state.model.InspectDocument(doc_key, &document))) return false;
      if (!document.url) {
        JS::RootedValue url(cx);
        { JSAutoRealm realm(cx, state.page); if (!state.environment.URL(&url)) return false; }
        if (!JS_WrapValue(cx, &url)) return false;
        args.rval().set(url); return true;
      }
      return Value(cx, document.url, args.rval());
    }
  }
  return false;
}
static bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), &args);
  if (!entry) return false;
  State& state = *entry->owner;
  DomString value;
  if (!entry || !String(cx, args.get(0), &value, true)) return false;
  Status status = FunctionSlot(args) == NodeValue ? state.model.SetNodeValue(entry->key, value) :
                                                   state.model.SetTextContent(entry->key, value);
  if (!state.Check(status)) return false;
  args.rval().setUndefined(); return true;
}
// The xmlns declaration one element carries for a prefix; empty is the default.
static bool DeclaredNamespace(State& state, NodeKey element, DomStringView prefix,
                              std::optional<DomString>* result) {
  std::vector<AttributeSnapshot> values;
  if (!state.Check(state.model.Attributes(element, &values))) return false;
  for (const AttributeSnapshot& value : values) {
    const bool default_declaration = prefix.empty() && value.local_name == u"xmlns";
    const bool prefixed_declaration =
        !prefix.empty() && DomStringView(value.name) == u"xmlns:" + DomString(prefix);
    if (default_declaration || prefixed_declaration) { *result = value.value; return true; }
  }
  *result = std::nullopt;
  return true;
}
// Namespace resolution walks the element's ancestor path; the first element that
// declares the prefix wins, and the xml/xmlns prefixes are predeclared.
static bool ResolveNamespace(State& state, NodeKey node, DomStringView prefix,
                            std::optional<DomString>* result) {
  if (prefix == u"xml") { *result = DomString(u"http://www.w3.org/XML/1998/namespace"); return true; }
  if (prefix == u"xmlns") { *result = DomString(u"http://www.w3.org/2000/xmlns/"); return true; }
  std::vector<NodeKey> path;
  if (!state.Check(state.model.AncestorPath(node, &path))) return false;
  for (NodeKey current : path) {
    NodeSnapshot snapshot;
    if (!state.Check(state.model.Inspect(current, &snapshot))) return false;
    if (snapshot.kind != NodeKind::Element) continue;
    std::optional<DomString> declared;
    if (!DeclaredNamespace(state, current, prefix, &declared)) return false;
    if (declared) { *result = declared; return true; }
  }
  *result = std::nullopt;
  return true;
}
static bool ResolvePrefix(State& state, NodeKey node, DomStringView namespace_uri,
                         std::optional<DomString>* result) {
  *result = std::nullopt;
  if (namespace_uri.empty()) return true;
  std::vector<NodeKey> path;
  if (!state.Check(state.model.AncestorPath(node, &path))) return false;
  for (NodeKey current : path) {
    NodeSnapshot snapshot;
    if (!state.Check(state.model.Inspect(current, &snapshot))) return false;
    if (snapshot.kind != NodeKind::Element) continue;
    if (snapshot.prefix && snapshot.namespace_uri && *snapshot.namespace_uri == namespace_uri) {
      *result = *snapshot.prefix; return true;
    }
    // An xmlns:name declaration maps its own local name to the declared URI.
    std::vector<AttributeSnapshot> values;
    if (!state.Check(state.model.Attributes(current, &values))) return false;
    for (const AttributeSnapshot& value : values)
      if (value.prefix == u"xmlns" && value.value == namespace_uri) {
        *result = value.local_name; return true;
      }
  }
  return true;
}
static bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& function = *FunctionState(args);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), &args);
  if (!entry) return false;
  State& state = *entry->owner;
  auto op = Method(FunctionSlot(args));
  if (op == Clone) {
    Draft draft;
    DomString url;
    std::optional<DomStringView> document_url;
    NodeSnapshot node;
    if (!state.Check(state.model.Inspect(entry->key, &node))) return false;
    if (!node.cloneable) { args.rval().setNull(); return true; }
    if (node.kind == NodeKind::Document) {
      DocumentSnapshot source;
      if (!state.Check(state.model.InspectDocument(entry->key, &source))) return false;
      if (!source.url) {
        JS::RootedValue current(cx);
        if (!state.environment.URL(&current) || !String(cx, current, &url)) return false;
        document_url = url;
      }
    }
    return state.Check(state.model.Clone(entry->key, JS::ToBoolean(args.get(0)), &draft, document_url)) &&
           state.Commit(draft, args.rval(), nullptr, &function);
  }
  if (op == Normalize) {
    if (!state.Check(state.model.Normalize(entry->key))) return false;
    args.rval().setUndefined(); return true;
  }
  if (op == Position) {
    if (!argc) return TypeError(cx, "compareDocumentPosition has too few arguments");
    NodeKey other;
    if (!NodeArgument(state, args[0], &other)) return false;
    uint32_t result = 0;
    if (!state.Check(state.model.CompareDocumentPosition(entry->key, other, &result))) return false;
    args.rval().setNumber(result); return true;
  }
  if (op == LookupPrefix || op == LookupNamespace || op == DefaultNamespace) {
    if (!argc) return TypeError(cx, "Node method has too few arguments");
    DomString text;
    if (!String(cx, args[0], &text, true)) return false;
    std::optional<DomString> result;
    if (op == LookupPrefix) {
      if (!ResolvePrefix(state, entry->key, text, &result)) return false;
      return Value(cx, result, args.rval());
    }
    if (!ResolveNamespace(state, entry->key, op == LookupNamespace ? DomStringView(text) : DomStringView(),
                         &result)) return false;
    if (op == LookupNamespace) return Value(cx, result, args.rval());
    args.rval().setBoolean(result.value_or(DomString()) == text); return true;
  }
  if (op == VariadicAppend || op == VariadicPrepend || op == Before || op == After ||
      op == ReplaceWith || op == Detach) {
    const InsertMode mode = op == VariadicAppend ? InsertMode::Append
        : op == VariadicPrepend ? InsertMode::Prepend
        : op == Before ? InsertMode::Before : op == After ? InsertMode::After
        : op == ReplaceWith ? InsertMode::ReplaceWith : InsertMode::Detach;
    if (!InsertNodeItems(*entry, mode, args)) return false;
    args.rval().setUndefined(); return true;
  }
  if (op == Root || op == HasChildren) {
    bool useComposed = false;
    if (op == Root && !args.get(0).isNullOrUndefined()) {
      if (!args[0].isObject()) return TypeError(cx, "getRootNode options require an object");
      JS::RootedObject options(cx, &args[0].toObject());
      JS::RootedValue composed(cx);
      if (!JS_GetProperty(cx, options, "composed", &composed)) return false;
      useComposed = JS::ToBoolean(composed);
    }
    if (op == Root) {
      NodeKey root;
      if (!state.Check(state.model.NodeLink(entry->key,
              useComposed ? NodeRelation::ComposedRoot : NodeRelation::Root, &root))) return false;
      return state.Wrap(root, args.rval());
    }
    bool has = false;
    if (!state.Check(state.model.HasChildren(entry->key, &has))) return false;
    args.rval().setBoolean(has); return true;
  }
  unsigned required = op == Insert || op == Replace ? 2 : 1;
  if (argc < required) return TypeError(cx, "Node method has too few arguments");
  NodeKey child, other;
  bool nullable = op == Contains || op == Equal || op == SameNode;
  if (op == Append || op == Insert || op == Replace) {
    Wrapper* source = NodeReceiver(cx, args[0]);
    if (!source) return false;
    if (source->owner != &state) {
      if ((op == Insert || op == Replace) &&
          !NodeArgument(state, args[1], &other, op == Insert)) return false;
      if (!InsertForeign(*entry, *source, op == Insert ? other : NodeKey{},
                          op == Replace ? other : NodeKey{})) return false;
      args.rval().set(op == Replace ? args[1] : args[0]); return true;
    }
  }
  if (!NodeArgument(state, args[0], &child, nullable)) return false;
  if (op == Contains || op == Equal || op == SameNode) {
    bool result = Same(entry->key, child);
    if (op == Contains && !state.Check(state.model.Contains(entry->key, child, &result))) return false;
    if (op == Equal && !state.Check(state.model.IsEqual(entry->key, child, &result))) return false;
    args.rval().setBoolean(result); return true;
  }
  if ((op == Insert || op == Replace) && !NodeArgument(state, args[1], &other, op == Insert)) return false;
  std::vector<std::pair<NodeKey, NodeKey>> rekey;
  Status status = op == Append || op == Insert
      ? state.model.Insert(entry->key, child, other, &rekey)
      : op == Remove ? state.model.Remove(entry->key, child)
                     : state.model.Replace(entry->key, child, other, &rekey);
  // A cross-document argument was adopted into this document; repoint the
  // realm's cached objects before the revision-driven sync below, so the caller
  // keeps the JS identity it passed in and collections reuse that same object.
  state.Rekey(rekey);
  if (!state.Check(status)) return false;
  args.rval().set(op == Replace ? args[1] : args[0]); return true;
}
bool InstallNode(State& state) {
  struct Property { const char* name; Field field; bool writable; };
  static const Property properties[] = {
    {"nodeType", Type, false}, {"nodeName", Name, false}, {"ownerDocument", Owner, false},
    {"parentNode", Parent, false}, {"parentElement", ParentElement, false},
    {"firstChild", First, false}, {"lastChild", Last, false},
    {"previousSibling", Previous, false}, {"nextSibling", Next, false},
    {"isConnected", Connected, false}, {"nodeValue", NodeValue, true}, {"textContent", Content, true},
    {"baseURI", BaseURI, false}
  };
  for (const auto& property : properties)
    if (!state.Accessor("Node", property.name, Get, property.field, property.writable ? Set : nullptr)) return false;
  struct Operation { const char* name; Method method; unsigned length; };
  static const Operation methods[] = {
    {"appendChild", Append, 1}, {"insertBefore", Insert, 2}, {"removeChild", Remove, 1},
    {"replaceChild", Replace, 2}, {"cloneNode", Clone, 0}, {"contains", Contains, 1},
    {"isEqualNode", Equal, 1}, {"isSameNode", SameNode, 1}, {"getRootNode", Root, 0},
    {"hasChildNodes", HasChildren, 0}, {"normalize", Normalize, 0},
    {"compareDocumentPosition", Position, 1}, {"lookupPrefix", LookupPrefix, 1},
    {"lookupNamespaceURI", LookupNamespace, 1}, {"isDefaultNamespace", DefaultNamespace, 1}
  };
  for (const auto& method : methods)
    if (!state.Method("Node", method.name, Call, method.length, method.method)) return false;
  // ParentNode/ChildNode mixins: append/prepend are on Element and Document, the
  // child-node methods on Element and CharacterData (Text and Comment inherit).
  static const Operation parents[] = {{"append", VariadicAppend, 0}, {"prepend", VariadicPrepend, 0}};
  static const Operation children[] = {{"before", Before, 0}, {"after", After, 0},
                                       {"replaceWith", ReplaceWith, 0}, {"remove", Detach, 0}};
  static const char* const parent_ifaces[] = {"Element", "Document", "DocumentFragment"};
  static const char* const child_ifaces[] = {"Element", "CharacterData"};
  for (const char* iface : parent_ifaces)
    for (const auto& method : parents)
      if (!state.Method(iface, method.name, Call, method.length, method.method)) return false;
  for (const char* iface : child_ifaces)
    for (const auto& method : children)
      if (!state.Method(iface, method.name, Call, method.length, method.method)) return false;
  const char* constants[] = {"ELEMENT_NODE", "ATTRIBUTE_NODE", "TEXT_NODE", "CDATA_SECTION_NODE",
    "ENTITY_REFERENCE_NODE", "ENTITY_NODE", "PROCESSING_INSTRUCTION_NODE", "COMMENT_NODE",
    "DOCUMENT_NODE", "DOCUMENT_TYPE_NODE", "DOCUMENT_FRAGMENT_NODE", "NOTATION_NODE"};
  auto* iface = state.Find("Node");
  // Prototype constants stay configurable until the binding order pass places them
  // last and makes them permanent (runtime/binding_order_table.cpp).
  unsigned attrs = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
  for (int n = 0; n < 12; ++n) {
    JS::RootedValue value(state.cx, JS::Int32Value(n + 1));
    if (!JS_DefineProperty(state.cx, iface->prototype, constants[n], value, attrs & ~JSPROP_PERMANENT) ||
        !JS_DefineProperty(state.cx, iface->constructor, constants[n], value, attrs)) return false;
  }
  const char* positions[] = {"DOCUMENT_POSITION_DISCONNECTED", "DOCUMENT_POSITION_PRECEDING",
    "DOCUMENT_POSITION_FOLLOWING", "DOCUMENT_POSITION_CONTAINS", "DOCUMENT_POSITION_CONTAINED_BY",
    "DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC"};
  for (int n = 0; n < 6; ++n) {
    JS::RootedValue value(state.cx, JS::Int32Value(1 << n));
    if (!JS_DefineProperty(state.cx, iface->prototype, positions[n], value, attrs & ~JSPROP_PERMANENT) ||
        !JS_DefineProperty(state.cx, iface->constructor, positions[n], value, attrs)) return false;
  }
  return InstallParentMembers(state);
}
}
