// Canvas text consumes glyph masks from Web Geometry and paints them with the
// context's existing surface, state, transform and blend owner.
#include "web_canvas/internal.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "web_canvas/color_glyph.h"
#include "web_canvas/raster_blitter.h"
#include "web_canvas/raster_color.h"
#include "web_canvas/raster_path.h"
#include "web_geometry/system_font.h"

namespace ruyijs::web_canvas {
namespace {
bool StateString(JSContext* cx, JS::HandleObject context, int32_t field,
                 std::string* out) {
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  JS::RootedValue value(cx);
  if (!drawing || !JS_GetElement(cx, drawing, uint32_t(field), &value)) return false;
  if (!value.isString()) return true;
  JS::RootedString text(cx, value.toString());
  JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, text);
  if (!bytes) return false;
  *out = bytes.get();
  return true;
}

bool StateNumber(JSContext* cx, JS::HandleObject context, int32_t field, double* out) {
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  JS::RootedValue value(cx);
  if (!drawing || !JS_GetElement(cx, drawing, uint32_t(field), &value)) return false;
  *out = value.isNumber() ? value.toNumber() : 1.0;
  return true;
}

bool BuildPaint(State& state, JS::HandleObject context, raster::Paint* paint, std::string* style) {
  std::string composite = "source-over";
  double alpha = 1.0;
  if (!StateString(state.cx, context, kFillStyle, style) ||
      !StateString(state.cx, context, kGlobalCompositeOperation, &composite) ||
      !StateNumber(state.cx, context, kGlobalAlpha, &alpha)) return false;
  return raster::PaintFromStyle(*style, alpha, composite, paint);
}

void Transform(const double matrix[6], float x, float y, raster::Point* out) {
  out->x = float(matrix[0] * x + matrix[2] * y + matrix[4]);
  out->y = float(matrix[1] * x + matrix[3] * y + matrix[5]);
}

bool ReadTextState(State& state, JS::HandleObject context, detail::CanvasFont* font,
                   std::string* align, std::string* baseline, std::string* direction) {
  std::string shorthand = "10px sans-serif";
  if (!StateString(state.cx, context, kFont, &shorthand)) return false;
  if (!detail::ParseCanvasFont(shorthand, font) || !font->valid) {
    if (!detail::ParseCanvasFont("10px sans-serif", font)) return false;
  }
  return StateString(state.cx, context, kTextAlign, align) &&
         StateString(state.cx, context, kTextBaseline, baseline) &&
         StateString(state.cx, context, kDirection, direction);
}
}  // namespace

