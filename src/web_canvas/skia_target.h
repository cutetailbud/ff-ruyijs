// Canvas drawing through Firefox's own Skia (gfx/skia), set up the way
// gfx/2d/DrawTargetSkia does for a canvas. The interface carries no Skia
// types: skia_target.cpp is the one translation unit built against the Skia
// headers (tools/skia_native.py), everything else stays on the product flags.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Windows: the DirectWrite COM face. Darwin: font_backend.h defines this
// same global name as the Core Text shim. It is not a namespaced type.
#include "runtime/font_face.h"

namespace ruyijs::web_canvas::skia {
// A colour as gfx::DeviceColor: straight (non-premultiplied) floats.
struct Color { float r = 0, g = 0, b = 0, a = 0; };
// CanvasGradient::AddColorStop's raw stop; DrawTargetSkia sorts them.
struct Stop { float offset = 0; Color color; };
// A source surface: premultiplied RGBA, row stride width * 4. opaque marks a
// B8G8R8X8 surface (a decoded image without alpha).
struct Image {
  const uint8_t* pixels = nullptr;
  int width = 0;
  int height = 0;
  bool opaque = false;
};
// gfx::Pattern as CanvasGeneralPattern::ForStyle builds it.
struct Pattern {
  enum class Kind { Color, Linear, Radial, Conic, Surface } kind = Kind::Color;
  Color color;
  // Linear: begin.x begin.y end.x end.y. Radial: center1.x center1.y radius1
  // center2.x center2.y radius2. Conic: angle center.x center.y.
  float geometry[6] = {};
  std::vector<Stop> stops;
  // Surface (CanvasPattern): the snapshot taken by createPattern, its
  // repetition (CanvasPattern::RepeatMode), its float setTransform() matrix
  // and the sampling imageSmoothingEnabled picks.
  // The pixels are premultiplied RGBA owned here; opaque marks B8G8R8X8.
  enum class Repeat { Repeat, RepeatX, RepeatY, NoRepeat } repeat = Repeat::Repeat;
  std::vector<uint8_t> pixels;
  int width = 0;
  int height = 0;
  bool opaque = false;
  float matrix[6] = {1, 0, 0, 1, 0, 0};
  bool linear = true;
};
// gfx::DrawOptions: global alpha, the canvas composite operation spelling
// and whether the paint is anti-aliased.
struct DrawOptions {
  float alpha = 1;
  std::string composite = "source-over";
  bool antialias = true;
};
// One clip() of the drawing state: the path record when clip() ran, its fill
// rule and the transform the DrawTarget had then (PushClip).
struct Clip {
  std::vector<double> commands;
  bool path2d = false;
  bool evenOdd = false;
  double matrix[6] = {1, 0, 0, 1, 0, 0};
};
// The canvas backing store: premultiplied RGBA bytes, row stride width * 4.
// DrawTargetSkia draws a B8G8R8A8 surface; the bridge presents the same
// format to Skia so the same blitters run. clips are the drawing state's
// clipsAndTransforms clips, oldest first.
struct Surface {
  uint8_t* pixels = nullptr;
  int width = 0;
  int height = 0;
  const std::vector<Clip>* clips = nullptr;
};
// gfx::ShadowOptions, present only when CanvasRenderingContext2D
// NeedToDrawShadow() holds (AdjustedTarget then draws the shadow directly).
struct ShadowStyle {
  Color color;
  float offsetX = 0, offsetY = 0;
  float sigma = 0;  // ShadowBlurSigma(): min(100, shadowBlur / 2)
};

// CanvasRenderingContext2D::FillRect + DrawTargetSkia::FillRect with the
// canvas transform (a b c d e f, as the float gfx::Matrix the DrawTarget
// holds). Returns false when the composite operation is not one canvas accepts.
bool FillRect(const Surface& surface, const double transform[6], const double rect[4],
              const Pattern& pattern, const DrawOptions& options,
              const ShadowStyle* shadow = nullptr);
// clearRect(): DrawTargetSkia::ClearRect under the transform and clips.
bool ClearRect(const Surface& surface, const double transform[6], const double rect[4]);

// A recorded canvas path (web_canvas/context2d_path.cpp): repeated
// [8, a b c d e f, opcode, numbers...] entries, the transform current when the
// path method ran followed by the method. Replaying it through the same
// builder logic as CanvasRenderingContext2D (or CanvasPath for a Path2D)
// rebuilds the exact float path Firefox holds.
struct PathRecord {
  const double* data = nullptr;
  size_t size = 0;
  bool path2d = false;
};
// gfx::StrokeOptions from the canvas state.
struct StrokeStyle {
  float lineWidth = 1;
  float miterLimit = 10;
  std::string cap = "butt";
  std::string join = "miter";
  std::vector<float> dash;  // as CanvasRenderingContext2D::SetLineDash stores it
  float dashOffset = 0;
};
// fill() / fill(path): EnsureUserSpacePath + FillImpl + DrawTargetSkia::Fill.
bool FillPath(const Surface& surface, const double transform[6], const PathRecord& path,
              bool evenOdd, const Pattern& pattern, const DrawOptions& options,
              const ShadowStyle* shadow = nullptr);
// stroke() / stroke(path): StrokeImpl + DrawTargetSkia::Stroke.
bool StrokePath(const Surface& surface, const double transform[6], const PathRecord& path,
                const StrokeStyle& stroke, const Pattern& pattern, const DrawOptions& options,
                const ShadowStyle* shadow = nullptr);
// strokeRect(): CanvasRenderingContext2D::StrokeRect.
bool StrokeRect(const Surface& surface, const double transform[6], const double rect[4],
                const StrokeStyle& stroke, const Pattern& pattern, const DrawOptions& options,
                const ShadowStyle* shadow = nullptr);

// One glyph run of a canvas text draw (web_geometry::CanvasGlyphRun): the face
// is borrowed for the call.
struct TextRun {
  ::IDWriteFontFace* face = nullptr;
  bool syntheticItalic = false;
#if defined(__linux__)
  bool syntheticBold = false;
#endif
  bool cjkBitmapStrike = false;
  double originX = 0, originY = 0;
  const uint16_t* glyphs = nullptr;
  const double* x = nullptr;
  const double* y = nullptr;
  size_t count = 0;
};
// strokeText(): gfxFont::DrawGlyphs with DrawMode::GLYPH_STROKE, i.e.
// DrawTargetSkia::StrokeGlyphs per run with a ScaledFontDWrite typeface of
// |size|, the run's synthetic-oblique skew applied to |transform|.
// |shadow| with |ink| (the text's tight ink box in user space, x y w h; null
// when it has none) draws through AdjustedTargetForShadow.
bool StrokeText(const Surface& surface, const double transform[6], const TextRun* runs,
                size_t runCount, float size, const StrokeStyle& stroke, const Pattern& pattern,
                const DrawOptions& options, const ShadowStyle* shadow = nullptr,
                const double* ink = nullptr);
// fillText(): DrawTargetSkia::FillGlyphs per run (colour fonts excepted, which
// the caller draws through gfxFont::RenderColorGlyph's path).
bool FillText(const Surface& surface, const double transform[6], const TextRun* runs,
              size_t runCount, float size, const Pattern& pattern, const DrawOptions& options,
              const ShadowStyle* shadow = nullptr, const double* ink = nullptr);

// DrawTargetSkia::DrawSurface with the rectangles DrawImage settled on (gfx::Rect,
// float) and its sampling choice; options.antialias is its AntialiasMode.
bool DrawImage(const Surface& surface, const double transform[6], const Image& image,
               const double source[4], const double dest[4], bool linear,
               const DrawOptions& options, const ShadowStyle* shadow = nullptr);
}  // namespace ruyijs::web_canvas::skia
