#include "web_workers/state.h"
#include "web_workers/clone.h"
#include "web_workers/error_event.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include <chrono>
#include <thread>

namespace ruyijs::web_workers {
namespace {
WorkerRecord* Record(JSObject* object) {
  return static_cast<WorkerRecord*>(JS::GetReservedSlot(object, 0).toPrivate());
}
}
State::~State() { Stop(); StopShared(*this); }
void State::Stop() {
  for (JSObject* object : active) Record(object)->Stop();
  active.clear();
  // SharedWorker processes are Context-scoped and must survive a navigation
  // (State::Stop runs on retirement), so they stop only when the origin State
  // is destroyed (shared.cpp's StopShared) or the process exits on its own.
}
void State::Prune() {
  for (size_t index = active.length(); index > 0; --index)
    if (Record(active[index - 1])->terminated) active.erase(active.begin() + index - 1);
  if (cursor >= active.length()) cursor = 0;
}
bool State::Schedule(double delay) {
  if (scheduled || (active.empty() && services.empty() && sharedWorkers.empty()) || !task) return true;
  if (!task(taskContext, pollTask, delay)) return false;
  scheduled = true; return true;
}
bool Poll(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  state.scheduled = false;
  JSAutoRealm realm(cx, state.kit.page);
  state.Prune();
  JS::RootedObject target(cx);
  std::string frame;
  // One message per task preserves a microtask checkpoint between deliveries.
  bool childRunning = false;
  const size_t count = state.active.length();
  for (size_t attempt = 0; attempt < count; ++attempt) {
    const size_t index = state.cursor++ % count;
    auto* record = Record(state.active[index]);
    if (record->terminated) continue;
    bool received = false;
    const auto status = record->process.Poll(&frame, &received);
    if (received) { target = state.active[index]; break; }
    if (status == PipeResult::Closed || status == PipeResult::Failed) record->Stop();
    else if (record->process.Running()) childRunning = true;
  }
  // The pipe poll does not wait. On a logical clock, advance() would otherwise
  // burn every 4 ms reschedule before the child process can start and reply,
  // so a Blob worker's postMessage never reaches the page. A short wall-clock
  // pause while the child is alive lets that reply land on a later poll.
  if (!target && childRunning)
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
  bool ok = true;
  if (target) {
    JS::RootedValue data(cx);
    if (!frame.empty() && frame[0] == kServiceRequest) {
      auto* record = Record(target);
      std::string reply;
      ok = record->service.owner && record->service.request &&
           record->service.request(record->service.owner.get(), std::string_view(frame).substr(1), &reply);
      if (ok) {
        reply.insert(reply.begin(), kServiceReply);
        ok = record->process.Queue(std::move(reply));
      }
      if (!ok) {
        record->Stop();
        if (!JS_IsExceptionPending(cx)) WebKit::Fail(cx, "Worker service failed");
      }
    } else if (!frame.empty() && frame[0] == kBroadcastFrame) {
      ok = RouteBroadcast(state, frame, Record(target));
    } else if (frame == std::string(1, '\x04')) ok = state.events->Fire(target, "error", false, false);
    else if (!frame.empty() && frame[0] == '\x03') {
      ok = DecodeMessage(cx, frame, &data, '\x03') && data.isObject();
      if (ok) {
        JS::RootedObject init(cx, &data.toObject());
        ok = FireError(cx, target, init, state.errorPrototype);
      }
    } else if (WorkerTokenEnabled() && frame.size() && frame[0] == '\x02') {
      auto* record = Record(target); WorkerMessageToken token;
      ok = DecodeMessageWithToken(cx, frame, &token, &data) && token.direction == 1 &&
           token.workerId == record->workerId && (record->lastIncoming = token, true) &&
           state.events->FireMessage(target, data, "", nullptr);
    } else ok = DecodeMessage(cx, frame, &data) && state.events->FireMessage(target, data, "", nullptr);
  }
  // Service worker processes share this poll task, one frame per task as well.
  bool delivered = false;
  if (ok && !target) ok = PollServices(state, &delivered);
  // Shared worker processes also take one frame per task from the same queue.
  if (ok && !target && !delivered) ok = PollShared(state, &delivered);
  state.Prune();
  const bool scheduled = state.Schedule(target || delivered ? 0 : 4);
  args.rval().setUndefined(); return ok && scheduled;
}
}
