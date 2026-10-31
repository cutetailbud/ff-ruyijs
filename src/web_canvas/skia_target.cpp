// A port of the parts of gfx/2d/DrawTargetSkia.cpp a canvas FillRect runs:
// GradientStopsSkia, SetPaintPattern, AutoPaintSetup and FillRect, drawing
// into Firefox's Skia with the B8G8R8A8 premultiplied surface it uses.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwrite.h>
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "usp10.lib")
#endif

#include "web_canvas/skia_target.h"
#if defined(__APPLE__)
#include "web_geometry/font_backend.h"
#include "include/ports/SkTypeface_mac.h"
#endif
#include "web_geometry/glyph_mask.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/core/SkSurfaceProps.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#ifdef _WIN32
#include "include/ports/SkTypeface_win.h"
#endif
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#include "include/private/base/SkTFitsIn.h"
#include "src/core/SkMask.h"
#include "src/core/SkMaskBlurFilter.h"

namespace ruyijs::web_canvas::skia {
namespace {
struct BlendName { const char* name; SkBlendMode mode; bool boundByMask; };
// The canvas spelling -> gfx::CompositionOp (CanvasRenderingContext2D
// SetGlobalCompositeOperation) -> SkBlendMode (HelpersSkia GfxOpToSkiaOp);
// boundByMask is gfx/2d/Tools.h IsOperatorBoundByMask.
constexpr BlendName kBlends[] = {
    {"source-over", SkBlendMode::kSrcOver, true},
    {"source-in", SkBlendMode::kSrcIn, false},
    {"source-out", SkBlendMode::kSrcOut, false},
    {"source-atop", SkBlendMode::kSrcATop, true},
    {"destination-over", SkBlendMode::kDstOver, true},
    {"destination-in", SkBlendMode::kDstIn, false},
    {"destination-out", SkBlendMode::kDstOut, true},
    {"destination-atop", SkBlendMode::kDstATop, false},
    {"lighter", SkBlendMode::kPlus, true},
    {"copy", SkBlendMode::kSrc, false},
    {"xor", SkBlendMode::kXor, true},
    {"multiply", SkBlendMode::kMultiply, true},
    {"screen", SkBlendMode::kScreen, true},
    {"overlay", SkBlendMode::kOverlay, true},
    {"darken", SkBlendMode::kDarken, true},
    {"lighten", SkBlendMode::kLighten, true},
    {"color-dodge", SkBlendMode::kColorDodge, true},
    {"color-burn", SkBlendMode::kColorBurn, true},
    {"hard-light", SkBlendMode::kHardLight, true},
    {"soft-light", SkBlendMode::kSoftLight, true},
    {"difference", SkBlendMode::kDifference, true},
    {"exclusion", SkBlendMode::kExclusion, true},
    {"hue", SkBlendMode::kHue, true},
    {"saturation", SkBlendMode::kSaturation, true},
    {"color", SkBlendMode::kColor, true},
    {"luminosity", SkBlendMode::kLuminosity, true},
};

const BlendName* FindBlend(const std::string& name) {
  for (const auto& entry : kBlends)
    if (name == entry.name) return &entry;
  return nullptr;
}

// HelpersSkia.h ColorFloatToByte.
U8CPU ColorFloatToByte(float color) { return U8CPU(color * 255.f + .5f); }
constexpr double kPi = 3.14159265358979323846;  // M_PI

SkColor4f ToSkColor4f(const Color& color) { return SkColor4f{color.r, color.g, color.b, color.a}; }

// GradientStopsSkia: DrawTargetSkia::CreateGradientStops stable-sorts by
// offset, then Skia gets explicit 0 and 1 stops (ExtendMode::CLAMP).
struct GradientStops {
  std::vector<SkColor4f> colors;
  std::vector<float> positions;
  int rawCount = 0;
  explicit GradientStops(std::vector<Stop> stops) : rawCount(int(stops.size())) {
    if (stops.empty()) return;
    std::stable_sort(stops.begin(), stops.end(),
                     [](const Stop& a, const Stop& b) { return a.offset < b.offset; });
    if (stops.front().offset != 0) {
      colors.push_back(ToSkColor4f(stops.front().color));
      positions.push_back(0);
    }
    for (const auto& stop : stops) {
      colors.push_back(ToSkColor4f(stop.color));
      positions.push_back(stop.offset);
    }
    if (stops.back().offset != 1) {
      colors.push_back(ToSkColor4f(stops.back().color));
      positions.push_back(1);
    }
  }
  // GradientStopsSkia::mCount counts the inserted 0/1 stops too.
  int count() const { return int(colors.size()); }
  SkGradient Gradient() const {
    return SkGradient(SkGradient::Colors(SkSpan(colors.data(), colors.size()),
                                         SkSpan(positions.data(), positions.size()),
                                         SkTileMode::kClamp),
                      SkGradient::Interpolation());
  }
};

// A premultiplied RGBA source as the B8G8R8A8 (or X8) SkImage Firefox holds.
sk_sp<SkImage> MakeImage(const uint8_t* pixels, int width, int height, bool opaque) {
  if (!pixels || width <= 0 || height <= 0) return nullptr;
  const size_t count = size_t(width) * size_t(height);
  std::vector<uint8_t> bgra(pixels, pixels + count * 4);
  for (size_t i = 0; i < count; ++i) std::swap(bgra[i * 4], bgra[i * 4 + 2]);
  const SkImageInfo info = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType,
                                             opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType);
  return SkImages::RasterFromPixmapCopy(SkPixmap(info, bgra.data(), size_t(width) * 4));
}

// SetPaintPattern, PatternType::SURFACE: CanvasGeneralPattern::ForStyle maps
// no-repeat to CLAMP and every other repetition to REPEAT; the pattern matrix
// is the CanvasPattern transform.
void SetSurfacePattern(SkPaint& paint, const Pattern& pattern) {
  sk_sp<SkImage> image =
      MakeImage(pattern.pixels.data(), pattern.width, pattern.height, pattern.opaque);
  if (!image) {
    paint.setColor(SK_ColorTRANSPARENT);
    return;
  }
  const float* m = pattern.matrix;
  SkMatrix matrix;
  matrix.setAll(m[0], m[2], m[4], m[1], m[3], m[5], 0, 0, 1);
  const SkTileMode tile =
      pattern.repeat == Pattern::Repeat::NoRepeat ? SkTileMode::kClamp : SkTileMode::kRepeat;
  sk_sp<SkShader> shader = image->makeShader(
      tile, tile, SkSamplingOptions(pattern.linear ? SkFilterMode::kLinear : SkFilterMode::kNearest),
      matrix);
  if (shader) paint.setShader(shader); else paint.setColor(SK_ColorTRANSPARENT);
}

bool Finite(float value) { return std::isfinite(value); }

// SetPaintPattern for colour and gradient patterns (pattern mMatrix is the
// identity for canvas gradients, and no extra matrix is passed).
void SetPaintPattern(SkPaint& paint, const Pattern& pattern, float alpha) {
  const float* g = pattern.geometry;
  switch (pattern.kind) {
    case Pattern::Kind::Color:
      paint.setColor(SkColorSetARGB(ColorFloatToByte(pattern.color.a * alpha),
                                    ColorFloatToByte(pattern.color.r),
                                    ColorFloatToByte(pattern.color.g),
                                    ColorFloatToByte(pattern.color.b)));
      return;
    case Pattern::Kind::Linear: {
      GradientStops stops(pattern.stops);
      if (stops.count() < 2 || !Finite(g[0]) || !Finite(g[1]) || !Finite(g[2]) ||
          !Finite(g[3]) || (g[0] == g[2] && g[1] == g[3])) {
        paint.setColor(SK_ColorTRANSPARENT);
        return;
      }
      SkPoint points[2] = {SkPoint::Make(g[0], g[1]), SkPoint::Make(g[2], g[3])};
      SkMatrix matrix;
      sk_sp<SkShader> shader = SkShaders::LinearGradient(points, stops.Gradient(), &matrix);
      if (shader) paint.setShader(shader); else paint.setColor(SK_ColorTRANSPARENT);
      return;
    }
    case Pattern::Kind::Radial: {
      GradientStops stops(pattern.stops);
      if (stops.count() < 2 || !Finite(g[0]) || !Finite(g[1]) || !Finite(g[2]) ||
          !Finite(g[3]) || !Finite(g[4]) || !Finite(g[5]) ||
          (g[0] == g[3] && g[1] == g[4] && g[2] == g[5])) {
        paint.setColor(SK_ColorTRANSPARENT);
        return;
      }
      SkMatrix matrix;
      sk_sp<SkShader> shader = SkShaders::TwoPointConicalGradient(
          SkPoint::Make(g[0], g[1]), g[2], SkPoint::Make(g[3], g[4]), g[5],
          stops.Gradient(), &matrix);
      if (shader) paint.setShader(shader); else paint.setColor(SK_ColorTRANSPARENT);
      return;
    }
    case Pattern::Kind::Surface:
      SetSurfacePattern(paint, pattern);
      return;
    case Pattern::Kind::Conic: {
      GradientStops stops(pattern.stops);
      if (stops.count() < 2 || !Finite(g[1]) || !Finite(g[2]) || !Finite(g[0])) {
        paint.setColor(SK_ColorTRANSPARENT);
        return;
      }
      SkMatrix matrix;
      const SkPoint center = SkPoint::Make(g[1], g[2]);
      // Skia's sweep angles are relative to the x-axis, not the y-axis.
      const float angle = float(double(g[0]) * 180.0 / kPi - 90.0);
      if (angle != 0.0) matrix.preRotate(angle, center.x(), center.y());
      // CanvasGeneralPattern passes start offset 0 and end offset 1.
      sk_sp<SkShader> shader =
          SkShaders::SweepGradient(center, 360 * 0.0f, 360 * 1.0f, stops.Gradient(), &matrix);
      if (shader) paint.setShader(shader); else paint.setColor(SK_ColorTRANSPARENT);
      return;
    }
  }
}

// DrawTargetSkia GetClipBounds: device clip bounds mapped back to user space.
bool ClipBounds(SkCanvas* canvas, SkRect* out) {
  SkIRect device;
  if (!canvas->getDeviceClipBounds(&device)) { *out = SkRect::MakeEmpty(); return true; }
  SkMatrix inverse;
  if (!canvas->getTotalMatrix().invert(&inverse)) { *out = SkRect::MakeEmpty(); return true; }
  inverse.mapRect(out, SkRect::Make(device));
  return true;
}

// gfx::Rect::Contains(Rect): an empty rect is contained by any rect.
bool Contains(const SkRect& outer, const SkRect& inner) {
  if (inner.isEmpty()) return true;
  return inner.fLeft >= outer.fLeft && inner.fRight <= outer.fRight &&
         inner.fTop >= outer.fTop && inner.fBottom <= outer.fBottom;
}

// The canvas store is RGBA; DrawTargetSkia's surface is B8G8R8A8. Swapping
// red and blue around the draw keeps every byte value Skia computes.
void SwapRedBlue(uint8_t* pixels, size_t count) {
  for (size_t i = 0; i < count; ++i) std::swap(pixels[i * 4], pixels[i * 4 + 2]);
}

// gfxPlatform::Init calls SkGraphics::Init once: it caches the CPU features
// and installs the SSE/AVX implementations Skia dispatches to. Without it the
// baseline routines run and some pixels round differently.
void InitializeSkia() {
  static const bool initialized = [] {
    SkGraphics::Init();
    return true;
  }();
  (void)initialized;
}

// DrawTargetSkia::Init + SetTransform over the canvas bytes.
struct Target {
  Surface surface;
  sk_sp<SkSurface> sk;
  SkCanvas* canvas = nullptr;
  Target(const Surface& s, const double transform[6]) : surface(s) {
    InitializeSkia();
    const SkImageInfo info = SkImageInfo::Make(s.width, s.height, kBGRA_8888_SkColorType,
                                               kPremul_SkAlphaType);
    // UpdateSurfaceProps on Windows: no flags, RGB subpixel order, no
    // contrast, linear gamma.
    const SkSurfaceProps props(0, kRGB_H_SkPixelGeometry, 0, SK_Scalar1);
    SwapRedBlue(s.pixels, size_t(s.width) * size_t(s.height));
    sk = SkSurfaces::WrapPixels(info, s.pixels, size_t(s.width) * 4, &props);
    if (!sk) return;
    canvas = sk->getCanvas();
    // GfxMatrixToSkiaMatrix over the float gfx::Matrix.
    SkMatrix matrix;
    matrix.setAll(float(transform[0]), float(transform[2]), float(transform[4]),
                  float(transform[1]), float(transform[3]), float(transform[5]), 0, 0, 1);
    canvas->setMatrix(matrix);
  }
  ~Target() {
    sk.reset();
    SwapRedBlue(surface.pixels, size_t(surface.width) * size_t(surface.height));
  }
};
// AutoPaintSetup: blend mode, anti-aliasing and global alpha, with a
// transparency layer for an operator not bound by the mask when the mask
// bounds do not cover the clip (no mask bounds: always).
struct AutoPaint {
  SkPaint paint;
  SkCanvas* canvas;
  bool restore = false;
  float alpha = 1.0f;
  AutoPaint(SkCanvas* c, const BlendName& blend, const DrawOptions& options,
            const SkRect* maskBounds)
      : canvas(c) {
    paint.setBlendMode(blend.mode);
    paint.setAntiAlias(options.antialias);
    bool needsGroup = false;
    if (!blend.boundByMask) {
      SkRect clip;
      ClipBounds(canvas, &clip);
      needsGroup = !maskBounds || !Contains(*maskBounds, clip);
    }
    if (needsGroup) {
      paint.setBlendMode(SkBlendMode::kSrcOver);
      SkPaint layer;
      layer.setBlendMode(blend.mode);
      layer.setAlpha(ColorFloatToByte(options.alpha));
      SkCanvas::SaveLayerRec record(nullptr, &layer, SkCanvas::kPreserveLCDText_SaveLayerFlag);
      canvas->saveLayer(record);
      restore = true;
    } else {
      paint.setAlpha(ColorFloatToByte(options.alpha));
      alpha = options.alpha;
    }
  }
  ~AutoPaint() {
    if (restore) canvas->restore();
  }
};

// ---- gfx::Point / gfx::Matrix (float) ----
struct Point {
  float x = 0, y = 0;
  bool operator==(const Point& o) const { return x == o.x && y == o.y; }
  bool operator!=(const Point& o) const { return !(*this == o); }
  bool IsFinite() const { return std::isfinite(x) && std::isfinite(y); }
  Point operator+(const Point& o) const { return {x + o.x, y + o.y}; }
  Point operator-(const Point& o) const { return {x - o.x, y - o.y}; }
  Point operator*(float f) const { return {x * f, y * f}; }
};
struct Matrix {
  float _11 = 1, _12 = 0, _21 = 0, _22 = 1, _31 = 0, _32 = 0;
  static Matrix From(const double m[6]) {
    return {float(m[0]), float(m[1]), float(m[2]), float(m[3]), float(m[4]), float(m[5])};
  }
  bool ExactlyEquals(const Matrix& o) const {
    return _11 == o._11 && _12 == o._12 && _21 == o._21 && _22 == o._22 && _31 == o._31 &&
           _32 == o._32;
  }
  Matrix operator*(const Matrix& m) const {
    Matrix r;
    r._11 = _11 * m._11 + _12 * m._21;
    r._12 = _11 * m._12 + _12 * m._22;
    r._21 = _21 * m._11 + _22 * m._21;
    r._22 = _21 * m._12 + _22 * m._22;
    r._31 = _31 * m._11 + _32 * m._21 + m._31;
    r._32 = _31 * m._12 + _32 * m._22 + m._32;
    return r;
  }
  bool Invert() {
    const float A = _22, B = -_21, C = _21 * _32 - _22 * _31;
    const float D = -_12, E = _11, F = _31 * _12 - _11 * _32;
    const float det = _11 * _22 - _12 * _21;
    if (!det) return false;
    const float inv = 1 / det;
    _11 = inv * A; _12 = inv * D; _21 = inv * B; _22 = inv * E; _31 = inv * C; _32 = inv * F;
    return true;
  }
  Point TransformPoint(const Point& p) const {
    return {p.x * _11 + p.y * _21 + _31, p.x * _12 + p.y * _22 + _32};
  }
  static Matrix Rotation(float angle) {
    const float s = sinf(angle), c = cosf(angle);
    Matrix m;
    m._11 = c; m._12 = s; m._21 = -s; m._22 = c;
    return m;
  }
  SkMatrix ToSkia() const {
    SkMatrix m;
    m.setAll(_11, _21, _31, _12, _22, _32, 0, 0, 1);
    return m;
  }
};

const float kKappaFactor = 0.55191497064665766025f;
// PathHelpers.h ComputeKappaFactor.
float ComputeKappaFactor(float angle) { return (4.0f / 3.0f) * tanf(angle / 4.0f); }

// ---- PathBuilderSkia ----
struct Builder {
  SkPathBuilder path;
  Point current, begin;
  bool IsActive() const { return path.countPoints() > 0; }
  void MoveTo(const Point& p) {
    path.moveTo(p.x, p.y);
    current = p;
    begin = p;
  }
  void LineTo(const Point& p) {
    if (!path.countPoints()) MoveTo(p); else path.lineTo(p.x, p.y);
    current = p;
  }
  void BezierTo(const Point& a, const Point& b, const Point& c) {
    if (!path.countPoints()) MoveTo(a);
    path.cubicTo(a.x, a.y, b.x, b.y, c.x, c.y);
    current = c;
  }
  void QuadraticBezierTo(const Point& a, const Point& b) {
    if (!path.countPoints()) MoveTo(a);
    path.quadTo(a.x, a.y, b.x, b.y);
    current = b;
  }
  void Close() {
    path.close();
    current = begin;
  }
  void Transform(const Matrix& m) {
    path.transform(m.ToSkia());
    current = m.TransformPoint(current);
    begin = m.TransformPoint(begin);
  }
  void Arc(const Point& origin, float radius, float start, float end, bool anticlockwise);
};

// PathHelpers.h PartialArcToBezier / ArcToBezier over any sink.
template <typename T>
void PartialArcToBezier(T* sink, const Point& startOffset, const Point& endOffset,
                        const Matrix& transform, float kappa = kKappaFactor) {
  const Point cp1 = startOffset + Point{-startOffset.y, startOffset.x} * kappa;
  const Point cp2 = endOffset + Point{endOffset.y, -endOffset.x} * kappa;
  sink->BezierTo(transform.TransformPoint(cp1), transform.TransformPoint(cp2),
                 transform.TransformPoint(endOffset));
}

template <typename T>
void ArcToBezier(T* sink, const Point& origin, float radiusX, float radiusY, float startAngle,
                 float endAngle, bool anticlockwise, float rotation = 0.0f) {
  const float sweepDirection = anticlockwise ? -1.0f : 1.0f;
  float arcSweepLeft = (endAngle - startAngle) * sweepDirection;
  if (arcSweepLeft < 0) {
    arcSweepLeft = float(2.0f * kPi) + fmodf(arcSweepLeft, float(2.0f * kPi));
    startAngle = endAngle - arcSweepLeft * sweepDirection;
  } else if (arcSweepLeft > float(2.0f * kPi)) {
    arcSweepLeft = float(2.0f * kPi);
  }
  float currentStartAngle = startAngle;
  Point currentStartOffset{cosf(startAngle), sinf(startAngle)};
  Matrix transform;  // Matrix::Scaling(radius)
  transform._11 = radiusX;
  transform._22 = radiusY;
  if (rotation != 0.0f) transform = transform * Matrix::Rotation(rotation);
  transform._31 += origin.x;  // PostTranslate(origin)
  transform._32 += origin.y;
  transform = transform * Matrix();  // *= aTransform (identity)
  sink->LineTo(transform.TransformPoint(currentStartOffset));
  while (arcSweepLeft > 0) {
    const float currentEndAngle =
        currentStartAngle + std::min(arcSweepLeft, float(kPi / 2.0f)) * sweepDirection;
    const Point currentEndOffset{cosf(currentEndAngle), sinf(currentEndAngle)};
    PartialArcToBezier(sink, currentStartOffset, currentEndOffset, transform,
                       ComputeKappaFactor(currentEndAngle - currentStartAngle));
    arcSweepLeft -= float(kPi / 2.0f);
    currentStartAngle = currentEndAngle;
    currentStartOffset = currentEndOffset;
  }
}

void Builder::Arc(const Point& origin, float radius, float start, float end,
                  bool anticlockwise) {
  ArcToBezier(this, origin, radius, radius, start, end, anticlockwise);
}

// CanvasRenderingContext2D's path members (path2d: CanvasPath's), with the
// zero-length segment pruning and the path transform bookkeeping.
struct CanvasPathState {
  Builder builder;
  bool pruned = false;
  bool path2d = false;
  bool hasPath = false;  // mPath || mPathBuilder
  Matrix pathTransform;

