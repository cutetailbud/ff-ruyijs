// Posted-message macrotasks. A task is a zero-delay, one-shot macrotask whose
// handler is the Window facade's delivery trampoline. It reuses the timer
// callback table so queue order, the clock and the drain budget all stay in the
// one Scheduling owner; this file never learns what a MessageEvent is.
#include "execution_scheduling/state.h"
#include "js/RootingAPI.h"
#include <new>

namespace ruyijs::execution_scheduling {
bool State::PostTask(JS::HandleObject handler, bool keepalive) {
  if (!handler) return Error(cx, "Message task requires a handler");
  JS::RootedValue ignored(cx);
  std::unique_ptr<Callback> callback(new (std::nothrow) Callback(cx));
  if (!callback) { JS_ReportOutOfMemory(cx); return false; }
  callback->kind = Kind::Message;
  // A poster creates its trampoline in the realm the delivery belongs to.
  callback->global = keepalive ? nullptr : GlobalOf(handler);
  callback->handler = JS::ObjectValue(*handler);
  return Register(std::move(callback), 0, &ignored);
}
bool State::PostMessageTask(JS::HandleObject handler, JS::HandleObject source,
                            JS::HandleObject target, uint64_t sequence, uint32_t kind) {
  if (!handler || !source || !target) return Error(cx, "Message task identity is incomplete");
  std::unique_ptr<Callback> callback(new (std::nothrow) Callback(cx));
  if (!callback) { JS_ReportOutOfMemory(cx); return false; }
  callback->kind = Kind::Message; callback->messageReplay = true;
  callback->global = GlobalOf(target);
  auto realm = [&](JSObject* value) -> uint32_t {
    for (size_t i = 0; i < publicHandles.size(); ++i)
      if (publicHandles[i].global == GlobalOf(value)) return uint32_t(i + 1);
    Handles(GlobalOf(value));
    for (size_t i = 0; i < publicHandles.size(); ++i)
      if (publicHandles[i].global == GlobalOf(value)) return uint32_t(i + 1);
    return 0;
  };
  callback->messageSourceRealm = realm(source);
  callback->messageTargetRealm = realm(target);
  callback->messageSequence = sequence; callback->messageKind = kind;
  callback->handler = JS::ObjectValue(*handler);
  JS::RootedValue ignored(cx);
  return Register(std::move(callback), 0, &ignored);
}
bool Realm::PostTask(JS::HandleObject handler) { return state_->PostTask(handler); }
bool Realm::PostMessageTask(JS::HandleObject handler, JS::HandleObject source,
                            JS::HandleObject target, uint64_t sequence, uint32_t kind) {
  return state_->PostMessageTask(handler, source, target, sequence, kind);
}
bool Realm::PostKeepaliveTask(JS::HandleObject handler) { return state_->PostTask(handler, true); }
bool Realm::PostDelayedTask(JS::HandleObject handler,double delay){
 auto& s=*state_;if(!handler)return Error(s.cx,"Message task requires a handler");
 std::unique_ptr<Callback> callback(new(std::nothrow) Callback(s.cx));
 if(!callback){JS_ReportOutOfMemory(s.cx);return false;}
 callback->kind=Kind::Message;callback->global=GlobalOf(handler);
 if(s.Retired(callback->global))return true;
 callback->handler=JS::ObjectValue(*handler);s.SyncClock();
 if(!s.Accept(s.model.ScheduleDelayed(delay,&callback->id)))return false;
 s.Track(std::move(callback));return true;
}
}
