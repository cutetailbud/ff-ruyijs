// File. It shares Blob's bytes slot and class lineage (File.prototype's
// [[Prototype]] is Blob.prototype) but adds a real name and a lastModified
// number; the brand check requires the File class, so Blob is not a File.
#include "web_platform/state.h"

#include "runtime/native_constructor_trace.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <chrono>
#include <cmath>

namespace ruyijs::web_platform {
namespace {
double NowMilliseconds() {
  return double(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());
}

const char* const kFieldNames[] = {"get name", "get lastModified", "get webkitRelativePath"};
bool Brand(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out, const char* member) {
  if (self.isObject() && JS::GetClass(&self.toObject()) == &kFileClass) {
    out.set(&self.toObject());
    return true;
  }
  const std::string message = std::string("'") + member + "' called on an object that does not implement interface File.";
  return WebKit::Fail(cx, message.c_str());
}
}  // namespace

// The one Blob-to-File materialization: FormData's append/set use it, so a Blob
// entry becomes a File with the caller's name and no second byte store.
JSObject* MakeFile(State& state, const std::string& bytes, const std::string& type,
                   const std::string& name, JS::HandleObject prototype) {
  JSContext* cx = state.cx;
  JS::RootedObject object(cx, MakeBlob(state, bytes, type, &kFileClass, prototype));
  JS::RootedValue name_value(cx);
  if (!object || !Value(cx, name, &name_value)) return nullptr;
  JS::SetReservedSlot(object, kFileSlotName, name_value);
  JS::SetReservedSlot(object, kFileSlotModified, JS::NumberValue(NowMilliseconds()));
  return object;
}

bool FileConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  std::string bytes;
  std::vector<std::pair<size_t, size_t>> strings;
  if (!CollectBlobParts(cx, args.get(0), &bytes, &strings)) return false;
  std::string name;
  if (argc >= 2 && !args.get(1).isUndefined() && !Convert(cx, args.get(1), &name))
    return false;
  if (!ApplyBlobEndings(cx, args.get(2), &bytes, strings, NativeCrlf(state))) return false;
  std::string type;
  if (!NormalizeTypeOption(cx, args.get(2), &type)) return false;
  double last_modified = NowMilliseconds();
  if (argc >= 3 && !args.get(2).isUndefined()) {
    JS::RootedObject options(cx, JS::ToObject(cx, args.get(2)));
    JS::RootedValue value(cx);
    if (!options) return false;
    if (!JS_GetProperty(cx, options, "lastModified", &value)) return false;
    if (!value.isUndefined() && !JS::ToNumber(cx, value, &last_modified)) return false;
    if (!std::isfinite(last_modified)) last_modified = 0;
    else last_modified = std::trunc(last_modified);
  }
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.file.prototype, &prototype)) return false;
  JS::RootedObject object(cx, MakeBlob(state, bytes, type, &kFileClass, prototype));
  JS::RootedValue name_value(cx);
  if (!object || !Value(cx, name, &name_value)) return false;
  JS::SetReservedSlot(object, kFileSlotName, name_value);
  JS::SetReservedSlot(object, kFileSlotModified, JS::NumberValue(last_modified));
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool FileGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args.thisv(), &self, kFieldNames[WebKit::Slot(args)])) return false;
  // No directory picker or directory drop creates a File, so the relative path is empty.
  if (FileField(WebKit::Slot(args)) == kFileRelativePathField) {
    args.rval().set(JS_GetEmptyStringValue(cx));
    return true;
  }
  if (FileField(WebKit::Slot(args)) == kFileNameField) {
    args.rval().set(JS::GetReservedSlot(self, kFileSlotName));
    return true;
  }
  args.rval().set(JS::GetReservedSlot(self, kFileSlotModified));
  return true;
}
}  // namespace ruyijs::web_platform