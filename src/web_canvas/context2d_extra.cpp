// The remaining public ruyijs methods; drawImage draws through Firefox's Skia.
#include "web_canvas/internal.h"
#include "document_model/state.h"
#include "web_media/image_codec.h"
#include "web_canvas/skia_target.h"
#include "js/ForOfIterator.h"
#include "js/Realm.h"

namespace ruyijs::web_canvas {
bool detail::ReadImagePixels(State& state, JS::HandleValue value, detail::SourcePixels* out) {
  if (!value.isObject()) return false;
  JS::RootedObject object(state.cx, js::CheckedUnwrapStatic(&value.toObject()));
  if (!object) return false;
  if (JS::GetClass(object) == &kBitmapClass) {
    // ImageBitmap bytes are already premultiplied (image_bitmap_create.cpp).
    out->width = uint32_t(JS::GetReservedSlot(object, 0).toNumber());
    out->height = uint32_t(JS::GetReservedSlot(object, 1).toNumber());
    JS::Value slot = JS::GetReservedSlot(object, 2);
    if (!slot.isObject()) return false;
    JSObject* view = &slot.toObject();
    out->bytes.resize(JS_GetArrayBufferViewByteLength(view));
    if (!out->bytes.empty()) std::memcpy(out->bytes.data(), detail::ViewBytes(view), out->bytes.size());
    return out->width && out->height && out->bytes.size() >= size_t(out->width) * out->height * 4;
  }
  if (CanvasBinding* binding = state.Find(object)) {
    // A canvas element: the snapshot of its premultiplied store.
    JSObject* bitmap = detail::BitmapOf(binding);
    const uint8_t* pixels = bitmap ? detail::ViewBytes(bitmap) : nullptr;
    if (!pixels || !binding->width || !binding->height) return false;
    out->width = binding->width;
    out->height = binding->height;
    out->bytes.assign(pixels, pixels + size_t(out->width) * out->height * 4);
    return true;
  }
  if (JS::GetClass(object) == &document_model::kNodeClass) {
    auto* wrapper = static_cast<document_model::Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (!wrapper || wrapper->dead || wrapper->imageBytes.empty() || wrapper->imageMime.empty()) return false;
    ruyijs::domain::web_media::ImageData decoded;
    std::vector<uint8_t> encoded(wrapper->imageBytes.begin(), wrapper->imageBytes.end());
    const auto status = web_media::DecodeImageBytes(wrapper->imageMime, encoded,
                                                     wrapper->imageNaturalWidth,
                                                     wrapper->imageNaturalHeight, false, &decoded);
    if (status != ruyijs::domain::web_media::ImageDecodeStatus::Ok || decoded.frames.empty()) return false;
    const auto& frame = decoded.frames.front();
    out->width = frame.width; out->height = frame.height; out->bytes = frame.pixels;
    if (!out->width || !out->height || out->bytes.size() < size_t(out->width) * out->height * 4)
      return false;
#if defined(__linux__)
    // Linux codecs return the domain's premultiplied BGRA. Canvas stores RGBA.
    for (size_t i = 0; i < out->bytes.size(); i += 4)
      std::swap(out->bytes[i], out->bytes[i + 2]);
    out->opaque = frame.opaque;
#else
    // The decoder output is straight alpha; Firefox's surface is premultiplied
    // with gfx's (c * a + 254) / 255, and X8 when every pixel is opaque.
    out->opaque = true;
    for (size_t i = 0; i < size_t(out->width) * out->height; ++i) {
      uint8_t* px = out->bytes.data() + i * 4;
      const unsigned a = px[3];
      if (a == 255) continue;
      out->opaque = false;
      for (int c = 0; c < 3; ++c) px[c] = uint8_t((unsigned(px[c]) * a + 254) / 255);
    }
#endif
    return true;
  }
  return false;
}

namespace {
// CanvasRenderingContext2D.cpp ClipImageDimension.
void ClipImageDimension(double& sourceCoord, double& sourceSize, double clipOrigin,
                        double clipSize, double& destCoord, double& destSize) {
  const double scale = destSize / sourceSize;
  double relative = sourceCoord - clipOrigin;
  if (relative < 0.0) {
    const double destEnd = destCoord + destSize;
    destCoord -= relative * scale;
    destSize = destEnd - destCoord;
    sourceSize += relative;
    sourceCoord = clipOrigin;
    relative = 0.0;
  }
  const double delta = clipSize - (relative + sourceSize);
  if (delta < 0.0) {
    destSize += delta * scale;
    sourceSize = clipSize - relative;
  }
}

// ValidateRect(…, true): float-finite, negative sizes flipped.
bool ValidateRect(double rect[4]) {
  for (int i = 0; i < 4; ++i)
    if (!std::isfinite(float(rect[i]))) return false;
  if (rect[2] < 0) { rect[2] = -rect[2]; rect[0] -= rect[2]; }
  if (rect[3] < 0) { rect[3] = -rect[3]; rect[1] -= rect[3]; }
  return true;
}

// CanvasRenderingContext2D::DrawImage for the 3-, 5- and 9-argument forms.
bool DrawImagePixels(State& state, JS::HandleObject context, JS::HandleValue image,
                     const double* values, unsigned count) {
  JSContext* cx = state.cx;
  double source[4] = {0, 0, 0, 0}, dest[4] = {0, 0, 0, 0};
  if (count == 2) { dest[0] = values[0]; dest[1] = values[1]; }
  else if (count == 4) { for (int i = 0; i < 4; ++i) dest[i] = values[i]; }
  else { for (int i = 0; i < 4; ++i) { source[i] = values[i]; dest[i] = values[4 + i]; } }
  if (!ValidateRect(dest)) return true;
  if (count == 8 && !ValidateRect(source)) return true;
  detail::SourcePixels pixels;
  if (!detail::ReadImagePixels(state, image, &pixels)) return true;
  const double width = pixels.width, height = pixels.height;
  if (count == 2) {
    source[2] = width; source[3] = height;
    dest[2] = width; dest[3] = height;
  } else if (count == 4) {
    source[2] = width; source[3] = height;
  }
  if (source[2] == 0.0 || source[3] == 0.0) return true;
  ClipImageDimension(source[0], source[2], 0.0, width, dest[0], dest[2]);
  ClipImageDimension(source[1], source[3], 0.0, height, dest[1], dest[3]);
  if (source[2] <= 0.0 || source[3] <= 0.0 || dest[2] <= 0.0 || dest[3] <= 0.0) return true;
  if (float(source[2]) <= 0.0 || float(source[3]) <= 0.0 || float(dest[2]) <= 0.0 ||
      float(dest[3]) <= 0.0) {
    return true;
  }
  CanvasBinding* binding = detail::BindingOf(state, context);
  JSObject* target = detail::BitmapOf(binding);
  uint8_t* bytes = target ? detail::ViewBytes(target) : nullptr;
  if (!binding || !bytes) return true;
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  double matrix[6];
  if (!detail::GetMatrix(cx, context, matrix)) return false;
  // gfx::Matrix::ScaleFactors over the float transform.
  const float m11 = float(matrix[0]), m12 = float(matrix[1]), m21 = float(matrix[2]),
              m22 = float(matrix[3]);
  float det = m11 * m22 - m12 * m21, xScale = 0, yScale = 0;
  if (det != 0.0f) {
    xScale = sqrtf(m11 * m11 + m12 * m12);
    if (det < 0.0f) det = -det;
    if (xScale) yScale = det / xScale;
  }
  const bool downScale = dest[2] * std::fabs(xScale) < source[2] &&
                         dest[3] * std::fabs(yScale) < source[3];
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(kImageSmoothingEnabled), &value)) return false;
  const bool smoothing = !value.isBoolean() || value.toBoolean();
  const bool linear = smoothing || downScale;
  skia::DrawOptions options;
  if (!JS_GetElement(cx, drawing, uint32_t(kGlobalAlpha), &value)) return false;
  options.alpha = value.isNumber() ? float(value.toNumber()) : 1.0f;
  if (!JS_GetElement(cx, drawing, uint32_t(kGlobalCompositeOperation), &value)) return false;
  if (value.isString()) {
    JS::RootedString text(cx, value.toString());
    if (!detail::StringToAscii(cx, text, &options.composite)) return false;
  }
  options.antialias = linear;
  std::vector<skia::Clip> clips;
  if (!ReadCanvasClips(cx, context, &clips)) return false;
  const skia::Surface surface{bytes, int(binding->width), int(binding->height), &clips};
  const skia::Image source_image{pixels.bytes.data(), int(pixels.width), int(pixels.height),
                                 pixels.opaque};
  skia::ShadowStyle shadow;
  bool shadowed = false;
  if (!ReadCanvasShadow(cx, context, &shadow, &shadowed)) return false;
  skia::DrawImage(surface, matrix, source_image, source, dest, linear, options,
                  shadowed ? &shadow : nullptr);
  return true;
}
} // namespace
namespace detail {
bool ResetContext2D(State& s, JS::HandleObject self) {
  JSContext* cx = s.cx;
  JS::RootedObject drawing(cx), stack(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject path(cx, JS::NewArrayObject(cx, 0));
  if (!stack || !path || !NewDrawingState(cx, &drawing)) return false;
  CanvasBinding* binding = BindingOf(s, self);
  if (binding && !ResizeBitmap(s, *binding, binding->width, binding->height)) return false;
  JS::SetReservedSlot(self, kContextStateSlot, JS::ObjectValue(*drawing));
  JS::SetReservedSlot(self, kContextStackSlot, JS::ObjectValue(*stack));
  JS::SetReservedSlot(self, kContextPathSlot, JS::ObjectValue(*path));
  return true;
}
}  // namespace detail
bool Context2DExtra(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  int op = detail::ContextSlot(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  args.rval().setUndefined();
  if (op == 0 || op == 1) { args.rval().setBoolean(false); return true; }
  if (op == 2) return detail::ResetContext2D(s, self);
  if (op == 3 || op == 4) {
    JS::RootedObject drawing(cx, detail::DrawingState(cx, self));
    JS::RootedObject dash(cx, JS::NewArrayObject(cx, 0));
    if (!drawing || !dash) return false;
    JS::RootedValue value(cx);
    uint32_t n = 0;
    if (op == 3) {
      if (!argc) return WebKit::Fail(cx, "setLineDash requires a sequence");
      JS::ForOfIterator it(cx);
      if (!it.init(args[0])) return false;
      bool done = false, valid = true;
      while (true) {
        if (!it.next(&value, &done)) return false;
        if (done) break;
        double number;
        if (!JS::ToNumber(cx, value, &number)) return false;
        valid &= std::isfinite(number) && number >= 0;
        if (n >= 8192) return WebKit::Fail(cx, "Line dash limit exceeded");
        value.setNumber(double(float(number)));  // ContextState::dash is Float
        if (!JS_SetElement(cx, dash, n++, value)) return false;
      }
      if (!valid) return true;
      if (n % 2) for (uint32_t i = 0; i < n; ++i) {
        if (!JS_GetElement(cx, dash, i, &value) || !JS_SetElement(cx, dash, n+i, value)) return false;
      }
      value.setObject(*dash);
      return JS_SetElement(cx, drawing, kLineDash, value);
    }
    if (!JS_GetElement(cx, drawing, kLineDash, &value)) return false;
    JS::RootedObject saved(cx, &value.toObject());
    if (!JS::GetArrayLength(cx, saved, &n)) return false;
    for (uint32_t i = 0; i < n; ++i)
      if (!JS_GetElement(cx, saved, i, &value) || !JS_SetElement(cx, dash, i, value)) return false;
    args.rval().setObject(*dash);
    return true;
  }
  // stroke() strokes the current path or a Path2D through Skia; clip()
  // pushes it onto the drawing state's clip list.
  if (op == 5 || op == 6) {
    unsigned rule = 0;
    bool evenOdd = false;
    JS::RootedObject owner(cx, self);
    if (args.get(0).isObject() && JS::GetClass(&args[0].toObject()) == &kPath2DClass) {
      rule = 1;
      owner = &args[0].toObject();
    }
    if (op == 6 && !args.get(rule).isUndefined()) {
      JS::RootedString text(cx, JS::ToString(cx, args.get(rule)));
      bool a, b;
      if (!text || !detail::StringEqualsAscii(cx, text, "nonzero", &a) ||
          !detail::StringEqualsAscii(cx, text, "evenodd", &b)) return false;
      if (!a && !b) return WebKit::Fail(cx, "Invalid CanvasFillRule");
      evenOdd = b;
    }
    if (op == 5) return StrokeRecordedPath(s, self, owner);
    return RecordClip(cx, self, owner, evenOdd);
  }
  if (op == 12) return true; // Offscreen commit, no presentation device.
  unsigned first = op == 8 || op == 9 ? 1 : 0;
  unsigned count = op == 7 || op == 11 ? 4 : op == 10 ? 5 : 2;
  if (argc < first+count) return WebKit::Fail(cx, "Missing drawing arguments");
  if (op == 8 && !JS::ToString(cx, args[0])) return false;
  if (op == 9 && !args[0].isObject()) return WebKit::Fail(cx, "drawImage requires an image");
  double values[8]; bool usable;
  if (op == 9) {
    if (argc != 3 && argc != 5 && argc != 9) return WebKit::Fail(cx, "Invalid drawImage overload");
    count = argc-1;
  } else if (op == 8 && argc > 3) count = 3;
  if (!detail::ReadNumbers(cx, args, first, count, values, &usable)) return false;
  if (op == 9 && usable && !DrawImagePixels(s, self, args[0], values, count)) return false;
  if (op == 10 && usable && values[4] < 0)
    return ThrowDOMException(s, "IndexSizeError", "Negative radius");
  if (op == 7 && usable) return StrokeRecordedRect(s, self, values);
  if (op == 8 && usable) {
    JS::RootedString text(cx, JS::ToString(cx, args[0]));
    std::u16string units;
    if (!text || !detail::StringUnits(cx, text, &units)) return false;
    return StrokeRecordedText(s, self, units, values[0], values[1], count > 2 ? values[2] : NAN);
  }
  if (op == 10 && usable) {
    // arcTo is replayed by the Skia path builder (opcode 10).
    const double command[6] = {10.0, values[0], values[1], values[2], values[3], values[4]};
    return AppendCanvasPath(cx, self, command, 6);
  }
  return true;
}
bool InstallContext2DExtra(State& s, WebFace& face) {
  const char* names[] = {"isContextLost","isPointInStroke","reset","setLineDash","getLineDash",
    "stroke","clip","strokeRect","strokeText","drawImage","arcTo","roundRect"};
  const unsigned lengths[] = {0,2,0,1,0,0,0,4,3,3,5,4};
  for (int i = 0; i < 11; ++i)
    if (!detail::Method2D(s, face, names[i], Context2DExtra, lengths[i], i)) return false;
  const char* resources[] = {"createLinearGradient","createRadialGradient","createConicGradient","createPattern"};
  const unsigned counts[] = {4,6,3,2};
  for (int i = 0; i < 4; ++i)
    if (!detail::Method2D(s, face, resources[i], CreateCanvasResource, counts[i], i)) return false;
  return detail::Method2D(s, face, "measureText", MeasureText, 1, 0) &&
         detail::Method2D(s, face, "roundRect", RoundRect, 4, 0);
}
bool InstallOffscreenContext(State& s) {
  WebFace& f = s.offscreen2d;
  const char* name = "OffscreenCanvasRenderingContext2D";
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  return s.kit.Define(name, &kOffscreen2DClass, parent, WebKit::Reject, &f) &&
         InstallContextMembers(s, f) &&
         detail::Method2D(s, f, "commit", Context2DExtra, 0, 12);
}
}
