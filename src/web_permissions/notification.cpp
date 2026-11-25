// Notification (docs/owners/PERMISSIONS.md slice 2). Its permission comes from
// the same table as navigator.permissions ("notifications": prompt -> default),
// so the two can never disagree. No notification is displayed: a notification
// without permission fires `error`, a permitted one fires `show` (D128).
#include "web_permissions/internal.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/StructuredClone.h"
#include "js/ValueArray.h"
#include <string>

namespace ruyijs::web_permissions {
const JSClass kNotificationClass = {"Notification", JSCLASS_HAS_RESERVED_SLOTS(NoteSlotCount)};
namespace {
State& Self(const JS::CallArgs& args) { return *static_cast<State*>(WebKit::Owner(args)); }
size_t NotificationsIndex() {
  for (size_t i = 0; i < kNameCount; ++i) if (std::string(kNames[i]) == "notifications") return i;
  return 0;
}
void CloneError(JSContext*, uint32_t, void* closure, const char*) { *static_cast<bool*>(closure) = true; }
const JSStructuredCloneCallbacks kCallbacks = {nullptr, nullptr, CloneError};
bool Text(JSContext* cx, JS::HandleObject options, const char* name, JS::MutableHandleValue out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, options, name, &value)) return false;
  if (value.isUndefined()) { out.setString(JS_GetEmptyString(cx)); return true; }
  JSString* text = JS::ToString(cx, value);
  if (!text) return false;
  out.setString(text);
  return true;
}
bool Boolean(JSContext* cx, JS::HandleObject options, const char* name, JS::MutableHandleValue out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, options, name, &value)) return false;
  out.setBoolean(JS::ToBoolean(value));
  return true;
}
// icon is a USVString resolved against the document URL, as Firefox reports it.
bool Icon(State& state, JS::HandleValue text, JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  out.set(text);
  if (!JS_GetStringLength(text.toString())) return true;
  JS::RootedValue location(cx), href(cx), constructor(cx);
  if (!JS_GetProperty(cx, state.kit.page, "location", &location) || !location.isObject()) return false;
  JS::RootedObject locationObject(cx, &location.toObject());
  if (!JS_GetProperty(cx, locationObject, "href", &href) ||
      !JS_GetProperty(cx, state.kit.page, "URL", &constructor)) return false;
  JS::RootedValueArray<2> values(cx);
  values[0].set(text);
  values[1].set(href);
  JS::RootedObject url(cx);
  if (!JS::Construct(cx, constructor, values, &url)) { JS_ClearPendingException(cx); return true; }
  return JS_GetProperty(cx, url, "href", out);
}
bool FireTask(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  JS::RootedObject record(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  JS::RootedValue target(cx), type(cx);
  args.rval().setUndefined();
  if (!JS_GetElement(cx, record, 0, &target) || !JS_GetElement(cx, record, 1, &type)) return false;
  JS::RootedObject note(cx, &target.toObject());
  JS::RootedString text(cx, type.toString());
  JS::UniqueChars name = JS_EncodeStringToUTF8(cx, text);
  return name && state.events.Fire(note, name.get(), false, false);
}
}
bool QueueNotificationEvent(State& state, JS::HandleObject note, const char* type) {
  JSContext* cx = state.cx;
  if (!state.taskPort) return true;
  JS::RootedObject record(cx, JS::NewArrayObject(cx, 2));
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  JS::RootedValue target(cx, JS::ObjectValue(*note));
  if (!record || !name || !JS_SetElement(cx, record, 0, target) || !JS_SetElement(cx, record, 1, name))
    return false;
  JSFunction* function = js::NewFunctionWithReserved(cx, FireTask, 0, 0, "Notification event");
  if (!function) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&state));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*record));
  return state.taskPort(state.taskContext, task);
}
const char* NotificationPermission(State& state) {
  static const char* const names[3] = {"granted", "denied", "default"};
  return names[EffectiveState(state, NotificationsIndex())];
}
bool NotificationCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "Notification constructor: 'new' is required");
  if (!args.length())
    return WebKit::Fail(cx, "Notification constructor: At least 1 argument required, but only 0 passed");
  JS::RootedString title(cx, JS::ToString(cx, args[0]));
  if (!title) return false;
  JS::RootedValue optionsValue(cx, args.get(1));
  if (!optionsValue.isNullOrUndefined() && !optionsValue.isObject())
    return WebKit::Fail(cx, "Notification constructor: Argument 2 can't be converted to a dictionary.");
  JS::RootedObject options(cx, optionsValue.isObject() ? &optionsValue.toObject() : JS_NewPlainObject(cx));
  JS::RootedValue body(cx), data(cx), dir(cx), icon(cx), lang(cx), interaction(cx), silent(cx), tag(cx);
  // WebIDL dictionary members in lexicographic order.
  if (!options || !Text(cx, options, "body", &body) || !JS_GetProperty(cx, options, "data", &data) ||
      !JS_GetProperty(cx, options, "dir", &dir)) return false;
  JS::RootedString direction(cx, dir.isUndefined() ? JS_NewStringCopyZ(cx, "auto") : JS::ToString(cx, dir));
  if (!direction) return false;
  bool valid = false;
  for (const char* allowed : {"auto", "ltr", "rtl"}) {
    bool same = false;
    if (!JS_StringEqualsAscii(cx, direction, allowed, &same)) return false;
    valid = valid || same;
  }
  if (!valid) {
    JS::UniqueChars text = JS_EncodeStringToUTF8(cx, direction);
    return text && WebKit::Fail(cx, (std::string("Notification constructor: '") + text.get() +
        "' (value of 'dir' member of NotificationOptions) is not a valid value for enumeration "
        "NotificationDirection.").c_str());
  }
  if (!Text(cx, options, "icon", &icon) || !Text(cx, options, "lang", &lang) ||
      !Boolean(cx, options, "requireInteraction", &interaction) ||
      !Boolean(cx, options, "silent", &silent) || !Text(cx, options, "tag", &tag) ||
      !Icon(state, icon, &icon)) return false;
  JS::RootedValue stored(cx, JS::NullValue());
  if (!data.isUndefined()) {
    bool cloneError = false;
    JSAutoStructuredCloneBuffer buffer(JS::StructuredCloneScope::SameProcess, &kCallbacks, &cloneError);
    JS::CloneDataPolicy policy;
    if (!buffer.write(cx, data, &kCallbacks, &cloneError) ||
        !buffer.read(cx, &stored, policy, &kCallbacks, &cloneError)) {
      if (!cloneError && JS_IsExceptionPending(cx)) return false;
      JS_ClearPendingException(cx);
      return WebKit::Fail(cx, "Notification constructor: The object could not be cloned");
    }
  }
  JS::RootedObject actions(cx, JS::NewArrayObject(cx, 0)), prototype(cx);
  if (!actions || !JS_FreezeObject(cx, actions) ||
      !state.kit.ConstructProto(args, state.notification.prototype, &prototype)) return false;
  JS::RootedObject note(cx, state.kit.New(&kNotificationClass, prototype));
  if (!note || !state.events.Attach(note)) return false;
  const JS::Value values[] = {JS::StringValue(title), JS::StringValue(direction), lang.get(),
                              body.get(), tag.get(), icon.get(), interaction.get(), silent.get(),
                              stored.get(), JS::ObjectValue(*actions)};
  for (int slot = 0; slot < NoteFieldCount; ++slot) JS::SetReservedSlot(note, slot, values[slot]);
  for (int slot = NoteFieldCount; slot < NoteSlotCount; ++slot) JS::SetReservedSlot(note, slot, JS::NullValue());
  const bool granted = std::string(NotificationPermission(state)) == "granted";
  if (!QueueNotificationEvent(state, note, granted ? "show" : "error")) return false;
  args.rval().setObject(*note);
  return true;
}
}