  // FlushPathTransform for the transform current at this call.
  void Flush(const Matrix& current) {
    if (hasPath) {
      Matrix inverse = current;
      if (!inverse.ExactlyEquals(pathTransform) && inverse.Invert())
        builder.Transform(pathTransform * inverse);
    }
    pathTransform = current;
  }
  void EnsureWritable(const Matrix& current) {
    if (!path2d) Flush(current);
    hasPath = true;
  }
  void EnsureCapped() {
    if (pruned) {
      builder.LineTo(builder.current);
      pruned = false;
    }
  }
  void EnsureActive() {
    if (pruned && !builder.IsActive()) {
      builder.MoveTo(builder.current);
      pruned = false;
    }
  }
  // The Point overloads the ellipse sink and the members share.
  void LineTo(const Point& p) {
    if (!p.IsFinite()) return;
    if (builder.current == p) { pruned = true; return; }
    EnsureActive();
    builder.LineTo(p);
    pruned = false;
  }
  void BezierTo(const Point& a, const Point& b, const Point& c) {
    if (!a.IsFinite() || !b.IsFinite() || !c.IsFinite()) return;
    if (a == builder.current && a == b && a == c) { pruned = true; return; }
    EnsureActive();
    builder.BezierTo(a, b, c);
    pruned = false;
  }
  void MoveTo(double x, double y) {
    const Point p{float(x), float(y)};
    if (!p.IsFinite()) return;
    EnsureCapped();
    builder.MoveTo(p);
  }
  void Close() {
    builder.Close();
    pruned = false;
  }
  void Quadratic(double cpx, double cpy, double x, double y) {
    const Point cp1{float(cpx), float(cpy)}, cp2{float(x), float(y)};
    if (!cp1.IsFinite() || !cp2.IsFinite()) return;
    if (cp1 == builder.current && cp1 == cp2) { pruned = true; return; }
    EnsureActive();
    builder.QuadraticBezierTo(cp1, cp2);
    pruned = false;
  }
  void Rect(double x, double y, double w, double h) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h)) return;
    if (path2d) {
      // CanvasPath::Rect goes through its own pruning members.
      MoveTo(x, y);
      if (w == 0 && h == 0) return;
      LineTo(Point{float(x + w), float(y)});
      LineTo(Point{float(x + w), float(y + h)});
      LineTo(Point{float(x), float(y + h)});
      Close();
      return;
    }
    EnsureCapped();
    builder.MoveTo(Point{float(x), float(y)});
    if (w == 0 && h == 0) return;
    builder.LineTo(Point{float(x + w), float(y)});
    builder.LineTo(Point{float(x + w), float(y + h)});
    builder.LineTo(Point{float(x), float(y + h)});
    builder.Close();
  }
  void Arc(double x, double y, double r, double start, double end, bool anticlockwise) {
    EnsureActive();
    builder.Arc(Point{float(x), float(y)}, float(r), float(start), float(end), anticlockwise);
    pruned = false;
  }
  // RoundRectImpl after its argument checks, then AppendRoundedRectToPath on
  // the builder itself (no pruning), for both the context and CanvasPath.
  void RoundRect(const double* a) {
    struct Size { float width, height; };
    EnsureCapped();
    float x = float(a[0]), y = float(a[1]), w = float(a[2]), h = float(a[3]);
    Size upperLeft{float(a[4]), float(a[5])}, upperRight{float(a[6]), float(a[7])};
    Size lowerRight{float(a[8]), float(a[9])}, lowerLeft{float(a[10]), float(a[11])};
    bool clockwise = true;
    if (w < 0) {
      clockwise = false;
      x += w;
      w = -w;
      std::swap(upperLeft, upperRight);
      std::swap(lowerLeft, lowerRight);
    }
    if (h < 0) {
      clockwise = !clockwise;
      y += h;
      h = -h;
      std::swap(upperLeft, lowerLeft);
      std::swap(upperRight, lowerRight);
    }
    const float top = upperLeft.width + upperRight.width;
    const float right = upperRight.height + lowerRight.height;
    const float bottom = lowerRight.width + lowerLeft.width;
    const float left = upperLeft.height + lowerLeft.height;
    const float scale = std::min({w / top, h / right, w / bottom, h / left});
    if (scale < 1.0f) {
      for (Size* s : {&upperLeft, &upperRight, &lowerLeft, &lowerRight}) {
        s->width = s->width * scale;
        s->height = s->height * scale;
      }
    }
    // AppendRoundedRectToPath(builder, Rect(x, y, w, h), radii, clockwise).
    const float alpha = float(0.55191497064665766025);
    struct TwoFloats { float a, b; };
    const TwoFloats cwCornerMults[4] = {{-1, 0}, {0, -1}, {+1, 0}, {0, +1}};
    const TwoFloats ccwCornerMults[4] = {{+1, 0}, {0, -1}, {-1, 0}, {0, +1}};
    const TwoFloats* cornerMults = clockwise ? cwCornerMults : ccwCornerMults;
    const Point cornerCoords[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    const Size radii[4] = {upperLeft, upperRight, lowerRight, lowerLeft};
    if (clockwise) builder.MoveTo(Point{x + radii[0].width, y});
    else builder.MoveTo(Point{x + w - radii[1].width, y});
    for (int i = 0; i < 4; ++i) {
      const int c = clockwise ? ((i + 1) % 4) : ((4 - i) % 4);
      const int i2 = (i + 2) % 4;
      const int i3 = (i + 3) % 4;
      const Point pc = cornerCoords[c];
      if (radii[c].width > 0.0 && radii[c].height > 0.0) {
        Point p0, p1, p2, p3;
        p0.x = pc.x + cornerMults[i].a * radii[c].width;
        p0.y = pc.y + cornerMults[i].b * radii[c].height;
        p3.x = pc.x + cornerMults[i3].a * radii[c].width;
        p3.y = pc.y + cornerMults[i3].b * radii[c].height;
        p1.x = p0.x + alpha * cornerMults[i2].a * radii[c].width;
        p1.y = p0.y + alpha * cornerMults[i2].b * radii[c].height;
        p2.x = p3.x - alpha * cornerMults[i3].a * radii[c].width;
        p2.y = p3.y - alpha * cornerMults[i3].b * radii[c].height;
        builder.LineTo(p0);
        builder.BezierTo(p1, p2, p3);
      } else {
        builder.LineTo(pc);
      }
    }
    builder.Close();
  }
  // ArcTo: the tangent arc computed in double precision from the user-space
  // current point, then LineTo + the canvas Arc member.
  void ArcTo(double x1, double y1, double x2, double y2, double radius) {
    const Point p0 = builder.current;
    const Point p1{float(x1), float(y1)}, p2{float(x2), float(y2)};
    if (!p1.IsFinite() || !p2.IsFinite() || !std::isfinite(radius)) return;
    if (p0 == p1 || p1 == p2 || radius == 0) { LineTo(p1); return; }
    const double dir = (p2.x - p1.x) * (p0.y - p1.y) + (p2.y - p1.y) * (p1.x - p0.x);
    if (dir == 0) { LineTo(p1); return; }
    const double a2 = (p0.x - x1) * (p0.x - x1) + (p0.y - y1) * (p0.y - y1);
    const double b2 = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
    const double c2 = (p0.x - x2) * (p0.x - x2) + (p0.y - y2) * (p0.y - y2);
    const double cosx = (a2 + b2 - c2) / (2 * std::sqrt(a2 * b2));
    const double sinx = std::sqrt(1 - cosx * cosx);
    const double d = radius / ((1 - cosx) / sinx);
    const double anx = (x1 - p0.x) / std::sqrt(a2);
    const double any = (y1 - p0.y) / std::sqrt(a2);
    const double bnx = (x1 - x2) / std::sqrt(b2);
    const double bny = (y1 - y2) / std::sqrt(b2);
    const double x3 = x1 - anx * d, y3 = y1 - any * d;
    const double x4 = x1 - bnx * d, y4 = y1 - bny * d;
    const bool anticlockwise = dir < 0;
    const double cx = x3 + any * radius * (anticlockwise ? 1 : -1);
    const double cy = y3 - anx * radius * (anticlockwise ? 1 : -1);
    const double angle0 = std::atan2(y3 - cy, x3 - cx);
    const double angle1 = std::atan2(y4 - cy, x4 - cx);
    LineTo(Point{float(x3), float(y3)});
    if (angle0 == angle1) {
      LineTo(Point{float(cx + radius * std::cos(angle0)), float(cy + radius * std::sin(angle0))});
      return;
    }
    Arc(cx, cy, radius, angle0, angle1, anticlockwise);
  }
  void Ellipse(double x, double y, double rx, double ry, double rotation, double start,
               double end, bool anticlockwise) {
    // The context (or CanvasPath) itself is ArcToBezier's sink here.
    ArcToBezier(this, Point{float(x), float(y)}, float(rx), float(ry), float(start),
                float(end), anticlockwise, float(rotation));
    pruned = false;
  }
};

