// The fill operations that write pixels: fill(path) and fillRect(rect) rasterize
// through the analytic scan converter into the canvas byte store, which is the
// premultiplied surface Firefox keeps (getImageData and toDataURL convert at
// their boundary).
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "web_canvas/internal.h"
#include "web_canvas/raster_color.h"
#include "web_canvas/raster_commands.h"
#include "web_canvas/skia_target.h"
#include "web_geometry/system_font.h"

namespace ruyijs::web_canvas {
bool ReadCanvasClips(JSContext* cx, JS::HandleObject context, std::vector<skia::Clip>* out);
namespace {
bool StateString(JSContext* cx, JS::HandleObject context, int32_t field, std::string* out) {
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(field), &value)) return false;
  if (!value.isString()) return true;
  JS::RootedString text(cx, value.toString());
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
  if (!utf8) return false;
  *out = utf8.get();
  return true;
}

bool StateNumber(JSContext* cx, JS::HandleObject context, int32_t field, double* out) {
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(field), &value)) return false;
  *out = value.isNumber() ? value.toNumber() : 1.0;
  return true;
}

// The premultiplied surface of the canvas this context draws into.
bool Target(State& state, JS::HandleObject context, raster::Surface* surface,
            JS::MutableHandleObject bitmapOut) {
  CanvasBinding* binding = detail::BindingOf(state, context);
  JSObject* bitmap = detail::BitmapOf(binding);
  if (!binding || !bitmap) return false;
  uint8_t* pixels = detail::ViewBytes(bitmap);
  if (!pixels) return false;
  surface->pixels = pixels;
  surface->width = int(binding->width);
  surface->height = int(binding->height);
  bitmapOut.set(bitmap);
  return true;
}

