#pragma once
#include <cstdint>
#include "domain/geometry/svg_values.h"
#include <memory>
#include <string>
#include <vector>

#include "web_geometry/glyph_mask.h"
#include "runtime/font_face.h"

namespace ruyijs::web_geometry {
struct FontInput {
  std::string family, language;
  double size = 16, x = 0, y = 0;
  float scale = 1;
  unsigned weight = 400;
  bool italic = false;
  // gfx::ShapedTextFlags::TEXT_OPTIMIZE_SPEED: the text run carries no
  // TEXT_NEED_BOUNDING_BOX, so LOOSE_INK_EXTENTS measurement (SVG run rects,
  // frame ink overflow) skips the glyph extents of simple glyphs.
  bool optimizeSpeed = false;
  // CSS letter-spacing in px, added after every cluster (nsTextFrame
  // PropertyProvider::GetSpacing, rounded to app units).
  double letterSpacing = 0;
  ruyijs::domain::geometry::SvgMatrix transform;
};
// nsLayoutUtils::GetTextRunFlagsForStyle: text-rendering:optimizeSpeed, or
// auto below browser.display.auto_quality_min_font_size (20 device pixels).
bool TextRunOptimizeSpeed(const std::string& textRendering, double cssFontSize);
// window.devicePixelRatio of the profile, for device-pixel prefs in layout.
void SetDevicePixelRatio(double ratio);
struct FontCollection;
// font.default.<group> for the untagged-content language group ("serif" if unset).
std::string DefaultGenericFamily();
// Overrides the OS locale Firefox uses for untagged text
// (nsLanguageAtomService::GetLocaleLanguage); "" restores the OS locale.
void SetBrowserLanguage(const std::string& language);
// The text metrics a 2D canvas context reports, in CSS pixels, computed the way
// Firefox reports them: the advance rounded to app units and divided by 60 in
// float, the tight ink box of the glyphs relative to the origin on the
// alphabetic baseline, and the rounded font box from the primary font.
struct CanvasTextMetrics {
  float width = 0;
  double ink_left = 0, ink_top = 0, ink_right = 0, ink_bottom = 0;
  bool ink = false;  // false when no glyph has ink, e.g. spaces only
  double font_ascent = 0, font_descent = 0;
  // emHeightAscent/Descent: the em box split in the font box's proportion, as
  // gfxDWriteFont::ComputeMetrics splits mAdjustedSize.
  double em_ascent = 0, em_descent = 0;
  double hanging_baseline = 0, alphabetic_baseline = 0, ideographic_baseline = 0;
};

// One glyph run as gfxTextRun hands it to the DrawTarget: the DirectWrite
// face (one reference the caller releases), whether gfxFont draws it with the
// synthetic-oblique skew, its origin on the baseline, and each glyph's id and
// position in CSS pixels.
struct CanvasGlyphRun {
  IDWriteFontFace* face = nullptr;
  bool syntheticItalic = false;
#if defined(__linux__)
  bool syntheticBold = false;
#endif
  // gfxDWriteFont::GetScaledFont's useEmbeddedBitmap without the rendering
  // mode test: a CJK font (OS/2 code pages) with an EBLC strike at the size.
  bool cjkBitmapStrike = false;
  // The face has a COLR table: gfxFont::DrawGlyphs paints its colour glyphs
  // through RenderColorGlyph, not FillGlyphs.
  bool colorFont = false;
  double origin_x = 0, origin_y = 0;
  std::vector<uint16_t> glyphs;
  std::vector<double> x, y;
};

// Releases the faces CanvasGlyphRuns referenced.
void ReleaseCanvasGlyphRuns(std::vector<CanvasGlyphRun>*);

class SystemFonts final {
 public:
  SystemFonts();
  ~SystemFonts();
  bool Measure(const std::u16string&, const FontInput&, ruyijs::domain::geometry::SvgTextMeasurement*);
  // The same fonts, measured for canvas TextMetrics: the text is measured as
  // given (no whitespace collapsing) and the ink stays unrounded.
  bool MeasureCanvas(const std::u16string&, const FontInput&, CanvasTextMetrics*);
  // Build glyph outlines in CSS pixels. The supplied x/y is the alphabetic
  // baseline origin; coordinates include it and input.scale is divided out.
  // The caller applies any Canvas transform after adapting the neutral verbs.
  bool BuildCanvasTextPath(const std::u16string&, const FontInput&, CanvasTextPath*);
  // The same run split, rasterised instead of outlined: one device-space
  // coverage mask per glyph (colour glyphs give a mask per palette layer), with
  // |matrix| mapping CSS pixels to canvas pixels and |rgb| the fill colour,
  // whose luminance selects Skia's mask gamma table.
  bool BuildCanvasGlyphMasks(const std::u16string&, const FontInput&, const float matrix[6],
                             const uint8_t rgb[3], std::vector<CanvasGlyphMask>*);
  // The runs BuildCanvasGlyphMasks rasterises, as glyph ids and positions, and
  // the em size (CSS pixels, app-unit rounded) the ScaledFont is created at.
  bool CanvasGlyphRuns(const std::u16string&, const FontInput&, std::vector<CanvasGlyphRun>*,
                       float* emsize);
  // The primary font's (first family with a space glyph) content-area height
  // and normal line height, rounded like gfxDWriteFont/nsFontMetrics.
  bool LineMetrics(const FontInput&, double* content, double* normal);
  // SVG ex/ch/ic/cap sizes from the shared collection (svg_length_font.cpp).
  bool SvgLengthMetrics(const FontInput&, float* ex, float* ch, float* ic, float* cap);
 private:
  std::unique_ptr<FontCollection> collection_;
};
}
