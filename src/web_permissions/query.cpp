// Permissions.query and the PermissionStatus members (docs/owners/PERMISSIONS.md).
// query is promise-returning, so every conversion error rejects instead of throwing.
#include "web_permissions/internal.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"
#include "js/Wrapper.h"
#include <cstring>
#include <string>

namespace ruyijs::web_permissions {
const JSClass kPermissionsClass = {"Permissions", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kStatusClass = {"PermissionStatus", JSCLASS_HAS_RESERVED_SLOTS(StatusSlotCount)};
namespace {
State& Self(const JS::CallArgs& args) { return *static_cast<State*>(WebKit::Owner(args)); }
bool Receiver(JSContext* cx, JS::HandleValue self, const JSClass* clasp, const char* member,
              JS::MutableHandleObject out) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != clasp)
    return WebKit::Fail(cx, (std::string("'") + member +
        "' called on an object that does not implement interface " + clasp->name + ".").c_str());
  out.set(object);
  return true;
}
bool Describe(State& state, JS::HandleValue descriptor, size_t* index) {
  JSContext* cx = state.cx;
  if (!descriptor.isObject()) return WebKit::Fail(cx, "Permissions.query: Argument 1 is not an object.");
  JS::RootedObject object(cx, &descriptor.toObject());
  JS::RootedValue name(cx);
  if (!JS_GetProperty(cx, object, "name", &name)) return false;
  if (name.isUndefined()) return WebKit::Fail(cx, "Missing required 'name' member of PermissionDescriptor.");
  JS::RootedString text(cx, JS::ToString(cx, name));
  if (!text) return false;
  for (*index = 0; *index < kNameCount; ++*index) {
    bool same = false;
    if (!JS_StringEqualsAscii(cx, text, kNames[*index], &same)) return false;
    if (same) return true;
  }
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
  if (!utf8) return false;
  return WebKit::Fail(cx, (std::string("'") + utf8.get() + "' (value of 'name' member of "
      "PermissionDescriptor) is not a valid value for enumeration PermissionName.").c_str());
}
bool NewStatus(State& state, size_t index, JS::MutableHandleObject out) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject status(cx, state.kit.New(&kStatusClass, state.status.prototype));
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, kNames[index]));
  if (!status || !name) return false;
  JS::SetReservedSlot(status, StatusName, JS::StringValue(name));
  JS::SetReservedSlot(status, StatusIndex, JS::Int32Value(int32_t(index)));
  JS::SetReservedSlot(status, StatusHandler, JS::NullValue());
  if (!state.events.Attach(status)) return false;
  out.set(status);
  return true;
}
bool Settle(State& state, const JS::CallArgs& args) {
  JSContext* cx = state.cx;
  JS::RootedObject self(cx), status(cx);
  size_t index = 0;
  if (!Receiver(cx, args.thisv(), &kPermissionsClass, "query", &self)) return false;
  if (!args.length())
    return WebKit::Fail(cx, "Permissions.query: At least 1 argument required, but only 0 passed");
  if (!Describe(state, args[0], &index) || !NewStatus(state, index, &status)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*status));
  if (!JS_WrapValue(cx, &value)) return false;
  return state.kit.Resolve(value, args.rval());
}
}
bool Query(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  if (Settle(state, args)) return true;
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  return state.kit.RejectWith(error, args.rval());
}
// name (0), state (1) and onchange (2) read the status slots; state is read
// through the shared table, so it reflects the current profile.
bool StatusGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  const int32_t which = WebKit::Slot(args);
  static const char* const names[3] = {"get name", "get state", "get onchange"};
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kStatusClass, names[which], &self)) return false;
  if (which == 1) {
    const int32_t index = JS::GetReservedSlot(self, StatusIndex).toInt32();
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, kStates[EffectiveState(state, size_t(index))]));
    if (!text) return false;
    args.rval().setString(text);
    return true;
  }
  args.rval().set(JS::GetReservedSlot(self, which == 0 ? StatusName : StatusHandler));
  return JS_WrapValue(cx, args.rval());
}
bool StatusHandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kStatusClass, "set onchange", &self)) return false;
  JS::RootedValue previous(cx, JS::GetReservedSlot(self, StatusHandler));
  JS::RootedValue desired(cx, args.get(0));
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JSAutoRealm realm(cx, state.kit.page);
  if (!JS_WrapValue(cx, &desired) || !JS_WrapValue(cx, &previous)) return false;
  JS::RootedObject prototype(cx, state.events.prototype());
  JS::RootedValue function(cx), receiver(cx, JS::ObjectValue(*self)), ignored(cx);
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, "change"));
  if (!type) return false;
  JS::RootedValueArray<2> parameters(cx);
  parameters[0].setString(type);
  for (int operation = 0; operation < 2; ++operation) {
    parameters[1].set(operation ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(cx, prototype, operation ? "addEventListener" : "removeEventListener",
                        &function) ||
        !JS::Call(cx, receiver, function, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(self, StatusHandler, desired);
  args.rval().setUndefined();
  return true;
}
}
