#ifndef RUYIJS_SCHEDULING_REALM_H
#define RUYIJS_SCHEDULING_REALM_H
#include <memory>
#include "execution_scheduling/model.h"
#include "execution_scheduling/resource_timing.h"
#include "execution_scheduling/navigation_timing.h"
#include "domain/execution_scheduling/navigation_replay.h"
#include "runtime/runtime.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::execution_scheduling {
struct State;
enum class Exposure { Window, Worker };
// Debug-session port, mirroring document_model::ApiMonitorPort. The owner
// reports accesses to members its installer registered, and the access kind
// travels as a plain int so Execution Scheduling keeps no observability
// dependency. Null outside debug mode, and every call site then skips.
enum class ApiAccess : int { Read = 0, Write = 1, Call = 2, Construct = 3 };
using ApiMonitorPort = void (*)(void* context, const char* iface,
                                const char* member, int type);
struct RunResult { Snapshot state; uint32_t executed = 0; bool exhausted = false; };
struct TimerSnapshot {
  uint64_t taskId = 0;
  uint32_t realmId = 0, publicHandle = 0, firings = 0;
  double deadline = 0;
  bool repeating = false, ready = false;
};
struct MessageSnapshot {
  uint64_t taskId = 0, sequence = 0;
  uint32_t sourceRealm = 0, targetRealm = 0, kind = 0;
  double deadline = 0;
  bool ready = false;
};
// Work that finishes outside the task queue - a background network transport -
// whose completions run as tasks at this owner's task boundaries
// (ARCHITECTURE.md, image fetch). `pending`: unfinished work exists; `wait`:
// blocks up to `ms` for a ready completion; `run`: runs ready completions until
// one of them ran page work, reporting whether one did.
struct ExternalWork {
  bool (*pending)(void*) = nullptr;
  bool (*wait)(void*, uint32_t ms) = nullptr;
  bool (*run)(void*) = nullptr;
  void* context = nullptr;
};
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, Checkpoint checkpoint);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install(PageHelpers& helpers, interaction::Realm& events, Exposure exposure = Exposure::Window);
  bool InstallTasks(JS::HandleObject page,PageHelpers& helpers,interaction::Realm& events);
  // Exposure::Worker: Install runs before the Worker owner creates the
  // WorkerGlobalScope interface, so this owner then places its
  // WorkerGlobalScope members (timers, queueMicrotask, performance, scheduler)
  // on that prototype, where Firefox's binding defines them. rAF stays on the
  // global as a DedicatedWorkerGlobalScope member.
  bool PlaceWorkerMembers(JS::HandleObject globalScopePrototype);
  // Installs this Context's scheduling surface (timers, rAF, queueMicrotask,
  // performance) into another browsing realm. There is still exactly one clock
  // owner, one macrotask queue and one microtask queue; only the realm-local
  // function objects and their Trace registration are new.
  bool InstallInto(JSContext* cx, JS::HandleObject global, PageHelpers& helpers, interaction::Realm& events);
  // Window facade port: one posted-message task on the shared macrotask queue.
  bool PostTask(JS::HandleObject handler);
  bool PostDelayedTask(JS::HandleObject handler,double delay);
  // A task that outlives its document: a beacon queued before unload is still
  // sent, as in Firefox. It runs in this owner's own realm.
  bool PostKeepaliveTask(JS::HandleObject handler);
  // Document retirement (navigated-away top-level document, removed iframe):
  // drops every queued timer, frame, idle callback and task of `global`, and
  // its later scheduling calls queue nothing.
  void Retire(JS::HandleObject global);
  bool Retired(JS::HandleObject global) const;
  // The one microtask queue this Context owns. The observer owner delivers
  // MutationObserver records through this port, on the same checkpoint as
  // queueMicrotask and promise jobs; it never opens a second queue.
  bool QueueMicrotask(JS::HandleObject callable);
  // One microtask checkpoint on this agent's job queue (exceptions reported).
  void PerformMicrotaskCheckpoint();
  void BeginRequest();
  void Freeze();
  bool Configure(bool logical, double epoch);
  Snapshot Inspect();
  bool ConfigureTimerReplay(bool enabled);
  bool ConfigureMessageReplay(bool enabled);
  bool VisitPendingMessages(bool (*visitor)(void*, const MessageSnapshot&), void* context);
  bool RunSelectedMessage(uint64_t taskId, uint32_t sourceRealm, uint32_t targetRealm,
                          uint64_t sequence, uint32_t kind, double at, RunResult* result);
  bool ValidateTimerRealmQuery();
  bool TimerRealmIdentity(JS::HandleObject target, uint32_t* identity);
  bool VisitPendingTimers(bool (*visitor)(void*, const TimerSnapshot&), void* context);
  bool ValidateTimerSelection(uint64_t taskId, uint32_t firings, double at);
  bool RunSelectedTimer(uint64_t taskId, uint32_t firings, double at, RunResult* result);
  bool PostMessageTask(JS::HandleObject handler, JS::HandleObject source,
                       JS::HandleObject target, uint64_t sequence, uint32_t kind = 0);
  // The one clock this Context owns, as seconds since its epoch, so a
  // consumer (Web Audio currentTime) never opens a second time source.
  double ClockSeconds();
  bool RecordResource(JS::HandleObject page,const ResourceTimingRecord&);
  // Records this document's navigation (docs/owners/NAVIGATION_TIMING.md): the
  // owner sets its time origin and publishes one PerformanceNavigationTiming
  // entry, so `performance.timeOrigin`, `timing.*` and the navigation entry all
  // read the same record. Called once per document, from the load path.
  bool RecordNavigation(JS::HandleObject page,const NavigationTiming& timing);
  bool ConfigureNavigationReplay(domain::execution_scheduling::NavigationReplayDocuments&&);
  const domain::execution_scheduling::NavigationReplayDocuments& NavigationReplayStatus() const;
  // One lifecycle milestone of `page`'s document (0 domInteractive, 1
  // DOMContentLoaded, 2 domComplete, 3 loadEventStart, 4 loadEventEnd), stamped
  // now and stored on that document's record. A document without a record gets
  // one whose origin is now, so a host-loaded document's offsets are its own.
  bool MarkNavigation(JS::HandleObject page,int milestone);
  // The page's own Performance owner, or null when none is installed there.
  JSObject* PerformanceOwner(JS::HandleObject page);
  // Milliseconds since `page`'s time origin, unrounded and constant within a task;
  // NaN when the page has no Performance owner (Web Animations timelines).
  double DocumentNow(JS::HandleObject page);
  // A Scheduling clock time (`clockMs` < 0: now, synced) relative to `page`'s
  // time origin, unclamped; NaN without a Performance owner.
  double OriginRelative(JS::HandleObject page, double clockMs);
  // A time-origin relative time as `page`'s content reads it (Firefox's
  // jittered 1 ms clamp with the document's seed); unchanged without an owner.
  double ReduceRelative(JS::HandleObject page, double ms);
  // Whether this page already has a navigation record.
  bool HasNavigation(JS::HandleObject page);
  bool ValidateAdvance(double ms, double step, uint32_t maxTasks);
  RunResult Advance(double ms, double step, uint32_t maxTasks);
  // A realm lifecycle may stop during the initial microtask checkpoint.
  RunResult Drain(uint32_t maxTasks, bool (*continueTasks)(void*) = nullptr, void* context = nullptr);
  // Debug-session port: reports accesses to the timer, frame, microtask and
  // Performance members this owner registered, for every realm it installed
  // into. Bound by the shared coordinator in debug mode and cleared before the
  // owner is destroyed.
  void SetApiMonitor(ApiMonitorPort port, void* context);
  // Bound by the shared coordinator to the Context's asynchronous HTTP owner
  // and cleared before that owner is destroyed.
  void SetExternalWork(const ExternalWork& work);
  // PageLoad's load-event delay: runs one external completion as a task,
  // waiting for it if needed; false once no external work is pending.
  bool RunExternalTask();
  // The load event's wait for images and style sheets (#31): as above, but on
  // the system clock the wait also ends when a queued task (a timer, a posted
  // message, a frame) falls due, so the caller can yield to it; the event loop
  // keeps turning while the load event is delayed (Firefox).
  enum class ExternalStep { Ran, Idle, TaskDue };
  ExternalStep RunExternalOrDueTask();
 private:
  std::unique_ptr<State> state_;
};
void BeginPageOperation(JSContext* cx, JS::HandleObject page);
bool Error(JSContext* cx, const char* message, bool range = false);
}
#endif
