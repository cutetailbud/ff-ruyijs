#if defined(__linux__)
// Linux font selection, OpenType shaping and outlines over Fontconfig/Skia.
#include "web_geometry/system_font.h"
#include "web_geometry/font_config.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"
#include <hb.h>
#include <hb-ot.h>
#include <algorithm>
#include <atomic>
#include <codecvt>
#include <cmath>
#include <cstdlib>
#include <locale>
#include <mutex>
#include <unordered_map>

namespace ruyijs::web_geometry {
struct FontCollection {
  sk_sp<SkFontMgr> manager = [] {
    auto* config = FcInitLoadConfigAndFonts();
    if (!config) return sk_sp<SkFontMgr>();
    const auto* directory = std::getenv("RUYIJS_FONT_DIRECTORY");
    if (directory && *directory && !FcConfigAppFontAddDir(config, reinterpret_cast<const FcChar8*>(directory))) {
      FcConfigDestroy(config); return sk_sp<SkFontMgr>();
    }
    return SkFontMgr_New_FontConfig(config, SkFontScanner_Make_FreeType());
  }();
  std::unordered_map<std::string, sk_sp<SkTypeface>> selected;
  std::unordered_map<uint32_t, hb_face_t*> faces;
  uint64_t generation = 0;
  ~FontCollection() { for (auto [id, face] : faces) hb_face_destroy(face); }
};
namespace {
std::atomic<double> pixelRatio{1};
std::mutex languageLock;
std::string browserLanguage = "en-US";
std::string FontLanguage(const FontInput& input) {
  if (!input.language.empty()) return input.language;
  { std::lock_guard<std::mutex> lock(languageLock); if (!browserLanguage.empty()) return browserLanguage; }
  for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
    const char* raw = std::getenv(name);
    if (!raw || !*raw) continue;
    std::string value(raw);
    if (const auto at = value.find_first_of(".@"); at != value.npos) value.resize(at);
    if (value == "C" || value == "POSIX") continue;
    std::replace(value.begin(), value.end(), '_', '-');
    return value;
  }
  return "en-US";
}
std::vector<std::string> Families(const FontInput& input) {
  std::vector<std::string> result;
  std::string name;
  char quote = 0;
  for (size_t i = 0; i <= input.family.size(); ++i) {
    const char ch = i == input.family.size() ? ',' : input.family[i];
    if (quote) { if (ch == quote) quote = 0; else name += ch; }
    else if (ch == '\'' || ch == '"') quote = ch;
    else if (ch != ',') name += ch;
    else {
      const auto first = name.find_first_not_of(" \t\r\n");
      if (first != name.npos) {
        auto value = name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);
        const auto replacement = GenericFamilyOverride(value);
        result.push_back(replacement.empty() ? std::move(value) : replacement);
      }
      name.clear();
    }
  }
  return result;
}
SkFontStyle Style(const FontInput& input) {
  return {int(input.weight), SkFontStyle::kNormal_Width,
           input.italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant};
}
sk_sp<SkTypeface> Select(FontCollection& fonts, const FontInput& input, uint32_t code) {
  if (!fonts.manager) return nullptr;
  std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
  const std::string languageValue = FontLanguage(input);
  const auto generation = FontConfigGeneration();
  if (fonts.generation != generation) { fonts.selected.clear(); fonts.generation = generation; }
  const auto key = input.family + '\0' + languageValue + '\0' + std::to_string(input.weight) +
      (input.italic ? ":i:" : ":n:") + std::to_string(code);
  if (auto found = fonts.selected.find(key); found != fonts.selected.end()) return found->second;
  if (fonts.selected.size() >= 4096) fonts.selected.clear();
  const char* language = languageValue.c_str();
  for (const auto& family : Families(input)) {
    if (!FontFamilyVisible(convert.from_bytes(family))) continue;
    auto face = fonts.manager->matchFamilyStyle(family.c_str(), Style(input));
    if (face && (!code || face->unicharToGlyph(code))) { fonts.selected[key] = face; return face; }
  }
  auto fallback = fonts.manager->matchFamilyStyleCharacter(nullptr, Style(input), &language, 1, code ? code : ' ');
  auto face = fallback ? std::move(fallback) : fonts.manager->matchFamilyStyle("sans-serif", Style(input));
  fonts.selected[key] = face;
  return face;
}
uint32_t Codepoint(const std::u16string& text, size_t* index) {
  uint32_t code = text[(*index)++];
  if (code >= 0xd800 && code <= 0xdbff && *index < text.size() &&
      text[*index] >= 0xdc00 && text[*index] <= 0xdfff)
    code = 0x10000 + ((code - 0xd800) << 10) + text[(*index)++] - 0xdc00;
  return code;
}
hb_blob_t* Table(hb_face_t*, hb_tag_t tag, void* value) {
  auto data = static_cast<SkTypeface*>(value)->copyTableData(tag);
  if (!data) return hb_blob_get_empty();
  const auto length = unsigned(data->size());
  const char* bytes = static_cast<const char*>(data->data());
  return hb_blob_create(bytes, length, HB_MEMORY_MODE_READONLY, data.release(),
                         [](void* owned) { static_cast<SkData*>(owned)->unref(); });
}
hb_face_t* Face(FontCollection& fonts, SkTypeface* typeface) {
  const auto id = typeface->uniqueID();
  if (auto found = fonts.faces.find(id); found != fonts.faces.end()) return hb_face_reference(found->second);
  if (fonts.faces.size() >= 64) {
    for (auto [key, face] : fonts.faces) hb_face_destroy(face);
    fonts.faces.clear();
  }
  typeface->ref();
  auto* face = hb_face_create_for_tables(Table, typeface, [](void* owned) { static_cast<SkTypeface*>(owned)->unref(); });
  fonts.faces[id] = face;
  return hb_face_reference(face);
}
bool Shape(FontCollection& fonts, const std::u16string& text, const FontInput& input,
           std::vector<CanvasGlyphRun>* runs, float* size,
           std::vector<domain::geometry::SvgBox>* characters = nullptr, double* advance = nullptr) {
  *size = float(std::round(input.size * input.scale * 60) / 60);
  if (text.size() > 65536 || !std::isfinite(*size) || *size < 0 || *size > 16384 ||
      !std::isfinite(input.scale) || input.scale <= 0 || !fonts.manager) return false;
  if (advance) *advance = 0;
  if (text.empty() || !*size) return true;
  if (characters) characters->assign(text.size(), {});
  int64_t pen = 0;
  for (size_t start = 0; start < text.size();) {
    size_t end = start;
    auto typeface = Select(fonts, input, Codepoint(text, &end));
    if (!typeface) return false;
    while (end < text.size()) {
      size_t next = end;
      const auto code = Codepoint(text, &next);
      if (!typeface->unicharToGlyph(code) && hb_unicode_combining_class(hb_unicode_funcs_get_default(), code) == 0) break;
      end = next;
    }
    hb_face_t* face = Face(fonts, typeface.get());
    hb_font_t* font = hb_font_create(face);
    hb_ot_font_set_funcs(font);
    // Same 16.16 shaper input and per-glyph app-unit rounding as Windows.
    hb_font_set_scale(font, int(std::round(*size * 65536)), int(std::round(*size * 65536)));
    hb_font_set_ppem(font, unsigned(*size), unsigned(*size));
    const bool syntheticItalic = input.italic && !typeface->isItalic();
    const bool syntheticBold = input.weight >= 600 && typeface->fontStyle().weight() < 600;
    if (syntheticItalic) hb_font_set_synthetic_slant(font, .249328f);
    if (syntheticBold) hb_font_set_synthetic_bold(font, .02f, 0, false);
    hb_buffer_t* buffer = hb_buffer_create();
    hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16_t*>(text.data()), int(text.size()), unsigned(start), int(end-start));
    const std::string language = FontLanguage(input);
    hb_buffer_set_language(buffer, hb_language_from_string(language.c_str(), -1));
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(font, buffer, nullptr, 0);
    unsigned count;
    const auto* infos = hb_buffer_get_glyph_infos(buffer, &count);
    const auto* positions = hb_buffer_get_glyph_positions(buffer, &count);
    CanvasGlyphRun run;
    run.syntheticItalic = syntheticItalic;
    run.syntheticBold = syntheticBold;
    run.colorFont = typeface->getTableSize(SkSetFourByteTag('C','O','L','R')) ||
        typeface->getTableSize(SkSetFourByteTag('C','B','D','T')) ||
        typeface->getTableSize(SkSetFourByteTag('s','b','i','x'));
    run.face = typeface.release();
    run.origin_x = input.x;
    run.origin_y = input.y;
    SkFontMetrics metrics;
    SkFont(sk_ref_sp(run.face), *size).getMetrics(&metrics);
    for (unsigned i = 0; i < count; ++i) {
      run.glyphs.push_back(uint16_t(infos[i].codepoint));
      const auto offset_x = int64_t(std::floor(positions[i].x_offset * 60.0 / 65536 + .5));
      const auto offset_y = int64_t(std::floor(positions[i].y_offset * 60.0 / 65536 + .5));
      const auto width = int64_t(std::floor(positions[i].x_advance * 60.0 / 65536 + .5));
      run.x.push_back(input.x + double(pen + offset_x) / 60.0 / input.scale);
      run.y.push_back(input.y - double(offset_y) / 60.0 / input.scale);
      const double glyph_advance = double(width) / 60.0 / input.scale;
      if (characters && infos[i].cluster < characters->size()) {
        auto& box = (*characters)[infos[i].cluster];
        if (box[3] == 0) box = {float(input.x + double(pen) / 60.0 / input.scale), float(input.y + metrics.fAscent / input.scale),
                                float(glyph_advance), float((metrics.fDescent - metrics.fAscent) / input.scale)};
        else box[2] += float(glyph_advance);
      }
      pen += width;
      if (i + 1 == count || infos[i + 1].cluster != infos[i].cluster) {
        const auto spacing = std::isfinite(input.letterSpacing)
            ? int64_t(std::floor(input.letterSpacing * input.scale * 60 + .5)) : 0;
        pen += spacing;
        if (characters && infos[i].cluster < characters->size())
          (*characters)[infos[i].cluster][2] += float(double(spacing) / 60.0 / input.scale);
      }
    }
    hb_buffer_destroy(buffer);
    hb_font_destroy(font);
    hb_face_destroy(face);
    runs->push_back(std::move(run));
    start = end;
  }
  if (characters) for (size_t i = 1; i < characters->size(); ++i)
    if ((*characters)[i][3] == 0) (*characters)[i] = (*characters)[i-1];
  if (advance) *advance = float(pen) / 60.0f / input.scale;
  return true;
}
struct Runs {
  std::vector<CanvasGlyphRun> values;
  ~Runs() { ReleaseCanvasGlyphRuns(&values); }
};
SkMatrix Matrix(const float values[6]) {
  SkMatrix result;
  result.setAll(values[0], values[2], values[4], values[1], values[3], values[5], 0, 0, 1);
  return result;
}
}
SystemFonts::SystemFonts() : collection_(std::make_unique<FontCollection>()) {}
SystemFonts::~SystemFonts() = default;
void SetDevicePixelRatio(double ratio) { if (std::isfinite(ratio) && ratio > 0) pixelRatio = ratio; }
void SetBrowserLanguage(const std::string& value) { std::lock_guard<std::mutex> lock(languageLock); browserLanguage = value; }
std::string DefaultGenericFamily() { return "serif"; }
bool TextRunOptimizeSpeed(const std::string& mode, double size) {
  return mode == "optimizeSpeed" || ((mode.empty() || mode == "auto") && size < 20 / pixelRatio.load());
}
void ReleaseCanvasGlyphRuns(std::vector<CanvasGlyphRun>* runs) {
  for (auto& run : *runs) if (run.face) run.face->unref();
  runs->clear();
}
bool SystemFonts::CanvasGlyphRuns(const std::u16string& text, const FontInput& input,
                                 std::vector<CanvasGlyphRun>* runs, float* size) {
  return Shape(*collection_, text, input, runs, size);
}
bool SystemFonts::MeasureCanvas(const std::u16string& text, const FontInput& input, CanvasTextMetrics* out) {
  *out = {};
  Runs runs;
  float size;
  double advance = 0;
  if (!Shape(*collection_, text, input, &runs.values, &size, nullptr, &advance)) return false;
  auto primary = Select(*collection_, input, ' ');
  if (!primary) return false;
  bool ink = false;
  double left = 0, top = 0, right = 0, bottom = 0;
  for (const auto& run : runs.values) {
    auto* face = Face(*collection_, run.face);
    auto* font = hb_font_create(face);
    hb_ot_font_set_funcs(font);
    if (run.syntheticItalic) hb_font_set_synthetic_slant(font, .249328f);
    if (run.syntheticBold) hb_font_set_synthetic_bold(font, .02f, 0, false);
    const auto units = hb_face_get_upem(face);
    hb_font_set_scale(font, units, units);
    const double factor = double(size) / units / input.scale;
    for (size_t i = 0; i < run.glyphs.size(); ++i) {
      hb_glyph_extents_t extents{};
      if (!hb_font_get_glyph_extents(font, run.glyphs[i], &extents)) continue;
      const double x = run.x[i] - input.x + extents.x_bearing * factor;
      const double y = run.y[i] - input.y - extents.y_bearing * factor;
      const double x2 = x + extents.width * factor, y2 = y - extents.height * factor;
      if (x2 > x && y2 > y) {
        if (!ink) { left = x; top = y; right = x2; bottom = y2; ink = true; }
        else { left = std::min(left, x); top = std::min(top, y); right = std::max(right, x2); bottom = std::max(bottom, y2); }
      }
    }
    hb_font_destroy(font);
    hb_face_destroy(face);
  }
  SkFontMetrics metrics;
  SkFont(primary, size).getMetrics(&metrics);
  out->width = float(advance);
  out->font_ascent = std::round(-metrics.fAscent);
  out->font_descent = std::round(metrics.fDescent);
  const double height = out->font_ascent + out->font_descent;
  out->em_ascent = height ? size * out->font_ascent / height : 0;
  out->em_descent = size - out->em_ascent;
  // OpenType BASE plus the same horizontal-extents fallback as Windows.
  auto* face = Face(*collection_, primary.get());
  auto* font = hb_font_create(face);
  hb_ot_font_set_funcs(font);
  hb_font_set_scale(font, int(std::round(size * 65536)), int(std::round(size * 65536)));
  hb_font_set_ppem(font, unsigned(size), unsigned(size));
  auto* measured = hb_font_create_sub_font(font);
  auto* funcs = hb_font_funcs_create();
  hb_font_funcs_set_font_h_extents_func(funcs, [](hb_font_t*, void* data, hb_font_extents_t* extents, void*) -> hb_bool_t {
    const auto& m = *static_cast<CanvasTextMetrics*>(data);
    extents->ascender = hb_position_t(m.font_ascent * 65536);
    extents->descender = -hb_position_t(m.font_descent * 65536);
    extents->line_gap = 0; return true;
  }, nullptr, nullptr);
  hb_font_set_funcs(measured, funcs, out, nullptr);
  const hb_ot_layout_baseline_tag_t tags[] = {HB_OT_LAYOUT_BASELINE_TAG_HANGING,
      HB_OT_LAYOUT_BASELINE_TAG_ROMAN, HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT};
  double* values[] = {&out->hanging_baseline, &out->alphabetic_baseline, &out->ideographic_baseline};
  for (size_t i = 0; i < 3; ++i) {
    hb_position_t position = 0;
    hb_ot_layout_get_baseline_with_fallback(measured, tags[i], HB_DIRECTION_LTR,
        HB_OT_TAG_DEFAULT_SCRIPT, HB_OT_TAG_DEFAULT_LANGUAGE, &position);
    *values[i] = position / 65536.0;
  }
  hb_font_destroy(measured); hb_font_funcs_destroy(funcs);
  hb_font_destroy(font); hb_face_destroy(face);
  out->ink = ink;
  if (ink) { out->ink_left = left; out->ink_top = top; out->ink_right = right; out->ink_bottom = bottom; }
  return true;
}
bool SystemFonts::LineMetrics(const FontInput& input, double* content, double* normal) {
  auto face = Select(*collection_, input, ' ');
  if (!face) return false;
  SkFontMetrics metrics;
  SkFont(face, float(input.size)).getMetrics(&metrics);
  *content = std::ceil(-metrics.fAscent) + std::ceil(metrics.fDescent);
  *normal = std::max(*content + metrics.fLeading, input.size * 1.2);
  return true;
}
bool SystemFonts::SvgLengthMetrics(const FontInput& input, float* ex, float* ch, float* ic, float* cap) {
  auto face = Select(*collection_, input, ' ');
  if (!face) return false;
  SkFont font(face, float(input.size));
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  *ex = metrics.fXHeight;
  *cap = metrics.fCapHeight;
  for (unsigned i = 0; i < 2; ++i) {
    uint16_t glyph = face->unicharToGlyph(i ? 0x6c34 : '0');
    float width = float(input.size * (i ? 1 : .5));
    if (glyph) font.getWidths(SkSpan(&glyph, 1), SkSpan(&width, 1));
    *(i ? ic : ch) = width;
  }
  return true;
}
bool SystemFonts::Measure(const std::u16string& text, const FontInput& input, domain::geometry::SvgTextMeasurement* out) {
  std::u16string normalized;
  bool previousSpace = true;
  for (char16_t ch : text) {
    const bool space = ch == u' ' || ch == u'\n' || ch == u'\r' || ch == u'\t';
    if (!space || !previousSpace) normalized += space ? u' ' : ch;
    previousSpace = space;
  }
  if (!normalized.empty() && normalized.back() == u' ') normalized.pop_back();
  FontInput local = input;
  local.scale = 1;
  CanvasTextMetrics metrics;
  if (!MeasureCanvas(normalized, local, &metrics)) return false;
  *out = {};
  out->length = metrics.width;
  out->box = {float(input.x), float(input.y - metrics.font_ascent), metrics.width,
                float(metrics.font_ascent + metrics.font_descent)};
  out->transformed = input.transform.Apply(out->box);
  Runs runs;
  float size;
  return Shape(*collection_, normalized, input, &runs.values, &size, &out->characters);
}
bool SystemFonts::BuildCanvasTextPath(const std::u16string& text, const FontInput& input, CanvasTextPath* out) {
  Runs runs;
  float size;
  if (!Shape(*collection_, text, input, &runs.values, &size)) return false;
  for (const auto& run : runs.values) {
    SkFont font(sk_ref_sp(run.face), size);
    font.setSubpixel(true);
    font.setEdging(SkFont::Edging::kAntiAlias);
    font.setSkewX(run.syntheticItalic ? -.249328f : 0);
    font.setEmbolden(run.syntheticBold);
    for (size_t i = 0; i < run.glyphs.size(); ++i) {
      auto path = font.getPath(run.glyphs[i]);
      if (!path) continue;
      *path = path->makeOffset(float(run.x[i]), float(run.y[i]));
      SkPath::Iter it(*path, false);
      SkPoint points[4];
      for (auto verb = it.next(points); verb != SkPath::kDone_Verb; verb = it.next(points)) {
        switch (verb) {
          case SkPath::kMove_Verb: out->Move(points[0].x(), points[0].y()); break;
          case SkPath::kLine_Verb: out->Line(points[1].x(), points[1].y()); break;
          case SkPath::kQuad_Verb: out->Quad(points[1].x(), points[1].y(), points[2].x(), points[2].y()); break;
          case SkPath::kCubic_Verb: out->Cubic(points[1].x(), points[1].y(), points[2].x(), points[2].y(), points[3].x(), points[3].y()); break;
          case SkPath::kClose_Verb: out->Close(); break;
          default: return false;
        }
      }
    }
  }
  return true;
}
bool SystemFonts::BuildCanvasGlyphMasks(const std::u16string& text, const FontInput& input, const float matrix[6],
                                      const uint8_t rgb[3], std::vector<CanvasGlyphMask>* out) {
  Runs runs;
  float size;
  if (!Shape(*collection_, text, input, &runs.values, &size)) return false;
  for (const auto& run : runs.values) {
    SkFont font(sk_ref_sp(run.face), size);
    font.setSubpixel(true);
    font.setEdging(SkFont::Edging::kAntiAlias);
    font.setSkewX(run.syntheticItalic ? -.249328f : 0);
    font.setEmbolden(run.syntheticBold);
    const SkMatrix transform = Matrix(matrix);
    for (size_t i = 0; i < run.glyphs.size(); ++i) {
      SkRect box;
      font.getBounds(SkSpan(&run.glyphs[i], 1), SkSpan(&box, 1), nullptr);
      box.offset(float(run.x[i]), float(run.y[i]));
      transform.mapRect(&box);
      const SkIRect rect = box.makeOutset(1, 1).roundOut();
      if (rect.isEmpty()) continue;
      if (rect.width() > 32768 || rect.height() > 32768 || int64_t(rect.width()) * rect.height() > 64000000) return false;
      const auto image = run.colorFont ? SkImageInfo::Make(rect.width(),rect.height(),kRGBA_8888_SkColorType,kPremul_SkAlphaType)
                                      : SkImageInfo::MakeA8(rect.width(), rect.height());
      auto surface = SkSurfaces::Raster(image);
      if (!surface) return false;
      auto* canvas = surface->getCanvas();
      canvas->clear(SK_ColorTRANSPARENT);
      canvas->translate(float(-rect.left()), float(-rect.top()));
      canvas->concat(transform);
      SkPaint paint;
      paint.setColor(run.colorFont ? SkColorSetRGB(rgb[0],rgb[1],rgb[2]) : SK_ColorWHITE);
      paint.setAntiAlias(true);
      SkPoint position = SkPoint::Make(float(run.x[i]), float(run.y[i]));
      canvas->drawGlyphs(SkSpan(&run.glyphs[i], 1), SkSpan(&position, 1), SkPoint::Make(0, 0), font, paint);
      CanvasGlyphMask mask;
      mask.left = rect.left(); mask.top = rect.top(); mask.width = rect.width(); mask.height = rect.height();
      auto& pixels = run.colorFont ? mask.rgba : mask.coverage;
      pixels.resize(image.computeMinByteSize());
      if (!surface->readPixels(image,pixels.data(),image.minRowBytes(),0,0)) return false;
      out->push_back(std::move(mask));
    }
  }
  return true;
}
}

#endif  // __linux__
