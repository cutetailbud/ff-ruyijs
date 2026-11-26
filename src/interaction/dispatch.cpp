#include "interaction/internal.h"
#include <cmath>
#include "interaction/close_watcher.h"
#include "runtime/exception_capture.h"
#include "js/CallAndConstruct.h"
#include "jsapi.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs::interaction {
void State::invoke(std::shared_ptr<Listener> listener, domain::DomEvent& value, uint64_t target) {
  JSAutoRealm realm(cx, page);
  Event* entry = event(value);
  JS::RootedObject callback(cx, listener->callback);
  JS::RootedValue function(cx, JS::ObjectValue(*callback));
  JSObject* endpoint = targetObject(target);
  if (!endpoint) { JS_ClearPendingException(cx); return; }
  JS::RootedValue receiver(cx, JS::ObjectValue(*endpoint));
  JS::RootedValue argument(cx, JS::ObjectValue(*entry->object));
  JS::RootedValue result(cx);
  bool ok = true;
  if (!JS::IsCallable(callback)) {
    receiver.setObject(*callback);
    ok = JS_GetProperty(cx, callback, "handleEvent", &function);
  }
  const JS::Value* previous = currentEvent;
  currentEvent = argument.address();
  if (ok) ok = JS::Call(cx, receiver, function, JS::HandleValueArray(argument), &result);
  if (!ok) {
    CaptureOperationFailure(cx);
    JS_ClearPendingException(cx);
  }
  // Firefox runs the callback's microtask checkpoint (CallSetup teardown)
  // before EventListenerManager restores window.event, so promise jobs queued
  // by the listener still observe this event.
  if (microtaskCheckpoint && !JS::DescribeScriptedCaller(nullptr, cx)) microtaskCheckpoint(microtaskContext);
  currentEvent = previous;
}
void Realm::SetMicrotaskCheckpointPort(void (*port)(void*), void* context) {
  state_->microtaskCheckpoint = port;
  state_->microtaskContext = context;
}
void Realm::SetGeometryPort(GeometryPort port, void* context) {
  state_->geometry = port;
  state_->geometryContext = context;
}
void Realm::SetEventClockPort(double (*now)(void*, double), double (*reduce)(void*, double),
                              void* context) {
  state_->eventClock = now; state_->eventReduce = reduce; state_->eventClockContext = context;
}
double State::EventTime(double clock_ms) const {
  if (eventClock) {
    const double value = eventClock(eventClockContext, clock_ms);
    if (std::isfinite(value)) return value;
  }
  return clock_ms >= 0 ? clock_ms
      : std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}
void Realm::SetBrandPort(bool (*port)(void*, int, JS::HandleValue, bool*), void* context) {
  state_->brandPort = port;
  state_->brandContext = context;
}
bool Realm::CurrentEvent(JS::MutableHandleValue out) {
  out.set(state_->currentEvent ? *state_->currentEvent : JS::UndefinedValue());
  return JS_WrapValue(state_->cx, out);
}
bool Dispatch(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject target(cx);
  State* state = nullptr;
  Target* entry = TargetReceiver(cx, args, &target, &state);
  if (!entry) return false;
  state->Observe("EventTarget", "dispatchEvent", int(ApiAccess::Call));
  // A same-origin Event from another realm may be dispatched here.
  State* creator = argc && args[0].isObject() ? Owner(cx, &args[0].toObject(), true) : nullptr;
  Event* event = creator ? creator->event(&args[0].toObject()) : nullptr;
  if (!event || !IsA(event->kind, Iface::Event))
    return Error(cx, "TypeError", "dispatchEvent requires an Event");
  if (event->value.dispatching() || !event->value.initialized()) {
    if (!state->exceptionReporter)
      return Error(cx, "TypeError", "Event exception reporter is not installed");
    return state->exceptionReporter(state->exceptionContext, "InvalidStateError",
                                    "Event is uninitialized or already being dispatched");
  }
  JS::RootedString type(cx, event->type);
  PreparedPath path(cx);
  if (!PreparePath(*state, target, *event, &path)) return false;
  bool result = Deliver(*event, path);
  args.rval().setBoolean(result);
  return true;
}
bool Realm::Dispatch(JS::HandleObject target, JS::HandleObject event) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  Event* entry = state.event(event);
  if (!entry || !IsA(entry->kind, Iface::Event))
    return Error(cx, "TypeError", "dispatchEvent requires an Event");
  JS::RootedString type(cx, entry->type);
  PreparedPath path(cx);
  if (!PreparePath(state, target, *entry, &path)) return false;
  Deliver(*entry, path);
  return true;
}
// Runtime-originated lifecycle events: created in the page realm with the
// native Event.prototype, marked trusted, and dispatched on the same path as
// dispatchEvent, so listeners, capture/bubble and the Window rules match.
bool Realm::Fire(JS::HandleObject target, const char* type, bool bubbles, bool cancelable, bool* allowed) {
  return FireTargetEvent(*state_,target,type,bubbles,cancelable,allowed);
}
bool FireTargetEvent(State& state,JS::HandleObject target,const char* type,bool bubbles,bool cancelable,bool* allowed) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  if (state.events.size() >= 100000) return Error(cx, "RangeError", "Event limit exceeded");
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  JS::RootedString text(cx);
  JS::RootedValue value(cx, name ? JS::StringValue(name) : JS::UndefinedValue());
  std::string key;  // Same UTF-16 listener key as the Event constructor.
  if (!name || !Text(cx, value, &text, &key)) return false;
  JS::RootedObject prototype(cx, state.eventPrototype);
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kEventClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, kEventKind,
                      JS::Int32Value(static_cast<int32_t>(Kind::Event)));
  if (!InstallTrusted(state, object)) return false;
  auto event = std::make_unique<Event>(cx, object, text, std::move(key), bubbles, cancelable);
  event->value.mark_trusted();
  event->value.set_time_stamp(state.EventTime());
  Event& entry = *event;
  state.events.push_back(std::move(event));
  PreparedPath path(cx);
  if (!PreparePath(state, target, entry, &path)) return false;
  bool result = Deliver(entry, path);
  if (allowed) *allowed = result;
  return true;
}
}
