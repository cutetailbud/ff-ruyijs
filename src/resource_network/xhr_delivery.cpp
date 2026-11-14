#include "resource_network/xhr.h"
#include "resource_network/async_http.h"
#include "jsfriendapi.h"
#include "js/Object.h"
namespace ruyijs::resource_network {
namespace {
bool Complete(JS::HandleObject self, uint64_t generation, bool asynchronous) {
  auto& r = XhrData(self);
  auto& s = *r.owner;
  if (asynchronous && (r.state.generation != generation || !r.state.pending)) return true;
  HttpResult http;
  bool ok;
  if (r.prepared) {
    http = std::move(*r.prepared); r.prepared.reset();
    ok = http.error.empty();
  } else {
    HttpOptions options;options.credentials=r.state.anonymous?Credentials::Omit:
        r.state.credentials?Credentials::Include:Credentials::SameOrigin;
    options.initiator="xmlhttprequest";options.cors=true;options.urgent=r.urgent;
    ok = PerformHttp(s.resources, r.method, r.url, r.requestHeaders, r.outgoing, &http,RedirectMode::Follow,options);
  }
  if (!ok) {
    r.response = HttpResult{}; r.state.pending = false;
    return XhrChange(self, 4) && XhrFire(self, "error") && XhrFire(self, "loadend");
  }
  r.response = std::move(http);
  JS::SetReservedSlot(self, XResponseCache, JS::UndefinedValue());
  if (!XhrChange(self, 2)) return false;
  if (asynchronous && s.xhrCheckpoint) s.xhrCheckpoint(s.cx);
  if (asynchronous && r.state.generation != generation) return true;
  if (!XhrChange(self, 3)) return false;
  if (asynchronous && s.xhrCheckpoint) s.xhrCheckpoint(s.cx);
  if (asynchronous && r.state.generation != generation) return true;
  r.state.pending = false;
  return XhrChange(self, 4) && XhrUploadFire(self, "load") && XhrUploadFire(self, "loadend") &&
         XhrFire(self, "load") && XhrFire(self, "loadend");
}
bool Task(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  uint64_t generation = uint64_t(js::GetFunctionNativeReserved(&args.callee(), 1).toNumber());
  bool ok = Complete(self, generation, true);
  if (ok) args.rval().setUndefined();
  return ok;
}
}
// An asynchronous request in flight on the asynchronous transport: the JS
// thread keeps running (Firefox's XHR does not block the event loop). The
// delivery task (which holds the XHR object) stays rooted until the response
// arrives; it is posted then, unless the request was aborted or reopened.
namespace {
struct Flight {
  Flight(JSContext* context, JSObject* fn) : cx(context), task(context, fn) {}
  JSContext* cx;
  JS::PersistentRootedObject task;
  uint64_t generation = 0;
};
void Arrived(void* target, AsyncHttpResult&& result) noexcept {
  auto& flight = *static_cast<Flight*>(target);
  JS::RootedObject task(flight.cx, flight.task);
  JS::RootedObject self(flight.cx, &js::GetFunctionNativeReserved(task, 0).toObject());
  auto& r = XhrData(self);
  if (r.state.generation != flight.generation || !r.state.pending) return;
  HttpResult http = std::move(result.response);
  if (result.outcome != AsyncHttpOutcome::Http || !result.ok) {
    if (http.error.empty()) http.error = "Network request failed";
  }
  r.prepared.emplace(std::move(http));
  auto& s = *r.owner;
  JSAutoRealm realm(s.cx, self);
  if (!s.xhrTask(s.xhrTaskContext, task)) JS_ClearPendingException(s.cx);
}
}  // namespace
void XhrCancelTransfer(XhrRecord& r) {
  const AsyncHttpTicket ticket = r.ticket;
  r.ticket = {};
  if (ticket.id && r.owner) CancelAsyncHttp(r.owner->asyncSource, ticket);
}
bool XhrDeliver(JS::HandleObject self) {
  auto& r = XhrData(self);
  auto& s = *r.owner;
  if (!r.state.asynchronous) return Complete(self, r.state.generation, false);
  if (!r.state.pending) return true;
  if (!s.xhrTask) return Error(s.cx, "XHR task owner is not bound");
  JSAutoRealm realm(s.cx, self);
  JS::RootedFunction fn(s.cx, js::NewFunctionWithReserved(s.cx, Task, 0, 0, nullptr));
  if (!fn) return false;
  JS::RootedObject task(s.cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(task, 0, JS::ObjectValue(*self));
  js::SetFunctionNativeReserved(task, 1, JS::DoubleValue(double(r.state.generation)));
  if (!r.prepared && s.asyncSource && !s.resources.HasRemoteHttp()) {
    HttpOptions options;options.credentials=r.state.anonymous?Credentials::Omit:
        r.state.credentials?Credentials::Include:Credentials::SameOrigin;
    options.initiator="xmlhttprequest";options.cors=true;options.urgent=r.urgent;
    auto flight = std::make_shared<Flight>(s.cx, task);
    flight->generation = r.state.generation;
    AsyncHttpTicket ticket;
    if (StartAsyncHttp(s.asyncSource, {r.method, r.url, r.requestHeaders, r.outgoing, RedirectMode::Follow, options},
                       {flight, Arrived}, &ticket).code == ErrorCode::None) {
      r.ticket = ticket;
      return true;
    }
  }
  return s.xhrTask(s.xhrTaskContext, task);
}
}
