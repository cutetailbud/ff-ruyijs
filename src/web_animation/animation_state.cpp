// Animation time and state (web-animations-1 §4.4): current time, play state,
// the pending play/pause task, "update an animation's finished state" with its
// end timer, playback events and the realm's relevant-animation list.
#include "web_animation/animation_internal.h"
#include "interaction/realm.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/GCVector.h"
#include "js/PropertyAndElement.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace ruyijs::web_animation {
namespace {
JSObject* Target(const JS::CallArgs& args) {
  return &js::GetFunctionNativeReserved(&args.callee(), 1).toObject();
}
// The pending play or pause completes at the timeline's ready time.
bool PendingTask(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject a(cx, Target(args));
  // rval() is the callee's slot (vp[0]): compare against the callee before
  // writing it, or the task never matches and never runs.
  const bool current = JS::GetReservedSlot(a, kAPendingTask) == JS::ObjectValue(args.callee());
  args.rval().setUndefined();
  if (!current) return true;
  JS::SetReservedSlot(a, kAPendingTask, JS::NullValue());
  const auto ready = TimelineTime(TimelineOf(a));
  if (!ready) return true;  // An inactive timeline leaves the task pending, as in Firefox.
  if (Flag(a, kAPendingPlay)) {
    if (const auto hold = Opt(a, kAHold)) {
      ApplyPendingRate(a);
      SetOpt(a, kAStart, Rate(a) == 0 ? *ready : *ready - *hold / Rate(a));
      if (Rate(a) != 0) SetOpt(a, kAHold, std::nullopt);
    } else if (const auto start = Opt(a, kAStart); start && !JS::GetReservedSlot(a, kAPendingRate).isUndefined()) {
      const double match = (*ready - *start) * Rate(a);
      ApplyPendingRate(a);
      if (Rate(a) == 0) SetOpt(a, kAHold, match);
      SetOpt(a, kAStart, Rate(a) == 0 ? *ready : *ready - match / Rate(a));
    }
  } else {
    const auto start = Opt(a, kAStart);
    if (start && !Opt(a, kAHold)) SetOpt(a, kAHold, (*ready - *start) * Rate(a));
    ApplyPendingRate(a);
    SetOpt(a, kAStart, std::nullopt);
  }
  CancelTask(a);
  return ResolveReady(s, a) && UpdateFinished(s, a, false, false);
}
bool EndTimer(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject a(cx, Target(args));
  const bool current = JS::GetReservedSlot(a, kATimerTask) == JS::ObjectValue(args.callee());
  args.rval().setUndefined();
  if (!current) return true;
  JS::SetReservedSlot(a, kATimerTask, JS::NullValue());
  return UpdateFinished(s, a, false, false);
}
bool EventTask(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject pair(cx, Target(args));
  JS::RootedValue a(cx), event(cx);
  args.rval().setUndefined();
  if (!JS_GetElement(cx, pair, 0, &a) || !JS_GetElement(cx, pair, 1, &event)) return false;
  JS::RootedObject target(cx, &a.toObject()), object(cx, &event.toObject());
  return s.events.Dispatch(target, object);
}
}  // namespace

std::optional<double> CurrentTime(JSObject* a) {
  if (const auto hold = Opt(a, kAHold)) return hold;
  const auto start = Opt(a, kAStart);
  const auto now = TimelineTime(TimelineOf(a));
  if (!start || !now) return std::nullopt;
  return (*now - *start) * Rate(a);
}
model::ComputedTiming AnimationTiming(JSObject* a) {
  JSObject* effect = EffectOf(a);
  if (!effect) return {};
  return model::Compute(ReadTiming(effect), CurrentTime(a), Rate(a) < 0);
}
const char* PlayState(JSObject* a) {
  const auto ct = CurrentTime(a);
  if (!ct && !Opt(a, kAStart) && !Pending(a)) return "idle";
  if (Flag(a, kAPendingPause) || (!Opt(a, kAStart) && !Flag(a, kAPendingPlay))) return "paused";
  const double rate = EffectiveRate(a);
  if (ct && ((rate > 0 && *ct >= EndTime(a)) || (rate < 0 && *ct <= 0))) return "finished";
  return "running";
}

