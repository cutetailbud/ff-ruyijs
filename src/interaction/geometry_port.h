#pragma once
#include "js/TypeDecls.h"
namespace ruyijs::interaction {
// MouseEvent coordinates that need the target document's layout, answered by
// the layout owner (web_geometry) at read time:
//   Page   - the client point plus the document's scroll position;
//   Offset - relative to the target's padding edge (Event::GetOffsetCoords);
//   Layer  - relative to the closest layer: a positioned or scrolled box, else
//            the scrolled root (UIEvent::GetLayerPoint).
// x/y are the event's client coordinates. False only on an engine failure; an
// element without a box leaves the outputs at their defaults.
enum class GeometryQuery { Page, Offset, Layer };
using GeometryPort = bool (*)(void*, GeometryQuery, JS::HandleObject target, double x, double y,
                              double* out_x, double* out_y);
}
