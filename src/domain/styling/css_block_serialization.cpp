// CSSOM §6.7.2 serialize a CSS declaration block, and the shorthand-aware
// getPropertyValue lookup. Every declaration block (rule style, inline style)
// is serialized here so a shorthand reads back the same through both.
#include "domain/styling/css_declarations.h"
#include "domain/styling/css_box_value.h"
#include "domain/styling/css_font_value.h"
#include <algorithm>
#include <unordered_set>

namespace ruyijs::domain::styling {
namespace {
// Preferred order: the shorthand with the most longhands first (Servo
// PropertyDeclarationBlock::to_css): border (17), border-image (5), the four-
// side border-width/style/color (4), then the per-side border-top... (3).
constexpr const char* kShorthands[] = {
  "font", "border", "border-image", "border-width", "border-style", "border-color",
  "border-top", "border-right", "border-bottom", "border-left", "margin", "padding", "inset",
  "flex", "flex-flow", "gap", "overflow"};
constexpr const char* kBorderImage[] = {"border-image-source", "border-image-slice",
                                        "border-image-width", "border-image-outset",
                                        "border-image-repeat"};
// shorthands.rs border_image::to_css: the non-initial components, the source
// alone when every component is initial.
std::string BorderImage(const std::vector<CssDeclarationValue>& rows, bool* important) {
  std::string values[5];
  bool flag = false;
  for (size_t i = 0; i < 5; ++i) {
    auto entry = std::find_if(rows.begin(), rows.end(), [&](const auto& r) { return r.name == kBorderImage[i]; });
    if (entry == rows.end() || (i && entry->important != flag)) return {};
    flag = entry->important; values[i] = entry->value;
  }
  constexpr const char* initial[] = {"none", "100%", "1", "0", "stretch"};
  bool has[5], any = false;
  for (size_t i = 0; i < 5; ++i) { has[i] = values[i] != initial[i]; any = any || has[i]; }
  if (important) *important = flag;
  std::string out;
  if (has[0] || !any) { out = values[0]; if (!any) return out; }
  const bool slice = has[1] || has[2] || has[3];
  if (slice) {
    if (has[0]) out += ' ';
    out += values[1];
    if (has[2] || has[3]) {
      out += " /";
      if (has[2]) out += " " + values[2];
      if (has[3]) out += " / " + values[3];
    }
  }
  if (has[4]) { if (has[0] || slice) out += ' '; out += values[4]; }
  return out;
}
std::vector<std::string> Longhands(std::string_view shorthand) {
  if (shorthand == "border-image") return {std::begin(kBorderImage), std::end(kBorderImage)};
  if (shorthand != "font") return CssBoxLonghands(shorthand);
  std::vector<std::string> names;
  for (const auto& row : CssFontDefaults()) names.push_back(row.name);
  return names;
}
std::string Shorthand(const std::vector<CssDeclarationValue>& rows, std::string_view name, bool* important) {
  if (name == "border-image") return BorderImage(rows, important);
  return name == "font" ? CssFontSerialize(rows, important) : CssBoxSerialize(rows, std::string(name), important);
}
}  // namespace

bool CssShorthandName(std::string_view name) { return name == "font" || CssBoxShorthand(name); }

std::string CssLookupDeclaration(const std::vector<CssDeclarationValue>& rows, const std::string& inputName,
                                 bool* important) {
  const auto name=CssCanonicalDeclarationName(inputName);
  *important = false;
  if (CssShorthandName(name)) return Shorthand(rows, name, important);
  for (const auto& row : rows)
    if (row.name == name) { *important = row.important; return row.value; }
  return {};
}

std::string CssLookupDeclaration(const CssParsedBlock& block, const std::string& inputName,
                                 bool* important) {
  const auto name=CssCanonicalDeclarationName(inputName);
  *important = false;
  if (CssShorthandName(name)) return Shorthand(block.rows, name, important);
  if (!block.index.empty()) {
    const auto it = block.index.find(name);
    if (it == block.index.end()) return {};
    const auto& row = block.rows[it->second];
    *important = row.important;
    return row.value;
  }
  for (const auto& row : block.rows)
    if (row.name == name) { *important = row.important; return row.value; }
  return {};
}

std::string CssSerializeBlock(const std::vector<CssDeclarationValue>& rows) {
  std::unordered_set<std::string> done;
  done.reserve(rows.size());
  std::string out;
  out.reserve(rows.size() * 20);
  for (const auto& row : rows) {
    if (done.count(row.name)) continue;
    std::string name = row.name, value = row.value;
    bool important = row.important;
    if (!(row.name.size() >= 2 && row.name[0] == '-' && row.name[1] == '-')) {
      for (const char* shorthand : kShorthands) {
        const auto names = Longhands(shorthand);
        if (std::find(names.begin(), names.end(), row.name) == names.end()) continue;
        if (std::any_of(names.begin(), names.end(), [&](const auto& n) { return done.count(n) != 0; })) continue;
        bool flag = false;
        auto text = Shorthand(rows, shorthand, &flag);
        if (text.empty()) continue;
        name = shorthand; value = std::move(text); important = flag;
        done.insert(names.begin(), names.end());
        break;
      }
    }
    done.insert(row.name);
    if (!out.empty()) out += ' ';
    out += name + ": " + value + (important ? " !important" : "") + ";";
  }
  return out;
}

const std::string& CssSerializeBlockCached(const CssParsedBlock& block) {
  if (!block.serialized_ready) {
    block.serialized = CssSerializeBlock(block.rows);
    block.serialized_ready = true;
  }
  return block.serialized;
}
}  // namespace ruyijs::domain::styling