bool FillCanvasText(State& state, JS::HandleObject context, const std::u16string& text,
                    double x, double y, double max_width, const TextTarget* into) {
  if (text.empty() || !std::isfinite(x) || !std::isfinite(y)) return true;
  if (std::isfinite(max_width) && max_width <= 0) return true;
  detail::CanvasFont font;
  std::string align, baseline, direction;
  if (!ReadTextState(state, context, &font, &align, &baseline, &direction)) return false;

  web_geometry::FontInput input;
  input.family = font.family;
  input.size = font.size;
  input.weight = font.weight;
  input.italic = font.italic;
  if (!detail::CanvasLanguage(state, context, &input.language)) return false;
  web_geometry::CanvasTextMetrics metrics;
  if (!detail::Fonts(state).MeasureCanvas(text, input, &metrics)) return false;
  const bool rtl = direction == "rtl";
  const bool start = align == "start" || align.empty();
  const bool right = align == "right" || (start && rtl) || (align == "end" && !rtl);
  const bool center = align == "center";
  // processor.mPt.x -= offsetX, offsetX a float of anchorX * totalWidth.
  const float offset_x = center ? float(.5 * metrics.width) : right ? metrics.width : 0.0f;
  const double origin_x = x - double(offset_x);
  // CanvasRenderingContext2D::DrawOrMeasureText: mPt.y += baselineAnchor.
  double anchor = metrics.alphabetic_baseline;
  if (baseline == "top") anchor = metrics.em_ascent;
  else if (baseline == "hanging") anchor = metrics.hanging_baseline;
  else if (baseline == "middle") anchor = (metrics.em_ascent - metrics.em_descent) * .5;
  else if (baseline == "ideographic") anchor = metrics.ideographic_baseline;
  else if (baseline == "bottom") anchor = -metrics.em_descent;
  y += anchor;

  input.x = origin_x;
  input.y = y;
#ifndef _WIN32
  {
    web_geometry::CanvasTextPath outline;
    if (!detail::Fonts(state).BuildCanvasTextPath(text, input, &outline)) return false;
  }
#endif
  double matrix[6];
  if (!detail::GetMatrix(state.cx, context, matrix)) return false;
  raster::Surface surface;
  if (into) {
    surface = raster::Surface{into->pixels, into->width, into->height};
  } else {
    CanvasBinding* binding = detail::BindingOf(state, context);
    JSObject* bitmap = detail::BitmapOf(binding);
    if (!binding || !bitmap) return true;
    uint8_t* pixels = detail::ViewBytes(bitmap);
    if (!pixels) return true;
    surface = raster::Surface{pixels, int(binding->width), int(binding->height)};
  }
  raster::Paint paint;
  std::string fill_style;
  if (!BuildPaint(state, context, &paint, &fill_style)) return true;
  if (into) {
    // The temp target is drawn source-over; the real operator applies when
    // it is composited (DrawSurfaceWithShadow(..., mCompositionOp)).
    double alpha = 1.0;
    if (!StateNumber(state.cx, context, kGlobalAlpha, &alpha)) return false;
    if (!raster::PaintFromStyle(fill_style, alpha, "source-over", &paint)) return true;
  }
  double horizontal_scale = 1.0;
  if (std::isfinite(max_width) && max_width > 0 && metrics.width > max_width)
    horizontal_scale = max_width / metrics.width;
  // The glyph mask gamma table is keyed by the paint colour's luminance, so the
  // un-premultiplied fillStyle bytes reach the font owner; maxWidth squeezes the
  // run horizontally, and that squeeze is the glyph run transform, so the mask
  // is rasterised at the squeezed width.
  uint8_t straight[4];
  if (!raster::ParseCSSColor(fill_style, straight)) return true;
  if (into) { matrix[4] += into->dx; matrix[5] += into->dy; }
  float text_matrix[6] = {float(matrix[0]), float(matrix[1]), float(matrix[2]),
                          float(matrix[3]), float(matrix[4]), float(matrix[5])};
  if (horizontal_scale != 1.0) {
    // Squeeze around the origin, as Firefox scales the run's x positions.
    const float scale_x = float(horizontal_scale);
    text_matrix[0] *= scale_x;
    text_matrix[2] *= scale_x;
    text_matrix[4] = float(matrix[0] * origin_x * (1.0 - horizontal_scale) + matrix[4]);
  }
  double global_alpha = 1.0;
  if (!StateNumber(state.cx, context, kGlobalAlpha, &global_alpha)) return false;
  const uint8_t global = uint8_t(std::floor(std::clamp(global_alpha, 0.0, 1.0) * 255.0 + 0.5));
  std::vector<web_geometry::CanvasGlyphMask> masks;
  if (!detail::Fonts(state).BuildCanvasGlyphMasks(text, input, text_matrix, straight, &masks))
    return false;
  for (const auto& mask : masks) {
#if defined(__linux__)
    if (!mask.rgba.empty()) {
      auto blend = paint;
      blend.rgba[3] = uint8_t(unsigned(global) * (straight[3] ? straight[3] : 255) / 255);
      raster::BlitSprite(surface, blend, mask.left, mask.top, mask.rgba.data(), mask.width, mask.height);
      continue;
    }
#endif
    if (mask.layers.empty()) {
      raster::BlitMask(surface, paint, mask.left, mask.top, mask.coverage.data(), mask.width,
                       mask.height);
      continue;
    }
    // gfxFont::RenderColorGlyph; a fill colour that is neither opaque nor
    // transparent makes gfxTextRun::Draw buffer the run and blend it back.
    raster::DrawColorGlyph(surface, mask, text_matrix, global, straight[3] ? straight[3] : 255);
  }
  return true;
}
// The glyph runs and text matrix fillText would draw, for strokeText: the
// same font, alignment, baseline and maxWidth squeeze.
bool CanvasTextRuns(State& state, JS::HandleObject context, const std::u16string& text, double x,
                    double y, double max_width, std::vector<web_geometry::CanvasGlyphRun>* runs,
                    double matrix_out[6], float* size, bool* draw, double ink[4], bool* hasInk) {
  *draw = false;
  if (text.empty() || !std::isfinite(x) || !std::isfinite(y)) return true;
  if (std::isfinite(max_width) && max_width <= 0) return true;
  detail::CanvasFont font;
  std::string align, baseline, direction;
  if (!ReadTextState(state, context, &font, &align, &baseline, &direction)) return false;
  web_geometry::FontInput input;
  input.family = font.family;
  input.size = font.size;
  input.weight = font.weight;
  input.italic = font.italic;
  if (!detail::CanvasLanguage(state, context, &input.language)) return false;
  web_geometry::CanvasTextMetrics metrics;
  if (!detail::Fonts(state).MeasureCanvas(text, input, &metrics)) return false;
  const bool rtl = direction == "rtl";
  const bool start = align == "start" || align.empty();
  const bool right = align == "right" || (start && rtl) || (align == "end" && !rtl);
  const bool center = align == "center";
  // processor.mPt.x -= offsetX, offsetX a float of anchorX * totalWidth.
  const float offset_x = center ? float(.5 * metrics.width) : right ? metrics.width : 0.0f;
  const double origin_x = x - double(offset_x);
  // CanvasRenderingContext2D::DrawOrMeasureText: mPt.y += baselineAnchor.
  double anchor = metrics.alphabetic_baseline;
  if (baseline == "top") anchor = metrics.em_ascent;
  else if (baseline == "hanging") anchor = metrics.hanging_baseline;
  else if (baseline == "middle") anchor = (metrics.em_ascent - metrics.em_descent) * .5;
  else if (baseline == "ideographic") anchor = metrics.ideographic_baseline;
  else if (baseline == "bottom") anchor = -metrics.em_descent;
  y += anchor;
  input.x = origin_x;
  input.y = y;
  double matrix[6];
  if (!detail::GetMatrix(state.cx, context, matrix)) return false;
  double horizontal_scale = 1.0;
  if (std::isfinite(max_width) && max_width > 0 && metrics.width > max_width)
    horizontal_scale = max_width / metrics.width;
  float text_matrix[6] = {float(matrix[0]), float(matrix[1]), float(matrix[2]),
                          float(matrix[3]), float(matrix[4]), float(matrix[5])};
  if (horizontal_scale != 1.0) {
    const float scale_x = float(horizontal_scale);
    text_matrix[0] *= scale_x;
    text_matrix[2] *= scale_x;
    text_matrix[4] = float(matrix[0] * origin_x * (1.0 - horizontal_scale) + matrix[4]);
  }
  for (int i = 0; i < 6; ++i) matrix_out[i] = text_matrix[i];
  // processor.mBoundingBox (tight ink, CSS px) moved by mPt.
  if (ink && hasInk) {
    *hasInk = metrics.ink;
    ink[0] = metrics.ink_left + origin_x;
    ink[1] = metrics.ink_top + y;
    ink[2] = metrics.ink_right - metrics.ink_left;
    ink[3] = metrics.ink_bottom - metrics.ink_top;
  }
  if (!detail::Fonts(state).CanvasGlyphRuns(text, input, runs, size)) return false;
  *draw = true;
  return true;
}
}  // namespace ruyijs::web_canvas
