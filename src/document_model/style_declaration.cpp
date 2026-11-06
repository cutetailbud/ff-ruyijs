// HTMLElement.style: the inline CSSStyleDeclaration for one element (T4).
// Owner: document_model. The element's style attribute is the only store; this
// facade parses on demand and writes back through SetAttribute, so no second
// declaration cache can drift. Named properties are the CSSStyleProperties
// prototype accessors (style_properties.cpp); an instance defines none, so any
// other name is an ordinary expando. Prototype members live in style_members.cpp.
#include "document_model/style_internal.h"
#include "runtime/weak_object.h"
#include "js/Class.h"
#include "js/Realm.h"
#include "jsfriendapi.h"

namespace ruyijs::document_model {
bool ReadStyle(JSContext* cx, JS::HandleObject element, DomString* out) {
  JS::RootedValue receiver(cx, JS::ObjectValue(*element));
  Wrapper* entry = ElementReceiver(cx, receiver);
  if (!entry) return false;
  State& state = *entry->owner;
  JSAutoRealm realm(cx, state.page);
  std::optional<DomString> value;
  if (!state.Check(state.model.GetAttribute(entry->key, u"style", &value))) return false;
  *out = value ? *value : DomString();
  return true;
}
bool WriteStyle(JSContext* cx, JS::HandleObject element, DomStringView text) {
  JS::RootedValue receiver(cx, JS::ObjectValue(*element));
  Wrapper* entry = ElementReceiver(cx, receiver);
  if (!entry) return false;
  State& state = *entry->owner;
  JSAutoRealm realm(cx, state.page);
  return state.Check(state.model.SetAttribute(entry->key, u"style", text));
}
namespace {
// A collected facade clears the element's link: the facade is the only reason
// the element object is alive, so this is what lets a detached element be
// released once script drops both (T41). Slot 2 carries the link.
void FinalizeStyle(JS::GCContext*, JSObject* object) {
  auto* link = static_cast<ReflectedLink*>(JS::GetReservedSlot(object, 2).toPrivate());
  auto* factory = static_cast<State*>(JS::GetReservedSlot(object, 1).toPrivate());
  if (link && link->style == object) link->style = nullptr;
  if (factory && link && !link->style && !link->dataset && !link->animatedClass)
    factory->DropReflected(link);
}
// The element's cached facade follows the engine when it moves the facade
// object, so element.style keeps returning the same live object (T41).
void TraceStyle(JSTracer*, JSObject* object) {
  auto* link = static_cast<ReflectedLink*>(JS::GetReservedSlot(object, 2).toPrivate());
  if (link && link->style != object) link->style = object;
}
const JSClassOps kStyleOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                              FinalizeStyle, nullptr, nullptr, TraceStyle};
const JSClass kStyleClass = {"CSSStyleDeclaration",
                             JSCLASS_HAS_RESERVED_SLOTS(3) | JSCLASS_FOREGROUND_FINALIZE,
                             &kStyleOps};
}  // namespace

bool StyleObject(State& state, JS::HandleObject element, JS::MutableHandleObject out) {
  Wrapper* entry = state.Find(element);
  if (!entry) return TypeError(state.cx, "style receiver is incompatible");
  ReflectedLink* link = state.Reflected(entry->key);
  if (!link) return false;
  if (JSObject* live = WeakLive(link->style)) { out.set(live); return true; }
  State& factory = entry->factory ? *entry->factory : state;
  JSAutoRealm realm(state.cx, factory.page);
  JS::RootedObject node(state.cx, element);
  if (!JS_WrapObject(state.cx, &node)) return false;
  JS::RootedObject prototype(state.cx, factory.Find("CSSStyleProperties")->prototype);
  JS::RootedObject object(state.cx, JS_NewObjectWithGivenProto(state.cx, &kStyleClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::ObjectValue(*node));
  JS::SetReservedSlot(object, 1, JS::PrivateValue(&factory));
  JS::SetReservedSlot(object, 2, JS::PrivateValue(link));
  link->style = object.get();
  out.set(object);
  return true;
}
bool IsStyleObject(JSObject* object) { return JS::GetClass(object) == &kStyleClass; }
}  // namespace ruyijs::document_model
