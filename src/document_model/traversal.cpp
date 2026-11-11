#include "document_model/traversal.h"

#include <new>

#include "document_model/state.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::document_model {
const JSClass kWalkerClass = {"DOM Traversal", JSCLASS_HAS_RESERVED_SLOTS(1), nullptr};

Walker* WalkerOf(State& state, const JS::CallArgs& args) {
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kWalkerClass) {
    TypeError(state.cx, "Traversal receiver is incompatible");
    return nullptr;
  }
  auto* walker = static_cast<Walker*>(
      JS::GetReservedSlot(&args.thisv().toObject(), 0).toPrivate());
  if (!walker || walker->owner != &state) {
    TypeError(state.cx, "Traversal receiver is incompatible with this realm");
    return nullptr;
  }
  return walker;
}

bool FilterNode(State& state, Walker& walker, NodeKey key, int32_t* result) {
  NodeKind kind = NodeKind::Document;
  if (!state.Check(state.model.Kind(key, &kind))) return false;
  const uint32_t type = uint32_t(kind);
  // whatToShow carries one bit per node type: ELEMENT_NODE is bit 0.
  if (type == 0 || type > 32 || !(walker.what & (uint32_t(1) << (type - 1)))) {
    *result = 3;  // FILTER_SKIP
    return true;
  }
  if (!walker.callback) {
    *result = 1;  // FILTER_ACCEPT
    return true;
  }
  JS::RootedValue node(state.cx);
  if (!state.Wrap(key, &node)) return false;
  JS::RootedValue receiver(state.cx);
  if (walker.filter) receiver.setObject(*walker.filter);
  JS::RootedValue callable(state.cx, JS::ObjectValue(*walker.callback));
  JS::RootedValueVector arguments(state.cx);
  JS::RootedValue returned(state.cx);
  if (!arguments.append(node) ||
      !JS::Call(state.cx, receiver, callable, arguments, &returned)) return false;
  uint16_t value = 0;
  if (!JS::ToUint16(state.cx, returned, &value)) return false;
  *result = value == 1 ? 1 : value == 2 ? 2 : 3;
  return true;
}

bool WalkerPresent(NodeKey key) { return key.document != 0 || key.node != 0; }

bool WalkerChild(State& state, NodeKey node, bool first, NodeKey* result) {
  return state.Check(state.model.NodeLink(
      node, first ? NodeRelation::FirstChild : NodeRelation::LastChild, result));
}
bool WalkerSibling(State& state, NodeKey node, bool next, NodeKey* result) {
  return state.Check(state.model.NodeLink(
      node, next ? NodeRelation::NextSibling : NodeRelation::PreviousSibling, result));
}
bool WalkerParent(State& state, NodeKey node, NodeKey* result) {
  return state.Check(state.model.NodeLink(node, NodeRelation::Parent, result));
}

namespace {
// Builds one walker object for this realm and links it into the realm-owned
// list, so its rooted filter outlives every JS reference to the object.
bool NewWalker(State& state, bool iterator, JS::HandleValue root,
               JS::HandleValue what, JS::HandleValue filter,
               JS::MutableHandleValue result) {
  NodeKey key;
  if (!NodeArgument(state, root, &key)) return false;
  uint32_t mask = 0xFFFFFFFF;
  if (!what.isUndefined() && !JS::ToUint32(state.cx, what, &mask)) return false;
  JS::RootedObject user_filter(state.cx), callback(state.cx);
  if (!filter.isUndefined() && !filter.isNull()) {
    if (!filter.isObject()) return TypeError(state.cx, "NodeFilter is not an object");
    user_filter = &filter.toObject();
    JS::RootedValue accept(state.cx);
    if (!JS_GetProperty(state.cx, user_filter, "acceptNode", &accept)) return false;
    if (accept.isObject() && JS::IsCallable(&accept.toObject())) {
      callback = &accept.toObject();
    } else if (JS::IsCallable(user_filter)) {
      callback = user_filter;
    } else {
      return TypeError(state.cx, "NodeFilter has no acceptNode callback");
    }
  }
  Interface* iface = state.Find(iterator ? "NodeIterator" : "TreeWalker");
  if (!iface) return TypeError(state.cx, "Traversal interface is not installed");
  JS::RootedObject prototype(state.cx, iface->prototype);
  JS::RootedObject object(state.cx,
      JS_NewObjectWithGivenProto(state.cx, &kWalkerClass, prototype));
  std::unique_ptr<Walker> entry(new (std::nothrow) Walker(state.cx, iterator));
  if (!object || !entry) {
    JS_ReportOutOfMemory(state.cx);
    return false;
  }
  entry->owner = &state;
  entry->root = key;
  entry->reference = key;
  entry->what = mask;
  entry->filter = user_filter;
  entry->callback = callback;
  entry->object = object;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(entry.get()));
  entry->next = std::move(state.walkers);
  state.walkers = std::move(entry);
  result.setObject(*object);
  return true;
}

bool Constant(State& state, const char* name, double value) {
  Interface* iface = state.Find("NodeFilter");
  if (!iface) return false;
  const unsigned attrs = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
  JS::RootedValue number(state.cx, JS::NumberValue(value));
  return JS_DefineProperty(state.cx, iface->prototype, name, number, attrs) &&
         JS_DefineProperty(state.cx, iface->constructor, name, number, attrs);
}
}  // namespace

bool CreateTreeWalker(State& state, JS::HandleValue root, JS::HandleValue what,
                      JS::HandleValue filter, JS::MutableHandleValue result) {
  return NewWalker(state, false, root, what, filter, result);
}
bool CreateNodeIterator(State& state, JS::HandleValue root, JS::HandleValue what,
                        JS::HandleValue filter, JS::MutableHandleValue result) {
  return NewWalker(state, true, root, what, filter, result);
}

bool InstallNodeFilter(State& state) {
  struct Entry { const char* name; double value; };
  static const Entry values[] = {
    {"FILTER_ACCEPT", 1}, {"FILTER_REJECT", 2}, {"FILTER_SKIP", 3},
    {"SHOW_ALL", 4294967295.0}, {"SHOW_ELEMENT", 1}, {"SHOW_ATTRIBUTE", 2},
    {"SHOW_TEXT", 4}, {"SHOW_CDATA_SECTION", 8}, {"SHOW_ENTITY_REFERENCE", 16},
    {"SHOW_ENTITY", 32}, {"SHOW_PROCESSING_INSTRUCTION", 64}, {"SHOW_COMMENT", 128},
    {"SHOW_DOCUMENT", 256}, {"SHOW_DOCUMENT_TYPE", 512},
    {"SHOW_DOCUMENT_FRAGMENT", 1024}, {"SHOW_NOTATION", 2048}
  };
  for (const Entry& entry : values)
    if (!Constant(state, entry.name, entry.value)) return false;
  return true;
}
}  // namespace ruyijs::document_model