// CanvasGeneralPattern::ForStyle for a colour, gradient or pattern style.
bool StylePattern(JSContext* cx, JS::HandleObject context, int32_t field,
                  skia::Pattern* pattern, bool* usable) {
  *usable = false;
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(field), &value)) return false;
  if (value.isString()) {
    JS::RootedString text(cx, value.toString());
    std::string ascii;
    uint8_t rgba[4];
    if (!detail::StringToAscii(cx, text, &ascii)) return false;
    if (!raster::ParseCSSColor(ascii, rgba)) return true;
    // nscolor -> sRGBColor::FromABGR -> DeviceColor (no colour management).
    pattern->kind = skia::Pattern::Kind::Color;
    // ToDeviceColor(nscolor): sRGBColor::FromABGR multiplies by (1.0f / 255.0f).
    pattern->color = {rgba[0] * (1.0f / 255.0f), rgba[1] * (1.0f / 255.0f),
                      rgba[2] * (1.0f / 255.0f), rgba[3] * (1.0f / 255.0f)};
    *usable = true;
    return true;
  }
  if (value.isObject() && JS::GetClass(&value.toObject()) == &kPatternClass) {
    // resources.cpp createPattern: slot 2 the snapshot, 3/4 its size, 5 the
    // flags (bit 0 opaque, bits 1-2 RepeatMode), 1 the float setTransform().
    JS::RootedObject object(cx, &value.toObject());
    if (!JS::GetReservedSlot(object, 2).isObject()) return true;
    pattern->kind = skia::Pattern::Kind::Surface;
    pattern->width = int(JS::GetReservedSlot(object, 3).toNumber());
    pattern->height = int(JS::GetReservedSlot(object, 4).toNumber());
    const int flags = JS::GetReservedSlot(object, 5).toInt32();
    pattern->opaque = (flags & 1) != 0;
    pattern->repeat = skia::Pattern::Repeat(flags >> 1);
    {
      JS::AutoCheckCannotGC nogc;
      const uint8_t* bytes = detail::ViewBytes(&JS::GetReservedSlot(object, 2).toObject());
      if (!bytes) return true;
      pattern->pixels.assign(bytes, bytes + size_t(pattern->width) * size_t(pattern->height) * 4);
    }
    JS::RootedValue transform(cx, JS::GetReservedSlot(object, 1));
    if (transform.isObject()) {
      JS::RootedObject matrix(cx, &transform.toObject());
      JS::RootedValue item(cx);
      for (uint32_t i = 0; i < 6; ++i) {
        if (!JS_GetElement(cx, matrix, i, &item)) return false;
        pattern->matrix[i] = float(item.toNumber());
      }
    }
    // imageSmoothingEnabled: SamplingFilter::GOOD, else POINT.
    if (!JS_GetElement(cx, drawing, uint32_t(kImageSmoothingEnabled), &value)) return false;
    pattern->linear = !value.isBoolean() || value.toBoolean();
    *usable = true;
    return true;
  }
  if (!value.isObject() || JS::GetClass(&value.toObject()) != &kGradientClass) return true;
  JS::RootedObject gradient(cx, &value.toObject());
  JS::RootedObject stops(cx, &JS::GetReservedSlot(gradient, 0).toObject());
  JS::RootedObject geometry(cx, &JS::GetReservedSlot(gradient, 1).toObject());
  uint32_t length = 0;
  JS::RootedValue item(cx);
  if (!JS::GetArrayLength(cx, geometry, &length) || !JS_GetElement(cx, geometry, 0, &item))
    return false;
  const int kind = item.toInt32();
  pattern->kind = kind == 0 ? skia::Pattern::Kind::Linear
                : kind == 1 ? skia::Pattern::Kind::Radial : skia::Pattern::Kind::Conic;
  // The gradient keeps gfx::Point/Float members.
  for (uint32_t i = 1; i < length && i <= 6; ++i) {
    if (!JS_GetElement(cx, geometry, i, &item)) return false;
    pattern->geometry[i - 1] = float(item.toNumber());
  }
  if (!JS::GetArrayLength(cx, stops, &length)) return false;
  for (uint32_t i = 0; i + 4 < length; i += 5) {
    double v[5];
    for (uint32_t j = 0; j < 5; ++j) {
      if (!JS_GetElement(cx, stops, i + j, &item)) return false;
      v[j] = item.toNumber();
    }
    // CanvasGradient::AddColorStop: ToDeviceColor(nscolor), sRGBColor::FromABGR.
    pattern->stops.push_back(skia::Stop{float(v[0]), {float(v[1]) * (1.0f / 255.0f),
                                                      float(v[2]) * (1.0f / 255.0f),
                                                      float(v[3]) * (1.0f / 255.0f),
                                                      float(v[4]) * (1.0f / 255.0f)}});
  }
  *usable = true;
  return true;
}

// The recorded path of a context or Path2D, in the flat command form.
bool PathCommands(JSContext* cx, JS::HandleObject owner, std::vector<double>* commands) {
  JS::RootedObject path(cx, &JS::GetReservedSlot(owner, kContextPathSlot).toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, path, &length)) return false;
  commands->reserve(length);
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx, path, index, &value)) return false;
    commands->push_back(value.toNumber());
  }
  return true;
}
// The globalAlpha / globalCompositeOperation half of gfx::DrawOptions, the
// transform, the clips and the backing store every draw shares.
bool DrawSetup(State& state, JS::HandleObject context, skia::Surface* target,
               skia::DrawOptions* options, double matrix[6], std::vector<skia::Clip>* clips,
               bool* usable) {
  JSContext* cx = state.cx;
  *usable = false;
  raster::Surface surface;
  JS::RootedObject bitmap(cx);
  if (!Target(state, context, &surface, &bitmap)) return true;
  if (!ReadCanvasClips(cx, context, clips)) return false;
  *target = skia::Surface{surface.pixels, surface.width, surface.height, clips};
  double alpha = 1.0;
  if (!StateString(cx, context, kGlobalCompositeOperation, &options->composite) ||
      !StateNumber(cx, context, kGlobalAlpha, &alpha) ||
      !detail::GetMatrix(cx, context, matrix)) {
    return false;
  }
  options->alpha = float(alpha);
  *usable = true;
  return true;
}

