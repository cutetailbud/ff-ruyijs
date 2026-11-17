#include "web_workers/realm.h"
#include "web_workers/state.h"
#include "runtime/page_helpers.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_workers {
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, Realm* parent)
    : state_(std::make_unique<State>(cx, page, helpers)) {
  if (state_) {
    state_->events = &events;
    state_->budget = parent ? parent->state_->budget : std::make_shared<Budget>();
    JoinGroup(*state_);
  }
}
Realm::~Realm() { if (state_) LeaveGroup(*state_); }
bool Realm::Install(bool sharedWorker) {
  if (!state_ || !state_->events) return false;
  JSAutoRealm realm(state_->cx, state_->kit.page);
  return web_workers::Install(*state_, *state_->events, sharedWorker);
}
void Realm::SetScriptUrlPort(ScriptUrlPort port, void* context) {
  if (!state_) return;
  state_->scriptUrl = port;
  state_->scriptUrlContext = context;
}
void Realm::SetObjectUrlPort(ObjectUrlPort port, void* context) {
  if (!state_) return;
  state_->objectUrl = port;
  state_->objectUrlContext = context;
}
void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->task = port;
  state_->taskContext = context;
}
void Realm::SetLogicalClockPort(LogicalClockPort port, void* context) {
  state_->logicalClock = port;
  state_->logicalClockContext = context;
}
void Realm::SetEnvironmentPort(EnvironmentPort port, void* context) {
  state_->environment = port;
  state_->environmentContext = context;
}
void Realm::SetMessagePortFactory(RemotePortFactory port, void* context) {
  state_->portFactory = port;
  state_->portFactoryContext = context;
}
void Realm::SetMessagePortDeliver(RemotePortDeliver port, void* context) {
  state_->portDeliver = port;
  state_->portDeliverContext = context;
}
void Realm::SetMessagePortClose(RemotePortClose port, void* context) {
  state_->portClose = port;
  state_->portCloseContext = context;
}
void Realm::Stop() { state_->Stop(); }
bool Realm::IsWorker(JSObject* object) {
  JSObject* unwrapped = object ? js::CheckedUnwrapStatic(object) : nullptr;
  return unwrapped && JS::GetClass(unwrapped) == &kWorkerClass;
}
void Realm::SetServiceFactory(ServiceFactory factory, void* context) {
  state_->serviceFactory = factory;
  state_->serviceContext = context;
}
}
