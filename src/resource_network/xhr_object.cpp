#include "runtime/web_kit.h"
#include "resource_network/xhr.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include <new>

namespace ruyijs::resource_network {
namespace {
void Unlink(XhrRecord& r) {
  if (!r.owner) return;
  XhrCancelTransfer(r);
  if (r.previous) r.previous->next = r.next;
  else r.owner->xhrRecords = r.next;
  if (r.next) r.next->previous = r.previous;
  r.owner->events->ReleaseBorrowed(r.eventRecord);
  r.eventRecord = nullptr;
  r.owner = nullptr;
}
void Finalize(JS::GCContext*, JSObject* object) {
  auto slot = JS::GetReservedSlot(object, XRecord);
  if (slot.isUndefined()) return;
  auto* record = static_cast<XhrRecord*>(slot.toPrivate());
  Unlink(*record);
  delete record;
}
void Trace(JSTracer*, JSObject* object) {
  auto slot = JS::GetReservedSlot(object, XRecord);
  if (slot.isUndefined()) return;
  auto* record = static_cast<XhrRecord*>(slot.toPrivate());
  if (record->owner && record->eventRecord)
    record->owner->events->RefreshBorrowed(record->eventRecord, object);
}
const JSClassOps kOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       Finalize, nullptr, nullptr, Trace};
bool Init(State& s, JS::HandleObject object) {
  auto* record = new(std::nothrow) XhrRecord;
  if (!record) { JS_ReportOutOfMemory(s.cx); return false; }
  record->owner = &s;
  record->next = s.xhrRecords;
  if (record->next) record->next->previous = record;
  s.xhrRecords = record;
  JS::SetReservedSlot(object, XRecord, JS::PrivateValue(record));
  for (unsigned slot = XUpload; slot < XSlotCount; ++slot)
    JS::SetReservedSlot(object, slot, JS::NullValue());
  JS::SetReservedSlot(object, XResponseCache, JS::UndefinedValue());
  return s.events->AttachBorrowed(object, &record->eventRecord);
}
}
const JSClass kXhrClass = {"XMLHttpRequest",
    JSCLASS_HAS_RESERVED_SLOTS(XSlotCount) | JSCLASS_FOREGROUND_FINALIZE, &kOps};
const JSClass kXhrUploadClass = {"XMLHttpRequestUpload",
    JSCLASS_HAS_RESERVED_SLOTS(XSlotCount) | JSCLASS_FOREGROUND_FINALIZE, &kOps};
void DetachXhrs(State& s) { while (s.xhrRecords) Unlink(*s.xhrRecords); }
XhrRecord& XhrData(JS::HandleObject object) {
  return *static_cast<XhrRecord*>(JS::GetReservedSlot(object, XRecord).toPrivate());
}
XhrRecord* XhrReceiver(JSContext* cx, const JS::CallArgs& args,
                       JS::MutableHandleObject object, bool upload) {
  JS::HandleValue input = args.thisv();
  object.set(input.isObject() ? js::CheckedUnwrapStatic(&input.toObject()) : nullptr);
  if (object && (JS::GetClass(object) == &kXhrClass ||
      (upload && JS::GetClass(object) == &kXhrUploadClass))) return &XhrData(object);
  WebKit::FailBrand(cx, args, upload ? "XMLHttpRequestUpload" : "XMLHttpRequest");
  return nullptr;
}
int XhrSelector(const JS::CallArgs& args) {
  return js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
}
bool XhrIllegalConstructor(JSContext* cx, unsigned, JS::Value*) {
  return Error(cx, "Illegal constructor");
}
bool XhrConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) {
    JS::RootedString message(cx, JS_NewStringCopyZ(cx, "XMLHttpRequest requires new"));
    if (!message) return false;
    JS::RootedValue error(cx, JS::StringValue(message));
    JS_SetPendingException(cx, error); return false;
  }
  auto& s = *FunctionOwner(args);
  s.Observe("XMLHttpRequest", "constructor", int(ApiAccess::Construct));
  JS::RootedObject object(cx, JS_NewObjectForConstructor(cx, &kXhrClass, args));
  JS::RootedObject upload(cx, JS_NewObjectWithGivenProto(cx, &kXhrUploadClass, s.xhrUploadPrototype));
  if (!object || !upload || !Init(s, object) || !Init(s, upload)) return false;
  // constructor(optional MozXMLHttpRequestParameters) or constructor(DOMString ignored).
  JS::RootedValue params(cx, args.get(0));
  if (params.isObject()) {
    JS::RootedObject dictionary(cx, &params.toObject());
    JS::RootedValue anon(cx), system(cx);
    if (!JS_GetProperty(cx, dictionary, "mozAnon", &anon) ||
        !JS_GetProperty(cx, dictionary, "mozSystem", &system)) return false;
    // Without mozAnon, a mozSystem request defaults to omitting credentials (Firefox pref).
    XhrData(object).state.anonymous = anon.isUndefined() ? JS::ToBoolean(system) : JS::ToBoolean(anon);
  } else if (!params.isNullOrUndefined() && !JS::ToString(cx, params)) {
    return false;
  }
  JS::SetReservedSlot(object, XUpload, JS::ObjectValue(*upload));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool XhrException(State& s, const char* name) {
  JS::RootedValue constructor(s.cx, s.xhrException);
  JS::RootedString text(s.cx, JS_NewStringCopyZ(s.cx, name));
  JS::RootedValueArray<2> arguments(s.cx);
  JS::RootedObject error(s.cx);
  if (!text || !JS_WrapValue(s.cx, &constructor)) return false;
  arguments[0].setString(text); arguments[1].setString(text);
  if (!JS::Construct(s.cx, constructor, arguments, &error)) return false;
  JS::RootedValue value(s.cx, JS::ObjectValue(*error));
  JS_SetPendingException(s.cx, value); return false;
}
bool XhrFire(JS::HandleObject object, const char* type) {
  auto& s = *XhrData(object).owner;
  JSAutoRealm realm(s.cx, object);
  return s.events->Fire(object, type, false, false);
}
bool XhrChange(JS::HandleObject object, int ready) {
  return !XhrData(object).state.change(ready) || XhrFire(object, "readystatechange");
}
bool XhrUploadFire(JS::HandleObject object, const char* type) {
  JSContext* cx = XhrData(object).owner->cx;
  JS::RootedObject upload(cx, &JS::GetReservedSlot(object, XUpload).toObject());
  return XhrFire(upload, type);
}
}
