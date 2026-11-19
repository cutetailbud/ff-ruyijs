#include "execution_scheduling/state.h"
#include "execution_scheduling/idle.h"
#include "execution_scheduling/scheduler.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"

namespace ruyijs::execution_scheduling {
thread_local State* activeState = nullptr;
State::State(JSContext* context, JS::HandleObject global, Checkpoint drain)
    : cx(context), page(context, global), fulfilled(context),
      idleFactories(context), schedulerFactories(context), performanceObjects(context),
      checkpoint(drain), previous(activeState) {
  activeState = this;
}
State::~State() {
  UnbindClock();
  callbackById.clear();
  activeIdles.clear();
  while (callbacks) {
    auto next = std::move(callbacks->next);
    if (next) next->prev = nullptr;
    callbacks = std::move(next);
  }
  State** link = &activeState;
  while (*link && *link != this) link = &(*link)->previous;
  if (*link) *link = previous;
}
bool State::Accept(Status status) {
  if (status.code == ErrorCode::None) return true;
  if (status.code == ErrorCode::OutOfMemory) { JS_ReportOutOfMemory(cx); return false; }
  return Error(cx, status.message, status.code == ErrorCode::Range);
}
static const JSErrorFormatString* ErrorFormat(void* data, unsigned) {
  return static_cast<const JSErrorFormatString*>(data);
}
bool Error(JSContext* cx, const char* message, bool range) {
  JSErrorFormatString format{"RUYIJS_SCHEDULING", "{0}", 1,
                            int16_t(range ? JSEXN_RANGEERR : JSEXN_TYPEERR)};
  JS_ReportErrorNumberASCII(cx, ErrorFormat, &format, 0, message);
  return false;
}
State* Receiver(JSContext* cx, const JS::CallArgs& args) {
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (args.thisv().isNullOrUndefined()) return state;
  if (args.thisv().isObject() && state->Owns(&args.thisv().toObject())) return state;
  Error(cx, "Scheduling member called on incompatible global receiver");
  return nullptr;
}
JSObject* ReceiverGlobal(const JS::CallArgs& args) {
  return args.thisv().isNullOrUndefined() ? GlobalOf(&args.callee())
                                        : GlobalOf(&args.thisv().toObject());
}
bool State::Owns(JSObject* receiver) const {
  if (receiver == page.get()) return true;
  for (JSObject* installed : realms)
    if (installed == receiver) return true;
  return false;
}
void State::AddRealm(JS::HandleObject global) {
  if (Owns(global.get())) return;
  realms.push_back(global.get());
}
bool State::Define(JSNative native, const char* name, unsigned length, int32_t operation) {
  return DefineOn(native, name, length, operation, page, *helpers);
}
bool State::DefineOn(JSNative native, const char* name, unsigned length, int32_t operation,
                     JS::HandleObject target, PageHelpers& realmHelpers) {
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, native, length, 0, name));
  if (!fn) return false;
  JS::RootedObject object(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(this));
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(operation));
  return JS_DefineProperty(cx, target, name, object, JSPROP_ENUMERATE) &&
         realmHelpers.Member(object, GlobalInterface(native == Frame || native == ClearFrame), name, "call");
}
Realm::Realm(JSContext* cx, JS::HandleObject page, Checkpoint checkpoint)
    : state_(std::make_unique<State>(cx, page, checkpoint)) {}
