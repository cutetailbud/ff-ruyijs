// getContext, toDataURL and toBlob, plus the one bitmap each canvas owns. The
// context and the bitmap hang off the realm-owned binding list, never off a
// second tree or a JS-side cache.
#include "web_canvas/internal.h"

#include "js/PropertyAndElement.h"
#include "runtime/page_helpers.h"

namespace ruyijs::web_canvas {
namespace {
// Firefox ImageEncoder::GetImageEncoder: a type with no encoder (e.g. "46",
// "image/foo") falls back to image/png. This owner encodes only PNG, so the
// types Firefox does encode otherwise (jpeg/bmp/ico/webp) are not PNG here.
bool EncodesAsPng(JSContext* cx, JS::HandleValue type, bool* png) {
  *png = true;
  if (type.isUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, type));
  if (!text) return false;
  for (const char* other : {"image/jpeg", "image/bmp", "image/x-icon", "image/vnd.microsoft.icon", "image/webp"}) {
    bool match = false;
    if (!detail::StringEqualsAsciiCI(cx, text, other, &match)) return false;
    if (match) *png = false;
  }
  return true;
}
bool EmptyDataURL(JSContext* cx, JS::Value* rval) {
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, "data:,"));
  if (!text) return false;
  rval->setString(text);
  return true;
}
}  // namespace

bool EnsureBitmap(State& state, JS::HandleObject canvas, CanvasBinding** out) {
  CanvasBinding* binding = detail::EnsureCanvas(state, canvas);
  if (!binding) return false;
  uint32_t width = 0, height = 0;
  if (JS::GetClass(canvas) == &kOffscreenClass) {
    if (!OffscreenDimension(state.cx, canvas, "width", &width) ||
        !OffscreenDimension(state.cx, canvas, "height", &height)) return false;
  } else if (!detail::CanvasDimensions(state, canvas, &width, &height)) return false;
  if (binding->width != width || binding->height != height)
    if (!detail::ResizeBitmap(state, *binding, width, height)) return false;
  *out = binding;
  return true;
}

bool CanvasGetContext(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::CanvasReceiver(state, args.thisv(), &self, &args)) return false;
  if (args.length() == 0) return WebKit::Fail(cx, "getContext requires a context type");
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  bool twoD = false;
  if (!detail::StringEqualsAscii(cx, text, "2d", &twoD)) return false;
  // An unsupported kind never fixes the mode, so the canvas still answers "2d".
  if (!twoD) {
    bool bitmap = false;
    if (!detail::StringEqualsAscii(cx, text, "bitmaprenderer", &bitmap)) return false;
    if (bitmap) return BitmapContext(state, self, args.rval());
    // Every other kind goes to the context port; an unbound port, or a kind the
    // port does not own, still answers null.
    if (state.contextPort)
      return state.contextPort(state.contextPortContext, self, text, args.get(1), args.rval());
    args.rval().setNull();
    return true;
  }
  CanvasBinding* binding = detail::EnsureCanvas(state, self);
  if (!binding) return false;
  if (binding->mode == kMode2D) {
    args.rval().setObject(*binding->context2d);
    return true;
  }
  if (binding->mode != kModeNone) {
    args.rval().setNull();
    return true;
  }
  JS::RootedObject context(cx);
  if (!NewContext2D(state, self, &context) || !ReadContextOptions(cx,context,args.get(1))) return false;
  uint32_t width = 0, height = 0;
  if (!detail::CanvasDimensions(state, self, &width, &height) ||
      !detail::ResizeBitmap(state, *binding, width, height)) return false;
  binding->context2d = context;
  binding->mode = kMode2D;
  if (!state.kit.helpers->Object("CanvasRenderingContext2D", context)) return false;
  args.rval().setObject(*context);
  return true;
}

bool CanvasToDataURL(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::CanvasReceiver(state, args.thisv(), &self, &args)) return false;
  bool supported = true;
  if (!EncodesAsPng(cx, args.get(0), &supported)) return false;
  if (!supported) return EmptyDataURL(cx, args.rval().address());
  CanvasBinding* binding = nullptr;
  if (!EnsureBitmap(state, self, &binding)) return false;
  if (!detail::BitmapOf(binding)) return EmptyDataURL(cx, args.rval().address());
  const std::string pixels = detail::PixelsOf(cx, binding);
  const std::string encoded = detail::EncodePNG(
      reinterpret_cast<const uint8_t*>(pixels.data()), binding->width, binding->height,
      detail::MetadataKeyFor(state, *binding));
  const std::string url = "data:image/png;base64," + detail::Base64(encoded);
  JS::RootedString out(cx, JS_NewStringCopyN(cx, url.data(), url.size()));
  if (!out) return false;
  args.rval().setString(out);
  return true;
}

bool CanvasToBlob(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::CanvasReceiver(state, args.thisv(), &self, &args)) return false;
  args.rval().setUndefined();
  if (args.length() == 0 || !args.get(0).isObject() ||
      !JS::IsCallable(&args.get(0).toObject()))
    return WebKit::Fail(cx, "toBlob requires a callback");
  JS::RootedValue callback(cx, args.get(0));
  bool supported = true;
  if (!EncodesAsPng(cx, args.get(1), &supported)) return false;
  JS::RootedValue result(cx, JS::NullValue());
  if (supported) {
    CanvasBinding* binding = nullptr;
    if (!EnsureBitmap(state, self, &binding)) return false;
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
    result.setObject(*blob);
  }
  return detail::QueueJob(state, callback, result);
}

bool InstallElementContext(State& state) {
  WebKit& kit = state.kit;
  JS::HandleObject prototype = state.canvasPrototype;
  return detail::MethodOn(kit, prototype, "HTMLCanvasElement", "getContext",
                          CanvasGetContext, 1, 0) &&
         detail::MethodOn(kit, prototype, "HTMLCanvasElement", "toDataURL",
                          CanvasToDataURL, 0, 0) &&
         detail::MethodOn(kit, prototype, "HTMLCanvasElement", "toBlob", CanvasToBlob,
                          1, 0);
}
}  // namespace ruyijs::web_canvas