// The recorded path of a context or Path2D owner.
bool OwnerRecord(JSContext* cx, JS::HandleObject owner, std::vector<double>* commands,
                 skia::PathRecord* record) {
  if (!PathCommands(cx, owner, commands)) return false;
  record->data = commands->data();
  record->size = commands->size();
  record->path2d = JS::GetClass(owner) == &kPath2DClass;
  return true;
}

// CanvasRenderingContext2D::NeedToDrawShadow and ShadowParams(): a shadow
// is drawn only for a visible colour with a blur or an offset.
bool CurrentShadow(JSContext* cx, JS::HandleObject context, skia::ShadowStyle* shadow,
                   bool* present) {
  *present = false;
  std::string color;
  double blur = 0.0, offsetX = 0.0, offsetY = 0.0;
  if (!StateString(cx, context, kShadowColor, &color)) return false;
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(kShadowBlur), &value)) return false;
  if (value.isNumber()) blur = value.toNumber();
  if (!JS_GetElement(cx, drawing, uint32_t(kShadowOffsetX), &value)) return false;
  if (value.isNumber()) offsetX = value.toNumber();
  if (!JS_GetElement(cx, drawing, uint32_t(kShadowOffsetY), &value)) return false;
  if (value.isNumber()) offsetY = value.toNumber();
  uint8_t rgba[4];
  if (color.empty() || !raster::ParseCSSColor(color, rgba)) return true;
  const float blurF = float(blur), offsetXF = float(offsetX), offsetYF = float(offsetY);
  if (rgba[3] == 0 || (blurF == 0.f && offsetXF == 0.f && offsetYF == 0.f)) return true;
  shadow->color = {rgba[0] * (1.0f / 255.0f), rgba[1] * (1.0f / 255.0f),
                   rgba[2] * (1.0f / 255.0f), rgba[3] * (1.0f / 255.0f)};
  shadow->offsetX = offsetXF;
  shadow->offsetY = offsetYF;
  shadow->sigma = std::min(100.0f, blurF / 2.0f);  // ShadowBlurSigma, SIGMA_MAX
  *present = true;
  return true;
}

// gfx::StrokeOptions from the drawing state.
bool CurrentStroke(JSContext* cx, JS::HandleObject context, skia::StrokeStyle* stroke) {
  double width = 1.0, miter = 10.0, offset = 0.0;
  if (!StateNumber(cx, context, kLineWidth, &width) ||
      !StateNumber(cx, context, kMiterLimit, &miter) ||
      !StateString(cx, context, kLineCap, &stroke->cap) ||
      !StateString(cx, context, kLineJoin, &stroke->join)) {
    return false;
  }
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(kLineDashOffset), &value)) return false;
  if (value.isNumber()) offset = value.toNumber();
  if (!JS_GetElement(cx, drawing, uint32_t(kLineDash), &value)) return false;
  if (value.isObject()) {
    JS::RootedObject dash(cx, &value.toObject());
    uint32_t length = 0;
    if (!JS::GetArrayLength(cx, dash, &length)) return false;
    for (uint32_t i = 0; i < length; ++i) {
      if (!JS_GetElement(cx, dash, i, &value)) return false;
      stroke->dash.push_back(float(value.toNumber()));
    }
  }
  stroke->lineWidth = float(width);
  stroke->miterLimit = float(miter);
  stroke->dashOffset = float(offset);
  return true;
}
}  // namespace

bool ReadCanvasShadow(JSContext* cx, JS::HandleObject context, skia::ShadowStyle* shadow,
                      bool* present) {
  return CurrentShadow(cx, context, shadow, present);
}

