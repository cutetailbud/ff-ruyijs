// OffscreenCanvas: an offscreen image surface with its own 2D context and a
// convertToBlob promise. It shares the realm-owned canvas binding list with
// HTMLCanvasElement, so its 2D context reads and writes the same RGBA store.
#include "web_canvas/internal.h"

#include <cstring>
#include "interaction/realm.h"

#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include <cmath>

namespace ruyijs::web_canvas {
const JSClass kOffscreenClass = {"OffscreenCanvas",
                                 JSCLASS_HAS_RESERVED_SLOTS(kOffscreenSlotCount)};

namespace {
enum { kWidthSlot = 0, kHeightSlot = 1, kContextSlot = 2 };

bool OffscreenReceiver(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return WebKit::Check(cx, self, &kOffscreenClass, out,
                       "OffscreenCanvas member called on an incompatible receiver");
}

bool Dimension(JSContext* cx, JS::HandleValue value, uint32_t* out) {
  double number;
  if (!JS::ToNumber(cx,value,&number)) return false;
  if (!std::isfinite(number) || std::trunc(number)<0 || std::trunc(number)>4294967295.0)
    return WebKit::Fail(cx,"OffscreenCanvas dimension is outside unsigned long range");
  *out=uint32_t(std::trunc(number));return true;
}
int32_t SlotFor(const char* name) {
  return std::strcmp(name, "width") == 0 ? kWidthSlot : kHeightSlot;
}

bool ResetOffscreenBitmap(State& state, JS::HandleObject self) {
  CanvasBinding* binding = state.Find(self);
  if (!binding) return true;
  uint32_t width = 0, height = 0;
  if (!OffscreenDimension(state.cx, self, "width", &width) ||
      !OffscreenDimension(state.cx, self, "height", &height)) return false;
  if (binding->width == width && binding->height == height) return true;
  return detail::ResizeBitmap(state, *binding, width, height);
}
}  // namespace

bool OffscreenDimension(JSContext* cx, JS::HandleObject offscreen, const char* name,
                        uint32_t* out) {
  JS::Value value = JS::GetReservedSlot(offscreen, SlotFor(name));
  *out = value.isNumber() ? uint32_t(value.toNumber()) : 0;
  return true;
}

bool OffscreenCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  NativeConstructorTrace trace(cx, args);
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.offscreen.prototype, &prototype)) return false;
  if (args.length() < 2) return WebKit::Fail(cx, "OffscreenCanvas requires width and height");
  uint32_t width = 0, height = 0;
  if (!Dimension(cx, args.get(0), &width) ||
      !Dimension(cx, args.get(1), &height)) return false;
  JS::RootedObject object(cx, state.kit.New(&kOffscreenClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, kWidthSlot, JS::NumberValue(width));
  JS::SetReservedSlot(object, kHeightSlot, JS::NumberValue(height));
  JS::SetReservedSlot(object, kContextSlot, JS::NullValue());
  if (!state.events.Attach(object) || !state.kit.helpers->Object("OffscreenCanvas", object)) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool OffscreenGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!OffscreenReceiver(cx, args.thisv(), &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return true;
}

bool OffscreenSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!OffscreenReceiver(cx, args.thisv(), &self)) return false;
  args.rval().setUndefined();
  uint32_t value = 0;
  if (!Dimension(cx, args.get(0), &value)) return false;
  JS::SetReservedSlot(self, slot, JS::NumberValue(value));
  return ResetOffscreenBitmap(state, self);
}

