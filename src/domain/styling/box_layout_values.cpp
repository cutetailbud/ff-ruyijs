#include "domain/styling/box_layout_internal.h"
#include "domain/styling/css_numeric_value.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace ruyijs::domain::styling {
const std::string& BoxStyle::Get(const char* name) const {
  static const std::string missing;
  if (!values) return missing;
  // Every computed style lists nearly the same properties in the same sorted
  // order, so where this thread last found a name (callers pass literals)
  // usually holds it again: one comparison instead of a search.
  struct Hint { const char* name = nullptr; size_t index = 0; };
  thread_local std::array<Hint, 256> hints;
  auto& hint = hints[(reinterpret_cast<uintptr_t>(name) * 0x9E3779B97F4A7C15ull) >> 56];
  const auto& list = *values;
  if (hint.name == name && hint.index < list.size() && list[hint.index].first == name)
    return list[hint.index].second;
  const std::string_view key(name);
  const auto found = std::lower_bound(list.begin(), list.end(), key,
      [](const auto& entry, std::string_view wanted) { return std::string_view(entry.first) < wanted; });
  if (found == list.end() || found->first != key) return missing;
  hint = {name, size_t(found - list.begin())};
  return found->second;
}
std::shared_ptr<const BoxStyle::Values> BoxStyle::Share(Values values) {
  // A repeated name keeps its first value, the one a scan in order finds.
  std::stable_sort(values.begin(), values.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  values.erase(std::unique(values.begin(), values.end(), [](const auto& a, const auto& b) { return a.first == b.first; }),
               values.end());
  return std::make_shared<const Values>(std::move(values));
}
namespace {
// Layout resolves the same few lengths ("0px", "8px", "auto") for most boxes
// of a pass, and CssNumericResolve is a pure function of its arguments: each
// thread keeps recent answers in a small direct-mapped table. The percentage
// basis, which differs from box to box, only matters to a value with a '%'.
CssNumericValue ResolveLength(const std::string& text, double em, double rem, double percent) {
  struct Entry { std::string text; uint64_t numbers[3] = {}; bool valid = false; CssNumericValue value; };
  thread_local std::array<Entry, 512> table;
  const double basis = text.find('%') == std::string::npos ? 0 : percent;
  uint64_t numbers[3];
  std::memcpy(&numbers[0], &em, 8); std::memcpy(&numbers[1], &rem, 8); std::memcpy(&numbers[2], &basis, 8);
  uint64_t hash = 1469598103934665603ull;
  for (const char c : text) { hash ^= static_cast<unsigned char>(c); hash *= 1099511628211ull; }
  for (const uint64_t bits : numbers) { hash ^= bits; hash *= 1099511628211ull; }
  auto& entry = table[(hash >> 32) & 511];
  if (entry.valid && std::memcmp(entry.numbers, numbers, sizeof numbers) == 0 && entry.text == text)
    return entry.value;
  entry.value = CssNumericResolve(text, em, rem, percent);
  entry.text.assign(text);
  std::memcpy(entry.numbers, numbers, sizeof numbers);
  entry.valid = true;
  return entry.value;
}
}
double BoxLength(const BoxStyle& style, const char* name, double percent, double fallback) {
  const auto& text = style.Get(name);
  // A plain block's used lengths are almost all auto, none, or 0px. Those
  // resolve to the fallback or to zero without parsing, and they are the
  // values Place reads a dozen times per box.
  if (text.empty() || text == "auto" || text == "none") return fallback;
  if (text == "0" || text == "0px") return 0;
  if (!std::isfinite(percent) && text.find('%') != text.npos) return fallback;
  // A bare percentage: Gecko resolves it against the app-unit basis with
  // LengthPercentage::to_used_value, multiplying in f32 and truncating to app
  // units (nsLayoutUtils::ComputeCBDependentValue / Au::from_f32_px_trunc).
  if (text.size() > 1 && text.back() == '%' && text.find_first_of("()") == std::string::npos) {
    char* end = nullptr;
    const double number = std::strtod(text.c_str(), &end);
    if (end == text.c_str() + text.size() - 1 && std::isfinite(number)) {
      const float basis = float(std::floor(percent * 60 + .5));
      const float used = basis * (float(number) / 100.0f);
      return std::trunc(std::clamp(double(used), -kBoxCoordMax, kBoxCoordMax)) / 60;
    }
  }
  const auto parsed = ResolveLength(text, style.font_size, style.root_font_size,
                                    std::isfinite(percent) ? percent : 0);
  return parsed.kind && (parsed.kind == CssLength || parsed.number == 0)
      ? BoxRound(parsed.number) : fallback;
}
bool BoxViewportLength(const std::string& text, double width, double height, double* out) {
  const char* units[] = {"vmin", "vmax", "vw", "vh"};
  for (const char* unit : units) {
    const size_t n = std::strlen(unit);
    if (text.size() <= n || text.compare(text.size() - n, n, unit) != 0) continue;
    char* end = nullptr;
    const std::string number = text.substr(0, text.size() - n);
    const double value = std::strtod(number.c_str(), &end);
    if (end != number.c_str() + number.size() || !std::isfinite(value)) return false;
    const double base = unit[1] == 'm' ? (unit[2] == 'i' ? std::min(width, height) : std::max(width, height))
                                       : (unit[1] == 'w' ? width : height);
    *out = BoxRound(value * base / 100);
    return true;
  }
  return false;
}
bool BoxReplaced(const BoxNode& node) {
  return node.tag == "iframe" || node.tag == "img" || node.tag == "canvas" ||
         node.tag == "video" || node.tag == "object" || node.tag == "embed";
}
bool BoxAbsolute(const BoxNode& node) {
  // A text node copies its parent's style but is never itself positioned.
  if (node.text) return false;
  const auto position = node.style.Get("position");
  return position == "absolute" || position == "fixed";
}
void BoxEdges(const BoxNode& node, double width, BoxLayout* box) {
  static const char* const margins[] = {"margin-top", "margin-right", "margin-bottom", "margin-left"};
  static const char* const paddings[] = {"padding-top", "padding-right", "padding-bottom", "padding-left"};
  static const char* const borders[] = {"border-top-width", "border-right-width", "border-bottom-width",
                                        "border-left-width"};
  for (unsigned i = 0; i < 4; ++i) {
    box->margin[i] = BoxLength(node.style, margins[i], width, 0);
    if (node.tag == "body" && !(node.style.specified_margin_mask & (1u << i))) box->margin[i] = 8;
    box->padding[i] = std::max(0.0, BoxLength(node.style, paddings[i], width, 0));
    box->edge[i] = std::max(0.0, BoxLength(node.style, borders[i], width, 0));
    if (node.tag == "iframe" && !(node.style.specified_border_mask & (1u << i))) box->edge[i] = node.no_frame_border ? 0 : 2;
    const bool text_input = node.tag == "input" && (node.input_type.empty() || node.input_type == "text" ||
        node.input_type == "password" || node.input_type == "email" || node.input_type == "url" ||
        node.input_type == "tel" || node.input_type == "search");
    if (text_input) {
      if (!(node.style.specified_border_mask & (1u << i))) box->edge[i] = 2;
      if (!(node.style.specified_padding_mask & (1u << i))) box->padding[i] = i % 2 ? 2 : 1;
    }
  }
}
double BoxLimit(const BoxStyle& style, bool width, double available, double value, double extras) {
  const auto minimum = BoxLength(style, width ? "min-width" : "min-height", available, 0);
  const auto maximum = BoxLength(style, width ? "max-width" : "max-height", available,
                                 std::numeric_limits<double>::infinity());
  const auto subtract = style.Get("box-sizing") == "border-box" ? extras : 0;
  return std::max(0.0, std::max(minimum - subtract, std::min(value, maximum - subtract)));
}
}
