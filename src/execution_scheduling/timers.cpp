#include "execution_scheduling/state.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/GCVector.h"
#include "js/Principals.h"
#include "js/String.h"
#include <new>

namespace ruyijs::execution_scheduling {
namespace {
// A string handler is dynamic code: as Firefox's CSPEvalChecker::CheckForWindow,
// ask the engine's one code-generation checker (the Content Security owner's
// hook, docs/owners/CONTENT_SECURITY.md) before the timer is registered.
bool StringHandlerAllowed(JSContext* cx, JS::HandleString code, bool* allowed) {
  *allowed = true;
  const JSSecurityCallbacks* callbacks = JS_GetSecurityCallbacks(cx);
  if (!callbacks || !callbacks->contentSecurityPolicyAllows) return true;
  JS::RootedVector<JSString*> parameters(cx);
  JS::RootedVector<JS::Value> arguments(cx);
  JS::RootedString body(cx);
  JS::RootedValue body_arg(cx);
  return callbacks->contentSecurityPolicyAllows(cx, JS::RuntimeCode::JS, code,
      JS::CompilationType::Undefined, parameters, body, arguments, body_arg, allowed);
}
}  // namespace
Callback* State::Find(uint64_t id) {
  auto it = callbackById.find(id);
  return it != callbackById.end() ? it->second : nullptr;
}
void State::Track(std::unique_ptr<Callback> callback) {
  Callback* raw = callback.get();
  callbackById[raw->id] = raw;
  if (raw->publicHandle) {
    auto& handles = Handles(raw->global);
    if (raw->kind == Kind::Frame) handles.activeFrames[raw->publicHandle] = raw->id;
    else if (raw->kind == Kind::Timeout || raw->kind == Kind::Interval)
      handles.activeTimers[raw->publicHandle] = raw->id;
  }
  if (raw->kind == Kind::Idle && raw->idleHandle && raw->arguments.length() > 0 &&
      raw->arguments[0].isObject()) {
    activeIdles[{&raw->arguments[0].toObject(), raw->idleHandle}] = raw->id;
  }
  raw->prev = nullptr;
  if (callbacks) callbacks->prev = raw;
  raw->next = std::move(callbacks);
  callbacks = std::move(callback);
}
void State::Remove(uint64_t id) {
  auto it = callbackById.find(id);
  if (it == callbackById.end() || it->second->running) return;
  Callback* cb = it->second;
  callbackById.erase(it);
  if (cb->publicHandle) {
    if (RealmHandles* handles = FindHandles(cb->global)) {
      auto& active = cb->kind == Kind::Frame ? handles->activeFrames : handles->activeTimers;
      auto hit = active.find(cb->publicHandle);
      if (hit != active.end() && hit->second == id) active.erase(hit);
    }
  }
  if (cb->kind == Kind::Idle && cb->idleHandle && cb->arguments.length() > 0 &&
      cb->arguments[0].isObject()) {
    auto hit = activeIdles.find({&cb->arguments[0].toObject(), cb->idleHandle});
    if (hit != activeIdles.end() && hit->second == id) activeIdles.erase(hit);
  }
  argumentSlots -= cb->arguments.length();
  sourceUnits -= cb->sourceUnits;
  std::unique_ptr<Callback> removed;
  if (cb->next) cb->next->prev = cb->prev;
  if (cb->prev) {
    removed = std::move(cb->prev->next);
    cb->prev->next = std::move(removed->next);
  } else {
    removed = std::move(callbacks);
    callbacks = std::move(removed->next);
  }
}
State::RealmHandles* State::FindHandles(JSObject* global) {
  for (auto& handles : publicHandles) if (handles.global == global) return &handles;
  return nullptr;
}
State::RealmHandles& State::Handles(JSObject* global) {
  if (RealmHandles* existing = FindHandles(global)) return *existing;
  publicHandles.push_back({global, {}, {}, {}, {}});
  return publicHandles.back();
}
void State::Cancel(uint32_t handle, bool frame, JSObject* global) {
  RealmHandles* handles = FindHandles(global);
  if (!handles) return;
  const auto& active = frame ? handles->activeFrames : handles->activeTimers;
  auto hit = active.find(handle);
  if (hit == active.end()) return;
  const uint64_t id = hit->second;
  // Public timer handles do not cancel posted messages or internal tasks.
  // Retirement uses Drop directly to cancel every task owned by that realm.
  model.Cancel(id, frame);
  if (!model.Exists(id)) Remove(id);
}
bool State::Register(std::unique_ptr<Callback> callback, int32_t delay,
                     JS::MutableHandleValue result) {
  // A retired document schedules nothing; the handle is never a live one.
  if (callback->global && Retired(callback->global)) { result.setInt32(0); return true; }
  SyncClock();
  if (callback->arguments.length() > 100000 - argumentSlots)
    return Error(cx, "Timer argument root limit exceeded", true);
  if (callback->sourceUnits > 16 * 1024 * 1024 - sourceUnits)
    return Error(cx, "Timer source limit exceeded", true);
  const bool frame = callback->kind == Kind::Frame;
  const bool publicId = frame || callback->kind == Kind::Timeout || callback->kind == Kind::Interval;
  domain::execution_scheduling::PublicHandleSequence* sequence = nullptr;
  if (publicId) {
    auto& handles = Handles(callback->global);
    sequence = frame ? &handles.frames : &handles.timers;
    const auto& active = frame ? handles.activeFrames : handles.activeTimers;
    callback->publicHandle = sequence->Reserve([&](uint32_t value) {
      return active.find(value) != active.end();
    });
    if (!callback->publicHandle) return Error(cx, "Public timer handles exhausted", true);
  }
  if (!Accept(model.Schedule(callback->kind, delay, &callback->id))) return false;
  if (sequence) sequence->Commit(callback->publicHandle);
  argumentSlots += callback->arguments.length();
  sourceUnits += callback->sourceUnits;
  result.setInt32(int32_t(publicId ? callback->publicHandle : 0));
  Track(std::move(callback));
  return true;
}
bool Timer(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  State* s = Receiver(cx, args);
  if (!s) return false;
  const bool interval = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32() != 0;
  s->Observe(s->GlobalInterface(), interval ? "setInterval" : "setTimeout", int(ApiAccess::Call));
  if (!argc) return Error(cx, "Timer requires a handler");
  JS::RootedValue handler(cx, args[0]);
  if (!handler.isObject() || !JS::IsCallable(&handler.toObject())) {
    JS::RootedString source(cx, JS::ToString(cx, handler));
    if (!source) return false;
    handler.setString(source);
    // A blocked string handler is not scheduled; the call returns 0.
    bool allowed = true;
    if (!StringHandlerAllowed(cx, source, &allowed)) return false;
    if (!allowed) { args.rval().setInt32(0); return true; }
  }
  int32_t delay = 0;
  if (argc > 1 && !JS::ToInt32(cx, args[1], &delay)) return false;
  const size_t slots = argc > 2 ? argc - 2 : 0;
  if (slots > 100000 - s->argumentSlots) return Error(cx, "Timer argument root limit exceeded", true);
  std::unique_ptr<Callback> callback(new (std::nothrow) Callback(cx));
  if (!callback) { JS_ReportOutOfMemory(cx); return false; }
  callback->kind = interval ? Kind::Interval : Kind::Timeout;
  callback->global = ReceiverGlobal(args);
  callback->handler = handler;
  callback->sourceUnits = handler.isString() ? JS_GetStringLength(handler.toString()) : 0;
  for (unsigned i = 2; i < argc; ++i) {
    if (!callback->arguments.append(args[i])) { JS_ReportOutOfMemory(cx); return false; }
  }
  return s->Register(std::move(callback), delay, args.rval());
}
bool ClearTimer(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  State* s = Receiver(cx, args);
  if (!s) return false;
  const bool interval = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32() != 0;
  s->Observe(s->GlobalInterface(), interval ? "clearInterval" : "clearTimeout", int(ApiAccess::Call));
  int32_t id = 0;
  if (argc && !JS::ToInt32(cx, args[0], &id)) return false;
  if (id > 0) s->Cancel(uint32_t(id), false, ReceiverGlobal(args));
  args.rval().setUndefined();
  return true;
}
}
