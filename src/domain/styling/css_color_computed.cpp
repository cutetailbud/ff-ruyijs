// The computed value of a <color>: Firefox serializes every resolved sRGB
// colour as rgb(r, g, b) or rgba(r, g, b, a) whatever its specified form
// (names, hex, rgb()/hsl() syntaxes, transparent, currentcolor).
#include "domain/styling/css_color_value.h"
#include "domain/styling/css_numeric_value.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <string>

namespace ruyijs::domain::styling {
namespace {
std::string Lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return out;
}
std::string Trim(std::string_view text) {
  size_t a = 0, b = text.size();
  while (a < b && (text[a] == ' ' || text[a] == '\t' || text[a] == '\n')) ++a;
  while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t' || text[b - 1] == '\n')) --b;
  return std::string(text.substr(a, b - a));
}
// CSS Color 4 serialization: the alpha of an 8-bit colour with two decimals,
// or three when two do not round-trip.
std::string Alpha(uint8_t a) {
  char buffer[16];
  for (int digits : {2, 3}) {
    const double scale = digits == 2 ? 100.0 : 1000.0;
    const double value = std::round(a / 255.0 * scale) / scale;
    if (digits == 3 || std::lround(value * 255.0) == a) {
      std::snprintf(buffer, sizeof buffer, digits == 2 ? "%.2f" : "%.3f", value);
      std::string text(buffer);
      while (!text.empty() && text.back() == '0') text.pop_back();
      if (!text.empty() && text.back() == '.') text.pop_back();
      return text;
    }
  }
  return "0";
}
// Firefox computes a math function in a colour channel to its value:
// rgb(calc(var(--a) * 2), ...) is rgb(<number>, ...) once var() is substituted,
// and the channel is then clamped and rounded like a literal one (even the
// specified value reads rgb(3, 0, 0) for rgb(calc(1 + 2), 0, 0)). Each math
// function directly inside the colour function whose value is a plain number
// is replaced by that number; any other one is left as written.
bool MathFunctionName(const std::string& name) {
  static const char* const kNames[] = {"calc", "min", "max", "clamp", "round", "mod", "rem", "abs",
                                       "sign", "pow", "sqrt", "hypot", "log", "exp"};
  for (const char* known : kNames) if (name == known) return true;
  return false;
}
std::string ResolveChannelMath(const std::string& text) {
  const size_t open = text.find('(');
  if (open == std::string::npos || text.empty() || text.back() != ')') return text;
  std::string out = text.substr(0, open + 1);
  const size_t end = text.size() - 1;
  bool changed = false;
  size_t i = open + 1;
  const auto ident = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-'; };
  while (i < end) {
    if (!ident(text[i])) { out += text[i]; ++i; continue; }
    size_t name_end = i;
    while (name_end < end && ident(text[name_end])) ++name_end;
    if (name_end >= end || text[name_end] != '(') { out.append(text, i, name_end - i); i = name_end; continue; }
    int depth = 0;
    size_t close = name_end;
    for (; close < end; ++close) {
      if (text[close] == '(') ++depth;
      else if (text[close] == ')' && --depth == 0) break;
    }
    if (close >= end) return text;
    const std::string component = text.substr(i, close + 1 - i);
    std::string value;
    if (MathFunctionName(Lower(text.substr(i, name_end - i))) && CssNumericParse(component).kind == CssNumber) {
      const CssNumericValue resolved = CssNumericResolve(component, 16, 16, 100);
      if (resolved.kind == CssNumber && std::isfinite(resolved.number)) {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%.6f", std::clamp(resolved.number, -1e9, 1e9));
        value = buffer;
      }
    }
    if (value.empty()) out += component;
    else { out += value; changed = true; }
    i = close + 1;
  }
  out += ')';
  return changed ? out : text;
}
}  // namespace
std::string CssComputedColor(std::string_view value, std::string_view current_color) {
  const std::string text = Trim(value);
  if (text.empty()) return {};
  const std::string lower = Lower(text);
  if (lower == "currentcolor") return current_color.empty() ? std::string() : CssComputedColor(current_color, {});
  uint8_t rgba[4];
  if (!CssColorRgba(ResolveChannelMath(text), rgba)) return {};
  char buffer[64];
  if (rgba[3] == 255) std::snprintf(buffer, sizeof buffer, "rgb(%u, %u, %u)", rgba[0], rgba[1], rgba[2]);
  else std::snprintf(buffer, sizeof buffer, "rgba(%u, %u, %u, %s)", rgba[0], rgba[1], rgba[2], Alpha(rgba[3]).c_str());
  return buffer;
}
}  // namespace ruyijs::domain::styling
