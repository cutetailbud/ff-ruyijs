#pragma once
// Glyph rasterisation for the canvas owner: the same DirectWrite mask Firefox
// draws, and the tone table Skia puts in front of it.
#include <cstdint>
#include <vector>

#include "runtime/font_face.h"
struct DWRITE_GLYPH_RUN;

namespace ruyijs::web_geometry {
// SkScalerContext_DW's rendering mode, measuring mode and em size for a
// grayscale mask with normal hinting and no embedded bitmaps, as the
// DWRITE_RENDERING_MODE / DWRITE_MEASURING_MODE values the caller passes on.
struct GlyphRenderStyle {
  int render = 5;  // DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC
  int measure = 0; // DWRITE_MEASURING_MODE_NATURAL
  float em = 0;
};
// glyph_style.cpp: the mode the size, the typeface and the gasp table select,
// and the platform text gamma and enhanced contrast they were read with.
GlyphRenderStyle GlyphStyle(IDWriteFontFace*, float deviceSize);
float SystemTextGamma();
float SystemTextContrast();

// A domain-neutral outline returned by the font owner.  Canvas adapts these
// verbs to its raster::Path; web_geometry does not depend on the Canvas owner.
struct CanvasTextPath {
  std::vector<float> points;  // x,y pairs; quad verbs consume two pairs, cubic three
  std::vector<uint8_t> verbs; // 0 move, 1 line, 2 cubic, 3 close, 4 quad
  bool evenOdd = false;       // the fill type the outline producer reports
  void Move(float x, float y) { points.push_back(x); points.push_back(y); verbs.push_back(0); }
  void Line(float x, float y) { points.push_back(x); points.push_back(y); verbs.push_back(1); }
  void Cubic(float x1, float y1, float x2, float y2, float x3, float y3) {
    points.insert(points.end(), {x1, y1, x2, y2, x3, y3}); verbs.push_back(2);
  }
  void Quad(float x1, float y1, float x2, float y2) {
    points.insert(points.end(), {x1, y1, x2, y2}); verbs.push_back(4);
  }
  void Close() { verbs.push_back(3); }
};
// One device-space bitmap a fillText is made of: a grayscale coverage mask, or
// the outline layers of a colour glyph (COLR), which the canvas fills into a
// premultiplied bitmap before blending it whole, the way Skia does.
struct CanvasGlyphMask {
  int left = 0, top = 0, width = 0, height = 0;
  std::vector<uint8_t> coverage;  // width*height, gamma-corrected A8
#if defined(__linux__)
  std::vector<uint8_t> rgba;  // Native colour glyph, premultiplied RGBA.
#endif
  // A COLR glyph is painted the way gfxFont::RenderColorGlyph paints it: each
  // palette layer is an ordinary grayscale glyph mask in its palette colour.
  // `sampled` glyphs (adjusted size <= 256) are drawn into a 2x snapshot whose
  // box is the font's head bbox rounded out (left/top/width/height, relative to
  // the rounded glyph origin `origin_x`/`origin_y` in user space), which the
  // canvas then resamples; the layer masks are placed in snapshot pixels.
  // Otherwise the layers are placed in device pixels like any other mask.
  struct Layer {
    int left = 0, top = 0, width = 0, height = 0;
    std::vector<uint8_t> coverage;   // width*height, gamma-corrected A8
    uint8_t rgba[4] = {0, 0, 0, 0};  // straight palette colour
  };
  std::vector<Layer> layers;  // non-empty for a colour glyph
  bool sampled = false;
  double origin_x = 0, origin_y = 0;
};

struct SelectedFont;
// glyph_mask.cpp: the alpha texture of one glyph at |dx|,|dy| (the subpixel part
// of its device position) with |linear| as the glyph run transform. Bounds are
// relative to the baseline origin the run was measured from.
bool GlyphMask(const SelectedFont&, float deviceSize, const float linear[4], float dx, float dy,
               uint16_t glyph, const uint8_t rgb[3], CanvasGlyphMask* out);

// glyph_palette.cpp: palette 0 of the font's CPAL table as ARGB bytes.
bool GlyphPalette(IDWriteFontFace*, std::vector<uint32_t>* colors);


// glyph_color.cpp: the COLR layers of a colour glyph (Segoe UI Emoji). With
// `sampled`, the layers are rendered at twice `emSize` into the snapshot box;
// otherwise at `emSize` with the run transform and subpixel offset, relative to
// the glyph origin. False when the glyph has no COLR layers.
bool GlyphColorMask(const SelectedFont&, float emSize, bool sampled, const float linear[4],
                    float dx, float dy, uint16_t glyph, const uint8_t foreground[3],
                    CanvasGlyphMask* out);

// glyph_gamma.cpp: SkMaskGamma's correcting table for the paint colour.
void GlyphMaskTable(float gamma, float contrast, const uint8_t rgb[3], uint8_t table[256]);
}  // namespace ruyijs::web_geometry
