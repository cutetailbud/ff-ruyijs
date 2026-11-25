#pragma once
// getComputedStyle(el).transform as Firefox serializes it: the transform list
// folded into one matrix(a, b, c, d, e, f) or matrix3d(...) (nsComputedDOMStyle
// ::MatrixToCSSValue over nsStyleTransformMatrix::ReadTransforms), "none" kept.
//
// gfx::Matrix4x4 is float, so every transform function is applied in single
// precision the way Gecko applies it (PreTranslate, PreScale, Perspective,
// RotateX/Y/Z with double sines and cosines, the rotate3d axis normalised in
// float, the 4x4 multiply), and each entry is printed with
// nsTSubstring::AppendFloat(float): the exact float rounded to 6 significant
// digits. A list the float path does not handle (other units, keywords) goes
// through the double-precision geometry parser instead; a list neither path
// reads is returned unchanged.
#include "domain/geometry/css_transform.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace ruyijs::domain::styling {
namespace transform_matrix_detail {
// m[0..15] are _11.._44, the matrix3d() argument order.
struct Matrix {
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  float& at(int row, int col) { return m[(row - 1) * 4 + (col - 1)]; }
};
inline constexpr double kRadPerDegree = 2.0 * 3.14159265358979323846 / 360.0;
inline double FlushToZero(double value) {  // FLT_EPSILON
  return (-1.1920928955078125e-07 < value && value < 1.1920928955078125e-07) ? 0.0 : value;
}
inline double SafeTangent(double theta) {
  const double epsilon = 0.0001;
  const double sine = std::sin(theta);
  double cosine = std::cos(theta);
  if (cosine >= 0 && cosine < epsilon) cosine = epsilon;
  else if (cosine < 0 && cosine >= -epsilon) cosine = -epsilon;
  return FlushToZero(sine / cosine);
}
inline Matrix Multiply(const Matrix& a, const Matrix& b) {  // a * b
  Matrix r;
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col) {
      const float* x = a.m + row * 4;
      r.m[row * 4 + col] = x[0] * b.m[col] + x[1] * b.m[4 + col] + x[2] * b.m[8 + col] + x[3] * b.m[12 + col];
    }
  return r;
}
inline void PreTranslate(Matrix& m, float x, float y, float z) {
  for (int col = 1; col <= 4; ++col) m.at(4, col) += x * m.at(1, col) + y * m.at(2, col) + z * m.at(3, col);
}
inline void PreScale(Matrix& m, float x, float y, float z) {
  for (int col = 1; col <= 4; ++col) { m.at(1, col) *= x; m.at(2, col) *= y; m.at(3, col) *= z; }
}
inline void Perspective(Matrix& m, float depth) {
  for (int col = 1; col <= 4; ++col) m.at(3, col) += -1.0 / depth * m.at(4, col);
}
// RotateX/RotateZ: rows (a, b) -> (c*a + s*b, -s*a + c*b); RotateY: (c*a - s*b, s*a + c*b).
inline void Rotate(Matrix& m, int first, int second, double theta, bool y_axis) {
  const double c = FlushToZero(std::cos(theta)), s = FlushToZero(std::sin(theta));
  for (int col = 1; col <= 4; ++col) {
    const float a = m.at(first, col), b = m.at(second, col);
    if (y_axis) { m.at(first, col) = c * a + -s * b; m.at(second, col) = s * a + c * b; }
    else { m.at(first, col) = c * a + s * b; m.at(second, col) = -s * a + c * b; }
  }
}
inline void SkewXY(Matrix& m, double x_skew, double y_skew) {
  const float tan_x = float(SafeTangent(x_skew)), tan_y = float(SafeTangent(y_skew));
  for (int col = 1; col <= 4; ++col) {
    const float first = m.at(1, col);
    m.at(1, col) += tan_y * m.at(2, col);
    m.at(2, col) += tan_x * first;
  }
}
inline Matrix RotateAxisAngle(float ax, float ay, float az, double theta) {
  Matrix r;
  float vx = ax, vy = ay, vz = az;
  float length = std::sqrt(vx * vx + vy * vy + vz * vz);
  if (!length) return r;
  if (std::isinf(length)) {
    const float big = std::numeric_limits<float>::max();
    vx /= big; vy /= big; vz /= big;
    length = std::sqrt(vx * vx + vy * vy + vz * vz);
  }
  vx /= length; vy /= length; vz /= length;
  const double x = vx, y = vy, z = vz;
  const double c = FlushToZero(std::cos(theta)), s = FlushToZero(std::sin(theta));
  const double sc = s / 2, sq = (1 - c) / 2;
  r.at(1, 1) = float(1 - 2 * (y * y + z * z) * sq); r.at(1, 2) = float(2 * (x * y * sq + z * sc));
  r.at(1, 3) = float(2 * (x * z * sq - y * sc));    r.at(1, 4) = 0;
  r.at(2, 1) = float(2 * (x * y * sq - z * sc));    r.at(2, 2) = float(1 - 2 * (x * x + z * z) * sq);
  r.at(2, 3) = float(2 * (y * z * sq + x * sc));    r.at(2, 4) = 0;
  r.at(3, 1) = float(2 * (x * z * sq + y * sc));    r.at(3, 2) = float(2 * (y * z * sq - x * sc));
  r.at(3, 3) = float(1 - 2 * (x * x + y * y) * sq); r.at(3, 4) = 0;
  return r;
}
inline std::string Lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return out;
}
inline std::string_view Trim(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\n')) text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\n')) text.remove_suffix(1);
  return text;
}
// The top-level items of `text` split at `separator` (whitespace when ' '),
// parentheses kept together.
inline std::vector<std::string> Items(std::string_view text, char separator) {
  std::vector<std::string> out;
  std::string current;
  int depth = 0;
  for (char c : text) {
    const bool split = depth == 0 && (separator == ' ' ? (c == ' ' || c == '\t' || c == '\n') : c == separator);
    if (c == '(') ++depth;
    else if (c == ')') --depth;
    if (split) {
      if (!Trim(current).empty()) out.emplace_back(Trim(current));
      current.clear();
      continue;
    }
    current += c;
  }
  if (!Trim(current).empty()) out.emplace_back(Trim(current));
  return out;
}
// One argument as Servo computes it: an f32 number with no unit, px for a
// length, degrees for an angle. False for anything else.
inline bool Argument(std::string_view text, bool angle, float* out) {
  const std::string owned(Trim(text));
  char* end = nullptr;
  const double number = std::strtod(owned.c_str(), &end);
  if (end == owned.c_str()) return false;
  const std::string unit = Lower(end);
  const float value = float(number);
  if (angle) {
    if (unit == "deg") *out = value;
    else if (unit == "turn") *out = value * 360.0f;
    else if (unit == "grad") *out = value * (360.0f / 400.0f);
    else if (unit == "rad") *out = value * (180.0f / 3.14159265358979323846f);
    else if (unit.empty() && value == 0) *out = 0;
    else return false;
    return true;
  }
  if (unit.empty() || unit == "px") { *out = value; return true; }
  return false;
}
inline bool FloatTransform(const std::vector<std::string>& functions, float values[16]) {
  Matrix m;
  for (const auto& function : functions) {
    const auto open = function.find('(');
    if (open == std::string::npos || function.back() != ')') return false;
    const std::string name = Lower(std::string_view(function).substr(0, open));
    const auto parts = Items(std::string_view(function).substr(open + 1, function.size() - open - 2), ',');
    std::vector<float> a(parts.size());
    const auto args = [&](size_t count, std::initializer_list<size_t> angles) {
      if (parts.size() != count) return false;
      for (size_t i = 0; i < count; ++i)
        if (!Argument(parts[i], std::find(angles.begin(), angles.end(), i) != angles.end(), &a[i])) return false;
      return true;
    };
    const auto radians = [](float degrees) { return double(degrees) * kRadPerDegree; };
    if (name == "perspective") {
      if (parts.size() == 1 && Lower(parts[0]) == "none") continue;
      if (!args(1, {})) return false;
      if (!std::isinf(a[0])) Perspective(m, std::max(a[0], 1.0f));
    } else if (name == "translate" && (parts.size() == 1 || parts.size() == 2)) {
      if (!args(parts.size(), {})) return false;
      PreTranslate(m, a[0], parts.size() == 2 ? a[1] : 0.0f, 0.0f);
    } else if (name == "translatex" || name == "translatey" || name == "translatez") {
      if (!args(1, {})) return false;
      PreTranslate(m, name == "translatex" ? a[0] : 0.0f, name == "translatey" ? a[0] : 0.0f,
                   name == "translatez" ? a[0] : 0.0f);
    } else if (name == "translate3d") {
      if (!args(3, {})) return false;
      PreTranslate(m, a[0], a[1], a[2]);
    } else if (name == "scale" && (parts.size() == 1 || parts.size() == 2)) {
      if (!args(parts.size(), {})) return false;
      PreScale(m, a[0], parts.size() == 2 ? a[1] : a[0], 1.0f);
    } else if (name == "scalex" || name == "scaley" || name == "scalez") {
      if (!args(1, {})) return false;
      PreScale(m, name == "scalex" ? a[0] : 1.0f, name == "scaley" ? a[0] : 1.0f, name == "scalez" ? a[0] : 1.0f);
    } else if (name == "scale3d") {
      if (!args(3, {})) return false;
      PreScale(m, a[0], a[1], a[2]);
    } else if (name == "rotate" || name == "rotatez") {
      if (!args(1, {0})) return false;
      Rotate(m, 1, 2, radians(a[0]), false);
    } else if (name == "rotatex") {
      if (!args(1, {0})) return false;
      Rotate(m, 2, 3, radians(a[0]), false);
    } else if (name == "rotatey") {
      if (!args(1, {0})) return false;
      Rotate(m, 1, 3, radians(a[0]), true);
    } else if (name == "rotate3d") {
      if (!args(4, {3})) return false;
      m = Multiply(RotateAxisAngle(a[0], a[1], a[2], radians(a[3])), m);
    } else if (name == "skew" && (parts.size() == 1 || parts.size() == 2)) {
      if (!args(parts.size(), {0, 1})) return false;
      SkewXY(m, radians(a[0]), parts.size() == 2 ? radians(a[1]) : 0.0);
    } else if (name == "skewx" || name == "skewy") {
      if (!args(1, {0})) return false;
      SkewXY(m, name == "skewx" ? radians(a[0]) : 0.0, name == "skewy" ? radians(a[0]) : 0.0);
    } else if (name == "matrix") {
      if (!args(6, {})) return false;
      Matrix t;
      t.at(1, 1) = a[0]; t.at(1, 2) = a[1]; t.at(2, 1) = a[2]; t.at(2, 2) = a[3]; t.at(4, 1) = a[4]; t.at(4, 2) = a[5];
      m = Multiply(t, m);
    } else if (name == "matrix3d") {
      if (!args(16, {})) return false;
      Matrix t;
      for (int i = 0; i < 16; ++i) t.m[i] = a[i];
      m = Multiply(t, m);
    } else {
      return false;
    }
  }
  for (int i = 0; i < 16; ++i) values[i] = m.m[i];
  return true;
}
// nsTSubstring::AppendFloat(float): double-conversion ToPrecision(value, 6)
// with NO_TRAILING_ZERO and UNIQUE_ZERO, exponential below 1e-6 or from 1e7 on.
inline std::string FloatText(float value) {
  const double v = value;
  if (std::isnan(v)) return "NaN";
  if (std::isinf(v)) return v < 0 ? "-Infinity" : "Infinity";
  if (v == 0) return "0";
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.5e", v);  // d.ddddde[+-]xx, correctly rounded
  std::string text(buffer);
  const bool negative = text[0] == '-';
  if (negative) text.erase(0, 1);
  const size_t e = text.find('e');
  std::string digits = text.substr(0, 1) + text.substr(2, e - 2);
  const int exponent = std::atoi(text.c_str() + e + 1);
  const int decimal_point = exponent + 1;
  const bool exponential = (1 - decimal_point > 6) || (decimal_point - 6 > 1);
  const size_t stop = exponential ? 1 : size_t(std::max(1, decimal_point));
  while (digits.size() > stop && digits.back() == '0') digits.pop_back();
  std::string out = negative ? "-" : "";
  if (exponential) {
    out += digits[0];
    if (digits.size() > 1) out += "." + digits.substr(1);
    out += exponent < 0 ? "e-" : "e+";
    out += std::to_string(exponent < 0 ? -exponent : exponent);
  } else if (decimal_point <= 0) {
    out += "0." + std::string(size_t(-decimal_point), '0') + digits;
  } else if (size_t(decimal_point) >= digits.size()) {
    out += digits + std::string(size_t(decimal_point) - digits.size(), '0');
  } else {
    out += digits.substr(0, size_t(decimal_point)) + "." + digits.substr(size_t(decimal_point));
  }
  return out;
}
// A translation percentage resolves against the reference box (the border
// box; an element without one has an empty box).
inline std::string ResolvePercent(const std::string& argument, double reference) {
  const std::string_view text = Trim(argument);
  if (text.empty() || text.back() != '%') return argument;
  const std::string owned(text.substr(0, text.size() - 1));
  char* end = nullptr;
  const double number = std::strtod(owned.c_str(), &end);
  if (end == owned.c_str() || *end) return argument;
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.9gpx", number * reference / 100.0);
  return buffer;
}
}  // namespace transform_matrix_detail

