#pragma once
#include "domain/document_model/element.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace ruyijs::domain::document_model {
inline double meter_raw(const Element& e, DomStringView name, double fallback) {
  const auto raw = to_utf8(e.get_attribute16(name).value_or(u""));
  char* end = nullptr;
  const double value = std::strtod(raw.c_str(), &end);
  return end == raw.c_str() ? fallback : value;
}
// The HTML rules for parsing floating-point number values as Firefox applies
// them to <progress> (nsAttrValue::ParseDoubleValue): leading ASCII
// whitespace is skipped, the whole rest must be one finite number ("3 ",
// "2.5x", "Infinity", "0x10" and "1e400" are errors), "+4" and ".5" parse.
inline bool progress_number(const DomString& raw, double* out) {
  size_t at = 0;
  while (at < raw.size() && (raw[at] == u' ' || raw[at] == u'\t' || raw[at] == u'\n' ||
                             raw[at] == u'\f' || raw[at] == u'\r')) ++at;
  std::string text;
  for (size_t i = at; i < raw.size(); ++i) {
    const char16_t c = raw[i];
    if (!((c >= u'0' && c <= u'9') || c == u'.' || c == u'+' || c == u'-' || c == u'e' || c == u'E'))
      return false;
    text += char(c);
  }
  if (text.empty() || text[0] == 'e' || text[0] == 'E') return false;
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end != text.c_str() + text.size() || !std::isfinite(value)) return false;
  *out = value;
  return true;
}
// HTMLProgressElement max, value and position (HTML §4.10.13).
inline double progress_value(const Element& e, DomStringView name) {
  double max = 1, value = 0;
  if (progress_number(e.get_attribute16(u"max").value_or(u""), &max) && max > 0) {} else max = 1;
  if (name == u"max") return max;
  const auto raw = e.get_attribute16(u"value");
  const bool determinate = raw.has_value() && progress_number(*raw, &value);
  if (!determinate) value = 0;
  value = std::min(std::max(value, 0.0), max);
  if (name == u"value") return value;
  return determinate ? value / max : -1;
}
// A reflected double as Firefox writes it: JavaScript Number-to-String.
inline std::string progress_text(double value) {
  if (value == 0) return "0";
  char buffer[32];
  for (int digits = 1; digits <= 17; ++digits) {
    std::snprintf(buffer, sizeof buffer, "%.*g", digits, value);
    if (std::strtod(buffer, nullptr) == value) break;
  }
  std::string text(buffer);
  // %g writes 1e+21 as "1e+21" (as JS) and small exponents as "1e-07" (JS: "1e-7").
  const auto e = text.find('e');
  if (e != std::string::npos) {
    std::string mantissa = text.substr(0, e), exponent = text.substr(e + 1);
    const bool negative = exponent[0] == '-';
    exponent = exponent.substr(1);
    while (exponent.size() > 1 && exponent[0] == '0') exponent.erase(0, 1);
    const int power = std::stoi(exponent) * (negative ? -1 : 1);
    if (power >= -7 && power < 21) {
      std::snprintf(buffer, sizeof buffer, "%.17g", value);
      for (int digits = 1; digits <= 17; ++digits) {
        std::snprintf(buffer, sizeof buffer, "%.*f", std::max(0, digits - 1 - power), value);
        if (std::strtod(buffer, nullptr) == value) break;
      }
      return buffer;
    }
    text = mantissa + "e" + (negative ? "-" : "+") + exponent;
  }
  return text;
}
inline double meter_value(const Element& e, DomStringView name) {
  const double min = meter_raw(e, u"min", 0);
  if (name == u"min") return min;
  const double max = std::max(meter_raw(e, u"max", 1), min);
  if (name == u"max") return max;
  const auto clamp = [&](double value, double low) { return std::min(std::max(value, low), max); };
  if (name == u"value") return clamp(meter_raw(e, u"value", 0), min);
  if (name == u"optimum") return clamp(meter_raw(e, u"optimum", (min + max) / 2), min);
  const double low = clamp(meter_raw(e, u"low", min), min);
  if (name == u"low") return low;
  return clamp(meter_raw(e, u"high", max), low);
}
}
