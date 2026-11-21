// SharedWorker process registry, poll and port routing (docs/owners/WORKER.md,
// "SharedWorker"). The Window-side constructor (shared.cpp) starts or reuses a
// process keyed by (url, name) in the origin State; this unit polls those
// processes, answers their service channel, and routes connect/message/close
// frames between the Window-side and worker-side MessagePorts.
#include "web_workers/state.h"
#include "web_workers/clone.h"
#include "web_workers/error_event.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_workers {
// web_messaging's remote relay for a Window-side SharedWorker port: encode the
// posted value and queue it to the process that owns this connection id.
bool RelaySharedWindow(void* context, uint32_t id, JS::HandleValue value, JS::HandleValue transfer) {
  auto* state = static_cast<State*>(context);
  State& owner = SharedOwner(*state);
  SharedWorkerProcess* process = nullptr;
  for (auto& entry : owner.sharedWorkers)
    for (auto& connection : entry->connections)
      if (connection.id == id) { process = entry.get(); break; }
  if (!process) {
    // A port transferred to or from a dedicated Worker: its worker's pipe.
    const auto found = owner.budget->ports.find(id);
    if (found == owner.budget->ports.end() || found->second->terminated) return true;
    WorkerRecord& record = *found->second;
    std::string frame;
    std::vector<uint32_t> bridged;
    if (!EncodeSharedMessage(state->cx, state->exceptionPrototype, id, value, transfer, &frame, &bridged))
      return false;
    record.AddPorts(bridged);
    return record.process.Queue(std::move(frame));
  }
  if (process->record.terminated) return true;  // refused/dropped
  std::string frame;
  if (!EncodeSharedMessage(state->cx, state->exceptionPrototype, id, value, transfer, &frame)) return false;
  return process->record.process.Queue(std::move(frame));
}

// web_messaging's remote close: a Window-side SharedWorker port closed, so tell
// the worker process to close the matching worker-side port.
void CloseSharedWindow(void* context, uint32_t id) {
  auto* state = static_cast<State*>(context);
  State& owner = SharedOwner(*state);
  for (auto& entry : owner.sharedWorkers)
    for (auto& connection : entry->connections)
      if (connection.id == id && !entry->record.terminated) {
        std::string frame;
        EncodeSharedPortFrame(id, kSharedClose, &frame);
        entry->record.process.Queue(std::move(frame));
        return;
      }
  const auto found = owner.budget->ports.find(id);
  if (found != owner.budget->ports.end() && !found->second->terminated) {
    std::string frame;
    EncodeSharedPortFrame(id, kSharedClose, &frame);
    found->second->process.Queue(std::move(frame));
  }
}

// A port frame from a dedicated Worker: a message for a port this realm holds
// (a remote port it received, or the bridge of one it transferred), or a close.
bool RouteDedicatedPort(State& state, WorkerRecord& record, std::string_view frame) {
  uint32_t id = 0;
  if (DecodeSharedPortFrame(frame, kSharedClose, &id)) {
    if (state.portClose) state.portClose(state.portCloseContext, id);
    return true;
  }
  JSContext* cx = state.cx;
  JS::RootedValue value(cx);
  std::vector<uint32_t> ids;
  PortSink sink{state.portFactory, state.portFactoryContext, nullptr, &ids};
  if (!DecodeSharedMessage(cx, frame, &id, &value, &sink)) return false;
  record.AddPorts(ids);
  return !state.portDeliver || state.portDeliver(state.portDeliverContext, id, value);
}

bool RouteShared(State& state, std::string_view frame) {
  uint32_t id = 0;
  if (DecodeSharedPortFrame(frame, kSharedClose, &id)) {
    for (auto& entry : state.sharedWorkers)
      for (auto& connection : entry->connections)
        if (connection.id == id && connection.realm && connection.realm->portClose)
          connection.realm->portClose(connection.realm->portCloseContext, id);
    return true;
  }
  if (frame.empty() || frame[0] != kSharedMessage) return true;
  JSContext* cx = state.cx;
  JS::RootedValue value(cx);
  if (!DecodeSharedMessage(cx, frame, &id, &value)) return false;
  for (auto& entry : state.sharedWorkers)
    for (auto& connection : entry->connections)
      if (connection.id == id && connection.realm && connection.realm->portDeliver)
        return connection.realm->portDeliver(connection.realm->portDeliverContext, id, value);
  return true;
}

bool PollShared(State& state, bool* delivered) {
  *delivered = false;
  JSContext* cx = state.cx;
  for (size_t i = 0; i < state.sharedWorkers.size(); ++i) {
    SharedWorkerProcess& entry = *state.sharedWorkers[(state.serviceCursor + i) % state.sharedWorkers.size()];
    if (entry.record.terminated) continue;
    std::string frame;
    bool received = false;
    const auto status = entry.record.process.Poll(&frame, &received);
    if (!received) {
      if (status != PipeResult::Closed && status != PipeResult::Failed) continue;
      entry.record.Stop();
      continue;
    }
    state.serviceCursor = (state.serviceCursor + i + 1) % state.sharedWorkers.size();
    *delivered = true;
    if (!frame.empty() && frame[0] == kServiceRequest) {
      std::string reply;
      if (!entry.record.service.owner || !entry.record.service.request ||
          !entry.record.service.request(entry.record.service.owner.get(), std::string_view(frame).substr(1), &reply)) {
        entry.record.Stop();
        if (!JS_IsExceptionPending(cx)) WebKit::Fail(cx, "Worker service failed");
        return false;
      }
      reply.insert(reply.begin(), kServiceReply);
      return entry.record.process.Queue(std::move(reply));
    }
    if (!frame.empty() && frame[0] == kBroadcastFrame) return RouteBroadcast(state, frame, &entry.record);
    if (frame == std::string(1, '\x04')) {
      for (auto& connection : entry.connections) {
        if (!connection.realm || !connection.object) continue;
        JS::RootedObject target(cx, connection.object);
        if (!connection.realm->events->Fire(target, "error", false, false)) return false;
      }
      return true;
    }
    if (!frame.empty() && frame[0] == '\x03') {
      JS::RootedValue data(cx);
      if (!DecodeMessage(cx, frame, &data, '\x03') || !data.isObject()) return false;
      JS::RootedObject init(cx, &data.toObject());
      for (auto& connection : entry.connections) {
        if (!connection.realm || !connection.object) continue;
        JSAutoRealm realm(cx, connection.realm->kit.page);
        JS::RootedObject target(cx, connection.object);
        JS::RootedObject wrapped(cx, init);
        if (!JS_WrapObject(cx, &wrapped) ||
            !FireError(cx, target, wrapped, connection.realm->errorPrototype)) return false;
      }
      return true;
    }
    return RouteShared(state, frame);
  }
  return true;
}

void StopShared(State& state) {
  if (state.sharedWorkers.empty()) return;
  for (auto& entry : state.sharedWorkers) entry->record.Stop();
  state.sharedWorkers.clear();
}
}  // namespace ruyijs::web_workers