// Opcodes of web_canvas/context2d_path.cpp.
enum : int { kOpMove = 0, kOpLine, kOpClose, kOpRect, kOpArc, kOpEllipse, kOpQuadratic,
             kOpBezier, kOpPrefix, kOpFinish, kOpArcTo, kOpRoundRect };

// Replays the record, then EnsureUserSpacePath (or CanvasPath::GetPath) for
// the transform current at the draw: the user-space path, capped.
void ReplayPath(const PathRecord& record, const Matrix& drawTransform, CanvasPathState* state) {
  state->path2d = record.path2d;
  const double* d = record.data;
  size_t i = 0;
  Matrix current;
  while (i < record.size) {
    const int op = int(d[i]);
    if (op == kOpPrefix) {
      if (i + 7 > record.size) break;
      current = Matrix::From(d + i + 1);
      i += 7;
      continue;
    }
    const double* a = d + i + 1;
    const size_t argc = op == kOpMove || op == kOpLine ? 2
                      : op == kOpRect || op == kOpQuadratic ? 4
                      : op == kOpArc || op == kOpBezier ? 6 : op == kOpEllipse ? 8
                      : op == kOpArcTo ? 5 : op == kOpRoundRect ? 12 : 0;
    if (i + 1 + argc > record.size) break;
    i += 1 + argc;
    if (op == kOpFinish) {
      // An earlier fill/stroke/clip: EnsureUserSpacePath capped the path.
      if (!state->path2d) state->Flush(current);
      if (state->hasPath) state->EnsureCapped();
      continue;
    }
    state->EnsureWritable(current);
    if (op == kOpArc && a[3] == a[4]) {
      // Arc with equal angles: LineTo(x + r cos(start), y + r sin(start)).
      state->LineTo(Point{float(a[0] + a[2] * std::cos(a[3])),
                          float(a[1] + a[2] * std::sin(a[3]))});
      continue;
    }
    switch (op) {
      case kOpMove: state->MoveTo(a[0], a[1]); break;
      case kOpLine: state->LineTo(Point{float(a[0]), float(a[1])}); break;
      case kOpClose: state->Close(); break;
      case kOpRect: state->Rect(a[0], a[1], a[2], a[3]); break;
      case kOpArc: state->Arc(a[0], a[1], a[2], a[3], a[4], a[5] != 0); break;
      case kOpEllipse:
        state->Ellipse(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7] != 0);
        break;
      case kOpQuadratic: state->Quadratic(a[0], a[1], a[2], a[3]); break;
      case kOpArcTo: state->ArcTo(a[0], a[1], a[2], a[3], a[4]); break;
      case kOpRoundRect: state->RoundRect(a); break;
      case kOpBezier:
        state->BezierTo(Point{float(a[0]), float(a[1])}, Point{float(a[2]), float(a[3])},
                        Point{float(a[4]), float(a[5])});
        break;
      default: break;
    }
  }
  if (!state->path2d) state->Flush(drawTransform);
  state->hasPath = true;
  state->EnsureCapped();
}

