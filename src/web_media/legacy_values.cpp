#include "web_media/legacy_values.h"
#include "web_media/media_runtime.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/String.h"
#include "js/ValueArray.h"
#include <string>

namespace ruyijs::web_media {
// The four MediaDeviceInfo strings are the object data: deviceId, kind, label
// and groupId in the accessor slots kMediaDeviceInfoFields registers.
const JSClass kMediaDeviceInfoClass = {
    "MediaDeviceInfo", JSCLASS_HAS_RESERVED_SLOTS(4)};
namespace {
const JSClass kMediaError = {"MediaError", 0};
// ruyijs decodes no media, so every TimeRanges an element hands out (buffered,
// played, seekable) is an empty list; a VideoPlaybackQuality carries the
// creationTime/totalVideoFrames/droppedVideoFrames HTMLVideoElement measured.
const JSClass kTimeRanges = {"TimeRanges", 0};
const JSClass kPlaybackQuality = {"VideoPlaybackQuality", JSCLASS_HAS_RESERVED_SLOTS(3)};
constexpr int kPlaybackFirstSlot = 9;  // creationTime's accessor slot below

bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}

// A MediaDeviceInfo answers from its own four string slots; the other media
// values carry no script-visible backing data.
bool Getter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  const JSClass* clasp = slot < 4 ? &kMediaDeviceInfoClass :
      slot < 6 ? &kMediaError : slot == 6 ? &kTimeRanges : &kPlaybackQuality;
  JS::RootedObject object(cx);
  if (!MediaReceiver(cx, args.thisv(), clasp, &object)) return false;
  if (clasp == &kMediaDeviceInfoClass) {
    args.rval().set(JS::GetReservedSlot(object, slot));
    return JS_WrapValue(cx, args.rval());
  }
  if (clasp == &kTimeRanges) {  // TimeRanges.length of an empty list
    args.rval().setInt32(0);
    return true;
  }
  if (clasp == &kPlaybackQuality) {
    args.rval().set(JS::GetReservedSlot(object, slot - kPlaybackFirstSlot));
    return true;
  }
  return WebKit::Fail(cx, "Media value has no script-visible backing data");
}

// TimeRanges::Start/End: any index is at or past the end of an empty list,
// NS_ERROR_DOM_INDEX_SIZE_ERR after the WebIDL unsigned long conversion.
bool IndexSizeError(JSContext* cx, const char* method, const JS::CallArgs& args) {
  if (!args.length())
    return WebKit::FailKind(cx, (std::string("TimeRanges.") + method +
                                 ": At least 1 argument required, but only 0 passed").c_str(), JSEXN_TYPEERR);
  uint32_t index = 0;
  if (!JS::ToUint32(cx, args[0], &index)) return false;
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  JS::RootedValue constructor(cx), error(cx);
  if (!JS_GetProperty(cx, global, "DOMException", &constructor)) return false;
  JSString* message = JS_NewStringCopyZ(cx, "Index or size is negative or greater than the allowed amount");
  if (!message) return false;
  JS::RootedValueArray<2> values(cx);
  values[0].setString(message);
  JSString* name = JS_NewStringCopyZ(cx, "IndexSizeError");
  if (!name) return false;
  values[1].setString(name);
  JS::RootedObject exception(cx);
  if (!JS::Construct(cx, constructor, values, &exception)) return false;
  error.setObject(*exception);
  JS_SetPendingException(cx, error);
  return false;
}

bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  const JSClass* clasp = slot == 0 ? &kMediaDeviceInfoClass : &kTimeRanges;
  JS::RootedObject object(cx);
  if (!MediaReceiver(cx, args.thisv(), clasp, &object)) return false;
  if (clasp == &kTimeRanges) return IndexSizeError(cx, slot == 2 ? "start" : "end", args);
  return WebKit::Fail(cx, "Media value has no script-visible backing data");
}

bool DefineValue(State& s, const char* name, const JSClass* clasp,
                 WebFace& face, const char* const* getters, size_t count,
                 int firstSlot) {
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define(name, clasp, parent, Illegal, &face)) return false;
  for (size_t i = 0; i < count; ++i)
    if (!s.kit.Accessor(face, name, getters[i], Getter, nullptr,
                        firstSlot + static_cast<int32_t>(i))) return false;
  return true;
}
}

