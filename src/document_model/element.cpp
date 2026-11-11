#include "document_model/state.h"
#include "document_model/foreign_adoption.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::document_model {
enum Field { Tag, Local, Prefix, Namespace, Id, Class };
enum Method { GetAttribute, SetAttribute, RemoveAttribute, HasAttribute, Names, HasAttributes,
              GetAttributeNS, SetAttributeNS, RemoveAttributeNS, HasAttributeNS, Toggle,
              Matches, Closest };
enum Adjacent { AdjacentHtml, AdjacentElement, AdjacentText };
static bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Element);
  if (!entry) return false;
  State& state = *entry->owner;
  int32_t field = FunctionSlot(args);
  if (field == Id || field == Class) {
    std::optional<DomString> value;
    if (!state.Check(state.model.GetAttribute(entry->key, field == Id ? u"id" : u"class", &value))) return false;
    return Value(cx, value ? DomStringView(*value) : DomStringView(), args.rval());
  }
  std::optional<DomString> value;
  const ElementTagField tag_field = field == Tag ? ElementTagField::TagName
      : field == Local ? ElementTagField::LocalName
      : field == Prefix ? ElementTagField::Prefix : ElementTagField::NamespaceURI;
  if (!state.Check(state.model.ElementTag(entry->key, tag_field, &value))) return false;
  return Value(cx, value, args.rval());
}
static bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Element);
  if (!entry) return false;
  DomString value;
  if (!String(cx, args.get(0), &value)) return false;
  State& state = *entry->owner;
  if (!state.Check(state.model.SetAttribute(entry->key, FunctionSlot(args) == Id ? u"id" : u"class", value)))
    return false;
  args.rval().setUndefined(); return true;
}
static bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Element);
  if (!entry) return false;
  auto op = Method(FunctionSlot(args));
  if (op == Names || op == HasAttributes) {
    State& state = *entry->owner;
    if (op == HasAttributes) {
      bool has = false;
      if (!state.Check(state.model.HasAttributes(entry->key, &has))) return false;
      args.rval().setBoolean(has); return true;
    }
    std::vector<AttributeSnapshot> values;
    if (!state.Check(state.model.Attributes(entry->key, &values))) return false;
    JS::RootedObject array(cx, JS::NewArrayObject(cx, values.size()));
    JS::RootedValue value(cx);
    if (!array) return false;
    for (size_t index = 0; index < values.size(); ++index)
      if (!Value(cx, DomStringView(values[index].name), &value) ||
          !JS_SetElement(cx, array, uint32_t(index), value)) return false;
    args.rval().setObject(*array); return true;
  }
  if (op == Toggle) {
    if (!argc) return TypeError(cx, "toggleAttribute has too few arguments");
    DomString name;
    if (!String(cx, args[0], &name)) return false;
    State& state = *entry->owner;
    bool result = false;
    if (!state.Check(state.model.ToggleAttribute(entry->key, name, JS::ToBoolean(args.get(1)),
                                                 argc > 1, &result))) return false;
    args.rval().setBoolean(result); return true;
  }
  if (op == Matches || op == Closest) {
    if (!argc) return TypeError(cx, "Element method has too few arguments");
    DomString text;
    if (!String(cx, args[0], &text)) return false;
    State& state = *entry->owner;
    if (op == Matches) {
      bool result = false;
      if (!state.Check(state.model.Matches(entry->key, text, &result))) return false;
      args.rval().setBoolean(result); return true;
    }
    NodeKey found;
    if (!state.Check(state.model.Closest(entry->key, text, &found))) return false;
    return state.Wrap(found, args.rval());
  }

  if (op == GetAttributeNS || op == SetAttributeNS || op == RemoveAttributeNS || op == HasAttributeNS) {
    if (argc < (op == SetAttributeNS ? 3u : 2u))
      return TypeError(cx, "Element method has too few arguments");
    DomString namespace_uri, local, value;
    if (!String(cx, args[0], &namespace_uri, true) || !String(cx, args[1], &local)) return false;
    if (op == SetAttributeNS && !String(cx, args[2], &value)) return false;
    State& state = *entry->owner;
    if (op == SetAttributeNS) {
      if (!state.Check(state.model.SetAttributeNS(entry->key, namespace_uri, local, value))) return false;
      args.rval().setUndefined(); return true;
    }
    if (op == RemoveAttributeNS) {
      if (!state.Check(state.model.RemoveAttributeNS(entry->key, namespace_uri, local))) return false;
      args.rval().setUndefined(); return true;
    }
    std::optional<DomString> result;
    if (!state.Check(state.model.GetAttributeNS(entry->key, namespace_uri, local, &result))) return false;
    if (op == GetAttributeNS) return Value(cx, result, args.rval());
    args.rval().setBoolean(result.has_value()); return true;
  }
  if (argc < (op == SetAttribute ? 2u : 1u)) return TypeError(cx, "Element method has too few arguments");
  DomString name, value;
  if (!String(cx, args[0], &name)) return false;
  if (op == SetAttribute && !String(cx, args[1], &value)) return false;
  State& state = *entry->owner;
  if (op == SetAttribute) {
    if (!state.Check(state.model.SetAttribute(entry->key, name, value))) return false;
  } else if (op == RemoveAttribute) {
    if (!state.Check(state.model.RemoveAttribute(entry->key, name))) return false;
  } else {
    std::optional<DomString> result;
    if (!state.Check(state.model.GetAttribute(entry->key, name, &result))) return false;
    if (op == GetAttribute) return Value(cx, result, args.rval());
    args.rval().setBoolean(result.has_value()); return true;
  }
  args.rval().setUndefined(); return true;
}
static bool GetMarkup(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Element);
  if (!entry) return false;
  State& state = *entry->owner;
  DomString value;
  const bool outer = FunctionSlot(args) == 1;
  return state.Check(outer ? state.model.OuterHtml(entry->key, &value)
                                    : state.model.InnerHtml(entry->key, &value)) &&
         Value(cx, DomStringView(value), args.rval());
}
static bool SetMarkup(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Element);
  if (!entry) return false;
  DomString value;  // [LegacyNullToEmptyString]: null clears the children or markup.
  if (!String(cx, args.get(0), &value, true)) return false;
  State& state = *entry->owner;
  Status status = FunctionSlot(args) == 1 ? state.model.ReplaceOuter(entry->key, value)
                                          : state.model.SetInnerHtml(entry->key, value);
  if (!state.Check(status)) return false;
  args.rval().setUndefined(); return true;
}
static int32_t AdjacentPosition(const DomString& text) {
  return text == u"beforebegin" ? 0 : text == u"afterbegin" ? 1
      : text == u"beforeend" ? 2 : text == u"afterend" ? 3 : -1;
}
static bool InsertCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Element);
  if (!entry) return false;
  if (argc < 2) return TypeError(cx, "insertAdjacent method has too few arguments");
  DomString text;
  if (!String(cx, args[0], &text)) return false;
  const int32_t position = AdjacentPosition(text);
  const Adjacent op = Adjacent(FunctionSlot(args));
  if (op == AdjacentHtml) {
    DomString markup;
    if (!String(cx, args[1], &markup)) return false;
    State& state = *entry->owner;
    if (!state.Check(state.model.InsertAdjacent(entry->key, position, markup))) return false;
    args.rval().setUndefined(); return true;
  }
  State& state = *entry->owner;
  if (position < 0) return ThrowDOM(state, "SyntaxError", "Invalid insertAdjacent position");
  const InsertMode mode = position == 0 ? InsertMode::Before : position == 1 ? InsertMode::Prepend
      : position == 2 ? InsertMode::Append : InsertMode::After;
  if (position == 0 || position == 3) {
    NodeSnapshot self;
    if (!state.Check(state.model.Inspect(entry->key, &self))) return false;
    if (self.parent.document == 0) {  // No parent: the element methods report null.
      if (op == AdjacentElement) args.rval().setNull(); else args.rval().setUndefined();
      return true;
    }
  }
  std::vector<Item> items(1);
  if (op == AdjacentElement) {
    Wrapper* source = NodeReceiver(cx, args[1], NodeKind::Element);
    if (!source) return false;
    if (source->owner != &state) {
      NodeSnapshot self;
      if (!state.Check(state.model.Inspect(entry->key, &self))) return false;
      const bool inside = position == 1 || position == 2;
      JS::RootedValue parent(cx);
      if (!state.Wrap(inside ? entry->key : self.parent, &parent)) return false;
      Wrapper* destination = NodeReceiver(cx, parent);
      const NodeKey before = position == 0 ? entry->key : position == 1 ? self.first_child
          : position == 3 ? self.next_sibling : NodeKey{};
      if (!destination || !InsertForeign(*destination, *source, before, {})) return false;
      args.rval().set(args[1]); return true;
    }
    if (!NodeArgument(state, args[1], &items[0].node)) return false;
  } else if (!String(cx, args[1], &items[0].text)) return false;
  State& current = *entry->owner;
  std::vector<std::pair<NodeKey, NodeKey>> rekey;
  const Status status = current.model.InsertItems(entry->key, mode, items, &rekey);
  current.Rekey(rekey);
  if (!current.Check(status)) return false;
  if (op == AdjacentElement) args.rval().set(args[1]); else args.rval().setUndefined();
  return true;
}
bool InstallElement(State& state) {
  if (!state.Accessor("Element", "innerHTML", GetMarkup, 0, SetMarkup)) return false;
  if (!state.Accessor("Element", "outerHTML", GetMarkup, 1, SetMarkup)) return false;
  const char* fields[] = {"tagName", "localName", "prefix", "namespaceURI", "id", "className"};
  for (int32_t index = 0; index < 6; ++index)
    if (!state.Accessor("Element", fields[index], Get, index, index >= Id ? Set : nullptr)) return false;
  struct Operation { const char* name; Method method; unsigned length; };
  static const Operation methods[] = {
    {"getAttribute", GetAttribute, 1}, {"setAttribute", SetAttribute, 2},
    {"removeAttribute", RemoveAttribute, 1}, {"hasAttribute", HasAttribute, 1},
    {"getAttributeNames", Names, 0}, {"hasAttributes", HasAttributes, 0},
    {"getAttributeNS", GetAttributeNS, 2}, {"setAttributeNS", SetAttributeNS, 3},
    {"removeAttributeNS", RemoveAttributeNS, 2}, {"hasAttributeNS", HasAttributeNS, 2},
    {"toggleAttribute", Toggle, 1}, {"matches", Matches, 1}, {"closest", Closest, 1}
  };
  for (const auto& method : methods)
    if (!state.Method("Element", method.name, Call, method.length, method.method)) return false;
  struct Insertion { const char* name; Adjacent adjacent; };
  static const Insertion insertions[] = {
    {"insertAdjacentHTML", AdjacentHtml}, {"insertAdjacentElement", AdjacentElement},
    {"insertAdjacentText", AdjacentText}
  };
  for (const auto& insertion : insertions)
    if (!state.Method("Element", insertion.name, InsertCall, 2, insertion.adjacent)) return false;
  return InstallElementView(state) && InstallShadowDOM(state) && InstallPart(state) &&
      InstallElementHTML(state) && InstallElementARIA(state);
}
}
