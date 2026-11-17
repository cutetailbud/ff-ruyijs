#ifndef RUYIJS_WEB_WORKERS_STATE_H
#define RUYIJS_WEB_WORKERS_STATE_H
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "runtime/web_kit.h"
#include "js/Value.h"
#include "web_workers/process.h"
#include "web_workers/clone.h"
#include "web_workers/service.h"

namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_workers {
struct State;
struct Budget {
  static constexpr size_t kLimit = 64;
  size_t active = 0;
  // Every web_workers realm of the Context: one BroadcastChannel group (broadcast.cpp).
  std::vector<State*> group;
  // SharedWorker port ids are unique across the Context (not per process), so
  // a frame's id routes to exactly one Window-side port (shared.cpp).
  uint32_t nextSharedPort = 0;
};
struct WorkerRecord {
  WorkerRecord(State* value, std::shared_ptr<Budget> capacity)
      : owner(value), budget(std::move(capacity)) { ++budget->active; }
  ~WorkerRecord() { Stop(); }
  State* owner;
  std::shared_ptr<Budget> budget;
  Process process;
  Service service;
  bool terminated = false;
  uint64_t workerId = 0, parentSequence = 0, childSequence = 0;
  WorkerMessageToken lastIncoming;
  // A reply may be on its way: the process just started or the page posted to
  // it, and no frame has come back since. On a logical clock poll.cpp waits up
  // to `patience` ms of wall time for such a worker (`waited` so far); a worker
  // that let it run out gets a short one until it answers again.
  static constexpr double kPatienceMs = 5000, kSilentPatienceMs = 250;
  bool awaiting = true;
  double waited = 0, patience = kPatienceMs;
  void Stop() {
    if (!terminated) { terminated = true; --budget->active; }
    process.Stop();
    service = {};
  }
};
// A service worker process: a WorkerRecord without a `Worker` object whose
// frames go to the Service Worker owner's sink (service_process.cpp).
using ServiceSink = bool (*)(void*, uint64_t, std::string_view);
struct ServiceProcess {
  ServiceProcess(State* owner, std::shared_ptr<Budget> budget) : record(owner, std::move(budget)) {}
  WorkerRecord record;
  ServiceSink sink = nullptr;
  void* context = nullptr;
  uint64_t id = 0;
};
using ObjectUrlPort = bool (*)(void*, const std::string&, std::string*, std::string*);
using BroadcastSink = bool (*)(void*, std::string_view, const void*);
// Window-side SharedWorker port bindings (shared.cpp): the runtime binds these
// to web_messaging's remote ports. The factory creates the SharedWorker.port
// MessagePort; the deliver/close ports receive worker messages for a port id.
using RemotePortFactory = JSObject* (*)(void* context, uint32_t id);
using RemotePortDeliver = bool (*)(void* context, uint32_t id, JS::HandleValue value);
using RemotePortClose = void (*)(void* context, uint32_t id);
inline constexpr char kBroadcastFrame = '\x07';
using TaskPort = bool (*)(void*, JS::HandleObject, double);
// True when the owning document runs on the logical clock (configure_clock).
using LogicalClockPort = bool (*)(void*);
using EnvironmentPort = bool (*)(void*, std::string*);
// Resolves a Worker script URL against the creating document's base URL and
// reports whether it is same-origin with the document. False: invalid URL.
using ScriptUrlPort = bool (*)(void*, const std::string& input, std::string* url, bool* sameOrigin);
// One SharedWorker connection: a Window-side MessagePort id whose worker-side
// peer the process opened on a connect frame. `realm` is the State whose
// messaging owns the port; `object` is the realm's SharedWorker object, kept
// alive in State::sharedObjects, so its `error` events fire in the right realm.
struct SharedConnection {
  uint32_t id = 0;
  State* realm = nullptr;
  JSObject* object = nullptr;
};
// A Context's one process for a (url, name) SharedWorker key. It lives in the
// origin State's registry, so it survives document navigation and is shared by
// every frame of the Context (docs/owners/WORKER.md, "Shared workers").
struct SharedWorkerProcess {
  SharedWorkerProcess(State* owner, std::shared_ptr<Budget> capacity)
      : record(owner, std::move(capacity)) {}
  std::string url;
  std::u16string name;
  WorkerRecord record;
  std::vector<SharedConnection> connections;
};
// The Window-side SharedWorker object's own state (shared.cpp).
struct SharedWorkerRecord {
  SharedWorkerRecord(State* value) : owner(value) {}
  State* owner;
  uint32_t id = 0;
};
struct State {
  JSContext* cx;
  WebKit kit;
  interaction::Realm* events = nullptr;
  ObjectUrlPort objectUrl = nullptr;
  void* objectUrlContext = nullptr;
  EnvironmentPort environment = nullptr;
  void* environmentContext = nullptr;
  ScriptUrlPort scriptUrl = nullptr;
  void* scriptUrlContext = nullptr;
  ServiceFactory serviceFactory = nullptr;
  void* serviceContext = nullptr;
  WebFace worker, sharedWorker;
  JS::PersistentRootedObject exceptionPrototype, errorPrototype, handlerSource, pollTask;
  JS::PersistentRootedObjectVector active, sharedObjects;
  TaskPort task = nullptr;
  void* taskContext = nullptr;
  LogicalClockPort logicalClock = nullptr;
  void* logicalClockContext = nullptr;
  bool scheduled = false;
  size_t cursor = 0;
  std::shared_ptr<Budget> budget;
  std::vector<std::unique_ptr<ServiceProcess>> services;
  // SharedWorker registry (origin State only, Context-wide) and port bindings.
  std::vector<std::unique_ptr<SharedWorkerProcess>> sharedWorkers;
  RemotePortFactory portFactory = nullptr;
  void* portFactoryContext = nullptr;
  RemotePortDeliver portDeliver = nullptr;
  void* portDeliverContext = nullptr;
  RemotePortClose portClose = nullptr;
  void* portCloseContext = nullptr;
  BroadcastSink broadcastSink = nullptr;
  void* broadcastContext = nullptr;
  uint64_t nextService = 0;
  size_t serviceCursor = 0;
  State(JSContext* context, JS::HandleObject page, PageHelpers& helpers)
      : cx(context), kit(context, page, helpers, this), worker(context), sharedWorker(context),
        exceptionPrototype(context), errorPrototype(context), handlerSource(context), pollTask(context), active(context),
        sharedObjects(context) {}
  ~State();
  void Prune();
  void Stop();
  bool Schedule(double delay = 0);
};
extern const JSClass kWorkerClass, kSharedWorkerClass;
WorkerRecord* Receiver(JSContext*, const JS::CallArgs&, JS::MutableHandleObject);
bool Poll(JSContext*, unsigned, JS::Value*);
bool PollServices(State&, bool* delivered);
bool RouteBroadcast(State&, std::string_view frame, const void* from);
void JoinGroup(State&);
void LeaveGroup(State&);
std::wstring WorkerExecutable();
bool Install(State&, interaction::Realm&, bool sharedWorker = false);
bool WorkerConstructor(JSContext*, unsigned, JS::Value*);
bool WorkerTerminate(JSContext*, unsigned, JS::Value*);
bool WorkerPostMessage(JSContext*, unsigned, JS::Value*);
// SharedWorker (shared.cpp): the Window-side constructor, `port` accessor and
// the Context-wide process registry plus its poll and relay.
bool SharedWorkerConstructor(JSContext*, unsigned, JS::Value*);
bool SharedWorkerPort(JSContext*, unsigned, JS::Value*);
State& SharedOwner(State&);
bool RelaySharedWindow(void* context, uint32_t id, JS::HandleValue value, JS::HandleValue transfer);
void CloseSharedWindow(void* context, uint32_t id);
bool PollShared(State&, bool* delivered);
bool RouteShared(State&, std::string_view frame);
void StopShared(State&);
}
#endif