// The drawing state's clip list as the bridge replays it.
bool ReadCanvasClips(JSContext* cx, JS::HandleObject context, std::vector<skia::Clip>* out) {
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(kClips), &value)) return false;
  if (!value.isObject()) return true;
  JS::RootedObject list(cx, &value.toObject());
  uint32_t count = 0;
  if (!JS::GetArrayLength(cx, list, &count)) return false;
  for (uint32_t i = 0; i < count; ++i) {
    if (!JS_GetElement(cx, list, i, &value) || !value.isObject()) return false;
    JS::RootedObject entry(cx, &value.toObject());
    uint32_t length = 0;
    if (!JS::GetArrayLength(cx, entry, &length) || length < 8) return false;
    std::vector<double> numbers(length);
    for (uint32_t j = 0; j < length; ++j) {
      if (!JS_GetElement(cx, entry, j, &value)) return false;
      numbers[j] = value.toNumber();
    }
    skia::Clip clip;
    clip.evenOdd = numbers[0] != 0;
    clip.path2d = numbers[1] != 0;
    for (int k = 0; k < 6; ++k) clip.matrix[k] = numbers[2 + k];
    clip.commands.assign(numbers.begin() + 8, numbers.end());
    out->push_back(std::move(clip));
  }
  return true;
}

// clip(): EnsureUserSpacePath(winding) + PushClip under the current transform.
// The list is copied, so a saved state keeps the clips it had.
bool RecordClip(JSContext* cx, JS::HandleObject context, JS::HandleObject owner, bool evenOdd) {
  constexpr uint32_t kClipLimit = 1024;
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(kClips), &value)) return false;
  JS::RootedObject old(cx, value.isObject() ? &value.toObject() : nullptr);
  uint32_t count = 0;
  if (old && !JS::GetArrayLength(cx, old, &count)) return false;
  if (count >= kClipLimit) return MarkPathFinished(cx, owner);
  std::vector<double> commands;
  double matrix[6];
  if (!PathCommands(cx, owner, &commands) || !detail::GetMatrix(cx, context, matrix)) return false;
  JS::RootedObject entry(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
  if (!entry || !list) return false;
  std::vector<double> numbers = {evenOdd ? 1.0 : 0.0,
                                 JS::GetClass(owner) == &kPath2DClass ? 1.0 : 0.0};
  numbers.insert(numbers.end(), matrix, matrix + 6);
  numbers.insert(numbers.end(), commands.begin(), commands.end());
  for (uint32_t j = 0; j < numbers.size(); ++j) {
    value.setNumber(numbers[j]);
    if (!JS_SetElement(cx, entry, j, value)) return false;
  }
  for (uint32_t i = 0; i < count; ++i) {
    if (!JS_GetElement(cx, old, i, &value) || !JS_SetElement(cx, list, i, value)) return false;
  }
  value.setObject(*entry);
  if (!JS_SetElement(cx, list, count, value)) return false;
  value.setObject(*list);
  if (!JS_SetElement(cx, drawing, uint32_t(kClips), value)) return false;
  return MarkPathFinished(cx, owner);
}

// clearRect(): CanvasRenderingContext2D::ClearRect + DrawTargetSkia::ClearRect.
bool ClearRecordedRect(State& state, JS::HandleObject context, const double rect[4]) {
  JSContext* cx = state.cx;
  raster::Surface surface;
  JS::RootedObject bitmap(cx);
  if (!Target(state, context, &surface, &bitmap)) return true;
  std::vector<skia::Clip> clips;
  double matrix[6];
  if (!ReadCanvasClips(cx, context, &clips) || !detail::GetMatrix(cx, context, matrix)) return false;
  skia::Surface target{surface.pixels, surface.width, surface.height, &clips};
  skia::ClearRect(target, matrix, rect);
  return true;
}

// EnsureUserSpacePath / CanvasPath::GetPath capped the path for this draw;
// later draws of the same path see that cap.
bool MarkPathFinished(JSContext* cx, JS::HandleObject owner) {
  const double command[1] = {9.0};
  return AppendCanvasPath(cx, owner, command, 1);
}