Realm::~Realm() = default;
bool Realm::InstallTasks(JS::HandleObject page,PageHelpers& helpers,interaction::Realm& events){
 return InstallScheduler(*state_,page,helpers,events);
}
void Realm::PerformMicrotaskCheckpoint() { state_->CheckpointJobs(); }
bool Realm::PlaceWorkerMembers(JS::HandleObject globalScopePrototype) {
  auto& s = *state_;
  if (s.exposure != Exposure::Worker) return false;
  JSAutoRealm realm(s.cx, s.page);
  for (const char* name : {"setTimeout", "clearTimeout", "setInterval", "clearInterval", "queueMicrotask",
                           "performance", "scheduler"}) {
    JS::RootedString atom(s.cx, JS_AtomizeAndPinString(s.cx, name));
    if (!atom) return false;
    JS::RootedId id(s.cx, JS::PropertyKey::fromPinnedString(atom));
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> found(s.cx);
    if (!JS_GetOwnPropertyDescriptorById(s.cx, s.page, id, &found)) return false;
    if (found.isNothing()) continue;
    JS::Rooted<JS::PropertyDescriptor> descriptor(s.cx, *found);
    JS::ObjectOpResult removed;
    if (!JS_DefinePropertyById(s.cx, globalScopePrototype, id, descriptor) ||
        !JS_DeletePropertyById(s.cx, s.page, id, removed) || !removed.ok()) return false;
  }
  return true;
}
void Realm::SetApiMonitor(ApiMonitorPort port, void* context) {
  state_->apiMonitor = port;
  state_->apiMonitorContext = context;
}
bool Realm::Install(PageHelpers& helpers, interaction::Realm& events, Exposure exposure) {
  auto& s = *state_;
  s.exposure = exposure;
  if (!s.Accept(s.model.Initialize(SystemEpoch()))) return false;
  s.initialized = true;
  s.helpers = &helpers;
  s.BindClock();
  JSAutoRealm realm(s.cx, s.page);
  s.fulfilled = JS::NewPromiseObject(s.cx, nullptr);
  return s.fulfilled && JS::ResolvePromise(s.cx, s.fulfilled, JS::UndefinedHandleValue) &&
      s.Define(Timer, "setTimeout", 1) && s.Define(Timer, "setInterval", 1, 1) &&
      s.Define(ClearTimer, "clearTimeout", 0) && s.Define(ClearTimer, "clearInterval", 0, 1) &&
      s.Define(Microtask, "queueMicrotask", 1) && s.Define(Frame, "requestAnimationFrame", 1) &&
      s.Define(ClearFrame, "cancelAnimationFrame", 1) &&
      InstallPerformance(s, helpers, events) &&
      (exposure == Exposure::Worker || InstallIdle(s,s.page,helpers));
}
bool Realm::InstallInto(JSContext* cx, JS::HandleObject global, PageHelpers& helpers, interaction::Realm& events) {
  auto& s = *state_;
  s.AddRealm(global);
  JSAutoRealm realm(s.cx, global);
  return s.DefineOn(Timer, "setTimeout", 1, 0, global, helpers) &&
         s.DefineOn(Timer, "setInterval", 1, 1, global, helpers) &&
         s.DefineOn(ClearTimer, "clearTimeout", 0, 0, global, helpers) &&
         s.DefineOn(ClearTimer, "clearInterval", 0, 1, global, helpers) &&
         s.DefineOn(Microtask, "queueMicrotask", 1, 0, global, helpers) &&
         s.DefineOn(Frame, "requestAnimationFrame", 1, 0, global, helpers) &&
         s.DefineOn(ClearFrame, "cancelAnimationFrame", 1, 0, global, helpers) &&
         InstallPerformance(s, global, helpers, events) && InstallIdle(s,global,helpers);
}
void Realm::BeginRequest() { state_->microEnqueued = state_->microExecuted = 0; }
void Realm::Freeze() { state_->frozen = true; }
bool Realm::Configure(bool logical, double epoch) {
  auto& s = *state_;
  if (s.frozen) return Error(s.cx, "Scheduling configuration is frozen after the first page operation");
  if (!s.Accept(s.model.Configure(logical, logical ? epoch : SystemEpoch()))) return false;
  s.started = std::chrono::steady_clock::now();
  return true;
}
Snapshot Realm::Inspect() { state_->SyncClock(); return state_->model.Inspect(); }
double Realm::ClockSeconds() { return Inspect().now / 1000.0; }
void BeginPageOperation(JSContext* cx, JS::HandleObject page) {
  for (State* s = activeState; s; s = s->previous) {
    if (s->cx != cx || !s->Owns(page.get())) continue;
    s->frozen = true;
    s->microEnqueued = s->microExecuted = 0;
    s->SyncClock();
    return;
  }
}
}