// The drawing state's clips: PushClip of each user-space path under the
// transform current at clip(), then the draw's own transform.
void ApplyClips(Target& target, const Surface& surface, const double transform[6]) {
  if (!surface.clips || surface.clips->empty()) return;
  for (const Clip& clip : *surface.clips) {
    CanvasPathState state;
    const PathRecord record{clip.commands.data(), clip.commands.size(), clip.path2d};
    const Matrix matrix = Matrix::From(clip.matrix);
    ReplayPath(record, matrix, &state);
    state.builder.path.setFillType(clip.evenOdd ? SkPathFillType::kEvenOdd
                                                : SkPathFillType::kWinding);
    target.canvas->setMatrix(matrix.ToSkia());
    target.canvas->save();
    target.canvas->clipPath(state.builder.path.detach(), SkClipOp::kIntersect, true);
  }
  target.canvas->setMatrix(Matrix::From(transform).ToSkia());
}

bool IsRectilinear(const Matrix& m) {
  auto fuzzy = [](float a, float b) { return std::fabs(b - a) < 1e-6; };
  if (fuzzy(m._12, 0) && fuzzy(m._21, 0)) return true;
  return fuzzy(m._22, 0) && fuzzy(m._11, 0);
}
bool HasNonAxisAlignedTransform(const Matrix& m) {
  return !(std::fabs(0.0 - m._21) < 1e-6) || !(std::fabs(0.0 - m._12) < 1e-6);
}

// HelpersSkia.h StrokeOptionsToPaint.
bool StrokeToPaint(SkPaint& paint, const StrokeStyle& stroke) {
  if (!stroke.lineWidth || !std::isfinite(stroke.lineWidth)) return false;
  paint.setStrokeWidth(stroke.lineWidth);
  paint.setStrokeMiter(stroke.miterLimit);
  paint.setStrokeCap(stroke.cap == "round" ? SkPaint::kRound_Cap
                     : stroke.cap == "square" ? SkPaint::kSquare_Cap : SkPaint::kButt_Cap);
  paint.setStrokeJoin(stroke.join == "round" ? SkPaint::kRound_Join
                      : stroke.join == "bevel" ? SkPaint::kBevel_Join : SkPaint::kMiter_Join);
  if (!stroke.dash.empty()) {
    const size_t length = stroke.dash.size();
    const size_t count = length % 2 == 0 ? length : length * 2;
    std::vector<SkScalar> pattern(count);
    for (size_t i = 0; i < count; ++i) pattern[i] = stroke.dash[i % length];
    paint.setPathEffect(SkDashPathEffect::Make({pattern.data(), count}, stroke.dashOffset));
  }
  paint.setStyle(SkPaint::kStroke_Style);
  return true;
}

// ---- Shadows: DrawTarget::DrawShadow + DrawTargetSkia::DrawSurfaceWithShadow ----

// gfx::Rect (float x, y, width, height).
struct FloatRect {
  float x = 0, y = 0, width = 0, height = 0;
  bool IsEmpty() const { return height <= 0 || width <= 0; }
};

