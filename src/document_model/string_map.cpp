// HTMLElement.dataset: the DOMStringMap facade over an element's data-*
// attributes (T4). Owner: document_model; the attributes are the only store,
// so no JS-side copy can drift. The installed DOMStringMap interface owns its
// unique prototype; calling its constructor raises TypeError in Firefox.
// WebIDL: [LegacyOverrideBuiltIns] interface DOMStringMap { getter; setter;
// deleter; }. A legacy platform object, so this is a proxy: the supported
// property names are exactly the element's current data-* attributes, in
// attribute order, each an own writable/enumerable/configurable data property
// that shadows the prototype; [[Set]]/[[DefineOwnProperty]] of a string key is
// the named setter, delete the deleter. Symbol keys live on an expando object.
#include "document_model/style_internal.h"
#include "runtime/weak_object.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/Proxy.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"
#include "js/String.h"
#include "jsfriendapi.h"

#include <string>

namespace ruyijs::document_model {
namespace {
// Reserved slots: 0 the element, 1 the factory State, 2 the ReflectedLink.
const JSClass kDataClass = PROXY_CLASS_DEF(
    "DOMStringMap", JSCLASS_HAS_RESERVED_SLOTS(3) | JSCLASS_FOREGROUND_FINALIZE);
char dataFamily;

DomString AttributeName(DomStringView key) {
  DomString out = u"data-";
  for (char16_t c : key) {
    if (c >= u'A' && c <= u'Z') { out.push_back(u'-'); c = char16_t(c + 32); }
    out.push_back(c);
  }
  return out;
}
// Spec: a name with "-" followed by a lowercase letter is not a valid key.
bool Valid(DomStringView key) {
  for (size_t i = 0; i + 1 < key.size(); ++i)
    if (key[i] == u'-' && key[i + 1] >= u'a' && key[i + 1] <= u'z') return false;
  return true;
}
// The key a data-* attribute contributes, or false when it contributes none
// (no "data-" prefix, or an ASCII upper alpha in the name).
bool KeyOf(DomStringView name, DomString* key) {
  if (name.size() < 5 || name.substr(0, 5) != u"data-") return false;
  key->clear();
  for (size_t i = 5; i < name.size(); ++i) {
    char16_t c = name[i];
    if (c >= u'A' && c <= u'Z') return false;
    if (c == u'-' && i + 1 < name.size() && name[i + 1] >= u'a' && name[i + 1] <= u'z') {
      key->push_back(char16_t(name[++i] - 32));
      continue;
    }
    key->push_back(c);
  }
  return true;
}
// A string-keyed id (an integer id is its decimal string); false for a symbol.
bool KeyText(JSContext* cx, JS::HandleId id, DomString* key, bool* named) {
  *named = !id.isSymbol();
  if (!*named) return true;
  JS::RootedValue value(cx);
  if (!JS_IdToValue(cx, id, &value)) return false;
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && CopyText(cx, text, key);
}
Wrapper* Element(JSContext* cx, JS::HandleObject proxy) {
  JS::RootedValue element(cx, js::GetProxyReservedSlot(proxy, 0));
  return ElementReceiver(cx, element);
}
bool Read(JSContext* cx, JS::HandleObject proxy, DomStringView key,
          std::optional<DomString>* out) {
  Wrapper* entry = Element(cx, proxy);
  if (!entry) return false;
  State& state = *entry->owner;
  JSAutoRealm realm(cx, state.page);
  return state.Check(state.model.GetAttribute(entry->key, AttributeName(key), out));
}
bool Write(JSContext* cx, JS::HandleObject proxy, DomStringView key, JS::HandleValue value) {
  DomString text;
  if (!String(cx, value, &text)) return false;
  Wrapper* entry = Element(cx, proxy);
  if (!entry) return false;
  State& state = *entry->owner;
  if (!Valid(key)) return ThrowDOM(state, "SyntaxError", "Invalid data key");
  JSAutoRealm realm(cx, state.page);
  return state.Check(state.model.SetAttribute(entry->key, AttributeName(key), text));
}
JSObject* Expando(JS::HandleObject proxy) { return &js::GetProxyPrivate(proxy).toObject(); }

class Handler final : public js::BaseProxyHandler {
 public:
  constexpr Handler() : BaseProxyHandler(&dataFamily) {}
  bool getOwnPropertyDescriptor(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const override {
    DomString key;
    bool named = false;
    if (!KeyText(cx, id, &key, &named)) return false;
    JS::RootedObject expando(cx, Expando(proxy));
    if (!named) return JS_GetOwnPropertyDescriptorById(cx, expando, id, desc);
    desc.reset();
    std::optional<DomString> value;
    if (!Read(cx, proxy, key, &value)) return false;
    if (!value) return true;
    JS::RootedValue text(cx);
    if (!Value(cx, DomStringView(*value), &text)) return false;
    desc.set(mozilla::Some(JS::PropertyDescriptor::Data(text,
        {JS::PropertyAttribute::Enumerable, JS::PropertyAttribute::Configurable,
         JS::PropertyAttribute::Writable})));
    return true;
  }
  bool defineProperty(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::Handle<JS::PropertyDescriptor> desc, JS::ObjectOpResult& result) const override {
    DomString key;
    bool named = false;
    if (!KeyText(cx, id, &key, &named)) return false;
    JS::RootedObject expando(cx, Expando(proxy));
    if (!named) return JS_DefinePropertyById(cx, expando, id, desc, result);
    if (desc.isAccessorDescriptor()) return result.failNotDataDescriptor();
    JS::RootedValue value(cx, desc.hasValue() ? desc.value() : JS::UndefinedValue());
    return Write(cx, proxy, key, value) && result.succeed();
  }
  bool ownPropertyKeys(JSContext* cx, JS::HandleObject proxy,
                       JS::MutableHandleIdVector keys) const override {
    Wrapper* entry = Element(cx, proxy);
    if (!entry) return false;
    State& state = *entry->owner;
    std::vector<AttributeSnapshot> attributes;
    {
      JSAutoRealm realm(cx, state.page);
      if (!state.Check(state.model.Attributes(entry->key, &attributes))) return false;
    }
    for (const auto& attribute : attributes) {
      DomString key;
      if (!attribute.namespace_uri.empty() || !KeyOf(attribute.name, &key)) continue;
      JS::RootedString text(cx, JS_NewUCStringCopyN(cx, key.data(), key.size()));
      JS::RootedId id(cx);
      if (!text || !JS_StringToId(cx, text, &id) || !keys.append(id)) return false;
    }
    JS::RootedObject expando(cx, Expando(proxy));
    JS::RootedIdVector extra(cx);
    return js::GetPropertyKeys(cx, expando, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS,
                               &extra) && keys.appendAll(extra);
  }
  bool delete_(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
               JS::ObjectOpResult& result) const override {
    DomString key;
    bool named = false;
    if (!KeyText(cx, id, &key, &named)) return false;
    JS::RootedObject expando(cx, Expando(proxy));
    if (!named) return JS_DeletePropertyById(cx, expando, id, result);
    Wrapper* entry = Element(cx, proxy);
    if (!entry) return false;
    State& state = *entry->owner;
    JSAutoRealm realm(cx, state.page);
    return state.Check(state.model.RemoveAttribute(entry->key, AttributeName(key))) &&
           result.succeed();
  }
  bool set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::HandleValue value,
           JS::HandleValue receiver, JS::ObjectOpResult& result) const override {
    DomString key;
    bool named = false;
    if (!KeyText(cx, id, &key, &named)) return false;
    // The named setter applies when the map itself is the receiver.
    if (!named || !receiver.isObject() || &receiver.toObject() != proxy)
      return BaseProxyHandler::set(cx, proxy, id, value, receiver, result);
    return Write(cx, proxy, key, value) && result.succeed();
  }
  bool getPrototypeIfOrdinary(JSContext* cx, JS::HandleObject proxy, bool* ordinary,
                              JS::MutableHandleObject proto) const override {
    *ordinary = true;
    return JS_GetPrototype(cx, proxy, proto);
  }
  bool preventExtensions(JSContext*, JS::HandleObject, JS::ObjectOpResult& result) const override {
    return result.failCantPreventExtensions();
  }
  bool isExtensible(JSContext*, JS::HandleObject, bool* extensible) const override {
    *extensible = true;
    return true;
  }
  const char* className(JSContext*, JS::HandleObject) const override { return "DOMStringMap"; }
  // The collected object clears the element's link so a detached element can be
  // released (T41); a moved object updates it, so element.dataset keeps one
  // live identity.
  void finalize(JS::GCContext*, JSObject* proxy) const override {
    auto* link = static_cast<ReflectedLink*>(js::GetProxyReservedSlot(proxy, 2).toPrivate());
    auto* factory = static_cast<State*>(js::GetProxyReservedSlot(proxy, 1).toPrivate());
    if (link && link->dataset == proxy) link->dataset = nullptr;
    if (factory && link && !link->style && !link->dataset && !link->animatedClass)
      factory->DropReflected(link);
  }
  size_t objectMoved(JSObject* proxy, JSObject* old) const override {
    auto* link = static_cast<ReflectedLink*>(js::GetProxyReservedSlot(proxy, 2).toPrivate());
    if (link && link->dataset == old) link->dataset = proxy;
    return 0;
  }
};
const Handler handler;
}  // namespace

bool IsDataObject(JSObject* object) { return JS::GetClass(object) == &kDataClass; }

bool DataObject(State& state, JS::HandleObject element, JS::MutableHandleObject out) {
  Wrapper* entry = state.Find(element);
  if (!entry) return TypeError(state.cx, "dataset receiver is incompatible");
  ReflectedLink* link = state.Reflected(entry->key);
  if (!link) return false;
  if (JSObject* live = WeakLive(link->dataset)) { out.set(live); return true; }
  State& factory = entry->factory ? *entry->factory : state;
  JSAutoRealm realm(state.cx, factory.page);
  JS::RootedObject node(state.cx, element);
  if (!JS_WrapObject(state.cx, &node)) return false;
  JS::RootedObject expando(state.cx, JS_NewObjectWithGivenProto(state.cx, nullptr, nullptr));
  if (!expando) return false;
  JS::RootedValue priv(state.cx, JS::ObjectValue(*expando));
  js::ProxyOptions options;
  options.setClass(&kDataClass);
  JS::RootedObject object(state.cx, js::NewProxyObject(state.cx, &handler, priv,
                                                       factory.datasetPrototype, options));
  if (!object) return false;
  js::SetProxyReservedSlot(object, 0, JS::ObjectValue(*node));
  js::SetProxyReservedSlot(object, 1, JS::PrivateValue(&factory));
  js::SetProxyReservedSlot(object, 2, JS::PrivateValue(link));
  link->dataset = object.get();
  out.set(object);
  return true;
}
bool InstallStringMap(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.DefineInterface("DOMStringMap", parent, Illegal)) return false;
  state.datasetPrototype = state.Find("DOMStringMap")->prototype;
  return true;
}
}  // namespace ruyijs::document_model
