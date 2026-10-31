#if defined(__linux__)
#include "web_geometry/svg_path.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathUtils.h"

namespace ruyijs::web_geometry {
bool SvgPathBounds(const std::vector<domain::geometry::SvgPathCommand>& commands,
                   float stroke, domain::geometry::SvgBox* out) {
  SkPathBuilder path;
  for (const auto& command : commands) {
    const auto& v = command.values;
    switch (command.kind) {
      case 'M': path.moveTo(v[0], v[1]); break;
      case 'L': path.lineTo(v[0], v[1]); break;
      case 'C': path.cubicTo(v[0], v[1], v[2], v[3], v[4], v[5]); break;
      case 'Q': path.quadTo(v[0], v[1], v[2], v[3]); break;
      case 'Z': path.close(); break;
      case 'A': path.arcTo({std::abs(v[0]), std::abs(v[1])}, v[2],
          v[3] ? SkPathBuilder::kLarge_ArcSize : SkPathBuilder::kSmall_ArcSize,
          v[4] ? SkPathDirection::kCW : SkPathDirection::kCCW, {v[5], v[6]}); break;
      default: return false;
    }
  }
  auto geometry = path.detach();
  if (stroke > 0) {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(stroke);
    paint.setStrokeMiter(4);
    SkPathBuilder outline;
    if (!skpathutils::FillPathWithPaint(geometry, paint, &outline)) return false;
    geometry = outline.detach();
  }
  const auto bounds = geometry.computeTightBounds();
  *out = {bounds.left(), bounds.top(), bounds.width(), bounds.height()};
  return true;
}
}

#endif  // __linux__
