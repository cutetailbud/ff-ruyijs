#include "browser_environment/cross_origin_internal.h"
#include "browser_environment/state.h"
#include "browser_environment/string_list.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include "domain/browser_environment/url_value.h"

namespace ruyijs::browser_environment {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;
JSObject* Environment::StringList(JS::HandleObject values) {
  return NewStringListArray(*state_,values);
}
JSObject* Environment::location() const {
  if (!state_->installed) return nullptr;
  return state_->instances.get()[int32_t(Kind::Location)];
}
bool Environment::URL(JS::MutableHandleValue result) {
  return state_->Baseline() && state_->Read(Kind::Location, 0, ValueType::String, result);
}
void Environment::SetGraphicsPort(bool (*port)(void*, JS::HandleObject), void* context) {
  state_->graphicsPort = port;
  state_->graphicsPortContext = context;
  // A group configured before the owner existed is flushed now; a cleared port
  // simply leaves the buffered group in place.
  state_->PushWebGL();
}
void Environment::SetGraphicsDescribe(bool (*port)(void*, JS::MutableHandleValue),
                                      void* context) {
  state_->graphicsDescribe = port;
  state_->graphicsDescribeContext = context;
}
void Environment::SetGpuPort(bool (*port)(void*, JS::HandleObject), void* context) {
  state_->gpuPort = port;
  state_->gpuPortContext = context;
  // A group configured before the owner existed is flushed now; a cleared port
  // simply leaves the buffered group in place.
  state_->PushWebGpu();
}
void Environment::SetGpuDescribe(bool (*port)(void*, JS::MutableHandleValue),
                                 void* context) {
  state_->gpuDescribe = port;
  state_->gpuDescribeContext = context;
}
void Environment::SetPermissionsPort(bool (*port)(void*, JS::HandleObject), void* context) {
  state_->permissionsPort = port;
  state_->permissionsPortContext = context;
  state_->PushPermissions();
}
void Environment::SetGeolocationPort(bool (*port)(void*, JS::HandleObject), void* context) {
  state_->geolocationPort = port;
  state_->geolocationPortContext = context;
  state_->PushGeolocation();
}
void Environment::SetSpeechPort(bool (*port)(void*, JS::HandleObject), void* context) {
  state_->speechPort = port;
  state_->speechPortContext = context;
  state_->PushSpeech();
}
void Environment::SetMediaPort(bool (*port)(void*, JS::HandleObject), void* context) {
  state_->mediaPort = port;
  state_->mediaPortContext = context;
  state_->PushMedia();
}
void Environment::SetMediaDescribe(bool (*port)(void*, JS::MutableHandleValue),
                                   void* context) {
  state_->mediaDescribe = port;
  state_->mediaDescribeContext = context;
}
void Environment::SetPermissionsDescribe(bool (*port)(void*, JS::MutableHandleValue),
                                         void* context) {
  state_->permissionsDescribe = port;
  state_->permissionsDescribeContext = context;
}
JSObject* Environment::navigatorPrototype() const {
  return state_->navigatorPrototype.get();
}
void Environment::SetNavigatorMediaCapabilities(JS::HandleObject value) {
  JS::RootedObject navigator(state_->cx, state_->instances.get()[int32_t(Kind::Navigator)]);
  JS::SetReservedSlot(navigator, 2, JS::ObjectValue(*value));
}
bool Environment::ReadNavigatorMediaCapabilities(JSContext* cx, JS::HandleValue value,
                                                  JS::MutableHandleValue result) {
  JS::RootedObject navigator(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  // Navigator, or WorkerNavigator in a Worker realm (both keep it in slot 2).
  if (!navigator || (JS::GetClass(navigator) != ClassFor(Kind::Navigator) &&
                     JS::GetClass(navigator) != ClassFor(Kind::Navigator, true)))
    return Error(cx, "Navigator.mediaCapabilities called on an incompatible receiver");
  result.set(JS::GetReservedSlot(navigator, 2));
  return JS_WrapValue(cx, result);
}
void Environment::SetFrameCount(size_t (*port)(void*), void* context) {
  state_->frameCount = port;
  state_->frameCountContext = context;
}
void Environment::SetFrameWindowPort(bool (*port)(void*, size_t, JS::MutableHandleValue),
                                     void* context) {
  state_->frameWindow = port;
  state_->frameWindowContext = context;
}
bool Environment::BrowsingContextName(JSContext* cx, JS::HandleObject window,
                                      JS::MutableHandleString name) {
  name.set(nullptr);
  JSObject* real = RealWindow(window);
  State* state = real ? WindowState(real) : nullptr;
  if (!state || !state->name.get().isString()) return true;
  JS::RootedValue value(cx, state->name);
  if (!JS_WrapValue(cx, &value)) return false;
  name.set(value.toString());
  return true;
}
bool Environment::SetBrowsingContextName(JSContext* cx, JS::HandleObject window,
                                         JS::HandleString name) {
  JSObject* real = RealWindow(window);
  State* state = real ? WindowState(real) : nullptr;
  if (!state) return true;
  JSAutoRealm realm(cx, state->page);
  JS::RootedValue value(cx, JS::StringValue(name));
  if (!JS_WrapValue(cx, &value)) return false;
  state->name = value;
  return true;
}
void Environment::SetWindowNamePort(WindowNamePort port, void* context) {
  state_->windowNamePort = port;
  state_->windowNameContext = context;
}
void Environment::SetExceptionPort(bool (*port)(void*, const char*, const char*),
                                   void* context) {
  state_->throwException = port;
  state_->exceptionContext = context;
}
void Environment::SetPostTaskPort(bool (*port)(void*, JS::HandleObject), void* context) {
  state_->postTask = port;
  state_->postTaskContext = context;
}
bool Environment::PostTask(JS::HandleObject task) {
  return state_->postTask && state_->postTask(state_->postTaskContext, task);
}
void Environment::SetPostMessageTaskPort(bool (*port)(void*, JS::HandleObject, JS::HandleObject,
                                                      JS::HandleObject, uint64_t, uint32_t), void* context) {
  state_->postMessageTask = port; state_->postMessageTaskContext = context;
}
void Environment::SetScrollExtentPort(bool (*port)(void*, double*, double*), void* context) {
  state_->scrollExtent = port;
  state_->scrollExtentContext = context;
}
void Environment::SetScrollEventPort(bool (*port)(void*, const char*), void* context) {
  state_->scrollEvent = port;
  state_->scrollEventContext = context;
}
void Environment::SetViewportPort(bool (*port)(void*, double*, double*), void* context) {
  state_->viewportPort = port;
  state_->viewportPortContext = context;
}
void Environment::RetireMessages() { state_->messageActive = false; }
void Environment::SetMessagePort(
    bool (*port)(void*, JS::HandleObject, JS::HandleValue, const char*, JS::HandleObject, JS::HandleObject), void* context) {
  state_->deliverMessage = port;
  state_->deliverMessageContext = context;
}
void Environment::SetClonePort(ClonePort port, void* context) {
  state_->clonePort = port; state_->cloneContext = context;
}
void Environment::SetPopStatePort(bool (*port)(void*, JS::HandleObject, JS::HandleValue),
                                  void* context) {
  state_->deliverPopState = port;
  state_->deliverPopStateContext = context;
}
void Environment::SetNavigatePort(bool (*port)(void*, const char*, bool, bool), void* context) {
  state_->navigate = port;
  state_->navigateContext = context;
}
void Environment::SetHashChangePort(bool (*port)(void*, JS::HandleObject, const char*, const char*),
                                    void* context) {
  state_->deliverHashChange = port;
  state_->deliverHashChangeContext = context;
}
void Environment::SetEventPort(bool (*port)(void*, JS::HandleObject, const char*),
                               void* context) {
  state_->fireEvent = port;
  state_->fireEventContext = context;
}
bool Environment::Origin(JS::MutableHandleValue result) {
  return state_->Baseline() && state_->ReadOrigin(result);
}
void Environment::SetSendBeaconPort(BeaconPort port, void* context) {
  state_->sendBeacon = port;
  state_->sendBeaconContext = context;
}
void Environment::SetApiMonitor(ApiMonitorPort port, void* context) {
  state_->apiMonitor = port;
  state_->apiMonitorContext = context;
}
bool Environment::CommitNavigation(const std::string& url, bool replace) {
  // A JS-driven navigation commit: the same atomic owner path as a profile
  // location and the same URL validator, but it appends (or replaces) one
  // session-history entry, which is what separates a navigation from the
  // initial page load. Special schemes serialize an empty path as "/".
  if (state_->shared) return Error(state_->cx, "iframe navigation is not implemented");
  std::string href = url;
  size_t scheme = href.find("://");
  if (scheme != std::string::npos) {
    size_t path = href.find_first_of("/?#", scheme + 3);
    if (path == std::string::npos) href += '/';
    else if (href[path] != '/') href.insert(path, 1, '/');
  }
  if (!state_->Baseline() || !ValidURL(state_->cx, href)) return false;
  if (!state_->model.Navigate(href, replace)) {
    JS_ReportOutOfMemory(state_->cx);
    return false;
  }
  return true;
}

bool Environment::SetFrameLocation(const std::string& url) {
  // The runtime frame owner is the only caller, and only for a child context:
  // a main context has no shared parent. The URL is already absolute (the
  // frame owner resolved it against the parent document), so this reuses the
  // one URL parser instead of a second resolver.
  if (!state_->shared)
    return Error(state_->cx, "frame location requires an iframe browsing context");
  UrlValue parsed;
  // `src` reaches this owner already absolute and HTTP(S)-filtered; `srcdoc`
  // commits the special `about:srcdoc` URL, which has no authority, so the
  // check is "parsed", not "valid" (see docs/DEVIATIONS.md D8).
  if (!UrlValue::Resolve(url, std::string(), &parsed) || !parsed.parsed()) {
    JS_ReportErrorASCII(state_->cx, "frame source is not an absolute URL");
    return false;
  }
  state_->frameUrl = parsed.href();
  return true;
}

bool Environment::Navigate(const std::string& url) {
  // A replace navigation commit: it goes through the same atomic owner path as
  // a profile location, is not subject to the configuration freeze and pushes
  // no history entry. Special schemes serialize an empty path as "/".
  if (state_->shared) return Error(state_->cx, "iframe navigation is not implemented");
  std::string href = url;
  size_t scheme = href.find("://");
  if (scheme != std::string::npos) {
    size_t path = href.find_first_of("/?#", scheme + 3);
    if (path == std::string::npos) href += '/';
    else if (href[path] != '/') href.insert(path, 1, '/');
  }
  if (!state_->Baseline() || !ValidURL(state_->cx, href)) return false;
  Change change{Kind::Location, 0, {}};
  change.value.text = std::move(href);
  if (!state_->model.Configure({change})) {
    JS_ReportOutOfMemory(state_->cx);
    return false;
  }
  return true;
}
}
