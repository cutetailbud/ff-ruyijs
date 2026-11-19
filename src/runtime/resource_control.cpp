#include "runtime/resource_control.h"
#include "runtime/resource_schema.h"
#include "js/PropertyAndElement.h"
#include "js/Value.h"

namespace ruyijs {
bool ResourceControl::Request(JS::HandleObject request, JS::MutableHandleValue result) {
  using namespace resource_wire;
  using resource_network::Report;
  std::string action;
  if (!Property(cx_, request, "action", &action, 16, TextKind::ASCII)) return false;
  if (action == "status" || action == "clear") {
    if (!Fields(cx_, request, {"id", "op", "action"})) return false;
    if (action == "clear") owner_.Clear();
    return Stats(cx_, owner_.Inspect(), result);
  }
  if (action == "put") {
    resource_network::ResourceInput input;
    resource_network::ResourceStats stats;
    return Input(cx_, request, &input) && Report(cx_, owner_.Put(input, &stats)) && Stats(cx_, stats, result);
  }
  if (action == "get") {
    std::string url;
    resource_network::ResourceSnapshot snapshot;
    bool found = false;
    if (!Fields(cx_, request, {"id", "op", "action", "url"}) ||
        !Property(cx_, request, "url", &url, resource_network::Limits::key_bytes) ||
        !Report(cx_, owner_.Get(url, &snapshot, &found))) return false;
    if (found) return Snapshot(cx_, snapshot, result);
    result.setNull();
    return true;
  }
  if (action == "log") {
    resource_network::NetLogRecords entries;
    JS::RootedValue drain(cx_);
    if (!Fields(cx_, request, {"id", "op", "action", "clear"}) ||
        !JS_GetProperty(cx_, request, "clear", &drain)) return false;
    if (!drain.isUndefined() && !drain.isBoolean()) return Error(cx_, "net log clear must be a boolean");
    // Read and drain in one command, as console_log(clear=True) does (#46).
    if (!Report(cx_, owner_.Log(&entries)) || !LogEntries(cx_, entries, result)) return false;
    return !(drain.isBoolean() && drain.toBoolean()) || Report(cx_, owner_.ClearLog());
  }
  return Error(cx_, "Unknown resource action");
}
}