bool InstallLegacyValues(State& s) {
  const char* devices[] = {"deviceId", "kind", "label", "groupId"};
  const char* error[] = {"code", "message"};
  const char* ranges[] = {"length"};
  const char* quality[] = {"creationTime", "totalVideoFrames", "droppedVideoFrames"};
  WebFace& device = s.deviceInfo;
  WebFace mediaError(s.cx), timeRanges(s.cx), playback(s.cx);
  if (!DefineValue(s, "MediaDeviceInfo", &kMediaDeviceInfoClass, device, devices, 4, 0) ||
      !s.kit.Method(device, "MediaDeviceInfo", "toJSON", Method, 0, 0) ||
      !MediaConstructorLast(s, device) ||
      !DefineValue(s, "MediaError", &kMediaError, mediaError, error, 2, 4) ||
      !DefineValue(s, "TimeRanges", &kTimeRanges, timeRanges, ranges, 1, 6) ||
      !s.kit.Method(timeRanges, "TimeRanges", "start", Method, 1, 2) ||
      !s.kit.Method(timeRanges, "TimeRanges", "end", Method, 1, 1) ||
      !MediaConstructorLast(s, timeRanges) ||
      !DefineValue(s, "VideoPlaybackQuality", &kPlaybackQuality, playback, quality, 3, kPlaybackFirstSlot))
    return false;
  if (!MediaConstructorLast(s, playback)) return false;
  s.timeRangesPrototype = timeRanges.prototype;
  s.playbackQualityPrototype = playback.prototype;
  const struct Constant { WebFace* face; const char* name; int value; } constants[] = {
      {&mediaError, "MEDIA_ERR_ABORTED", 1}, {&mediaError, "MEDIA_ERR_NETWORK", 2},
      {&mediaError, "MEDIA_ERR_DECODE", 3}, {&mediaError, "MEDIA_ERR_SRC_NOT_SUPPORTED", 4}};
  for (const auto& item : constants) {
    JS::RootedValue value(s.cx, JS::Int32Value(item.value));
    const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(s.cx, item.face->prototype, item.name, value, flags) ||
        !JS_DefineProperty(s.cx, item.face->constructor, item.name, value, flags)) return false;
  }
  return MediaConstructorLast(s, mediaError);
}

// HTMLMediaElement buffered/played/seekable (document_model::MediaValuePorts):
// a new empty TimeRanges in the current realm on every read (Firefox's
// getters are not [SameObject]: v.buffered !== v.buffered).
bool NewEmptyTimeRanges(JSContext* cx, JS::MutableHandleObject out) {
  State* state = StateForGlobal(JS::CurrentGlobalOrNull(cx));
  if (!state || !state->timeRangesPrototype) return WebKit::Fail(cx, "TimeRanges has no media realm");
  out.set(state->kit.New(&kTimeRanges, state->timeRangesPrototype));
  return out != nullptr;
}
// HTMLVideoElement.getVideoPlaybackQuality(): the values the element measured.
bool NewVideoPlaybackQuality(JSContext* cx, double creationTime, uint32_t totalFrames,
                             uint32_t droppedFrames, JS::MutableHandleObject out) {
  State* state = StateForGlobal(JS::CurrentGlobalOrNull(cx));
  if (!state || !state->playbackQualityPrototype)
    return WebKit::Fail(cx, "VideoPlaybackQuality has no media realm");
  JS::RootedObject quality(cx, state->kit.New(&kPlaybackQuality, state->playbackQualityPrototype));
  if (!quality) return false;
  JS::SetReservedSlot(quality, 0, JS::DoubleValue(creationTime));
  JS::SetReservedSlot(quality, 1, JS::NumberValue(totalFrames));
  JS::SetReservedSlot(quality, 2, JS::NumberValue(droppedFrames));
  out.set(quality);
  return true;
}

// The one MediaDeviceInfo factory: an enumerated device is a plain value
// object over the four accessor slots installed above.
bool NewMediaDeviceInfo(State& s, const char* deviceId, const char* kind,
                        const char* label, const char* groupId,
                        JS::MutableHandleObject out) {
  JSContext* cx = s.cx;
  const char* values[] = {deviceId, kind, label, groupId};
  JS::RootedObject object(cx, s.kit.New(&kMediaDeviceInfoClass, s.deviceInfo.prototype));
  if (!object) return false;
  for (int32_t slot = 0; slot < 4; ++slot) {
    JSString* text = JS_NewStringCopyZ(cx, values[slot] ? values[slot] : "");
    if (!text) return false;
    JS::SetReservedSlot(object, slot, JS::StringValue(text));
  }
  out.set(object);
  return true;
}
}