bool NewReady(State& s, JS::HandleObject a, bool resolved) {
  JS::RootedObject promise(s.cx, JS::NewPromiseObject(s.cx, nullptr));
  JS::RootedValue value(s.cx, JS::ObjectValue(*a));
  if (!promise || (resolved && !JS::ResolvePromise(s.cx, promise, value))) return false;
  JS::SetReservedSlot(a, kAReady, JS::ObjectValue(*promise));
  return true;
}
bool NewFinished(State& s, JS::HandleObject a) {
  JSObject* promise = JS::NewPromiseObject(s.cx, nullptr);
  if (!promise) return false;
  JS::SetReservedSlot(a, kAFinished, JS::ObjectValue(*promise));
  return true;
}
bool ResolveReady(State& s, JS::HandleObject a) {
  JS::RootedObject promise(s.cx, &JS::GetReservedSlot(a, kAReady).toObject());
  JS::RootedValue value(s.cx, JS::ObjectValue(*a));
  if (JS::GetPromiseState(promise) != JS::PromiseState::Pending) return true;
  return JS::ResolvePromise(s.cx, promise, value);
}
bool RejectSlot(State& s, JS::HandleObject a, uint32_t slot) {
  JS::RootedObject promise(s.cx, &JS::GetReservedSlot(a, slot).toObject());
  if (JS::GetPromiseState(promise) != JS::PromiseState::Pending) return true;
  if (DomFail(s, "AbortError", "The operation was aborted. ")) return false;
  JS::RootedValue error(s.cx);
  if (!JS_GetPendingException(s.cx, &error)) return false;
  JS_ClearPendingException(s.cx);
  return JS::RejectPromise(s.cx, promise, error) && JS::SetSettledPromiseIsHandled(s.cx, promise);
}
bool ScheduleTask(State& s, JS::HandleObject a) {
  JS::RootedObject task(s.cx, NewTask(s, PendingTask, a));
  if (!task) return false;
  JS::SetReservedSlot(a, kAPendingTask, JS::ObjectValue(*task));
  return !s.scheduling.post || s.scheduling.post(s.scheduling.context, task);
}
bool QueueEvent(State& s, JS::HandleObject a, const char* type, std::optional<double> currentTime) {
  JSContext* cx = s.cx;
  JS::RootedObject init(cx, JS_NewPlainObject(cx)), event(cx), pair(cx, JS::NewArrayObject(cx, 0));
  JS::RootedValue v(cx);
  const auto timeline = TimelineTime(TimelineOf(a));
  if (!init || !pair) return false;
  if (currentTime) v.setDouble(*currentTime); else v.setNull();
  if (!JS_DefineProperty(cx, init, "currentTime", v, JSPROP_ENUMERATE)) return false;
  if (timeline) v.setDouble(*timeline); else v.setNull();
  JS::RootedValueArray<2> values(cx);
  values[1].setObject(*init);
  if (!JS_DefineProperty(cx, init, "timelineTime", v, JSPROP_ENUMERATE) || !NewString(cx, type, values[0]) ||
      !JS::Construct(cx, s.playbackEvent, values, &event)) return false;
  v.setObject(*a);
  JS::RootedValue e(cx, JS::ObjectValue(*event));
  if (!JS_DefineElement(cx, pair, 0, v, 0) || !JS_DefineElement(cx, pair, 1, e, 0)) return false;
  JS::RootedObject task(cx, NewTask(s, EventTask, pair));
  return task && (!s.scheduling.post || s.scheduling.post(s.scheduling.context, task));
}

bool UpdateFinished(State& s, JS::HandleObject a, bool didSeek, bool sync) {
  std::optional<double> unconstrained;
  if (didSeek) unconstrained = CurrentTime(a);
  else if (const auto start = Opt(a, kAStart); const auto now = TimelineTime(TimelineOf(a)))
    if (start) unconstrained = (*now - *start) * Rate(a);
  const double rate = Rate(a), end = EndTime(a);
  const auto previous = Opt(a, kAPrevious);
  if (unconstrained && Opt(a, kAStart) && !Pending(a)) {
    if (rate > 0 && *unconstrained >= end)
      SetOpt(a, kAHold, didSeek ? *unconstrained : previous ? std::max(*previous, end) : end);
    else if (rate < 0 && *unconstrained <= 0)
      SetOpt(a, kAHold, didSeek ? *unconstrained : previous ? std::min(*previous, 0.0) : 0.0);
    else if (rate != 0 && TimelineTime(TimelineOf(a))) {
      if (const auto hold = Opt(a, kAHold); didSeek && hold)
        SetOpt(a, kAStart, *TimelineTime(TimelineOf(a)) - *hold / rate);
      SetOpt(a, kAHold, std::nullopt);
    }
  }
  SetOpt(a, kAPrevious, CurrentTime(a));
  const char* state = PlayState(a);
  JS::RootedObject finished(s.cx, &JS::GetReservedSlot(a, kAFinished).toObject());
  const bool settled = JS::GetPromiseState(finished) != JS::PromiseState::Pending;
  if (!std::strcmp(state, "finished") && !settled) {
    JS::RootedValue value(s.cx, JS::ObjectValue(*a));
    if (!JS::ResolvePromise(s.cx, finished, value) || !QueueEvent(s, a, "finish", CurrentTime(a))) return false;
  } else if (std::strcmp(state, "finished") && settled && !NewFinished(s, a)) {
    return false;
  }
  (void)sync;  // Finish notification is queued in both cases; the event is a task (recorded).
  if (std::strcmp(state, "idle") && !Track(s, a)) return false;
  // The end timer: the time at which a running animation reaches its boundary.
  JS::SetReservedSlot(a, kATimerTask, JS::NullValue());
  const auto ct = CurrentTime(a);
  if (!std::strcmp(state, "running") && !Pending(a) && ct && rate != 0 && s.scheduling.postDelayed) {
    const double delay = rate > 0 ? (end - *ct) / rate : -*ct / rate;
    if (std::isfinite(delay)) {
      JS::RootedObject timer(s.cx, NewTask(s, EndTimer, a));
      if (!timer) return false;
      JS::SetReservedSlot(a, kATimerTask, JS::ObjectValue(*timer));
      if (!s.scheduling.postDelayed(s.scheduling.context, timer, std::max(delay, 0.0))) return false;
    }
  }
  return true;
}

}  // namespace ruyijs::web_animation