// fill() / fill(path, rule): CanvasRenderingContext2D::Fill through
// DrawTargetSkia::Fill.
bool FillRecordedPath(State& state, JS::HandleObject context, JS::HandleObject pathOwner,
                      bool evenOdd) {
  JSContext* cx = state.cx;
  skia::Surface target;
  skia::DrawOptions options;
  double matrix[6];
  bool usable = false;
  std::vector<skia::Clip> clips;
  if (!DrawSetup(state, context, &target, &options, matrix, &clips, &usable)) return false;
  if (usable) {
    skia::Pattern pattern;
    if (!StylePattern(cx, context, kFillStyle, &pattern, &usable)) return false;
    std::vector<double> commands;
    skia::PathRecord record;
    skia::ShadowStyle shadow;
    bool shadowed = false;
    if (usable) {
      if (!OwnerRecord(cx, pathOwner, &commands, &record) ||
          !CurrentShadow(cx, context, &shadow, &shadowed)) {
        return false;
      }
      skia::FillPath(target, matrix, record, evenOdd, pattern, options,
                     shadowed ? &shadow : nullptr);
    }
  }
  return MarkPathFinished(cx, pathOwner);
}

// stroke() / stroke(path): CanvasRenderingContext2D::Stroke through
// DrawTargetSkia::Stroke.
bool StrokeRecordedPath(State& state, JS::HandleObject context, JS::HandleObject pathOwner) {
  JSContext* cx = state.cx;
  skia::Surface target;
  skia::DrawOptions options;
  double matrix[6];
  bool usable = false;
  std::vector<skia::Clip> clips;
  if (!DrawSetup(state, context, &target, &options, matrix, &clips, &usable)) return false;
  if (usable) {
    skia::Pattern pattern;
    skia::StrokeStyle stroke;
    if (!StylePattern(cx, context, kStrokeStyle, &pattern, &usable) ||
        !CurrentStroke(cx, context, &stroke)) {
      return false;
    }
    std::vector<double> commands;
    skia::PathRecord record;
    skia::ShadowStyle shadow;
    bool shadowed = false;
    if (usable) {
      if (!OwnerRecord(cx, pathOwner, &commands, &record) ||
          !CurrentShadow(cx, context, &shadow, &shadowed)) {
        return false;
      }
      skia::StrokePath(target, matrix, record, stroke, pattern, options,
                       shadowed ? &shadow : nullptr);
    }
  }
  return MarkPathFinished(cx, pathOwner);
}

// strokeRect(): CanvasRenderingContext2D::StrokeRect.
bool StrokeRecordedRect(State& state, JS::HandleObject context, const double rect[4]) {
  JSContext* cx = state.cx;
  skia::Surface target;
  skia::DrawOptions options;
  double matrix[6];
  bool usable = false;
  std::vector<skia::Clip> clips;
  if (!DrawSetup(state, context, &target, &options, matrix, &clips, &usable)) return false;
  if (!usable) return true;
  skia::Pattern pattern;
  skia::StrokeStyle stroke;
  if (!StylePattern(cx, context, kStrokeStyle, &pattern, &usable) ||
      !CurrentStroke(cx, context, &stroke)) {
    return false;
  }
  skia::ShadowStyle shadow;
  bool shadowed = false;
  if (!CurrentShadow(cx, context, &shadow, &shadowed)) return false;
  if (usable)
    skia::StrokeRect(target, matrix, rect, stroke, pattern, options, shadowed ? &shadow : nullptr);
  return true;
}

