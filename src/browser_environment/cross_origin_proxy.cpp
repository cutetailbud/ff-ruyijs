// The cross-origin WindowProxy / Location proxy handler (cross_origin.h):
// forwards while the caller is same origin with the owning window, otherwise
// HTML [[GetOwnProperty]] / CrossOriginGetOwnPropertyHelper / CrossOriginGet /
// CrossOriginSet of WindowProxy and Location, as Firefox 160 answers them
// (dom/base/MaybeCrossOriginObject.cpp, js/xpconnect/wrappers/AccessCheck.cpp).
#include "browser_environment/cross_origin_internal.h"
#include "browser_environment/state.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
namespace ruyijs::browser_environment {
namespace {
const char kFamily = 0;
// CrossOriginProperties (HTML 7.2.1.3.1) in Firefox's own-keys order: the
// attributes, then the methods. Attributes are accessors, methods data.
struct Member { const char* name; bool attribute; bool settable; };
const Member kWindowMembers[] = {
    {"window", true, false}, {"self", true, false}, {"location", true, true}, {"closed", true, false},
    {"frames", true, false}, {"length", true, false}, {"top", true, false}, {"opener", true, false},
    {"parent", true, false}, {"close", false, false}, {"focus", false, false}, {"blur", false, false},
    {"postMessage", false, false}};
// Location: href is set-only across origins (no getter), replace a method.
const Member kLocationMembers[] = {{"href", true, true}, {"replace", false, false}};
const JS::SymbolCode kBlankSymbols[] = {JS::SymbolCode::toStringTag, JS::SymbolCode::hasInstance,
                                        JS::SymbolCode::isConcatSpreadable};
bool Accessor(JSContext* cx, unsigned argc, JS::Value* vp);
struct Handler final : js::BaseProxyHandler {
  bool location;
  explicit constexpr Handler(bool isLocation) : BaseProxyHandler(&kFamily, false, true), location(isLocation) {}
  static JSObject* Target(JSObject* proxy) { return &js::GetProxyPrivate(proxy).toObject(); }
  static bool Same(JSContext* cx, JS::HandleObject proxy, bool* same) {
    if (HostCaller(cx)) return *same = true, true;
    return SameOriginWindows(cx, JS::CurrentGlobalOrNull(cx), &js::GetProxyReservedSlot(proxy, 1).toObject(), same);
  }
  static bool Named(JSContext* cx, JS::HandleId id, std::string* name) {
    name->clear();
    if (!id.isString()) return true;
    JS::RootedString string(cx, id.toString());
    JS::UniqueChars text = JS_EncodeStringToUTF8(cx, string);
    if (!text) return false;
    *name = text.get();
    return true;
  }
  const Member* Find(const std::string& name) const {
    const Member* begin = location ? std::begin(kLocationMembers) : std::begin(kWindowMembers);
    const Member* end = location ? std::end(kLocationMembers) : std::end(kWindowMembers);
    const Member* found = std::find_if(begin, end, [&](const Member& m) { return name == m.name; });
    return found == end ? nullptr : found;
  }
  // CrossOriginPropertyFallback: "then", @@toStringTag, @@hasInstance and
  // @@isConcatSpreadable read as undefined.
  static bool Blank(JSContext* cx, JS::HandleId id, const std::string& name) {
    if (id.isString()) return name == "then";
    if (!id.isSymbol()) return false;
    for (auto code : kBlankSymbols)
      if (id == JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, code))) return true;
    return false;
  }
  // Firefox's SecurityError text: the key as JS_ValueToSource prints it
  // ("name", 0, Symbol.iterator, Symbol.for("x"), Symbol("d")).
  static bool Deny(JSContext* cx, JS::HandleId id, const char* verb = "access") {
    JS::RootedValue key(cx);
    if (!JS_IdToValue(cx, id, &key)) return false;
    JS::RootedString source(cx, JS_ValueToSource(cx, key));
    if (!source) return false;
    JS::UniqueChars text = JS_EncodeStringToUTF8(cx, source);
    if (!text) return false;
    const std::string message = std::string("Permission denied to ") + verb + " property " +
                                text.get() + " on cross-origin object";
    State* caller = WindowState(JS::CurrentGlobalOrNull(cx));
    if (caller) return ThrowDOM(*caller, "SecurityError", message.c_str());
    JS_ReportErrorUTF8(cx, "%s", message.c_str());
    return false;
  }
  // A cached per-view function: a method forwarder, or the getter/setter of a
  // cross-origin attribute ("get <name>"/"set <name>", as Firefox names them).
  bool Function(JSContext* cx, JS::HandleObject proxy, const std::string& key, JS::MutableHandleValue out) const {
    JS::RootedObject cache(cx, &js::GetProxyReservedSlot(proxy, 0).toObject());
    if (!JS_GetProperty(cx, cache, key.c_str(), out)) return false;
    if (out.isObject()) return true;
    JS::RootedObject object(cx);
    const bool getter = key.rfind("get ", 0) == 0, setter = key.rfind("set ", 0) == 0;
    if (getter || setter) {
      JSFunction* fn = js::NewFunctionWithReserved(cx, Accessor, setter ? 1 : 0, 0, key.c_str());
      JS::RootedString which(cx, JS_NewStringCopyZ(cx, key.c_str()));
      if (!fn || !which) return false;
      object = JS_GetFunctionObject(fn);
      js::SetFunctionNativeReserved(object, 0, JS::ObjectValue(*proxy));
      js::SetFunctionNativeReserved(object, 1, JS::StringValue(which));
    } else {
      JS::RootedObject target(cx, Target(proxy));
      object = NewForwarder(cx, target, key.c_str());
    }
    if (!object) return false;
    out.setObject(*object);
    return JS_DefineProperty(cx, cache, key.c_str(), out, 0);
  }
  // A window object as the caller may see it: the real window (unwrapped from
  // a compartment wrapper or another caller's cross-origin view) through
  // WindowView, or null when |value| is no window of this Context.
  static bool View(JSContext* cx, JS::HandleValue value, JS::MutableHandleValue out) {
    JSObject* window = value.isObject() ? RealWindow(&value.toObject()) : nullptr;
    if (!window) { out.setNull(); return true; }
    JS::RootedObject real(cx, window);
    return WindowView(cx, nullptr, real, out);
  }
  // The document-tree child navigables of the target window: the index-th
  // one, or the first whose browsing context name is |name|.
  static bool ChildCount(JSObject* target, size_t* count) {
    State* owner = WindowState(target);
    *count = owner && owner->frameCount ? owner->frameCount(owner->frameCountContext) : 0;
    return owner && owner->frameWindow;
  }
  static bool ChildAt(JSContext* cx, JSObject* target, size_t index, JS::MutableHandleValue out) {
    out.setUndefined();
    size_t count = 0;
    if (!ChildCount(target, &count) || index >= count) return true;
    State* owner = WindowState(target);
    JS::RootedValue child(cx);
    if (!owner->frameWindow(owner->frameWindowContext, index, &child)) return false;
    if (!child.isObject() || !RealWindow(&child.toObject())) return true;  // no browsing context yet
    return View(cx, child, out);
  }
  static bool ChildNamed(JSContext* cx, JSObject* target, JS::HandleId id, JS::MutableHandleValue out) {
    out.setUndefined();
    size_t count = 0;
    if (!id.isString() || !ChildCount(target, &count)) return true;
    JS::RootedString name(cx, id.toString());
    if (!JS_GetStringLength(name)) return true;
    for (size_t index = 0; index < count; ++index) {
      JS::RootedValue child(cx);
      if (!ChildAt(cx, target, index, &child)) return false;
      JSObject* window = child.isObject() ? RealWindow(&child.toObject()) : nullptr;
      State* state = window ? WindowState(window) : nullptr;
      if (!state || !state->name.get().isString()) continue;
      int32_t order = 1;
      if (!JS_CompareStrings(cx, state->name.get().toString(), name, &order)) return false;
      if (order == 0) { out.set(child); return true; }
    }
    return true;
  }
  static bool Index(JS::HandleId id, size_t* index) {
    if (!id.isInt() || id.toInt() < 0) return false;
    *index = size_t(id.toInt());
    return true;
  }
  // CrossOriginGet of one allowed member.
  bool CrossGet(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, const Member& member,
                JS::MutableHandleValue out) const {
    const std::string name = member.name;
    if (!member.attribute) return Function(cx, proxy, name, out);
    if (location) return Deny(cx, id, "get");  // href has no cross-origin getter
    JS::RootedObject target(cx, Target(proxy));
    State* owner = WindowState(target);
    if (name == "window" || name == "self" || name == "frames") { out.setObject(*proxy); return true; }
    if (!owner) return Deny(cx, id);
    if (name == "location") {
      JS::RootedObject real(cx, owner->instances.get()[int32_t(Kind::Location)]);
      return LocationView(cx, owner, real, out);
    }
    // The window's own state, never a page-replaced property of the target.
    if (name == "closed") { out.setBoolean(owner->closed); return true; }
    if (name == "length") {
      size_t count = 0;
      ChildCount(target, &count);
      out.setNumber(double(count));
      return true;
    }
    if (name == "opener") { out.setNull(); return true; }  // no opener browsing contexts
    JS::RootedValue window(cx, JS::ObjectValue(*(name == "top" ? owner->topWindow : owner->parentWindow).get()));
    return View(cx, window, out);
  }
  // [[GetOwnProperty]] of a cross-origin WindowProxy / Location. |found| is
  // false when the key falls through to CrossOriginPropertyFallback.
  bool CrossOwn(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
                JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const {
    desc.reset();
    JS::RootedObject target(cx, Target(proxy));
    JS::RootedValue value(cx);
    size_t index = 0;
    if (!location && Index(id, &index)) {
      if (!ChildAt(cx, target, index, &value)) return false;
      if (!value.isUndefined())
        desc.set(mozilla::Some(JS::PropertyDescriptor::Data(value,
            {JS::PropertyAttribute::Configurable, JS::PropertyAttribute::Enumerable})));
      return true;
    }
    std::string name;
    if (!Named(cx, id, &name)) return false;
    if (const Member* member = id.isString() ? Find(name) : nullptr) {
      if (!member->attribute)
        return Function(cx, proxy, name, &value) &&
               (desc.set(mozilla::Some(JS::PropertyDescriptor::Data(value,
                    {JS::PropertyAttribute::Configurable}))), true);
      JS::RootedValue get(cx), set(cx);
      if (!(location) && !Function(cx, proxy, "get " + name, &get)) return false;
      if (member->settable && !Function(cx, proxy, "set " + name, &set)) return false;
      JS::RootedObject getter(cx, get.isObject() ? &get.toObject() : nullptr);
      JS::RootedObject setter(cx, set.isObject() ? &set.toObject() : nullptr);
      desc.set(mozilla::Some(JS::PropertyDescriptor::Accessor(getter, setter,
          {JS::PropertyAttribute::Configurable})));
      return true;
    }
    if (!location) {
      if (!ChildNamed(cx, target, id, &value)) return false;
      if (!value.isUndefined()) {
        desc.set(mozilla::Some(JS::PropertyDescriptor::Data(value, {JS::PropertyAttribute::Configurable})));
        return true;
      }
    }
    if (Blank(cx, id, name))
      desc.set(mozilla::Some(JS::PropertyDescriptor::Data(JS::UndefinedHandleValue,
          {JS::PropertyAttribute::Configurable})));
    return true;
  }
  bool getOwnPropertyDescriptor(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
                                JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_GetOwnPropertyDescriptorById(cx, target, id, desc);
    if (!CrossOwn(cx, proxy, id, desc)) return false;
    return desc.isSome() || Deny(cx, id);
  }
  bool defineProperty(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::Handle<JS::PropertyDescriptor> desc,
                      JS::ObjectOpResult& result) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_DefinePropertyById(cx, target, id, desc, result);
    return Deny(cx, id, "define");
  }
  // CrossOriginOwnPropertyKeys, after the child indices of a WindowProxy.
  bool ownPropertyKeys(JSContext* cx, JS::HandleObject proxy, JS::MutableHandleIdVector props) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) return js::GetPropertyKeys(cx, target, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, props);
    size_t count = 0;
    if (!location) ChildCount(target, &count);
    for (size_t index = 0; index < count && index <= size_t(JS::PropertyKey::IntMax); ++index)
      if (!props.append(JS::PropertyKey::Int(int32_t(index)))) return false;
    std::vector<const char*> names;
    for (const Member& member : location ? std::vector<Member>(std::begin(kLocationMembers), std::end(kLocationMembers))
                                         : std::vector<Member>(std::begin(kWindowMembers), std::end(kWindowMembers)))
      names.push_back(member.name);
    names.push_back("then");
    for (const char* name : names) {
      JS::RootedString atom(cx, JS_AtomizeAndPinString(cx, name));
      if (!atom || !props.append(JS::PropertyKey::fromPinnedString(atom))) return false;
    }
    for (auto code : kBlankSymbols)
      if (!props.append(JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, code)))) return false;
    return true;
  }
  bool delete_(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::ObjectOpResult& result) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_DeletePropertyById(cx, target, id, result);
    return Deny(cx, id, "delete");
  }
  bool getPrototypeIfOrdinary(JSContext*, JS::HandleObject, bool* ordinary, JS::MutableHandleObject) const override {
    return *ordinary = false, true;
  }
  bool getPrototype(JSContext* cx, JS::HandleObject proxy, JS::MutableHandleObject protop) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_GetPrototype(cx, target, protop);
    protop.set(nullptr);
    return true;
  }
  // [[SetPrototypeOf]] of WindowProxy and Location is SetImmutablePrototype.
  bool setPrototype(JSContext* cx, JS::HandleObject proxy, JS::HandleObject proto,
                    JS::ObjectOpResult& result) const override {
    JS::RootedObject current(cx);
    if (!getPrototype(cx, proxy, &current)) return false;
    return current == proto ? result.succeed() : result.failCantSetProto();
  }
  bool preventExtensions(JSContext*, JS::HandleObject, JS::ObjectOpResult& r) const override { return r.failCantPreventExtensions(); }
  bool isExtensible(JSContext*, JS::HandleObject, bool* extensible) const override { return *extensible = true, true; }
  bool has(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, bool* bp) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_HasPropertyById(cx, target, id, bp);
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
    if (!CrossOwn(cx, proxy, id, &desc)) return false;
    if (desc.isNothing()) return Deny(cx, id);
    *bp = true;
    return true;
  }
  // CrossOriginGet: a data property's value, an accessor's getter (none: deny).
  bool get(JSContext* cx, JS::HandleObject proxy, JS::HandleValue receiver, JS::HandleId id,
           JS::MutableHandleValue vp) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) {
      JS::RootedValue self(cx, receiver.isObject() && &receiver.toObject() == proxy ? JS::ObjectValue(*target) : receiver);
      return JS_ForwardGetPropertyTo(cx, target, id, self, vp);
    }
    std::string name;
    if (!Named(cx, id, &name)) return false;
    if (const Member* member = id.isString() ? Find(name) : nullptr) return CrossGet(cx, proxy, id, *member, vp);
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
    if (!CrossOwn(cx, proxy, id, &desc)) return false;
    if (desc.isNothing()) return Deny(cx, id);
    vp.set(desc->value());
    return true;
  }
  // CrossOriginSet: only window.location and location.href have a setter;
  // every other key the caller may see throws "set", the rest "access".
  bool set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::HandleValue v, JS::HandleValue receiver,
           JS::ObjectOpResult& result) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) {
      JS::RootedValue self(cx, receiver.isObject() && &receiver.toObject() == proxy ? JS::ObjectValue(*target) : receiver);
      return JS_ForwardSetPropertyTo(cx, target, id, v, self, result);
    }
    std::string name;
    if (!Named(cx, id, &name)) return false;
    const Member* member = id.isString() ? Find(name) : nullptr;
    if (member && member->settable)
      return JS_SetProperty(cx, target, name.c_str(), v) && result.succeed();
    if (!member) {
      JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
      if (!CrossOwn(cx, proxy, id, &desc)) return false;
      if (desc.isNothing()) return Deny(cx, id);
    }
    return Deny(cx, id, "set");
  }
  const char* className(JSContext*, JS::HandleObject) const override { return "Object"; }
};
const Handler kWindowHandler(false), kLocationHandler(true);
// The getter/setter functions of the cross-origin attributes: reserved slot 0
// is the view they were taken from, slot 1 "get <name>" or "set <name>". They
// run that view's own CrossOriginGet/CrossOriginSet, whatever |this| is.
bool Accessor(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject proxy(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  JS::RootedString key(cx, js::GetFunctionNativeReserved(&args.callee(), 1).toString());
  JS::UniqueChars text = JS_EncodeStringToUTF8(cx, key);
  if (!text) return false;
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, text.get() + 4));
  JS::RootedId id(cx);
  if (!name || !JS_StringToId(cx, name, &id)) return false;
  JS::RootedValue receiver(cx, JS::ObjectValue(*proxy));
  const auto* handler = static_cast<const Handler*>(js::GetProxyHandler(proxy));
  if (std::strncmp(text.get(), "set ", 4)) return handler->get(cx, proxy, receiver, id, args.rval());
  JS::ObjectOpResult result;
  if (!handler->set(cx, proxy, id, args.get(0), receiver, result)) return false;
  args.rval().setUndefined();
  return true;
}
}  // namespace
const js::BaseProxyHandler* CrossOriginWindowHandler() { return &kWindowHandler; }
const js::BaseProxyHandler* CrossOriginLocationHandler() { return &kLocationHandler; }
// The real window behind a value a script holds: through compartment wrappers
// and either cross-origin view; null when it is no window of this Context.
JSObject* RealWindow(JSObject* object) {
  if (!object) return nullptr;
  object = js::UncheckedUnwrap(object);
  if (js::IsProxy(object) && js::GetProxyHandler(object) == &kWindowHandler)
    object = &js::GetProxyPrivate(object).toObject();
  return WindowState(object) ? object : nullptr;
}
}  // namespace ruyijs::browser_environment
