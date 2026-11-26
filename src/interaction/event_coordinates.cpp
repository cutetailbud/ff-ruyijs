// MouseEvent/UIEvent coordinates as Firefox's MouseEvent and UIEvent report
// them. A PointerEvent keeps fractional coordinates (except click, auxclick
// and contextmenu, which stay MouseEvent-compatible); every other mouse event
// floors its input point and rounds what it reports
// (DOMEventShouldUseFractionalCoords). pageX/offsetX/layerX need the target
// document's layout, which the layout owner answers through the geometry port.
#include "interaction/internal.h"
#include "runtime/page_helpers.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <cmath>
#include <cstring>

namespace ruyijs::interaction {
bool EventTypeIs(const Event& event, const char* name) {
  const std::string& key = event.value.type().value();
  const size_t length = std::strlen(name);
  if (key.size() != length * sizeof(char16_t)) return false;
  for (size_t i = 0; i < length; ++i) {
    char16_t unit;
    std::memcpy(&unit, key.data() + i * sizeof(char16_t), sizeof unit);
    if (unit != char16_t(static_cast<unsigned char>(name[i]))) return false;
  }
  return true;
}
bool FractionalCoordinates(const Event& event) {
  if (event.kind != Kind::Pointer) return false;
  return !EventTypeIs(event, "click") && !EventTypeIs(event, "auxclick") &&
         !EventTypeIs(event, "contextmenu");
}
namespace {
double Client(const Event& event, unsigned slot) {
  const double value = JS::GetReservedSlot(event.object, slot).toNumber();
  return FractionalCoordinates(event) ? value : std::floor(value);
}
enum Query { kClient = -1, kPage = int(GeometryQuery::Page), kOffset = int(GeometryQuery::Offset),
             kLayer = int(GeometryQuery::Layer) };
bool Coordinate(JSContext* cx, const JS::CallArgs& args, Iface iface, const char* name, int query,
                bool horizontal) {
  Event* event = Receiver(cx, args, iface, iface == Iface::UI);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), name, int(ApiAccess::Read));
  if (!IsA(event->kind, Iface::Mouse)) { args.rval().setInt32(0); return true; }
  double x = Client(*event, kEventClientX), y = Client(*event, kEventClientY);
  // A script-made event's layer point is never computed (UIEvent::GetLayerPoint
  // returns the stored zero for an internal event).
  if (query == kLayer && !event->value.is_trusted()) { args.rval().setInt32(0); return true; }
  if (query != kClient) {
    State* state = Owner(cx, event->object, true);
    if (!state) return false;
    JSObject* raw = event->value.target_id() ? state->targetObject(event->value.target_id()) : nullptr;
    // An event without a target (not dispatched yet): Event::GetOffsetCoords
    // answers the page coordinates.
    if (!event->value.target_id() && query == kOffset) query = kPage;
    if (state->geometry && (raw || query == kPage)) {
      JS::RootedObject target(cx, raw);
      double outX = query == kOffset ? 0 : x, outY = query == kOffset ? 0 : y;
      {
        JSAutoRealm realm(cx, state->page);
        if (!state->geometry(state->geometryContext, GeometryQuery(query), target, x, y, &outX, &outY))
          return false;
      }
      x = outX; y = outY;
    } else if (query == kOffset) {
      x = 0; y = 0;  // a target that is gone: no padding edge
    }
  }
  double value = horizontal ? x : y;
  if (query == kLayer || !FractionalCoordinates(*event)) value = std::floor(value + 0.5);
  args.rval().setDouble(value);
  if (query == kLayer) args.rval().setInt32(int32_t(value));
  return true;
}
}
bool ClientCoordinate(JSContext* cx, unsigned argc, JS::Value* vp, const char* name, bool horizontal,
                      unsigned slot) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Mouse);
  if (!event) return false;
  ReportEventAccess(cx, event, "MouseEvent", name, int(ApiAccess::Read));
  double value = Client(*event, slot);
  if (!FractionalCoordinates(*event)) value = std::floor(value + 0.5);
  args.rval().setDouble(value);
  (void)horizontal;
  return true;
}
namespace {
#define COORDINATE(fn, iface, name, query, horizontal) \
  bool fn(JSContext* cx, unsigned argc, JS::Value* vp) { \
    return Coordinate(cx, JS::CallArgsFromVp(argc, vp), Iface::iface, name, query, horizontal); \
  }
COORDINATE(X, Mouse, "x", kClient, true)
COORDINATE(Y, Mouse, "y", kClient, false)
COORDINATE(PageX, Mouse, "pageX", kPage, true)
COORDINATE(PageY, Mouse, "pageY", kPage, false)
COORDINATE(OffsetX, Mouse, "offsetX", kOffset, true)
COORDINATE(OffsetY, Mouse, "offsetY", kOffset, false)
COORDINATE(LayerX, UI, "layerX", kLayer, true)
COORDINATE(LayerY, UI, "layerY", kLayer, false)
#undef COORDINATE
bool WheelValue(JSContext* cx, unsigned argc, JS::Value* vp, unsigned slot, const char* name) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Wheel);
  if (!event) return false;
  ReportEventAccess(cx, event, "WheelEvent", name, int(ApiAccess::Read));
  JS::RootedValue value(cx, JS::DoubleValue(-JS::GetReservedSlot(event->object, slot).toNumber()));
  int32_t number;
  if (!JS::ToInt32(cx, value, &number)) return false;
  args.rval().setInt32(number);
  return true;
}
bool WheelDelta(JSContext* cx, unsigned argc, JS::Value* vp) { return WheelValue(cx, argc, vp, kEventDeltaY, "wheelDelta"); }
bool WheelDeltaX(JSContext* cx, unsigned argc, JS::Value* vp) { return WheelValue(cx, argc, vp, kEventDeltaX, "wheelDeltaX"); }
bool WheelDeltaY(JSContext* cx, unsigned argc, JS::Value* vp) { return WheelValue(cx, argc, vp, kEventDeltaY, "wheelDeltaY"); }
const JSPropertySpec mouse[] = {
  JS_PSG("x", X, JSPROP_ENUMERATE), JS_PSG("y", Y, JSPROP_ENUMERATE),
  JS_PSG("pageX", PageX, JSPROP_ENUMERATE), JS_PSG("pageY", PageY, JSPROP_ENUMERATE),
  JS_PSG("offsetX", OffsetX, JSPROP_ENUMERATE), JS_PSG("offsetY", OffsetY, JSPROP_ENUMERATE),
  JS_PS_END};
const JSPropertySpec ui[] = {
  JS_PSG("layerX", LayerX, JSPROP_ENUMERATE), JS_PSG("layerY", LayerY, JSPROP_ENUMERATE),
  JS_PS_END};
const JSPropertySpec wheel[] = {
  JS_PSG("wheelDelta", WheelDelta, JSPROP_ENUMERATE),
  JS_PSG("wheelDeltaX", WheelDeltaX, JSPROP_ENUMERATE),
  JS_PSG("wheelDeltaY", WheelDeltaY, JSPROP_ENUMERATE), JS_PS_END};
}
bool InstallEventCoordinates(State& state, Iface iface, JS::HandleObject prototype) {
  const JSPropertySpec* properties = iface == Iface::Mouse ? mouse :
      iface == Iface::UI ? ui : iface == Iface::Wheel ? wheel : nullptr;
  if (!properties) return true;
  if (!JS_DefineProperties(state.cx, prototype, properties)) return false;
  for (auto p = properties; p->name.string(); ++p)
    if (!state.helpers->Accessor(InterfaceName(iface), prototype, p->name.string()))
      return false;
  return true;
}
}  // namespace ruyijs::interaction