bool OffscreenGetContext(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!OffscreenReceiver(cx, args.thisv(), &self)) return false;
  if (args.length() == 0) return WebKit::Fail(cx, "getContext requires a context type");
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  bool twoD = false;
  if (!detail::StringEqualsAscii(cx, text, "2d", &twoD)) return false;
  if (!twoD) {
    bool bitmap = false;
    if (!detail::StringEqualsAscii(cx, text, "bitmaprenderer", &bitmap)) return false;
    if (bitmap) return BitmapContext(state, self, args.rval());
    // webgl/webgl2 go to the same WebGL owner port HTMLCanvasElement uses.
    if (state.contextPort) return state.contextPort(state.contextPortContext, self, text, args.get(1), args.rval());
    args.rval().setNull();
    return true;
  }
  JS::Value existing = JS::GetReservedSlot(self, kContextSlot);
  if (existing.isObject()) {
    args.rval().set(existing);
    return true;
  }
  CanvasBinding* binding = detail::EnsureCanvas(state, self);
  if (!binding) return false;
  JS::RootedObject context(cx);
  if (!NewContext2D(state, self, &context) || !ReadContextOptions(cx,context,args.get(1))) return false;
  uint32_t width = 0, height = 0;
  if (!OffscreenDimension(cx, self, "width", &width) ||
      !OffscreenDimension(cx, self, "height", &height) ||
      !detail::ResizeBitmap(state, *binding, width, height)) return false;
  binding->mode = kMode2D;
  binding->context2d = context;
  JS::SetReservedSlot(self, kContextSlot, JS::ObjectValue(*context));
  if (!state.kit.helpers->Object("OffscreenCanvasRenderingContext2D", context)) return false;
  args.rval().setObject(*context);
  return true;
}

bool OffscreenConvertToBlob(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!OffscreenReceiver(cx, args.thisv(), &self)) return false;
  const bool legacy=WebKit::Slot(args)==1;
  bool supported = true;
  if (legacy && !args.get(0).isUndefined()) {
    JS::RootedString text(cx,JS::ToString(cx,args.get(0)));
    if (!text) return false;
    bool empty=false;
    if (!detail::StringEqualsAscii(cx,text,"",&empty) ||
        !detail::StringEqualsAsciiCI(cx,text,"image/png",&supported)) return false;
    supported |= empty;
  }
  if (!legacy && args.length() > 0 && args.get(0).isObject()) {
    JS::RootedObject options(cx, &args.get(0).toObject());
    JS::RootedValue type(cx);
    if (!JS_GetProperty(cx, options, "type", &type)) return false;
    if (type.isString()) {
      JS::RootedString text(cx, type.toString());
      supported = false;
      if (!detail::StringEqualsAsciiCI(cx, text, "image/png", &supported)) return false;
    }
  }
  if (!supported) {
    JS::RootedString message(cx, JS_NewStringCopyZ(
        cx, "OffscreenCanvas.convertToBlob: unsupported image type"));
    if (!message) return false;
    JS::RootedValue reason(cx, JS::StringValue(message));
    return state.kit.RejectWith(reason, args.rval());
  }
  CanvasBinding* binding = detail::EnsureCanvas(state, self);
  if (!binding) return false;
  uint32_t width = 0, height = 0;
  if (!OffscreenDimension(cx, self, "width", &width) ||
      !OffscreenDimension(cx, self, "height", &height)) return false;
  if (binding->width != width || binding->height != height)
    if (!detail::ResizeBitmap(state, *binding, width, height)) return false;
  std::string encoded;
  if (detail::BitmapOf(binding)) {
    const std::string pixels = detail::PixelsOf(cx, binding);
    encoded = detail::EncodePNG(reinterpret_cast<const uint8_t*>(pixels.data()),
                                binding->width, binding->height,
                                detail::MetadataKeyFor(state, *binding));
  }
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, "image/png"));
  JS::RootedObject blob(cx);
  if (!detail::PageConstructBlob(state, encoded, type, &blob)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*blob));
  return state.kit.Resolve(value, args.rval());
}

bool InstallOffscreen(State& state) {
  WebKit& kit = state.kit;
  JSContext* cx = state.cx;
  JS::RootedObject objectProto(cx, state.events.prototype());
  if (!kit.Define("OffscreenCanvas", &kOffscreenClass, objectProto, OffscreenCtor,
                  &state.offscreen))
    return false;
  return kit.Accessor(state.offscreen, "OffscreenCanvas", "width", OffscreenGet,
                      OffscreenSet, 0) &&
         kit.Accessor(state.offscreen, "OffscreenCanvas", "height", OffscreenGet,
                      OffscreenSet, 1) &&
         kit.Method(state.offscreen, "OffscreenCanvas", "getContext", OffscreenGetContext,
                    1, 0) &&
         kit.Method(state.offscreen, "OffscreenCanvas", "convertToBlob",
                    OffscreenConvertToBlob, 0, 0) &&
         kit.Method(state.offscreen,"OffscreenCanvas","toBlob",OffscreenConvertToBlob,0,1);
}
}  // namespace ruyijs::web_canvas