// strokeText(): CanvasRenderingContext2D::DrawOrMeasureText with
// TextDrawOperation::STROKE.
bool StrokeRecordedText(State& state, JS::HandleObject context, const std::u16string& text,
                        double x, double y, double maxWidth) {
  JSContext* cx = state.cx;
  skia::Surface target;
  skia::DrawOptions options;
  double matrix[6];
  bool usable = false;
  std::vector<skia::Clip> clips;
  if (!DrawSetup(state, context, &target, &options, matrix, &clips, &usable)) return false;
  if (!usable) return true;
  skia::Pattern pattern;
  skia::StrokeStyle stroke;
  if (!StylePattern(cx, context, kStrokeStyle, &pattern, &usable) ||
      !CurrentStroke(cx, context, &stroke)) {
    return false;
  }
  if (!usable) return true;
  std::vector<web_geometry::CanvasGlyphRun> runs;
  double textMatrix[6];
  float size = 0;
  bool draw = false;
  double ink[4] = {0, 0, 0, 0};
  bool hasInk = false;
  const bool ok = CanvasTextRuns(state, context, text, x, y, maxWidth, &runs, textMatrix, &size, &draw,
                                 ink, &hasInk);
  skia::ShadowStyle shadow;
  bool shadowed = false;
  if (ok && draw && !CurrentShadow(cx, context, &shadow, &shadowed)) return false;
  if (ok && draw) {
    std::vector<skia::TextRun> bridge;
    for (const auto& run : runs) {
      skia::TextRun r;
      r.face = run.face;
      r.syntheticItalic = run.syntheticItalic;
#if defined(__linux__)
      r.syntheticBold = run.syntheticBold;
#endif
      r.cjkBitmapStrike = run.cjkBitmapStrike;
      r.originX = run.origin_x;
      r.originY = run.origin_y;
      r.glyphs = run.glyphs.data();
      r.x = run.x.data();
      r.y = run.y.data();
      r.count = run.glyphs.size();
      bridge.push_back(r);
    }
    skia::StrokeText(target, textMatrix, bridge.data(), bridge.size(), size, stroke, pattern, options,
                     shadowed ? &shadow : nullptr, hasInk ? ink : nullptr);
  }
  web_geometry::ReleaseCanvasGlyphRuns(&runs);
  return ok;
}

bool FillRecordedText(State& state, JS::HandleObject context, const std::u16string& text,
                      double x, double y, double maxWidth, bool* handled) {
  JSContext* cx = state.cx;
  *handled = true;
  skia::Surface target;
  skia::DrawOptions options;
  double matrix[6];
  bool usable = false;
  std::vector<skia::Clip> clips;
  if (!DrawSetup(state, context, &target, &options, matrix, &clips, &usable)) return false;
  if (!usable) return true;
  skia::Pattern pattern;
  if (!StylePattern(cx, context, kFillStyle, &pattern, &usable)) return false;
  if (!usable) return true;
  std::vector<web_geometry::CanvasGlyphRun> runs;
  double textMatrix[6];
  float size = 0;
  bool draw = false;
  double ink[4] = {0, 0, 0, 0};
  bool hasInk = false;
  const bool ok = CanvasTextRuns(state, context, text, x, y, maxWidth, &runs, textMatrix, &size, &draw,
                                 ink, &hasInk);
  skia::ShadowStyle shadow;
  bool shadowed = false;
  if (ok && draw && !CurrentShadow(cx, context, &shadow, &shadowed)) return false;
  bool color = false;
  for (const auto& run : runs) color = color || run.colorFont;
  if (ok && draw && color && shadowed) {
    // gfxFont::RenderColorGlyph into AdjustedTargetForShadow's temp, which
    // DrawTargetSkia::DrawSurfaceWithShadow then composites with the shadow.
    // The temp covers the canvas grown by the shadow offset and blur margin.
    const int margin = int(std::ceil(3 * double(shadow.sigma))) + 1;
    const int dx = margin + int(std::ceil(std::max(0.0, double(shadow.offsetX))));
    const int dy = margin + int(std::ceil(std::max(0.0, double(shadow.offsetY))));
    const int w = target.width + 2 * margin + int(std::ceil(std::fabs(double(shadow.offsetX))));
    const int h = target.height + 2 * margin + int(std::ceil(std::fabs(double(shadow.offsetY))));
    if (w > 0 && h > 0 && w <= 32767 && h <= 32767 && size_t(w) * size_t(h) <= (size_t(1) << 26)) {
      std::vector<uint8_t> temp(size_t(w) * size_t(h) * 4, 0);
      TextTarget into{temp.data(), w, h, dx, dy};
      web_geometry::ReleaseCanvasGlyphRuns(&runs);
      if (!FillCanvasText(state, context, text, x, y, maxWidth, &into)) return false;
      skia::Image image{temp.data(), w, h, false};
      const double identity[6] = {1, 0, 0, 1, 0, 0};
      const double source[4] = {0, 0, double(w), double(h)};
      const double dest[4] = {double(-dx), double(-dy), double(w), double(h)};
      skia::DrawOptions composite = options;
      composite.alpha = 1.0f;  // globalAlpha is already in the temp
      skia::DrawImage(target, identity, image, source, dest, false, composite, &shadow);
      return true;
    }
    *handled = false;
  } else if (ok && draw && color) {
    *handled = false;
  } else if (ok && draw) {
    std::vector<skia::TextRun> bridge;
    for (const auto& run : runs) {
      skia::TextRun r;
      r.face = run.face;
      r.syntheticItalic = run.syntheticItalic;
#if defined(__linux__)
      r.syntheticBold = run.syntheticBold;
#endif
      r.cjkBitmapStrike = run.cjkBitmapStrike;
      r.originX = run.origin_x;
      r.originY = run.origin_y;
      r.glyphs = run.glyphs.data();
      r.x = run.x.data();
      r.y = run.y.data();
      r.count = run.glyphs.size();
      bridge.push_back(r);
    }
    skia::FillText(target, textMatrix, bridge.data(), bridge.size(), size, pattern, options,
                   shadowed ? &shadow : nullptr, hasInk ? ink : nullptr);
  }
  web_geometry::ReleaseCanvasGlyphRuns(&runs);
  return ok;
}

