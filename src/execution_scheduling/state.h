#ifndef RUYIJS_SCHEDULING_STATE_H
#define RUYIJS_SCHEDULING_STATE_H
#include "execution_scheduling/realm.h"
#include "domain/execution_scheduling/public_handles.h"
#include <chrono>
#include <functional>
#include <unordered_map>
#include <vector>
#include "jsapi.h"
#include "js/Date.h"
#include "js/GCVector.h"

namespace ruyijs::execution_scheduling {
struct IdleHandleKey {
  JSObject* factory = nullptr;
  uint32_t handle = 0;
  bool operator==(const IdleHandleKey& other) const noexcept {
    return factory == other.factory && handle == other.handle;
  }
};
struct IdleHandleKeyHash {
  size_t operator()(const IdleHandleKey& key) const noexcept {
    return std::hash<JSObject*>{}(key.factory) ^
           (std::hash<uint32_t>{}(key.handle) + 0x9e3779b9u);
  }
};
struct Callback {
  uint64_t id = 0;
  uint32_t replayFirings = 0;
  bool messageReplay = false;
  uint32_t messageSourceRealm = 0, messageTargetRealm = 0;
  uint64_t messageSequence = 0;
  uint32_t messageKind = 0;
  Kind kind = Kind::Timeout;
  bool running = false;
  uint32_t idleHandle = 0;
  uint32_t publicHandle = 0;
  int taskPriority = 1;
  bool dynamicPriority = false;
  size_t sourceUnits = 0;
  // The browsing realm whose global this callback belongs to; the callback runs
  // in it and is dropped when that realm's document is retired. Null for a
  // keepalive task, which outlives its document (a queued beacon). Raw pointer
  // for the same reason as `realms`: every global stays rooted by its kept-alive
  // BrowsingContext for the whole Context.
  JSObject* global = nullptr;
  JS::PersistentRootedValue handler;
  JS::PersistentRooted<JS::GCVector<JS::Value>> arguments;
  Callback* prev = nullptr;
  std::unique_ptr<Callback> next;
  explicit Callback(JSContext* cx) : handler(cx), arguments(cx, JS::GCVector<JS::Value>(cx)) {}
};
struct State {
  JSContext* cx;
  JS::PersistentRootedObject page, fulfilled;
  JS::PersistentRootedObjectVector idleFactories;
  JS::PersistentRootedObjectVector schedulerFactories,performanceObjects;
  uint64_t runningWebTask = 0;
  Checkpoint checkpoint;
  Model model;
  PageHelpers* helpers = nullptr;
  Exposure exposure = Exposure::Window;
  const char* GlobalInterface(bool dedicated = false) const {
    return exposure == Exposure::Window ? "Window" :
        dedicated ? "DedicatedWorkerGlobalScope" : "WorkerGlobalScope";
  }
  // Debug-session port, bound by the shared coordinator in debug mode. Reports
  // one access to an install-registered member of this owner; a null port
  // (non-debug mode) skips without touching engine state.
  ApiMonitorPort apiMonitor = nullptr;
  void* apiMonitorContext = nullptr;
  void Observe(const char* iface, const char* member, int type) const {
    if (apiMonitor) apiMonitor(apiMonitorContext, iface, member, type);
  }
  // Extra browsing realms (iframes) this Context-level surface was installed
  // into. The clock and task queues stay one owner; public handles are local
  // to each global. Raw pointers are safe because a child
  // BrowsingContext roots its global and outlives this State.
  std::vector<JSObject*> realms;
  struct RealmHandles {
    JSObject* global;
    domain::execution_scheduling::PublicHandleSequence timers, frames;
    std::unordered_map<uint32_t, uint64_t> activeTimers, activeFrames;
  };
  std::vector<RealmHandles> publicHandles;
  domain::execution_scheduling::NavigationReplayDocuments navigationReplay;
  bool ApplyNavigationReplay(NavigationTiming&, size_t* selected);
  // Globals whose document was unloaded (a navigated-away top-level document or
  // a removed iframe). Their queued work was dropped and they schedule no more.
  std::vector<JSObject*> retired;
  State* previous = nullptr;
  std::unique_ptr<Callback> callbacks;
  std::unordered_map<uint64_t, Callback*> callbackById;
  std::unordered_map<IdleHandleKey, uint64_t, IdleHandleKeyHash> activeIdles;
  size_t argumentSlots = 0, sourceUnits = 0;
  uint32_t microPending = 0, microEnqueued = 0, microExecuted = 0;
  bool frozen = false, initialized = false, clockBound = false;
  bool timerReplay = false;
  bool messageReplay = false;
  std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  ExternalWork external;
  bool ExternalPending() const { return external.pending && external.pending(external.context); }
  bool ExternalReady(uint32_t ms) { return external.wait && external.wait(external.context, ms); }
  // Runs one external completion as a task, then its microtask checkpoint.
  bool RunExternal();
  // Waits for an external completion: on the system clock until `until` at
  // the latest; on the logical clock while work is pending, without moving
  // logical time (a transport takes no logical time, as the former
  // synchronous transport did). True when one is ready.
  bool AwaitExternal(double until, double step);
  State(JSContext* context, JS::HandleObject global, Checkpoint drain);
  ~State();
  bool Accept(Status status);
  void SyncClock();
  double SystemElapsed() const;
  void WaitUntil(double time, double step);
  void BindClock();
  void UnbindClock();
  bool Define(JSNative native, const char* name, unsigned length, int32_t operation = 0);
  // The same installer body against another browsing realm: the function is
  // created in the current realm and registered with that realm's PageHelpers,
  // while every callback below still reads this one State.
  bool DefineOn(JSNative native, const char* name, unsigned length, int32_t operation,
                JS::HandleObject target, PageHelpers& helpers);
  bool Owns(JSObject* receiver) const;
  void AddRealm(JS::HandleObject global);
  Callback* Find(uint64_t id);
  void Track(std::unique_ptr<Callback> callback);
  void Remove(uint64_t id);
  void Cancel(uint32_t handle, bool frame, JSObject* global);
  RealmHandles* FindHandles(JSObject* global);
  RealmHandles& Handles(JSObject* global);
  bool Register(std::unique_ptr<Callback> callback, int32_t delay, JS::MutableHandleValue result);
  // Enqueues a zero-delay macrotask whose handler delivers one posted message.
  // The task belongs to the handler's realm unless `keepalive` is set.
  bool PostTask(JS::HandleObject handler, bool keepalive = false);
  bool PostMessageTask(JS::HandleObject handler, JS::HandleObject source,
                       JS::HandleObject target, uint64_t sequence, uint32_t kind);
  bool Retired(JSObject* global) const;
  // Cancels and removes every queued callback of `global` and refuses its new
  // work, as Firefox drops the tasks of a document that is no longer active.
  void Retire(JSObject* global);
  // Cancels one queued callback through the model path of its kind.
  void Drop(uint64_t id, Kind kind);
  void Invoke(const TaskSnapshot& task);
  void CheckpointJobs();
};
extern thread_local State* activeState;
// The global of the realm an object was created in, looking through a wrapper.
JSObject* GlobalOf(JSObject* object);
State* Receiver(JSContext* cx, const JS::CallArgs& args);
JSObject* ReceiverGlobal(const JS::CallArgs& args);
bool Timer(JSContext*, unsigned, JS::Value*);
bool ClearTimer(JSContext*, unsigned, JS::Value*);
bool Frame(JSContext*, unsigned, JS::Value*);
bool ClearFrame(JSContext*, unsigned, JS::Value*);
bool Microtask(JSContext*, unsigned, JS::Value*);
// One enqueue path shared by queueMicrotask and the microtask port above.
bool EnqueueMicrotask(State&, JS::HandleValue callback);
double SystemEpoch();
extern const JSClass kPerformanceClass;
bool InstallPerformance(State&, PageHelpers&, interaction::Realm&);
// Installs the Performance surface into `target` (the scheduling owner's own
// page, or an iframe realm) reading the same clock owner.
bool InstallPerformance(State&, JS::HandleObject target, PageHelpers&, interaction::Realm&);
}
#endif
