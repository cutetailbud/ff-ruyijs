// SharedWorker port frames in a Worker process (docs/owners/WORKER.md, "Shared
// workers"): a connect frame creates a worker-side MessagePort through the bound
// factory and fires the SharedWorkerGlobalScope `connect` event with it; message
// and close frames route to the matching port. RelaySharedPort is the
// web_messaging remote relay bound by worker_mode.cpp, so a worker-side
// postMessage returns to the Window through this process's pipe.
#include "web_workers/child.h"
#include "web_workers/clone.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_workers {
bool Child::HandleSharedFrame(std::string_view frame) {
  uint32_t id = 0;
  if (DecodeSharedPortFrame(frame, kSharedConnect, &id)) {
    if (!portFactory_) return WebKit::Fail(cx_, "SharedWorker port factory is not bound");
    JS::RootedObject port(cx_, portFactory_(portFactoryContext_, id));
    JS::RootedObject ports(cx_, JS::NewArrayObject(cx_, 1));
    if (!port || !ports) return false;
    JS::RootedValue element(cx_, JS::ObjectValue(*port));
    if (!JS_DefineElement(cx_, ports, 0, element, JSPROP_ENUMERATE)) return false;
    return events_.FireConnect(kit_.page, ports);
  }
  if (DecodeSharedPortFrame(frame, kSharedClose, &id)) {
    if (portClose_) portClose_(portCloseContext_, id);
    return true;
  }
  if (frame.empty() || frame[0] != kSharedMessage)
    return WebKit::Fail(cx_, "Unknown SharedWorker frame");
  if (!portDeliver_) return WebKit::Fail(cx_, "SharedWorker port deliver is not bound");
  JS::RootedValue value(cx_);
  PortSink sink{portFactory_, portFactoryContext_, nullptr, nullptr};
  return DecodeSharedMessage(cx_, frame, &id, &value, &sink) && portDeliver_(portDeliverContext_, id, value);
}
bool Child::RelaySharedPort(uint32_t id, JS::HandleValue value, JS::HandleValue transfer) {
  std::string frame;
  std::vector<uint32_t> bridged;
  if (!EncodeSharedMessage(cx_, exception_, id, value, transfer, &frame, &bridged)) return false;
  return Queue(std::move(frame));
}
void Child::CloseSharedPort(uint32_t id) {
  std::string frame;
  EncodeSharedPortFrame(id, kSharedClose, &frame);
  Queue(std::move(frame));
}
}  // namespace ruyijs::web_workers