// CanvasRenderingContext2D::FillRect: ValidateRect (zero size is drawn,
// negative sizes flip, anything not finite as a float is skipped), then
// DrawTargetSkia::FillRect through Firefox's Skia.
bool FillRecordedRect(State& state, JS::HandleObject context, const double input[4]) {
  JSContext* cx = state.cx;
  double rect[4] = {input[0], input[1], input[2], input[3]};
  for (double value : rect)
    if (!std::isfinite(float(value))) return true;
  if (rect[2] < 0) { rect[2] = -rect[2]; rect[0] -= rect[2]; }
  if (rect[3] < 0) { rect[3] = -rect[3]; rect[1] -= rect[3]; }
  raster::Surface surface;
  JS::RootedObject bitmap(cx);
  if (!Target(state, context, &surface, &bitmap)) return true;
  skia::Pattern pattern;
  bool usable = false;
  if (!StylePattern(cx, context, kFillStyle, &pattern, &usable)) return false;
  if (!usable) return true;
  skia::DrawOptions options;
  double alpha = 1.0;
  if (!StateString(cx, context, kGlobalCompositeOperation, &options.composite) ||
      !StateNumber(cx, context, kGlobalAlpha, &alpha)) {
    return false;
  }
  options.alpha = float(alpha);
  // FillRect anti-aliases only while imageSmoothingEnabled is true.
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  JS::RootedValue smoothing(cx);
  if (!drawing || !JS_GetElement(cx, drawing, uint32_t(kImageSmoothingEnabled), &smoothing))
    return false;
  options.antialias = !smoothing.isBoolean() || smoothing.toBoolean();
  double matrix[6];
  if (!detail::GetMatrix(cx, context, matrix)) return false;
  std::vector<skia::Clip> clips;
  if (!ReadCanvasClips(cx, context, &clips)) return false;
  skia::Surface target{surface.pixels, surface.width, surface.height, &clips};
  skia::ShadowStyle shadow;
  bool shadowed = false;
  if (!CurrentShadow(cx, context, &shadow, &shadowed)) return false;
  skia::FillRect(target, matrix, rect, pattern, options, shadowed ? &shadow : nullptr);
  return true;
}
}  // namespace ruyijs::web_canvas
