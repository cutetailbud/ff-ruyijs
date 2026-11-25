// The one production installer of the Permissions owner, and its Browser
// Environment "permissions" group port.
#include "web_permissions/internal.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Symbol.h"
#include <cstring>

namespace ruyijs::web_permissions {
namespace {
bool Navigator(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  args.rval().setObject(*state.instance);
  return JS_WrapValue(cx, args.rval());
}
bool Last(JSContext* cx, WebFace& face) {
  JS::RootedValue constructor(cx, JS::ObjectValue(*face.constructor));
  return JS_DeleteProperty(cx, face.prototype, "constructor") &&
         JS_DefineProperty(cx, face.prototype, "constructor", constructor, 0);
}
}
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, Realm* parent)
    : state_(std::make_unique<State>(cx, page, helpers, events)) {
  state_->table = parent ? parent->state_->table : std::make_shared<Table>();
}
Realm::~Realm() = default;
void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->taskPort = port;
  state_->taskContext = context;
}
void Realm::SetFramePolicyPort(FramePolicyPort port, void* context) {
  state_->framePolicy = port;
  state_->frameContext = context;
}
uint8_t EffectiveState(State& state, size_t index) {
  // kNames order. sPermissionsMap: geo, camera, microphone, screen-wake-lock,
  // loopback-network and local-network follow the Permissions Policy feature
  // of that name; desktop-notification (notifications and push) and
  // persistent-storage are ePersistDeniedCrossOrigin.
  static const char* const kFeatures[kNameCount] = {"geolocation", nullptr, nullptr, nullptr,
      nullptr, nullptr, "screen-wake-lock", "camera", "microphone", "loopback-network",
      "local-network"};
  static const bool kDeniedCrossOrigin[kNameCount] = {false, true, true, true, false, false,
      false, false, false, false, false};
  const uint8_t stored = state.table->states[index];
  bool crossOrigin = false, allowed = true;
  if (!state.framePolicy || (!kFeatures[index] && !kDeniedCrossOrigin[index])) return stored;
  if (!state.framePolicy(state.frameContext, kFeatures[index], &crossOrigin, &allowed)) {
    JS_ClearPendingException(state.cx);
    return stored;
  }
  if ((kDeniedCrossOrigin[index] && crossOrigin) || (kFeatures[index] && !allowed)) return 1;
  return stored;
}
bool Realm::Install(JS::HandleObject navigatorPrototype) {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject eventTarget(cx, s.events.prototype());
  if (!eventTarget || !navigatorPrototype ||
      !s.kit.Define("Permissions", &kPermissionsClass, nullptr, WebKit::Reject, &s.permissions) ||
      !s.kit.Method(s.permissions, "Permissions", "query", Query, 1, 0) ||
      !Last(cx, s.permissions) ||
      !s.kit.Define("PermissionStatus", &kStatusClass, eventTarget, WebKit::Reject, &s.status) ||
      !s.kit.Accessor(s.status, "PermissionStatus", "name", StatusGet, nullptr, 0) ||
      !s.kit.Accessor(s.status, "PermissionStatus", "state", StatusGet, nullptr, 1) ||
      !s.kit.Accessor(s.status, "PermissionStatus", "onchange", StatusGet, StatusHandlerSet, 2) ||
      !Last(cx, s.status) || !InstallNotification(s)) return false;
  s.instance = s.kit.New(&kPermissionsClass, s.permissions.prototype);
  JS::RootedObject getter(cx, s.kit.Function(Navigator, "get permissions", 0, 0)), setter(cx);
  if (!s.instance || !getter) return false;
  return JS_DefineProperty(cx, navigatorPrototype, "permissions", getter, setter,
                           JSPROP_ENUMERATE) &&
         s.kit.helpers->Accessor("Navigator", navigatorPrototype, "permissions") &&
         s.kit.helpers->Object("Permissions", s.instance);
}
bool Realm::Configure(JS::HandleObject group) {
  State& s = *state_;
  JSContext* cx = s.cx;
  for (size_t index = 0; index < kNameCount; ++index) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, group, kNames[index], &value)) return false;
    if (!value.isString()) continue;
    JS::RootedString text(cx, value.toString());
    for (uint8_t state = 0; state < 3; ++state) {
      bool same = false;
      if (!JS_StringEqualsAscii(cx, text, kStates[state], &same)) return false;
      if (same) s.table->states[index] = state;
    }
  }
  return true;
}
int Realm::StateOf(const char* name) const {
  for (size_t index = 0; index < kNameCount; ++index)
    if (!std::strcmp(kNames[index], name)) return EffectiveState(*state_, index);
  return -1;
}
bool Realm::Describe(JS::MutableHandleValue result) {
  State& s = *state_;
  JS::RootedObject object(s.cx, JS_NewPlainObject(s.cx));
  if (!object) return false;
  for (size_t index = 0; index < kNameCount; ++index) {
    JS::RootedString text(s.cx, JS_NewStringCopyZ(s.cx, kStates[s.table->states[index]]));
    if (!text || !JS_DefineProperty(s.cx, object, kNames[index], text, JSPROP_ENUMERATE))
      return false;
  }
  result.setObject(*object);
  return true;
}
}
