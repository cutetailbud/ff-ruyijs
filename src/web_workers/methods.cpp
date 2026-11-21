#include "runtime/web_kit.h"
#include "web_workers/state.h"
#include "web_workers/clone.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::web_workers {
WorkerRecord* Receiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject object) {
  object.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kWorkerClass) {
    WebKit::FailBrand(cx, args, "Worker"); return nullptr;
  }
  auto slot = JS::GetReservedSlot(object, 0);
  return slot.isUndefined() ? nullptr : static_cast<WorkerRecord*>(slot.toPrivate());
}
bool WorkerTerminate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  auto* record = Receiver(cx, args, &self);
  if (!record) return false;
  record->Stop();
  record->owner->Prune();
  args.rval().setUndefined(); return true;
}
bool WorkerPostMessage(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  auto* record = Receiver(cx, args, &self);
  if (!record) return false;
  if (!argc) return WebKit::Fail(cx, "Worker.postMessage requires a message");
  // Serialize and detach transfers before checking the closed channel.
  std::string frame;
  JS::RootedObject exception(cx, record->owner->exceptionPrototype);
  if (WorkerTokenEnabled()) {
    WorkerMessageToken token{record->workerId, ++record->parentSequence, 0};
    if (!EncodeMessageWithToken(cx, exception, args[0], args.get(1), token, &frame)) return false;
  } else {
    std::vector<uint32_t> bridged;
    if (!EncodeMessage(cx, exception, args[0], args.get(1), &frame, &bridged)) return false;
    record->AddPorts(bridged);
  }
  if (!record->terminated && !record->process.Queue(std::move(frame))) {
    if (record->process.Running()) return WebKit::Fail(cx, "Worker message queue is full");
    record->Stop(); record->owner->Prune();
  } else if (!record->terminated && !record->awaiting) {
    // Posts while a reply is still outstanding share that wait (poll.cpp).
    record->awaiting = true;
    record->waited = 0;
  }
  args.rval().setUndefined(); return true;
}
}
