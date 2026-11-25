// Firefox MediaStream bindings: the stream factory and constructor. Each stream
// owns an ordered track set (StreamTracks, stream_tracks.cpp); tracks come
// from the one track factory (track.cpp).
#include "web_media/stream.h"
#include "runtime/native_constructor_trace.h"
#include "mozilla/RandomNum.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/ForOfIterator.h"
#include "js/GCVector.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cstdio>
#include <string>
#include "web_media/track.h"
namespace ruyijs::web_media {
JSString* NewMediaId(State& s) {
  uint8_t bytes[16];
  if (!mozilla::GenerateRandomBytesFromOS(bytes, sizeof(bytes))) {
    WebKit::Fail(s.cx, "MediaStream random source failed");
    return nullptr;
  }
  bytes[6] = (bytes[6] & 15) | 64;
  bytes[8] = (bytes[8] & 63) | 128;
  char text[39];
  std::snprintf(text, sizeof(text),
      "{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
      bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
      bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
  return JS_NewStringCopyN(s.cx, text, 38);
}
namespace {
bool InitStream(State& s, JS::HandleObject object) {
  JS::RootedString id(s.cx, NewMediaId(s));
  if (!id) return false;
  JS::RootedObject tracks(s.cx, JS::NewArrayObject(s.cx, 0));
  if (!tracks) return false;
  JS::SetReservedSlot(object, StreamTracks, JS::ObjectValue(*tracks));
  JS::SetReservedSlot(object, StreamId, JS::StringValue(id));
  JS::SetReservedSlot(object, StreamAddHandler, JS::NullValue());
  JS::SetReservedSlot(object, StreamRemoveHandler, JS::NullValue());
  JS::SetReservedSlot(object, StreamAddListener, JS::NullValue());
  JS::SetReservedSlot(object, StreamRemoveListener, JS::NullValue());
  JS::SetReservedSlot(object, StreamOwner, JS::PrivateValue(&s));
  return s.events.Attach(object);
}
// The constructor's argument: another stream (its track set is copied) or a
// sequence of tracks, converted completely before the stream exists.
bool ReadTracks(JSContext* cx, JS::HandleValue input, JS::MutableHandle<JS::StackGCVector<JSObject*>> out) {
  JS::RootedObject object(cx, input.isObject() ? js::CheckedUnwrapStatic(&input.toObject()) : nullptr);
  if (object && JS::GetClass(object) == &kStreamClass) {
    JS::RootedObject list(cx, &JS::GetReservedSlot(object, StreamTracks).toObject());
    uint32_t length = 0;
    JS::RootedValue item(cx);
    JSAutoRealm realm(cx, list);
    if (!JS::GetArrayLength(cx, list, &length)) return false;
    for (uint32_t i = 0; i < length; ++i)
      if (!JS_GetElement(cx, list, i, &item) || !out.append(&item.toObject())) return false;
    return true;
  }
  if (!input.isObject()) return WebKit::Fail(cx, "MediaStream requires a stream or sequence");
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(input, JS::ForOfIterator::AllowNonIterable)) return false;
  if (!iterator.valueIsIterable()) return WebKit::Fail(cx, "MediaStream requires a stream or sequence");
  JS::RootedValue item(cx);
  JS::RootedObject track(cx);
  while (true) {
    bool done;
    if (!iterator.next(&item, &done)) return false;
    if (done) return true;
    if (!TrackReceiver(cx, item, &track, "MediaStream constructor: Element of argument 1") ||
        !out.append(&item.toObject())) return false;
  }
}
}
bool TrackReceiver(JSContext* cx, JS::HandleValue input, JS::MutableHandleObject object,
                   const char* where) {
  object.set(input.isObject() ? js::CheckedUnwrapStatic(&input.toObject()) : nullptr);
  if (object && IsTrack(object)) return true;
  return WebKit::Fail(cx, (std::string(where) + " does not implement interface MediaStreamTrack.").c_str());
}
bool StreamReceiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject object) {
  object.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  return (object && JS::GetClass(object) == &kStreamClass) || WebKit::FailBrand(cx, args, "MediaStream");
}
bool StreamArgument(JSContext* cx, JS::HandleValue input, JS::MutableHandleObject object) {
  object.set(input.isObject() ? js::CheckedUnwrapStatic(&input.toObject()) : nullptr);
  return (object && JS::GetClass(object) == &kStreamClass) ||
         WebKit::Fail(cx, "Illegal MediaStream receiver");
}
JSObject* NewStream(State& s, JS::HandleObject prototype) {
  JS::RootedObject object(s.cx, s.kit.New(&kStreamClass, prototype));
  return object && InitStream(s, object) ? object.get() : nullptr;
}
bool NewEmptyMediaStream(JSContext* cx, JS::MutableHandleObject out) {
  State* state = StateForGlobal(JS::CurrentGlobalOrNull(cx));
  if (!state) return WebKit::Fail(cx, "MediaStream has no media realm");
  out.set(NewStream(*state, state->stream.prototype));
  return out != nullptr;
}
bool IsMediaStream(JSObject* object) {
  object = object ? js::CheckedUnwrapStatic(object) : nullptr;
  return object && JS::GetClass(object) == &kStreamClass;
}
bool DesiredStreamPrototype(JSContext* cx, const JS::CallArgs& args,
                            JS::MutableHandleObject out) {
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, target, "prototype", &value)) return false;
  if (value.isObject()) { out.set(&value.toObject()); return true; }
  JS::Realm* realm = JS::GetFunctionRealm(cx, target);
  if (!realm) return false;
  State* owner = StateForGlobal(JS::GetRealmGlobalOrNull(realm));
  if (!owner) return WebKit::Fail(cx, "MediaStream target realm is not installed");
  out.set(owner->stream.prototype);
  return JS_WrapObject(cx, out);
}
bool StreamConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "MediaStream requires new");
  auto& s = *static_cast<State*>(WebKit::Owner(args));
  // Firefox uses the newTarget realm's retained interface prototype when the
  // supplied prototype is primitive; public constructor overrides are ignored.
  JS::RootedObject prototype(cx);
  if (!DesiredStreamPrototype(cx, args, &prototype)) return false;
  JS::Rooted<JS::StackGCVector<JSObject*>> tracks(cx, JS::StackGCVector<JSObject*>(cx));
  if (argc && !ReadTracks(cx, args[0], &tracks)) return false;
  JS::RootedObject object(cx, NewStream(s, prototype)), track(cx);
  if (!object) return false;
  for (JSObject* item : tracks) {
    track = item;
    if (!AddStreamTrack(cx, object, track)) return false;
  }
  args.rval().setObject(*object);
  return trace.Finish(true);
}
}  // namespace ruyijs::web_media
