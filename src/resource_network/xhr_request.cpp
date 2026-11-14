#include "resource_network/xhr.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include <algorithm>
namespace ruyijs::resource_network {
namespace {
bool Open(JSContext* cx, const JS::CallArgs& args, JS::HandleObject self) {
  auto& r = XhrData(self);
  if (args.length() < 2) return Error(cx, "XMLHttpRequest.open requires a method and URL");
  std::string method, input, base, url;
  if (!args[0].isSymbol() && !EncodeBodyText(cx, args[0], &method)) return false;
  if (!args[1].isSymbol() && !EncodeBodyText(cx, args[1], &input)) return false;
  const std::string separators = "()<>@,;:\\\"/[]?={} \t";
  if (method.empty() || std::any_of(method.begin(), method.end(), [&](unsigned char c) {
        return c < 33 || c > 126 || separators.find(char(c)) != std::string::npos;
      })) return XhrException(*r.owner, "SyntaxError");
  std::string upper = method;
  for (char& c : upper) if (c >= 'a' && c <= 'z') c -= 'a'-'A';
  if (upper == "CONNECT" || upper == "TRACE" || upper == "TRACK") return XhrException(*r.owner, "SecurityError");
  if (upper == "DELETE" || upper == "GET" || upper == "HEAD" || upper == "OPTIONS" || upper == "POST" || upper == "PUT") method = upper;
  if (!PageBaseURL(*r.owner, &base)) return false;
  if (input.empty()) url = base;
  else if (!ResolveURL(base, input, &url)) return XhrException(*r.owner, "SyntaxError");
  // XHR open() step 12: terminate the ongoing fetch.
  XhrCancelTransfer(r);
  r.method = std::move(method); r.url = std::move(url);
  r.response = HttpResult{}; r.requestHeaders.clear(); r.outgoing.clear(); r.mime.clear();
  r.prepared.reset();
  JS::SetReservedSlot(self, XResponseCache, JS::UndefinedValue());
  bool changed = r.state.open(args.length() < 3 || JS::ToBoolean(args[2]));
  return !changed || XhrFire(self, "readystatechange");
}
bool Send(JSContext* cx, const JS::CallArgs& args, JS::HandleObject self) {
  auto& r = XhrData(self);
  if (!r.state.writable()) return XhrException(*r.owner, "InvalidStateError");
  // XHR send() step 4: GET/HEAD ignore the body; otherwise Fetch "extract a
  // body" gives bytes and a type that becomes Content-Type when none was set.
  BodyRecord body;
  JS::RootedValue input(cx, args.get(0));
  if (r.method != "GET" && r.method != "HEAD" && !BodyInput(*r.owner, input, &body)) return false;
  if (body.bytes.size() > Limits::body_bytes) return Error(cx, "XHR body exceeds product limit", true);
  if (!r.state.begin()) return XhrException(*r.owner, "InvalidStateError");
  AddBodyContentType(body, &r.requestHeaders);
  r.outgoing = std::move(body.bytes);
  r.urgent = r.owner->userInput && r.owner->userInput();
  const auto generation = r.state.generation;
  if (r.url.compare(0, 5, "blob:") == 0) {
    r.prepared.emplace();
    PerformHttp(r.owner->resources, r.method, r.url, r.requestHeaders, r.outgoing, &*r.prepared);
  }
  if (!XhrUploadFire(self, "loadstart") || !XhrFire(self, "loadstart")) return false;
  if (r.state.generation != generation || !r.state.pending) return true;
  return XhrDeliver(self);
}
bool Abort(JS::HandleObject self) {
  auto& r = XhrData(self);
  // XHR abort() step 1: abort the fetch controller.
  XhrCancelTransfer(r);
  if (!r.state.abort()) return true;
  r.response = HttpResult{};
  r.prepared.reset();
  JS::SetReservedSlot(self, XResponseCache, JS::UndefinedValue());
  return XhrFire(self, "readystatechange") && XhrFire(self, "abort") && XhrFire(self, "loadend");
}
}
bool XhrRequest(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int op = XhrSelector(args);
  JS::RootedObject self(cx);
  auto* r = XhrReceiver(cx, args, &self);
  if (!r) return false;
  r->owner->Observe("XMLHttpRequest", op == XOpen ? "open" : op == XSend ? "send" : "abort", int(ApiAccess::Call));
  bool ok = op == XOpen ? Open(cx, args, self) : op == XSend ? Send(cx, args, self) : Abort(self);
  if (ok) args.rval().setUndefined();
  return ok;
}
}