// BaseMatrix::TransformBounds.
FloatRect TransformBounds(const Matrix& m, const FloatRect& r) {
  const Point quad[4] = {m.TransformPoint({r.x, r.y}), m.TransformPoint({r.x + r.width, r.y}),
                         m.TransformPoint({r.x, r.y + r.height}),
                         m.TransformPoint({r.x + r.width, r.y + r.height})};
  float minX = quad[0].x, maxX = quad[0].x, minY = quad[0].y, maxY = quad[0].y;
  for (int i = 1; i < 4; i++) {
    if (quad[i].x < minX) minX = quad[i].x;
    if (quad[i].x > maxX) maxX = quad[i].x;
    if (quad[i].y < minY) minY = quad[i].y;
    if (quad[i].y > maxY) maxY = quad[i].y;
  }
  return {minX, minY, maxX - minX, maxY - minY};
}

// BaseRect::Intersect.
FloatRect Intersect(const FloatRect& a, const FloatRect& b) {
  FloatRect r;
  r.x = std::max<float>(a.x, b.x);
  r.y = std::max<float>(a.y, b.y);
  r.width = std::min<float>(a.x - r.x + a.width, b.x - r.x + b.width);
  r.height = std::min<float>(a.y - r.y + a.height, b.y - r.y + b.height);
  if (r.width < 0 || r.height < 0) r.width = r.height = 0;
  return r;
}

// GaussianBlur::CalculateBlurRadius (one axis).
int32_t BlurRadius(float sigma) {
  constexpr double sqrt_2_PI = 0x1.40d931ff62705p+1;
  constexpr float kGaussianScaleFactor = float((3 * sqrt_2_PI / 4) * 1.5);
  return static_cast<int32_t>(std::floor(sigma * kGaussianScaleFactor + 0.5f));
}

void FreeBlurredMask(const void*, void* image) { SkMaskBuilder::FreeImage(image); }

// GaussianBlur::BlurAlphaMask.
sk_sp<SkImage> BlurAlphaMask(SkImage* alphaImage, float sigma, SkPoint& offset) {
  SkPixmap pixmap;
  if (sigma > 135.0f || alphaImage->colorType() != kAlpha_8_SkColorType ||
      !alphaImage->peekPixels(&pixmap) || !SkTFitsIn<uint32_t>(pixmap.rowBytes())) {
    return nullptr;
  }
  SkMask src(pixmap.addr8(0, 0), pixmap.bounds(), uint32_t(pixmap.rowBytes()),
             SkMask::kA8_Format);
  SkMaskBuilder dst;
  SkMaskBlurFilter blurFilter(sigma, sigma);
  if (blurFilter.hasNoBlur()) return nullptr;
  blurFilter.blur(src, &dst);
  if (dst.image() && dst.format() == SkMask::kA8_Format) {
    SkPixmap dstPixmap(SkImageInfo::MakeA8(dst.bounds().width(), dst.bounds().height()),
                       dst.image(), dst.rowBytes());
    if (sk_sp<SkImage> blurred = SkImages::RasterFromPixmap(dstPixmap, FreeBlurredMask,
                                                             dst.image())) {
      offset = SkPoint::Make(dst.bounds().left(), dst.bounds().top());
      return blurred;
    }
  }
  if (dst.image()) SkMaskBuilder::FreeImage(dst.image());
  return nullptr;
}

// DrawTarget::DrawShadow for a path filled (stroke null) or stroked with the
// pattern, composited with the draw's operator onto the canvas target.
void DrawShadow(Target& target, const Matrix& transform, const SkPath& path,
                const Pattern& pattern, const ShadowStyle& shadow, const DrawOptions& options,
                SkBlendMode op, const StrokeStyle* stroke) {
  // PathSkia::GetFastBounds (stroke bounds without the dash effect).
  if (!path.isFinite()) return;
  SkRect skBounds = path.getBounds();
  if (stroke) {
    StrokeStyle solid = *stroke;
    solid.dash.clear();
    SkPaint paint;
    if (!StrokeToPaint(paint, solid)) return;
    SkRect out = SkRect::MakeEmpty();
    skBounds = paint.computeFastStrokeBounds(skBounds, &out);
  }
  FloatRect bounds = TransformBounds(
      transform, {skBounds.x(), skBounds.y(), skBounds.width(), skBounds.height()});
  if (bounds.IsEmpty()) return;
  bounds.x += shadow.offsetX;
  bounds.y += shadow.offsetY;
  const int32_t blurRadius = BlurRadius(shadow.sigma);
  const float inflate = float(blurRadius);
  bounds = {bounds.x - inflate, bounds.y - inflate, bounds.width + 2 * inflate,
            bounds.height + 2 * inflate};
  {  // RoundOut
    const float x0 = std::floor(bounds.x), y0 = std::floor(bounds.y);
    const float x1 = std::ceil(bounds.x + bounds.width), y1 = std::ceil(bounds.y + bounds.height);
    bounds = {x0, y0, x1 - x0, y1 - y0};
  }
  const FloatRect viewport{-inflate, -inflate, float(target.surface.width) + 2 * inflate,
                           float(target.surface.height) + 2 * inflate};
  bounds = Intersect(bounds, viewport);
  if (bounds.IsEmpty()) return;
  const int ix = int(bounds.x), iy = int(bounds.y), iw = int(bounds.width), ih = int(bounds.height);
  if (float(ix) != bounds.x || float(iy) != bounds.y || float(iw) != bounds.width ||
      float(ih) != bounds.height) {
    return;  // !ToIntRect
  }
  // CreateSimilarDrawTarget(A8): zeroed rows of a 4-aligned stride.
  const size_t stride = (size_t(iw) + 3) & ~size_t(3);
  std::vector<uint8_t> mask(stride * size_t(ih), 0);
  const SkSurfaceProps props(0, kRGB_H_SkPixelGeometry, 0, SK_Scalar1);
  sk_sp<SkSurface> shadowSurface =
      SkSurfaces::WrapPixels(SkImageInfo::MakeA8(iw, ih), mask.data(), stride, &props);
  if (!shadowSurface) return;
  SkCanvas* shadowCanvas = shadowSurface->getCanvas();
  const Point offset{float(ix) - shadow.offsetX, float(iy) - shadow.offsetY};
  Matrix shadowTransform = transform;
  shadowTransform._31 += -offset.x;  // PostTranslate(-offset)
  shadowTransform._32 += -offset.y;
  shadowCanvas->setMatrix(shadowTransform.ToSkia());
  {
    DrawOptions maskOptions = options;
    maskOptions.antialias = blurRadius > 1 ? false : options.antialias;
    AutoPaint paint(shadowCanvas, kBlends[0], maskOptions, nullptr);  // OP_OVER
    SetPaintPattern(paint.paint, pattern, paint.alpha);
    if (stroke && !StrokeToPaint(paint.paint, *stroke)) return;
    if (!path.isFinite()) return;
    shadowCanvas->drawPath(path, paint.paint);
  }
  sk_sp<SkImage> snapshot = shadowSurface->makeImageSnapshot();
  if (!snapshot) return;
  // DrawSurfaceWithShadow(snapshot, offset, shadow, op).
  SkCanvas* canvas = target.canvas;
  canvas->save();
  canvas->resetMatrix();
  SkPaint shadowPaint;
  shadowPaint.setBlendMode(op);
  shadowPaint.setColor(SkColorSetARGB(ColorFloatToByte(shadow.color.a * 1.0f),
                                      ColorFloatToByte(shadow.color.r),
                                      ColorFloatToByte(shadow.color.g),
                                      ColorFloatToByte(shadow.color.b)));
  const int destX = int32_t(floorf(offset.x + shadow.offsetX + 0.5f));
  const int destY = int32_t(floorf(offset.y + shadow.offsetY + 0.5f));
  SkPoint blurOffset = SkPoint::Make(0, 0);
  sk_sp<SkImage> blurred = BlurAlphaMask(snapshot.get(), shadow.sigma, blurOffset);
  if (blurred) {
    canvas->drawImage(blurred, destX + blurOffset.x(), destY + blurOffset.y(),
                      SkSamplingOptions(SkFilterMode::kLinear), &shadowPaint);
  } else {
    shadowPaint.setImageFilter(SkImageFilters::Blur(shadow.sigma, shadow.sigma, nullptr));
    canvas->drawImage(snapshot, destX, destY, SkSamplingOptions(SkFilterMode::kLinear),
                      &shadowPaint);
  }
  canvas->restore();
}

// AppendRectToPath(clockwise) on a fresh builder: MakePathForRect.
SkPath RectPath(const SkRect& r) {
  SkPathBuilder builder;
  builder.moveTo(r.fLeft, r.fTop);
  builder.lineTo(r.fRight, r.fTop);
  builder.lineTo(r.fRight, r.fBottom);
  builder.lineTo(r.fLeft, r.fBottom);
  builder.close();
  return builder.detach();
}

bool Usable(const Surface& surface) {
  return surface.pixels && surface.width > 0 && surface.height > 0;
}
}  // namespace

