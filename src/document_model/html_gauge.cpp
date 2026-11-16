#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Realm.h"
#include <cmath>

namespace ruyijs::document_model {
namespace {
struct Member { const char* iface; const char* name; const char16_t* attr; bool set; bool dormant; };
const Member kMembers[] = {
  {"HTMLMeterElement","value",u"value",true,false},
  {"HTMLMeterElement","min",u"min",true,false},
  {"HTMLMeterElement","max",u"max",true,false},
  {"HTMLMeterElement","low",u"low",true,false},
  {"HTMLMeterElement","high",u"high",true,false},
  {"HTMLMeterElement","optimum",u"optimum",true,false},
  {"HTMLMeterElement","labels",u"",false,false},
  {"HTMLProgressElement","value",u"value",true,false},
  {"HTMLProgressElement","max",u"max",true,false},
  {"HTMLProgressElement","position",u"position",false,false},
  {"HTMLProgressElement","labels",u"",false,false}
};
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& m = kMembers[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), m.iface);
  if (!entry) return false;
  double value = 0;
  {
    JSAutoRealm realm(cx, entry->owner->page);
    entry->owner->Observe(m.iface, m.name, int(ApiAccess::Read));
    if (m.dormant) return TypeError(cx, "Public progress member has no initialized implementation");
    if (*m.attr && !entry->owner->Check(entry->owner->model.GaugeValue(entry->key, m.attr, &value))) return false;
  }
  if (*m.attr) args.rval().setNumber(value);
  else args.rval().setNull();
  return true;
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& m = kMembers[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), m.iface);
  if (!entry) return false;
  // The WebIDL conversion: a non-finite number is a TypeError, as Firefox
  // rejects it before the attribute is written.
  double value = 0;
  if (!m.dormant && (!JS::ToNumber(cx, args.get(0), &value) || !std::isfinite(value))) {
    if (!JS_IsExceptionPending(cx)) return TypeError(cx, "The number provided is not finite");
    return false;
  }
  {
    JSAutoRealm realm(cx, entry->owner->page);
    entry->owner->Observe(m.iface, m.name, int(ApiAccess::Write));
    if (m.dormant) return TypeError(cx, "Public progress member has no initialized implementation");
    // Assigning max or value is an attribute write; position is read-only.
    if (m.attr[0] != u'p' &&
        !entry->owner->Check(entry->owner->model.SetGaugeValue(entry->key, m.attr, value))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallGaugeElements(State& state) {
  for (size_t i = 0; i < std::size(kMembers); ++i) {
    const auto& m = kMembers[i];
    if (!state.Accessor(m.iface, m.name, Read, int32_t(i), m.set ? Write : nullptr)) return false;
  }
  return true;
}
}
