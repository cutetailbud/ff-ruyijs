#include "interaction/internal.h"
#include "interaction/close_watcher.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"


namespace ruyijs::interaction {
thread_local State* activeState = nullptr;
// Target ids are unique across every realm on this thread, so a same-origin
// event dispatched into another realm resolves its target unambiguously.
static thread_local uint64_t nextTarget = 0;
// Count of realms on this thread whose debug port is bound. Only the main
// page binds one, so this stays 0 for every non-debug Context.
static thread_local unsigned boundMonitors = 0;
bool MonitorBound() { return boundMonitors != 0; }
const JSClass kEventClass = {"Event", JSCLASS_HAS_RESERVED_SLOTS(kEventSlotCount)};
const JSClass kTargetClass = {"EventTarget", 0};
State::State(JSContext* context, JS::HandleObject global)
    : cx(context), previous(activeState), page(context, global),
      targetPrototype(context), eventPrototype(context), trustedGetter(context),
      speechTrustedGetter(context), speechErrorTrustedGetter(context),
      pictureInPictureTrustedGetter(context),
      navigateTrustedGetter(context),
      mediaEncryptedTrustedGetter(context), mediaKeyTrustedGetter(context),
      mediaStreamTrustedGetter(context), mediaTrackTrustedGetter(context),
      rtcToneTrustedGetter(context), rtcErrorTrustedGetter(context), rtcIceErrorTrustedGetter(context),
      audioProcessingTrustedGetter(context), fontLoadTrustedGetter(context), taskPriorityTrustedGetter(context),
      mousePrototype(context), pointerPrototype(context), legacyPrototypes(context),
      touchPrototype(context), touchListPrototype(context),
      touchDescriptor(context), touchListDescriptor(context), touchEventDescriptor(context),
      transferPrototype(context), transferListPrototype(context), transferItemPrototype(context),
      deviceAccelerationPrototype(context), deviceRotationRatePrototype(context) {
  activeState = this;
}
State::~State() {
  ClearCloseManager(closeManager);
  DetachCloseWatchers(*this);
  State** link = &activeState;
  while (*link && *link != this) link = &(*link)->previous;
  if (*link) *link = previous;
}
Target* State::target(JSObject* object) {
  if (!object) return nullptr;
  auto found = targetByObject.find(object);
  if (found == targetByObject.end()) return nullptr;
  Target* value = found->second;
  if (!value->released && value->live() == object) return value;
  targetByObject.erase(found);
  // The address was reused, or a trace moved an owned object. The vector is
  // the record of what is still live.
  for (auto& item : targets)
    if (!item->released && item->live() == object) {
      targetByObject.emplace(object, item.get());
      return item.get();
    }
  return nullptr;
}
Event* State::event(JSObject* object) {
  if (!object) return nullptr;
  for (auto& value : events) if (value->object == object) return value.get();
  return nullptr;
}
Event* State::event(domain::DomEvent& event) {
  for (auto& value : events) if (&value->value == &event) return value.get();
  for (State* owner = activeState; owner; owner = owner->previous)
    if (owner != this)
      for (auto& value : owner->events) if (&value->value == &event) return value.get();
  return nullptr;
}
JSObject* State::targetObject(uint64_t id) {
  for (auto& value : targets)
    if (!value->released && value->aggregate.id() == id) return WeakLive(value->live());
  for (State* owner = activeState; owner; owner = owner->previous)
    if (owner != this)
      for (auto& value : owner->targets)
        if (!value->released && value->aggregate.id() == id) return WeakLive(value->live());
  return nullptr;
}
// Live targets per realm: Screen, Window, standalone EventTargets and every
// wrapped node. Matches the Document Model's 100000 live-node cap; a framework
// mount of a ~13000-node page wraps every node it creates.
constexpr size_t kTargetLimit = 100000;
bool State::attach(JS::HandleObject object, bool owns, void** record) {
  if (Target* found = target(object)) {
    if (record) *record = found;
    return true;
  }
  // Borrowed records of collected node wrappers do not consume the shared
  // quota; only targets that are still alive do.
  prune();
  if (targets.size() >= kTargetLimit && reclaim) {
    // The quota is about to fail, so ask the owner for the same real
    // collection the node quota runs: the finalizers mark every collected
    // wrapper's borrowed record released and prune() then drops them. Without
    // this the quota would fail before the node quota ever ran that
    // collection, and it would bound created targets instead of live ones.
    reclaim(reclaimContext);
    prune();
  }
  if (targets.size() >= kTargetLimit) return Error(cx, "RangeError", "EventTarget limit exceeded");
  auto value = std::make_unique<Target>(cx, object, ++nextTarget, owns);
  targetMap.emplace(value->aggregate.id(), &value->aggregate);
  targetByObject[object] = value.get();
  if (record) *record = value.get();
  targets.push_back(std::move(value));
  return true;
}
void State::release(void* record) {
  if (!record) return;
  static_cast<Target*>(record)->released = true;
  anyReleased = true;
}
void State::prune() {
  if (!anyReleased) return;
  anyReleased = false;
  for (auto it = targets.begin(); it != targets.end();) {
    if ((*it)->released) {
      Target* raw = it->get();
      targetMap.erase(raw->aggregate.id());
      for (auto mapped = targetByObject.begin(); mapped != targetByObject.end();)
        if (mapped->second == raw) mapped = targetByObject.erase(mapped);
        else ++mapped;
      it = targets.erase(it);
    } else {
      ++it;
    }
  }
  // A listener whose target is gone must be dropped, not delivered to a stale
  // record; this is the same rule cleanup() applies to a removed listener.
  cleanup();
}
State* RealmState(JSContext* cx) {
  for (State* value = activeState; value; value = value->previous)
    if (value->cx == cx && value->page == JS::CurrentGlobalOrNull(cx)) return value;
  return nullptr;
}
State* Owner(JSContext* cx, JSObject* object, bool event) {
  for (State* value = activeState; value; value = value->previous)
    if (value->cx == cx && (event ? value->event(object) != nullptr : value->target(object) != nullptr))
      return value;
  return nullptr;
}
Realm::Realm(JSContext* cx, JS::HandleObject page) : state_(std::make_unique<State>(cx, page)) {}
Realm::~Realm() = default;
bool Realm::Install(PageHelpers& helpers) {
  JSAutoRealm realm(state_->cx, state_->page);
  state_->helpers = &helpers;
  return InstallEvent(*state_) && InstallTarget(*state_);
}
bool Realm::InstallDerived(bool touch_enabled) {
  if (!state_->helpers) return Error(state_->cx, "Error", "Interaction is not installed");
  JSAutoRealm realm(state_->cx, state_->page);
  if (!InstallDerivedEvents(*state_, touch_enabled, Exposure::Window) || !InstallCloseWatcher(*state_)) return false;
  JS::RootedObject page(state_->cx, state_->page);
  for (const char* name : {"MIDIConnectionEvent", "MIDIMessageEvent", "CookieChangeEvent",
                           "DocumentPictureInPictureEvent"})
    if (!state_->secure.Add(page, name)) return false;
  return true;
}
bool Realm::ConfigureExposure() {
  JSAutoRealm realm(state_->cx, state_->page);
  JS::RootedObject page(state_->cx, state_->page);
  JS::RootedValue secure(state_->cx);
  return JS_GetProperty(state_->cx, page, "isSecureContext", &secure) && state_->secure.Apply(secure.isTrue());
}
bool Realm::InstallWorkerDerived(bool service, bool dedicated) {
  if (!state_->helpers) return Error(state_->cx, "Error", "Interaction is not installed");
  JSAutoRealm realm(state_->cx, state_->page);
  return InstallDerivedEvents(*state_, false, service ? Exposure::ServiceWorker :
                              dedicated ? Exposure::DedicatedWorker : Exposure::Worker);
}
JSObject* Realm::prototype() const { return state_->targetPrototype; }
void Realm::SetFileListPort(bool (*port)(void*, JS::HandleObject, JS::MutableHandleObject), void* context) {
  state_->fileList = port;
  state_->fileListContext = context;
}
bool Realm::Attach(JS::HandleObject object) { return state_->attach(object, true, nullptr); }
bool Realm::AttachBorrowed(JS::HandleObject object, void** record) {
  return state_->attach(object, false, record);
}
void Realm::ReleaseBorrowed(void* record) { state_->release(record); }
void Realm::RefreshBorrowed(void* record, JSObject* object) {
  if (record) static_cast<Target*>(record)->object = object;
}
void Realm::SetPathResolver(PathResolver callback, RetargetResolver retarget, void* context) {
  state_->pathResolver = callback;
  state_->retargetResolver = retarget;
  state_->pathContext = context;
}
void Realm::SetExceptionReporter(ExceptionReporter callback, void* context) {
  state_->exceptionReporter = callback;
  state_->exceptionContext = context;
}
void Realm::SetStorageBrand(StorageBrand callback) { state_->storageBrand = callback; }
void Realm::SetReclaimPort(ReclaimPort callback, void* context) {
  state_->reclaim = callback;
  state_->reclaimContext = context;
}
void Realm::SetApiMonitor(ApiMonitorPort callback, void* context) {
  const bool bound = state_->apiMonitor != nullptr;
  const bool next = callback != nullptr;
  if (bound != next) {
    if (next) ++boundMonitors;
    else --boundMonitors;
  }
  state_->apiMonitor = callback;
  state_->apiMonitorContext = context;
}
}