bool FillRect(const Surface& surface, const double transform[6], const double input[4],
              const Pattern& pattern, const DrawOptions& options, const ShadowStyle* shadow) {
  const BlendName* blend = FindBlend(options.composite);
  if (!blend) return false;
  if (!Usable(surface)) return true;
  double rect[4] = {input[0], input[1], input[2], input[3]};
  const bool surfacePattern = pattern.kind == Pattern::Kind::Surface;
  const Matrix patternMatrix{pattern.matrix[0], pattern.matrix[1], pattern.matrix[2],
                             pattern.matrix[3], pattern.matrix[4], pattern.matrix[5]};
  if (surfacePattern) {
    // CanvasRenderingContext2D::FillRect: a non-repeating axis bounds the rect
    // to the transformed pattern for a rectilinear pattern transform.
    bool limitx = pattern.repeat == Pattern::Repeat::NoRepeat ||
                  pattern.repeat == Pattern::Repeat::RepeatY;
    bool limity = pattern.repeat == Pattern::Repeat::NoRepeat ||
                  pattern.repeat == Pattern::Repeat::RepeatX;
    if ((limitx || limity) && IsRectilinear(patternMatrix)) {
      const FloatRect bounds = TransformBounds(
          patternMatrix, {0, 0, float(pattern.width), float(pattern.height)});
      if (HasNonAxisAlignedTransform(patternMatrix)) std::swap(limitx, limity);
      if (limitx) {
        const double x2 = rect[0] + rect[2];
        rect[0] = std::max(rect[0], double(bounds.x));
        rect[2] = std::max(std::min(x2, double(bounds.x + bounds.width)) - rect[0], 0.0);
      }
      if (limity) {
        const double y2 = rect[1] + rect[3];
        rect[1] = std::max(rect[1], double(bounds.y));
        rect[3] = std::max(std::min(y2, double(bounds.y + bounds.height)) - rect[1], 0.0);
      }
    }
  }
  // gfx::Rect is float; the fill rect is also the mask bounds.
  const SkRect bounds = SkRect::MakeXYWH(float(rect[0]), float(rect[1]), float(rect[2]),
                                         float(rect[3]));
  // DrawTargetSkia::FillRect routes an untiled, unclamped surface pattern to
  // DrawSurface (the sprite blitter) for operators other than source.
  bool asSurface = false;
  double source[4] = {0, 0, 0, 0};
  if (surfacePattern && blend->mode != SkBlendMode::kSrc &&
      (blend->mode != SkBlendMode::kSrcOver || !pattern.opaque) &&
      !HasNonAxisAlignedTransform(patternMatrix)) {
    FloatRect patRect{bounds.x() - patternMatrix._31, bounds.y() - patternMatrix._32,
                      bounds.width(), bounds.height()};
    const float scaleX = 1.0f / patternMatrix._11, scaleY = 1.0f / patternMatrix._22;
    patRect = {patRect.x * scaleX, patRect.y * scaleY, patRect.width * scaleX,
               patRect.height * scaleY};
    const float x0 = std::floor(patRect.x), y0 = std::floor(patRect.y);
    const float x1 = std::ceil(patRect.x + patRect.width);
    const float y1 = std::ceil(patRect.y + patRect.height);
    const int ix0 = int32_t(x0), iy0 = int32_t(y0), iw = int32_t(x1 - x0), ih = int32_t(y1 - y0);
    const bool contained = iw <= 0 || ih <= 0 ||
                           (0 <= ix0 && ix0 + iw <= pattern.width && 0 <= iy0 &&
                            iy0 + ih <= pattern.height);
    if (!patRect.IsEmpty() && contained) {
      asSurface = true;
      source[0] = patRect.x;
      source[1] = patRect.y;
      source[2] = patRect.width;
      source[3] = patRect.height;
    }
  }
  {
    Target target(surface, transform);
    if (!target.canvas) return true;
    ApplyClips(target, surface, transform);
    if (shadow)
      DrawShadow(target, Matrix::From(transform), RectPath(bounds), pattern, *shadow, options,
                 blend->mode, nullptr);
    if (!asSurface) {
      AutoPaint paint(target.canvas, *blend, options, &bounds);
      SetPaintPattern(paint.paint, pattern, paint.alpha);
      target.canvas->drawRect(bounds, paint.paint);
      return true;
    }
  }
  const double dest[4] = {bounds.x(), bounds.y(), bounds.width(), bounds.height()};
  const Image image{pattern.pixels.data(), pattern.width, pattern.height, pattern.opaque};
  return DrawImage(surface, transform, image, source, dest, pattern.linear, options);
}

bool ClearRect(const Surface& surface, const double transform[6], const double input[4]) {
  double rect[4] = {input[0], input[1], input[2], input[3]};
  // ValidateRect(…, false).
  if (rect[2] == 0.0 || rect[3] == 0.0) return true;
  for (double value : rect)
    if (!std::isfinite(float(value))) return true;
  if (rect[2] < 0) { rect[2] = -rect[2]; rect[0] -= rect[2]; }
  if (rect[3] < 0) { rect[3] = -rect[3]; rect[1] -= rect[3]; }
  if (!Usable(surface)) return true;
  Target target(surface, transform);
  if (!target.canvas) return true;
  ApplyClips(target, surface, transform);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorTRANSPARENT);
  paint.setBlendMode(SkBlendMode::kSrc);
  target.canvas->drawRect(SkRect::MakeXYWH(float(rect[0]), float(rect[1]), float(rect[2]),
                                           float(rect[3])),
                          paint);
  return true;
}

bool FillPath(const Surface& surface, const double transform[6], const PathRecord& path,
              bool evenOdd, const Pattern& pattern, const DrawOptions& options,
              const ShadowStyle* shadow) {
  const BlendName* blend = FindBlend(options.composite);
  if (!blend) return false;
  if (!Usable(surface)) return true;
  CanvasPathState state;
  ReplayPath(path, Matrix::From(transform), &state);
  state.builder.path.setFillType(evenOdd ? SkPathFillType::kEvenOdd : SkPathFillType::kWinding);
  const SkPath skPath = state.builder.path.detach();
  if (skPath.isEmpty()) return true;  // FillImpl: aPath.IsEmpty()
  Target target(surface, transform);
  if (!target.canvas) return true;
  ApplyClips(target, surface, transform);
  // DrawOptions default: anti-aliased.
  DrawOptions fill = options;
  fill.antialias = true;
  if (shadow)
    DrawShadow(target, Matrix::From(transform), skPath, pattern, *shadow, fill, blend->mode,
               nullptr);
  AutoPaint paint(target.canvas, *blend, fill, nullptr);
  SetPaintPattern(paint.paint, pattern, paint.alpha);
  if (!skPath.isFinite()) return true;
  target.canvas->drawPath(skPath, paint.paint);
  return true;
}

bool StrokePath(const Surface& surface, const double transform[6], const PathRecord& path,
                const StrokeStyle& stroke, const Pattern& pattern, const DrawOptions& options,
                const ShadowStyle* shadow) {
  const BlendName* blend = FindBlend(options.composite);
  if (!blend) return false;
  if (!Usable(surface)) return true;
  CanvasPathState state;
  ReplayPath(path, Matrix::From(transform), &state);
  const SkPath skPath = state.builder.path.detach();
  if (skPath.isEmpty()) return true;  // StrokeImpl: aPath.IsEmpty()
  Target target(surface, transform);
  if (!target.canvas) return true;
  ApplyClips(target, surface, transform);
  DrawOptions draw = options;
  draw.antialias = true;
  if (shadow)
    DrawShadow(target, Matrix::From(transform), skPath, pattern, *shadow, draw, blend->mode,
               &stroke);
  AutoPaint paint(target.canvas, *blend, draw, nullptr);
  SetPaintPattern(paint.paint, pattern, paint.alpha);
  if (!StrokeToPaint(paint.paint, stroke)) return true;
  if (!skPath.isFinite()) return true;
  target.canvas->drawPath(skPath, paint.paint);
  return true;
}

