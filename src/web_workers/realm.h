#ifndef RUYIJS_WEB_WORKERS_REALM_H
#define RUYIJS_WEB_WORKERS_REALM_H
#include <memory>
#include <string>
#include <cstdint>
#include <string_view>
#include "js/RootingAPI.h"
#include "web_workers/service.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_workers {
struct State;
using ObjectUrlPort = bool (*)(void*, const std::string&, std::string*, std::string*);
using TaskPort = bool (*)(void*, JS::HandleObject, double);
using LogicalClockPort = bool (*)(void*);
using EnvironmentPort = bool (*)(void*, std::string*);
using ServiceSink = bool (*)(void*, uint64_t, std::string_view);
// A BroadcastChannel frame from one of this realm's worker processes; `from`
// identifies that process for Broadcast's exclusion.
using BroadcastSink = bool (*)(void*, std::string_view frame, const void* from);
// Resolves a Worker script URL against the creating document's base URL and
// reports whether it is same-origin with the document. False: invalid URL.
using ScriptUrlPort = bool (*)(void*, const std::string& input, std::string* url, bool* sameOrigin);
// Window-side SharedWorker port bindings, set by the runtime to web_messaging.
using RemotePortFactory = JSObject* (*)(void* context, uint32_t id);
using RemotePortDeliver = bool (*)(void* context, uint32_t id, JS::HandleValue value);
using RemotePortClose = void (*)(void* context, uint32_t id);
class Realm final {
 public:
  Realm(JSContext*, JS::HandleObject, PageHelpers&, interaction::Realm&, Realm* parent = nullptr);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install(bool sharedWorker = false);
  void SetObjectUrlPort(ObjectUrlPort, void*);
  void SetEnvironmentPort(EnvironmentPort, void*);
  void SetScriptUrlPort(ScriptUrlPort, void*);
  void SetServiceFactory(ServiceFactory, void*);
  void SetTaskPort(TaskPort, void*);
  void SetLogicalClockPort(LogicalClockPort, void*);
  void SetMessagePortFactory(RemotePortFactory, void*);
  void SetMessagePortDeliver(RemotePortDeliver, void*);
  void SetMessagePortClose(RemotePortClose, void*);
  void Stop();
  // Service Worker owner port: a service-kind worker process for the resolved
  // script `url`; every frame it sends (empty when it exited) goes to `sink`.
  bool StartService(const std::string& url, ServiceSink sink, void* context, uint64_t* id);
  bool QueueService(uint64_t id, std::string frame);
  void StopService(uint64_t id);
  // BroadcastChannel transport (broadcast.cpp): queue `frame` to every worker
  // process of the Context's group except `except`; frames they send go to the sink.
  bool Broadcast(const std::string& frame, const void* except);
  void SetBroadcastSink(BroadcastSink sink, void* context);
  bool HasBroadcastTargets() const;  // any live worker process in the group
  // WebIDL `Worker` interface check for other owners (RTCRtpScriptTransform):
  // true for a Worker object of any realm, after unwrapping a cross-compartment
  // wrapper. Answers only identity; the Worker state stays in this owner.
  static bool IsWorker(JSObject* object);
  State* state() const { return state_.get(); }
 private:
  std::unique_ptr<State> state_;
};
}
#endif
