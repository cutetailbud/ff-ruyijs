// Document load completion on the original Scheduling queue (PAGE_LOAD owner).
#include "runtime/page_load.h"
#include "runtime/browsing_context.h"
#include "runtime/exception_capture.h"
#include "document_model/realm.h"
#include "interaction/realm.h"
#include "execution_scheduling/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallArgs.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs {
namespace {
// A frame navigation task uses the same shape (Url set), so the parent's
// completion reads it as the child's pending token through CompletionPending.
enum Slot { Loader, Page, Document, Generation, Frame, Done, Callback, Url, Count };
const JSClass kCompletion = {"Document completion task", JSCLASS_HAS_RESERVED_SLOTS(Count)};
bool CompletionPending(JSContext* cx, JS::HandleObject task, execution_scheduling::Realm& scheduling) {
  if (JS::GetReservedSlot(task, Done).toBoolean()) return false;
  auto& document = *static_cast<document_model::Realm*>(
      JS::GetReservedSlot(task, Document).toPrivate());
  JS::RootedObject page(cx, &JS::GetReservedSlot(task, Page).toObject());
  return document.ScriptGeneration() == uint64_t(JS::GetReservedSlot(task, Generation).toNumber()) &&
         !scheduling.Retired(page);
}
}
bool PageLoad::QueueCompletion(Owners& owners, bool frame, JS::MutableHandleObject result) {
  auto* cx = owners.cx;
  JS::RootedObject task(cx, JS_NewObjectWithGivenProto(cx, &kCompletion, nullptr));
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, DeliverCompletion, 0, 0,
                                                      "Document completion task"));
  if (!task || !fn) return false;
  JS::RootedObject callback(cx, JS_GetFunctionObject(fn));
  JS::SetReservedSlot(task, Loader, JS::PrivateValue(this));
  JS::SetReservedSlot(task, Page, JS::ObjectValue(*owners.page));
  JS::SetReservedSlot(task, Document, JS::PrivateValue(&owners.document));
  JS::SetReservedSlot(task, Generation, JS::DoubleValue(owners.document.ScriptGeneration()));
  JS::SetReservedSlot(task, Frame, JS::BooleanValue(frame));
  JS::SetReservedSlot(task, Done, JS::BooleanValue(false));
  JS::SetReservedSlot(task, Callback, JS::ObjectValue(*callback));
  js::SetFunctionNativeReserved(callback, 0, JS::ObjectValue(*task));
  if (!scheduling_.PostTask(callback)) return false;
  if (auto* context = Find(owners.document)) context->loadCompletion = task;
  result.set(task);
  return true;
}
bool PageLoad::DeliverCompletion(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject task(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  args.rval().setUndefined();
  auto& loader = *static_cast<PageLoad*>(JS::GetReservedSlot(task, Loader).toPrivate());
  return loader.Complete(task);
}
bool PageLoad::Complete(JS::HandleObject task) {
  if (!CompletionPending(cx_, task, scheduling_)) return true;
  auto& document = *static_cast<document_model::Realm*>(
      JS::GetReservedSlot(task, Document).toPrivate());
  auto* target = Find(document);
  if (!target) return true;
  auto pending = [](void* context, JS::HandleObject candidate) {
    auto& loader = *static_cast<PageLoad*>(context);
    return CompletionPending(loader.cx_, candidate, loader.scheduling_);
  };
  if (frames_.LoadEventsPending(*target, pending, this)) {
    JS::RootedObject callback(cx_, &JS::GetReservedSlot(task, Callback).toObject());
    return scheduling_.PostTask(callback);
  }
  JS::RootedObject page(cx_, &JS::GetReservedSlot(task, Page).toObject());
  JSAutoRealm realm(cx_, page);
  Owners owners = OwnersOf(*target, page);
  const bool frame = JS::GetReservedSlot(task, Frame).toBoolean();
  // Existing image load blocking stays on the same external-completion port;
  // stylesheets in flight block the load event too (Firefox). Only the load
  // event waits: once another task falls due (a page timer), this task queues
  // itself behind it and the event loop runs it first (#31).
  auto* styling = target->styling.get();
  while ((!frame && document.ImageLoadsPending()) || (styling && styling->LoadsInFlight())) {
    const auto step = scheduling_.RunExternalOrDueTask();
    if (!CompletionPending(cx_, task, scheduling_)) return true;
    if (step == execution_scheduling::Realm::ExternalStep::Idle) break;
    if (step == execution_scheduling::Realm::ExternalStep::TaskDue) {
      JS::RootedObject callback(cx_, &JS::GetReservedSlot(task, Callback).toObject());
      return scheduling_.PostTask(callback);
    }
  }
  if (!CompletionPending(cx_, task, scheduling_)) return true;
  if (styling && styling->LoadsPending()) {  // an arrived sheet's task runs first
    JS::RootedObject callback(cx_, &JS::GetReservedSlot(task, Callback).toObject());
    return scheduling_.PostTask(callback);
  }
  if (frames_.LoadEventsPending(*target, pending, this)) {
    JS::RootedObject callback(cx_, &JS::GetReservedSlot(task, Callback).toObject());
    return scheduling_.PostTask(callback);
  }
  JS::SetReservedSlot(task, Done, JS::BooleanValue(true));
  const uint64_t generation = document.ScriptGeneration();
  JS::RootedObject object(cx_, document.document());
  if (!object || !Milestone(page, MilestoneKind::Complete) ||
      !document.SetReadyState(document_model::ReadyState::Complete) ||
      !owners.events.Fire(object, "readystatechange", false, false)) return false;
  if (document.ScriptGeneration() != generation || scheduling_.Retired(page)) return true;
  if (!Milestone(page, MilestoneKind::LoadStart) ||
      !owners.events.Fire(page, "load", false, false) ||
      !Milestone(page, MilestoneKind::LoadEnd)) return false;
  checkpoint_(cx_);
  if (JS_IsExceptionPending(cx_)) CaptureOperationFailure(cx_);
  return !frame || document.ScriptGeneration() != generation ||
         scheduling_.Retired(page) || FireFrameElementLoad(*target);
}
bool PageLoad::QueueFrameLoad(BrowsingContext& target, const std::string& url) {
  JSAutoRealm realm(cx_, target.page);
  JS::RootedObject task(cx_, JS_NewObjectWithGivenProto(cx_, &kCompletion, nullptr));
  JS::RootedFunction fn(cx_, js::NewFunctionWithReserved(cx_, DeliverFrameLoad, 0, 0,
                                                       "Frame navigation task"));
  JS::RootedString href(cx_, JS_NewStringCopyUTF8N(cx_, JS::UTF8Chars(url.data(), url.size())));
  if (!task || !fn || !href) return false;
  JS::RootedObject callback(cx_, JS_GetFunctionObject(fn));
  JS::SetReservedSlot(task, Loader, JS::PrivateValue(this));
  JS::SetReservedSlot(task, Page, JS::ObjectValue(*target.page));
  JS::SetReservedSlot(task, Document, JS::PrivateValue(target.document.get()));
  JS::SetReservedSlot(task, Generation, JS::DoubleValue(target.document->ScriptGeneration()));
  JS::SetReservedSlot(task, Frame, JS::BooleanValue(true));
  JS::SetReservedSlot(task, Done, JS::BooleanValue(false));
  JS::SetReservedSlot(task, Callback, JS::ObjectValue(*callback));
  JS::SetReservedSlot(task, Url, JS::StringValue(href));
  js::SetFunctionNativeReserved(callback, 0, JS::ObjectValue(*task));
  if (!scheduling_.PostTask(callback)) return false;
  target.loadCompletion = task;
  return true;
}
bool PageLoad::DeliverFrameLoad(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject task(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  args.rval().setUndefined();
  auto& loader = *static_cast<PageLoad*>(JS::GetReservedSlot(task, Loader).toPrivate());
  return loader.RunFrameLoad(task);
}
bool PageLoad::RunFrameLoad(JS::HandleObject task) {
  if (!CompletionPending(cx_, task, scheduling_)) return true;
  auto& document = *static_cast<document_model::Realm*>(
      JS::GetReservedSlot(task, Document).toPrivate());
  // A later load of this frame replaced the token, or the frame was removed.
  BrowsingContext* target = Find(document);
  if (!target || !frames_.Live(*target) || target->loadCompletion.get() != task) return true;
  JS::SetReservedSlot(task, Done, JS::BooleanValue(true));
  JS::RootedString href(cx_, JS::GetReservedSlot(task, Url).toString());
  std::string url;
  if (!ToUTF8(cx_, href, &url)) return false;
  // As before, a child document that cannot load does not fail the task queue
  // (Firefox reports an element `error`, D8).
  if (!LoadFrame(*target, url)) JS_ClearPendingException(cx_);
  return true;
}
bool PageLoad::WaitForCompletion(JS::HandleObject task) {
  // The handle keeps the task alive; the callback reads its current rooted address.
  struct WaitState { JSContext* cx; JS::HandleObject task;
    execution_scheduling::Realm& scheduling; } state{cx_, task, scheduling_};
  auto waiting = [](void* value) {
    auto& state = *static_cast<WaitState*>(value);
    return CompletionPending(state.cx, state.task, state.scheduling);
  };
  scheduling_.Drain(100000, waiting, &state);
  if (!waiting(&state)) return true;
  JS_ReportErrorASCII(cx_, "Document completion exceeded the task budget");
  return false;
}
}