bool StrokeRect(const Surface& surface, const double transform[6], const double input[4],
                const StrokeStyle& stroke, const Pattern& pattern, const DrawOptions& options,
                const ShadowStyle* shadow) {
  const BlendName* blend = FindBlend(options.composite);
  if (!blend) return false;
  double rect[4] = {input[0], input[1], input[2], input[3]};
  if (!rect[2] && !rect[3]) return true;
  // ValidateRect(…, true).
  for (double value : rect)
    if (!std::isfinite(float(value))) return true;
  if (rect[2] < 0) { rect[2] = -rect[2]; rect[0] -= rect[2]; }
  if (rect[3] < 0) { rect[3] = -rect[3]; rect[1] -= rect[3]; }
  if (!Usable(surface)) return true;
  Target target(surface, transform);
  if (!target.canvas) return true;
  ApplyClips(target, surface, transform);
  DrawOptions draw = options;
  draw.antialias = true;
  if (!rect[3] || !rect[2]) {
    // A degenerate rect strokes one line, capped round for a round join.
    StrokeStyle line = stroke;
    line.cap = stroke.join == "round" ? "round" : "butt";
    const float x0 = float(rect[0]), y0 = float(rect[1]);
    const float x1 = !rect[3] ? float(rect[0] + rect[2]) : float(rect[0]);
    const float y1 = !rect[3] ? float(rect[1]) : float(rect[1] + rect[3]);
    if (shadow) {
      SkPathBuilder builder;
      builder.moveTo(x0, y0);
      builder.lineTo(x1, y1);
      DrawShadow(target, Matrix::From(transform), builder.detach(), pattern, *shadow, draw,
                 blend->mode, &line);
    }
    AutoPaint paint(target.canvas, *blend, draw, nullptr);
    SetPaintPattern(paint.paint, pattern, paint.alpha);
    if (!StrokeToPaint(paint.paint, line)) return true;
    target.canvas->drawLine(x0, y0, x1, y1, paint.paint);
    return true;
  }
  // DrawTargetSkia::StrokeRect. With a dash it first shrinks the rect to the
  // clip (ShrinkClippedStrokedRect); not ported yet.
  const SkRect box = SkRect::MakeXYWH(float(rect[0]), float(rect[1]), float(rect[2]),
                                      float(rect[3]));
  if (shadow)
    DrawShadow(target, Matrix::From(transform), RectPath(box), pattern, *shadow, draw,
               blend->mode, &stroke);
  AutoPaint paint(target.canvas, *blend, draw, nullptr);
  SetPaintPattern(paint.paint, pattern, paint.alpha);
  if (!StrokeToPaint(paint.paint, stroke)) return true;
  target.canvas->drawRect(box, paint.paint);
  return true;
}

bool DrawImage(const Surface& surface, const double transform[6], const Image& image,
               const double source[4], const double dest[4], bool linear,
               const DrawOptions& options, const ShadowStyle* shadow) {
  const BlendName* blend = FindBlend(options.composite);
  if (!blend) return false;
  if (!Usable(surface) || !image.pixels || image.width <= 0 || image.height <= 0) return true;
  // gfx::Rect, float.
  const SkRect sourceRect = SkRect::MakeXYWH(float(source[0]), float(source[1]),
                                             float(source[2]), float(source[3]));
  const SkRect destRect = SkRect::MakeXYWH(float(dest[0]), float(dest[1]), float(dest[2]),
                                           float(dest[3]));
  if (sourceRect.isEmpty()) return true;  // DrawSurface: aSource.IsEmpty()
  // The surface as Firefox holds it: B8G8R8A8 (or X8) premultiplied. Copied
  // first, so drawing a canvas into itself reads the pre-draw pixels.
  const size_t count = size_t(image.width) * size_t(image.height);
  std::vector<uint8_t> bgra(image.pixels, image.pixels + count * 4);
  SwapRedBlue(bgra.data(), count);
  const SkImageInfo info = SkImageInfo::Make(image.width, image.height, kBGRA_8888_SkColorType,
                                             image.opaque ? kOpaque_SkAlphaType
                                                          : kPremul_SkAlphaType);
  sk_sp<SkImage> skImage =
      SkImages::RasterFromPixmapCopy(SkPixmap(info, bgra.data(), size_t(image.width) * 4));
  if (!skImage) return true;
  Target target(surface, transform);
  if (!target.canvas) return true;
  ApplyClips(target, surface, transform);
  if (shadow) {
    // AdjustedTarget::DrawSurface: the source rect as a path, the surface as a
    // clamped pattern (point-sampled under a blur radius above 1), drawn under
    // Scaling(dest/source).PreTranslate(-source).PostTranslate(dest) * transform.
    Matrix local;
    local._11 = destRect.width() / sourceRect.width();
    local._22 = destRect.height() / sourceRect.height();
    local._31 += local._11 * -sourceRect.x() + local._21 * -sourceRect.y();
    local._32 += local._12 * -sourceRect.x() + local._22 * -sourceRect.y();
    local._31 += destRect.x();
    local._32 += destRect.y();
    Pattern surfacePattern;
    surfacePattern.kind = Pattern::Kind::Surface;
    surfacePattern.repeat = Pattern::Repeat::NoRepeat;
    surfacePattern.pixels.assign(image.pixels, image.pixels + count * 4);
    surfacePattern.width = image.width;
    surfacePattern.height = image.height;
    surfacePattern.opaque = image.opaque;
    surfacePattern.linear = BlurRadius(shadow->sigma) > 1 ? false : linear;
    DrawShadow(target, local * Matrix::From(transform), RectPath(sourceRect), surfacePattern,
               *shadow, options, blend->mode, nullptr);
  }
  // AutoPaintSetup(mCanvas, aOptions, &aDest, forceGroup): an RGBA image is
  // never alpha-only, so no forced group.
  AutoPaint paint(target.canvas, *blend, options, &destRect);
  target.canvas->drawImageRect(skImage, sourceRect, destRect,
                               SkSamplingOptions(linear ? SkFilterMode::kLinear
                                                        : SkFilterMode::kNearest),
                               &paint.paint, SkCanvas::kStrict_SrcRectConstraint);
  return true;
}

// ---- Text: DrawTargetSkia::DrawGlyphs. Windows uses ScaledFontDWrite.
// Darwin uses the Core Text typeface already in the donor Skia archive.
namespace {
#ifdef _WIN32
IDWriteFactory* DWriteFactory() {
  static IDWriteFactory* factory = [] {
    IDWriteFactory* created = nullptr;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&created))))
      return static_cast<IDWriteFactory*>(nullptr);
    return created;
  }();
  return factory;
}
#endif
// DWriteSettings from the default rendering params, clamped the way
// ScaledFontDWrite::CreateSkTypeface clamps them.
struct TextSettings { int mode = 0; float gamma = 2.2f, contrast = 1.0f, clearType = 1.0f; };
const TextSettings& Settings() {
  static const TextSettings settings = [] {
    TextSettings s;
#ifdef _WIN32
    IDWriteFactory* factory = DWriteFactory();
    IDWriteRenderingParams* params = nullptr;
    if (factory && SUCCEEDED(factory->CreateRenderingParams(&params)) && params) {
      s.mode = int(params->GetRenderingMode());
      s.clearType = params->GetClearTypeLevel();
      params->Release();
    }
    s.gamma = web_geometry::SystemTextGamma();
    s.contrast = web_geometry::SystemTextContrast();
    if (s.gamma < 0.0f || s.gamma > 4.0f) s.gamma = 2.2f;
#elif defined(__APPLE__)
    s.gamma = web_geometry::SystemTextGamma();
    s.contrast = web_geometry::SystemTextContrast();
    if (s.gamma < 0.0f || s.gamma > 4.0f) s.gamma = 1.8f;
#endif
    if (s.contrast < 0.0f || s.contrast > 1.0f) s.contrast = 1.0f;
    if (s.clearType < 0.0f || s.clearType > 1.0f) s.clearType = 1.0f;
    return s;
  }();
  return settings;
}
#ifdef _WIN32
sk_sp<SkTypeface> TypefaceFor(IDWriteFontFace* face) {
  IDWriteFactory* factory = DWriteFactory();
  if (!factory || !face) return nullptr;
  IDWriteFontCollection* collection = nullptr;
  IDWriteFont* font = nullptr;
  IDWriteFontFamily* family = nullptr;
  if (SUCCEEDED(factory->GetSystemFontCollection(&collection, FALSE)) && collection) {
    if (SUCCEEDED(collection->GetFontFromFontFace(face, &font)) && font) font->GetFontFamily(&family);
    collection->Release();
  }
  const TextSettings& s = Settings();
  sk_sp<SkTypeface> typeface(SkCreateTypefaceFromDWriteFont(factory, face, font, family, SkFontStyle(),
                                                            s.mode, s.gamma, s.contrast, s.clearType));
  if (family) family->Release();
  if (font) font->Release();
  return typeface;
}
#elif defined(__linux__)
sk_sp<SkTypeface> TypefaceFor(SkTypeface* face) { return sk_ref_sp(face); }
#else
sk_sp<SkTypeface> TypefaceFor(::IDWriteFontFace* face) {
  if (!face || !face->CoreFont()) return nullptr;
  return SkMakeTypefaceFromCTFont(static_cast<CTFontRef>(face->CoreFont()));
}
#endif
}  // namespace

