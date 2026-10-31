// Window.createImageBitmap over this owner's own pixel stores (WEB_CANVAS.md
// slice 25): canvas, OffscreenCanvas, ImageBitmap and ImageData sources, the
// crop rectangle and resizeWidth/resizeHeight, with Firefox's errors.
#include "web_canvas/internal.h"
#include "document_model/state.h"
#include "web_media/image_codec.h"
#include "js/Conversions.h"
#include "js/GlobalObject.h"
#include "js/Wrapper.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>

namespace ruyijs::web_canvas {
namespace {
constexpr const char* kPrefix = "Window.createImageBitmap: ";
struct Pixels { uint32_t width = 0, height = 0; std::vector<uint8_t> bytes; };
bool Type(JSContext* cx, const std::string& message) {
  return WebKit::Fail(cx, (kPrefix + message).c_str());
}
bool Invalid(State& s, const char* message) {
  return ThrowDOMException(s, "InvalidStateError", (std::string(kPrefix) + message).c_str());
}
void Copy(JSObject* view, uint32_t width, uint32_t height, Pixels* out) {
  out->width = width; out->height = height;
  out->bytes.assign(size_t(width) * height * 4, 0);
  JS::AutoCheckCannotGC nogc;
  const size_t length = std::min(out->bytes.size(), JS_GetTypedArrayByteLength(view));
  if (length) std::memcpy(out->bytes.data(), detail::ViewBytes(view), length);
}
bool InstanceOf(State& s, JS::HandleObject object, const char* iface, bool* result) {
  JS::RootedValue ctor(s.cx), proto(s.cx);
  *result = false;
  if (!JS_GetProperty(s.cx, s.kit.page, iface, &ctor) || !ctor.isObject()) return true;
  JS::RootedObject constructor(s.cx, &ctor.toObject());
  JS::RootedValue value(s.cx, JS::ObjectValue(*object));
  return JS::OrdinaryHasInstance(s.cx, constructor, value, result);
}
// Reads the source into straight-alpha RGBA, the form every store here keeps.
bool Source(State& s, JS::HandleValue value, Pixels* out) {
  JSContext* cx = s.cx;
  const std::string types = "Argument 1 could not be converted to any of: HTMLImageElement, "
      "SVGImageElement, HTMLCanvasElement, HTMLVideoElement, OffscreenCanvas, ImageBitmap, "
      "VideoFrame, Blob, CanvasRenderingContext2D, ImageData.";
  if (!value.isObject()) return Type(cx, types);
  JS::RootedObject object(cx, &value.toObject());
  const JSClass* clasp = JS::GetClass(object);
  if (clasp == &kContext2DClass) {
    JS::RootedValue canvas(cx, JS::GetReservedSlot(object, kContextCanvasSlot));
    if (!canvas.isObject()) return Type(cx, types);
    object = &canvas.toObject();
    clasp = JS::GetClass(object);
  }
  JS::RootedValue candidate(cx, JS::ObjectValue(*object));
  if (clasp == &kOffscreenClass || detail::IsCanvas(s, candidate)) {
    // A canvas's pixels belong to its own realm's Canvas owner; this owner has
    // no binding for another realm's canvas and must not create a second one.
    if (js::IsCrossCompartmentWrapper(object) ||
        JS::GetNonCCWObjectGlobal(object) != s.kit.page.get()) return Type(cx, types);
    CanvasBinding* binding = nullptr;
    uint32_t width = 0, height = 0;
    if (clasp == &kOffscreenClass) {
      if (!OffscreenDimension(cx, object, "width", &width) ||
          !OffscreenDimension(cx, object, "height", &height)) return false;
    } else if (!detail::CanvasDimensions(s, object, &width, &height)) {
      return false;
    }
    if (!width) return Invalid(s, "Passed-in canvas has width 0");
    if (!height) return Invalid(s, "Passed-in canvas has height 0");
    if (!EnsureBitmap(s, object, &binding) || !binding) return false;
    if (!binding->bitmap) { *out = Pixels{width, height, std::vector<uint8_t>(size_t(width) * height * 4)}; return true; }
    Copy(binding->bitmap, binding->width, binding->height, out);
    return true;
  }
  if (clasp == &kBitmapClass) {
    JS::RootedValue pixels(cx, JS::GetReservedSlot(object, 2));
    if (!pixels.isObject()) return Invalid(s, "Passed-in ImageBitmap has been closed or transferred");
    Copy(&pixels.toObject(), uint32_t(JS::GetReservedSlot(object, 0).toNumber()),
         uint32_t(JS::GetReservedSlot(object, 1).toNumber()), out);
    return true;
  }
  // HTMLImageElement keeps the encoded response on its Document Model wrapper.
  // Decode through the same WIC/AVIF codec used by ImageDecoder so image fetch,
  // createImageBitmap and Canvas share one byte/mime source of truth.
  if (clasp == &document_model::kNodeClass) {
    auto* wrapper = static_cast<document_model::Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (!wrapper || wrapper->dead || wrapper->imageBytes.empty() || wrapper->imageMime.empty())
      return Invalid(s, "The source image could not be decoded");
    ruyijs::domain::web_media::ImageData decoded;
    std::vector<uint8_t> encoded(wrapper->imageBytes.begin(), wrapper->imageBytes.end());
    const auto status = web_media::DecodeImageBytes(wrapper->imageMime, encoded,
                                                     wrapper->imageNaturalWidth,
                                                     wrapper->imageNaturalHeight, false, &decoded);
    if (status != ruyijs::domain::web_media::ImageDecodeStatus::Ok || decoded.frames.empty())
      return Invalid(s, "The source image could not be decoded");
    const auto& frame = decoded.frames.front();
    out->width = frame.width; out->height = frame.height; out->bytes = frame.pixels;
#if defined(__linux__)
    for (size_t i = 0; i < out->bytes.size(); i += 4)
      std::swap(out->bytes[i], out->bytes[i + 2]);
#endif
    return true;
  }
  if (clasp == &kImageDataClass) {
    JS::RootedValue data(cx, JS::GetReservedSlot(object, kImageDataSlot));
    if (!data.isObject()) return Type(cx, types);
    Copy(&data.toObject(), uint32_t(JS::GetReservedSlot(object, kImageWidthSlot).toNumber()),
         uint32_t(JS::GetReservedSlot(object, kImageHeightSlot).toNumber()), out);
    // Firefox stores the bitmap premultiplied: (c*a+254)/255 in, floor(c*255/a) out.
    for (size_t i = 0; i + 3 < out->bytes.size(); i += 4) {
      const uint32_t a = out->bytes[i + 3];
      for (size_t k = 0; k < 3; ++k) {
        const uint32_t p = (uint32_t(out->bytes[i + k]) * a + 254) / 255;
        out->bytes[i + k] = uint8_t(a ? (p * 255) / a : 0);
      }
    }
    return true;
  }
  for (const char* iface : {"HTMLImageElement", "SVGImageElement", "Blob"}) {
    bool is = false;
    if (!InstanceOf(s, object, iface, &is)) return false;
    if (is) return Invalid(s, "The source image could not be decoded");
  }
  bool video = false;
  if (!InstanceOf(s, object, "HTMLVideoElement", &video)) return false;
  if (video) return Invalid(s, "Passed-in video has not loaded any data");
  return Type(cx, types);
}
void Crop(const Pixels& in, int32_t sx, int32_t sy, int32_t sw, int32_t sh, Pixels* out) {
  if (sw < 0) { sx += sw; sw = -sw; }
  if (sh < 0) { sy += sh; sh = -sh; }
  out->width = uint32_t(sw); out->height = uint32_t(sh);
  out->bytes.assign(size_t(sw) * sh * 4, 0);
  for (int64_t y = 0; y < sh; ++y) {
    const int64_t from = int64_t(sy) + y;
    if (from < 0 || from >= int64_t(in.height)) continue;
    for (int64_t x = 0; x < sw; ++x) {
      const int64_t column = int64_t(sx) + x;
      if (column < 0 || column >= int64_t(in.width)) continue;
      std::memcpy(&out->bytes[(size_t(y) * sw + size_t(x)) * 4],
                  &in.bytes[(size_t(from) * in.width + size_t(column)) * 4], 4);
    }
  }
}
// resizeWidth/resizeHeight of ImageBitmapOptions, nearest-neighbour (D130).
bool Resize(State& s, JS::HandleValue options, Pixels* image) {
  if (options.isNullOrUndefined()) return true;
  if (!options.isObject()) return Type(s.cx, "Argument 2 can't be converted to a dictionary.");
  JS::RootedObject dictionary(s.cx, &options.toObject());
  JS::RootedValue height(s.cx), width(s.cx);
  if (!JS_GetProperty(s.cx, dictionary, "resizeHeight", &height) ||
      !JS_GetProperty(s.cx, dictionary, "resizeWidth", &width)) return false;
  uint32_t w = image->width, h = image->height;
  if (!width.isUndefined() && !detail::ToUnsignedLong(s.cx, width, &w)) return false;
  if (!height.isUndefined() && !detail::ToUnsignedLong(s.cx, height, &h)) return false;
  if ((!width.isUndefined() && !w) || (!height.isUndefined() && !h))
    return Invalid(s, "The resize width and height must be nonzero");
  if (width.isUndefined() && !height.isUndefined() && image->height)
    w = uint32_t(double(image->width) * h / image->height + 0.5);
  if (height.isUndefined() && !width.isUndefined() && image->width)
    h = uint32_t(double(image->height) * w / image->width + 0.5);
  if (w == image->width && h == image->height) return true;
  if (uint64_t(w) * h > kCanvasAreaLimit) return Invalid(s, "The resized ImageBitmap is too large");
  Pixels scaled{w, h, std::vector<uint8_t>(size_t(w) * h * 4)};
  for (uint32_t y = 0; y < h && image->height; ++y)
    for (uint32_t x = 0; x < w && image->width; ++x)
      std::memcpy(&scaled.bytes[(size_t(y) * w + x) * 4],
                  &image->bytes[(size_t(uint64_t(y) * image->height / h) * image->width +
                                 size_t(uint64_t(x) * image->width / w)) * 4], 4);
  *image = std::move(scaled);
  return true;
}
bool Build(State& s, const JS::CallArgs& args, JS::MutableHandleObject result) {
  JSContext* cx = s.cx;
  const unsigned count = args.length();
  if (count == 0 || count == 3 || count == 4)
    return Type(cx, std::to_string(count) + " is not a valid argument count for any overload.");
  Pixels image;
  if (!Source(s, args[0], &image)) return false;
  if (count >= 5) {
    int32_t box[4];
    for (int index = 0; index < 4; ++index)
      if (!detail::ToLong(cx, args[1 + index], &box[index])) return false;
    if (!box[2]) return WebKit::FailKind(cx, "Window.createImageBitmap: The crop rect width passed to createImageBitmap must be nonzero", JSEXN_RANGEERR);
    if (!box[3]) return WebKit::FailKind(cx, "Window.createImageBitmap: The crop rect height passed to createImageBitmap must be nonzero", JSEXN_RANGEERR);
    if (uint64_t(std::abs(int64_t(box[2]))) * uint64_t(std::abs(int64_t(box[3]))) > kCanvasAreaLimit)
      return Invalid(s, "The cropped ImageBitmap is too large");
    Pixels cropped;
    Crop(image, box[0], box[1], box[2], box[3], &cropped);
    image = std::move(cropped);
  }
  JS::RootedValue options(cx, count == 2 ? args[1] : count == 6 ? args[5] : JS::UndefinedValue());
  if (!Resize(s, options, &image)) return false;
  JS::RootedObject bitmap(cx, s.kit.New(&kBitmapClass, s.bitmap.prototype));
  JS::RootedObject pixels(cx, JS_NewUint8ClampedArray(cx, image.bytes.size()));
  if (!bitmap || !pixels) return false;
  if (!image.bytes.empty()) {
    JS::AutoCheckCannotGC nogc;
    std::memcpy(detail::ViewBytes(pixels), image.bytes.data(), image.bytes.size());
  }
  JS::SetReservedSlot(bitmap, 0, JS::NumberValue(image.width));
  JS::SetReservedSlot(bitmap, 1, JS::NumberValue(image.height));
  JS::SetReservedSlot(bitmap, 2, JS::ObjectValue(*pixels));
  result.set(bitmap);
  return true;
}
}
// A promise-returning operation: every failure, including argument conversion,
// becomes a rejection rather than a synchronous throw.
bool CreateImageBitmap(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  JS::RootedObject bitmap(cx);
  if (Build(s, args, &bitmap)) {
    JS::RootedValue value(cx, JS::ObjectValue(*bitmap));
    return s.kit.Resolve(value, args.rval());
  }
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  return s.kit.RejectWith(error, args.rval());
}
}