inline std::string CssComputedTransformMatrix(const std::string& input, double width, double height) {
  using namespace transform_matrix_detail;
  const std::string_view trimmed = Trim(input);
  if (trimmed.empty() || Lower(trimmed) == "none") return input;
  std::vector<std::string> functions = Items(trimmed, ' ');
  std::string resolved;
  for (auto& function : functions) {
    const auto open = function.find('(');
    if (open != std::string::npos && function.back() == ')') {
      const std::string name = Lower(std::string_view(function).substr(0, open));
      if (name.rfind("translate", 0) == 0) {
        auto parts = Items(std::string_view(function).substr(open + 1, function.size() - open - 2), ',');
        for (size_t i = 0; i < parts.size() && i < 2; ++i)
          parts[i] = ResolvePercent(parts[i], (name == "translatey" || i == 1) ? height : width);
        function = name + "(";
        for (size_t i = 0; i < parts.size(); ++i) function += (i ? ", " : "") + parts[i];
        function += ")";
      }
    }
    resolved += (resolved.empty() ? "" : " ") + function;
  }
  float values[16];
  bool is3D = false;
  if (FloatTransform(functions, values)) {
    is3D = !(values[2] == 0 && values[3] == 0 && values[6] == 0 && values[7] == 0 && values[8] == 0 &&
             values[9] == 0 && values[10] == 1 && values[11] == 0 && values[14] == 0 && values[15] == 1);
  } else if (!ruyijs::domain::geometry::ParseCssTransform(resolved, values, &is3D)) {
    return input;
  }
  std::string out = is3D ? "matrix3d(" : "matrix(";
  static const unsigned k2D[] = {0, 1, 4, 5, 12, 13};
  for (unsigned i = 0; i < (is3D ? 16u : 6u); ++i) {
    if (i) out += ", ";
    out += FloatText(values[is3D ? i : k2D[i]]);
  }
  return out + ")";
}
}  // namespace ruyijs::domain::styling