namespace {
// gfxFont::DrawGlyphs -> DrawTargetSkia::FillGlyphs / StrokeGlyphs for each run,
// on |canvas| whose base transform is |base|.
void DrawRuns(SkCanvas* canvas, const Matrix& base, const TextRun* runs, size_t runCount,
              float size, const StrokeStyle* stroke, const Pattern& pattern,
              const DrawOptions& options, const BlendName& blend) {
  static const float kTanDefaultAngle = float(std::tan(14.0 * (2.0 * kPi / 360.0)));
  for (size_t r = 0; r < runCount; ++r) {
    const TextRun& run = runs[r];
    if (!run.count) continue;
    sk_sp<SkTypeface> typeface = TypefaceFor(run.face);
    if (!typeface) continue;
    // The synthetic-oblique run matrix (see glyph_runs.cpp).
    Matrix m = base;
    if (run.syntheticItalic) {
      const float px = float(run.originX), py = float(run.originY);
      m._31 += m._11 * px + m._21 * py;
      m._32 += m._12 * px + m._22 * py;
      Matrix skew;
      skew._21 = -kTanDefaultAngle;
      m = skew * m;
      m._31 += m._11 * -px + m._21 * -py;
      m._32 += m._12 * -px + m._22 * -py;
    }
    canvas->setMatrix(m.ToSkia());
    DrawOptions draw = options;
    draw.antialias = true;
    AutoPaint paint(canvas, blend, draw, nullptr);
    SetPaintPattern(paint.paint, pattern, paint.alpha);
    if (stroke && !StrokeToPaint(paint.paint, *stroke)) return;
    // GetDefaultAAMode is SUBPIXEL: anti-aliased; a transparent canvas does
    // not permit subpixel AA, so the edging is grayscale.
    paint.paint.setAntiAlias(true);
    SkFont font(typeface, size);
#if defined(__linux__)
    font.setEmbolden(run.syntheticBold);
#endif
    font.setEdging(SkFont::Edging::kAntiAlias);
    // ScaledFontDWrite::SetupSkFontDrawOptions (canvas text never forces GDI).
    font.setEmbeddedBitmaps(Settings().mode == 0 /* DWRITE_RENDERING_MODE_DEFAULT */ &&
                            run.cjkBitmapStrike);
    font.setSubpixel(true);
    SkTextBlobBuilder builder;
    const auto& buffer = builder.allocRunPos(font, int(run.count));
    for (size_t i = 0; i < run.count; ++i) {
      buffer.glyphs[i] = run.glyphs[i];
      buffer.points()[i] = SkPoint::Make(float(run.x[i]), float(run.y[i]));
    }
    sk_sp<SkTextBlob> blob = builder.make();
    if (blob) canvas->drawTextBlob(blob, 0, 0, paint.paint);
  }
}

// BaseRect::Union: an empty rect contributes nothing.
FloatRect Union(const FloatRect& a, const FloatRect& b) {
  if (a.IsEmpty()) return b;
  if (b.IsEmpty()) return a;
  const float x = std::min(a.x, b.x), y = std::min(a.y, b.y);
  const float xm = std::max(a.x + a.width, b.x + b.width), ym = std::max(a.y + a.height, b.y + b.height);
  return {x, y, xm - x, ym - y};
}

bool DrawTextRuns(const Surface& surface, const double transform[6], const TextRun* runs,
                  size_t runCount, float size, const StrokeStyle* stroke, const Pattern& pattern,
                  const DrawOptions& options, const ShadowStyle* shadow, const double* ink) {
  const BlendName* blend = FindBlend(options.composite);
  if (!blend) return false;
  if (!Usable(surface) || !runCount) return true;
  Target target(surface, transform);
  if (!target.canvas) return true;
  ApplyClips(target, surface, transform);
  const Matrix base = Matrix::From(transform);
  if (!shadow) {
    DrawRuns(target.canvas, base, runs, runCount, size, stroke, pattern, options, *blend);
    return true;
  }
  // AdjustedTarget(mCtx, bounds, /* aAllowOptimization */ false).
  const int32_t blurRadius = BlurRadius(shadow->sigma);
  const FloatRect canvasRect{0, 0, float(surface.width), float(surface.height)};
  FloatRect source{canvasRect.x - shadow->offsetX, canvasRect.y - shadow->offsetY,
                   canvasRect.width, canvasRect.height};
  source = {source.x - blurRadius, source.y - blurRadius, source.width + 2 * blurRadius,
            source.height + 2 * blurRadius};
  FloatRect bounds = Union(source, canvasRect);
  if (ink) {
    FloatRect box{float(ink[0]), float(ink[1]), float(ink[2]), float(ink[3])};
    if (stroke) {
      const float half = float(stroke->lineWidth / 2.0);
      box = {box.x - half, box.y - half, box.width + 2 * half, box.height + 2 * half};
    }
    box = TransformBounds(base, box);
    if (!box.IsEmpty()) bounds = Intersect(bounds, box);
  }
  // AdjustedTargetForShadow.
  bounds = {bounds.x - blurRadius, bounds.y - blurRadius, bounds.width + 2 * blurRadius,
            bounds.height + 2 * blurRadius};
  const float x0 = std::floor(bounds.x), y0 = std::floor(bounds.y);
  const float x1 = std::ceil(bounds.x + bounds.width), y1 = std::ceil(bounds.y + bounds.height);
  const int tx = int(x0), ty = int(y0), tw = int(x1 - x0), th = int(y1 - y0);
  if (tw <= 0 || th <= 0 || tw > 32767 || th > 32767) {
    DrawRuns(target.canvas, base, runs, runCount, size, stroke, pattern, options, *blend);
    return true;
  }
  const SkImageInfo info = SkImageInfo::Make(tw, th, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  const SkSurfaceProps props(0, kRGB_H_SkPixelGeometry, 0, SK_Scalar1);
  sk_sp<SkSurface> temp = SkSurfaces::Raster(info, size_t(tw) * 4, &props);
  if (!temp) return true;
  temp->getCanvas()->clear(SK_ColorTRANSPARENT);
  Matrix shifted = base;
  shifted._31 += float(-tx);
  shifted._32 += float(-ty);
  DrawOptions over = options;
  over.composite = "source-over";
  DrawRuns(temp->getCanvas(), shifted, runs, runCount, size, stroke, pattern, over, kBlends[0]);
  sk_sp<SkImage> snapshot = temp->makeImageSnapshot();
  if (!snapshot) return true;
  // DrawTargetSkia::DrawSurfaceWithShadow(snapshot, tempRect.TopLeft(), ...).
  SkCanvas* canvas = target.canvas;
  canvas->save();
  canvas->resetMatrix();
  std::vector<uint8_t> alpha(size_t(tw) * size_t(th));
  const SkImageInfo a8 = SkImageInfo::MakeA8(tw, th);
  if (snapshot->readPixels(SkPixmap(a8, alpha.data(), size_t(tw)), 0, 0)) {
    sk_sp<SkImage> alphaImage = SkImages::RasterFromPixmapCopy(SkPixmap(a8, alpha.data(), size_t(tw)));
    SkPaint shadowPaint;
    shadowPaint.setBlendMode(blend->mode);
    shadowPaint.setColor(SkColorSetARGB(ColorFloatToByte(shadow->color.a * 1.0f),
                                        ColorFloatToByte(shadow->color.r),
                                        ColorFloatToByte(shadow->color.g),
                                        ColorFloatToByte(shadow->color.b)));
    const int destX = int32_t(floorf(float(tx) + shadow->offsetX + 0.5f));
    const int destY = int32_t(floorf(float(ty) + shadow->offsetY + 0.5f));
    SkPoint blurOffset = SkPoint::Make(0, 0);
    sk_sp<SkImage> blurred = alphaImage ? BlurAlphaMask(alphaImage.get(), shadow->sigma, blurOffset) : nullptr;
    if (blurred) {
      canvas->drawImage(blurred, destX + blurOffset.x(), destY + blurOffset.y(),
                        SkSamplingOptions(SkFilterMode::kLinear), &shadowPaint);
    } else if (alphaImage) {
      shadowPaint.setImageFilter(SkImageFilters::Blur(shadow->sigma, shadow->sigma, nullptr));
      canvas->drawImage(alphaImage, destX, destY, SkSamplingOptions(SkFilterMode::kLinear),
                        &shadowPaint);
    }
  }
  SkPaint imagePaint;
  imagePaint.setBlendMode(blend->mode);
  canvas->drawImage(snapshot, tx, ty, SkSamplingOptions(SkFilterMode::kLinear), &imagePaint);
  canvas->restore();
  return true;
}
}  // namespace

bool StrokeText(const Surface& surface, const double transform[6], const TextRun* runs,
                size_t runCount, float size, const StrokeStyle& stroke, const Pattern& pattern,
                const DrawOptions& options, const ShadowStyle* shadow, const double* ink) {
  return DrawTextRuns(surface, transform, runs, runCount, size, &stroke, pattern, options, shadow, ink);
}

bool FillText(const Surface& surface, const double transform[6], const TextRun* runs,
              size_t runCount, float size, const Pattern& pattern, const DrawOptions& options,
              const ShadowStyle* shadow, const double* ink) {
  return DrawTextRuns(surface, transform, runs, runCount, size, nullptr, pattern, options, shadow, ink);
}
}  // namespace ruyijs::web_canvas::skia

#if defined(__APPLE__)
// SkCTFontCreateExactCopy calls this Firefox widget helper before it reapplies
// font variations. The shell targets macOS 26, so that path is the one taken.
// widget/cocoa/nsCocoaFeatures.mm is not part of this link.
namespace nsCocoaFeatures {
bool OnVenturaOrLater() { return true; }
}
#endif
