/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
namespace ruyijs::domain::interaction {
// Firefox's WidgetPointerHelper conversion between tilt and altitude/azimuth
// (PointerEvent::Constructor). Explicit values stay as given; only the
// missing ones are derived, and a pointer with neither reads altitude pi/2,
// azimuth 0 and no tilt, as every mouse does.
struct PointerAngles {
  std::optional<int32_t> x, y;
  std::optional<double> altitude, azimuth;
  static constexpr double pi = 3.14159265358979323846, half = pi / 2;
  static int Tilt(int value) { value %= 180; if (value > 90) value -= 180; if (value < -90) value += 180; return value; }
  void Complete() {
    if ((x || y) && !altitude && !azimuth) {
      if (!x) x = 0;
      if (!y) y = 0;
      const int tx = Tilt(*x), ty = Tilt(*y);
      const double xr = pi / 180 * tx, yr = pi / 180 * ty;
      altitude = (std::abs(tx) == 90 || std::abs(ty) == 90) ? 0 : !tx ? half - std::abs(yr) : !ty ? half - std::abs(xr)
                 : std::atan(1 / std::hypot(std::tan(xr), std::tan(yr)));
      double a = 0;
      if (!tx) a = ty > 0 ? half : ty < 0 ? 3 * half : 0;
      else if (!ty) a = tx < 0 ? pi : 0;
      else if (std::abs(tx) != 90 && std::abs(ty) != 90) { a = std::atan2(std::tan(yr), std::tan(xr)); if (a < 0) a += 2 * pi; }
      azimuth = a;
    } else if ((altitude || azimuth) && !x && !y) {
      if (!altitude) altitude = half;
      if (!azimuth) azimuth = 0;
      const double a = std::clamp(*altitude, 0.0, half);
      double z = std::fmod(*azimuth, 2 * pi);
      if (z < 0) z += 2 * pi;
      if (a == 0) {
        x = (z >= 0 && z < half) || (z > 3 * half && z <= 2 * pi) ? 90 : z > half && z < 3 * half ? -90 : 0;
        y = z > 0 && z < pi ? 90 : z > pi && z < 2 * pi ? -90 : 0;
      } else {
        x = int32_t(std::floor(std::atan(std::cos(z) / std::tan(a)) * 180 / pi + .5));
        y = int32_t(std::floor(std::atan(std::sin(z) / std::tan(a)) * 180 / pi + .5));
      }
    } else {
      if (!x) x = 0;
      if (!y) y = 0;
      if (!altitude) altitude = half;
      if (!azimuth) azimuth = 0;
    }
  }
};
}  // namespace ruyijs::domain::interaction
