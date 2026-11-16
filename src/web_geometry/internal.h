#pragma once
#include "web_geometry/realm.h"
#include "runtime/web_kit.h"
#include "web_geometry/system_font.h"
#include "jsapi.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <map>
#include <array>
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_geometry {
enum Kind { RectReadOnly, Rect, PointReadOnly, Point, Quad };
extern const JSClass kRectReadOnlyClass, kRectClass, kPointReadOnlyClass, kPointClass, kQuadClass;
inline constexpr const char* kNames[] = {"DOMRectReadOnly", "DOMRect", "DOMPointReadOnly", "DOMPoint", "DOMQuad"};
inline constexpr const char* kRectFields[] = {"x", "y", "width", "height", "top", "right", "bottom", "left"};
inline constexpr const char* kPointFields[] = {"x", "y", "z", "w"};
struct LayoutCache;  // box_input.cpp
struct State {
  JSContext* cx;
  WebKit kit;
  WebFace faces[5];
  WebFace matrixReadOnly, matrix, rectList, svgRect, svgAnimatedRect, svgPoint, svgPointList, svgLength,
      svgAnimatedLength;
  WebFace svgNumber, svgAngle, svgMatrix, svgTransform, svgPreserveAspectRatio;
  WebFace svgAnimatedBoolean, svgAnimatedEnumeration, svgAnimatedInteger, svgAnimatedNumber,
      svgAnimatedAngle, svgAnimatedLengthList, svgAnimatedNumberList, svgAnimatedPreserveAspectRatio,
      svgAnimatedTransformList;
  WebFace svgLengthList, svgNumberList, svgStringList, svgTransformList;
  SvgPorts svgPorts;
  BoxPorts boxPorts;
  SystemFonts fonts;
  // A Worker realm: DOMMatrix's [Exposed=Window] stringifier/setMatrixValue
  // and the WebKitCSSMatrix legacy alias are absent.
  bool worker = false;
  // The last full layout, reused while its tree, cascade, focus and viewport
  // are unchanged (box_input.cpp). shared_ptr: the type is complete only there.
  std::shared_ptr<LayoutCache> layoutCache;
  // Scroll positions of element scroll containers by (document, node), the
  // origin omitted; bounded (box_scroll.cpp).
  std::map<std::pair<uint64_t, uint64_t>, std::array<double, 2>> elementScroll;
  // A text run's measurement by font and font configuration (box_text.cpp): a
  // layout rebuilt after one element changed does not reshape every other run.
  struct TextMeasure { double width = 0, height = 0, normal = 0, space = 0; std::vector<float> words; };
  std::unordered_map<std::string, TextMeasure> textMeasures;
  bool (*rangeContents)(void*, JS::HandleValue, JS::MutableHandleObjectVector) = nullptr;
  void* rangeContext = nullptr;
  bool (*elementCheck)(void*, JS::HandleValue) = nullptr;
  void* elementContext = nullptr;
  bool (*exception)(void*, const char*, const char*) = nullptr;
  void* exceptionContext = nullptr;
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
      : cx(cx), kit(cx, page, helpers, this),
        faces{WebFace(cx), WebFace(cx), WebFace(cx), WebFace(cx), WebFace(cx)},
        matrixReadOnly(cx), matrix(cx), rectList(cx), svgRect(cx), svgAnimatedRect(cx), svgPoint(cx),
        svgPointList(cx), svgLength(cx), svgAnimatedLength(cx),
        svgNumber(cx), svgAngle(cx), svgMatrix(cx), svgTransform(cx), svgPreserveAspectRatio(cx),
        svgAnimatedBoolean(cx), svgAnimatedEnumeration(cx), svgAnimatedInteger(cx),
        svgAnimatedNumber(cx), svgAnimatedAngle(cx), svgAnimatedLengthList(cx),
        svgAnimatedNumberList(cx), svgAnimatedPreserveAspectRatio(cx), svgAnimatedTransformList(cx),
        svgLengthList(cx), svgNumberList(cx), svgStringList(cx), svgTransformList(cx) {}
};
inline State& Self(const JS::CallArgs& args) { return *static_cast<State*>(WebKit::Owner(args)); }
bool Receiver(JSContext*, JS::HandleValue, Kind, JS::MutableHandleObject);
bool NewValue(State&, Kind, const double*, const JS::CallArgs*, JS::MutableHandleValue);
bool NewValueObject(JSContext*, const JSClass*, JS::HandleObject, const double*, unsigned, JS::MutableHandleValue);
bool ReadDictionary(State&, JS::HandleValue, const char* const*, unsigned, double*, bool strict);
bool PlainValues(State&, const char* const*, const double*, unsigned, JS::MutableHandleValue);
bool RectSnapshot(State&, const double*, JS::MutableHandleValue);
bool Construct(JSContext*, unsigned, JS::Value*, Kind);
bool InstallSvgUnitTypes(State&);
bool ScalarGet(JSContext*, unsigned, JS::Value*);
bool ScalarSet(JSContext*, unsigned, JS::Value*);
bool ScalarJSON(JSContext*, unsigned, JS::Value*);
bool ScalarFrom(JSContext*, unsigned, JS::Value*);
bool TransformPoint(JSContext*, unsigned, JS::Value*);
bool ConstructQuad(State&, const JS::CallArgs&);
bool QuadGet(JSContext*, unsigned, JS::Value*);
bool QuadJSON(JSContext*, unsigned, JS::Value*);
bool QuadBounds(JSContext*, unsigned, JS::Value*);
bool QuadFrom(JSContext*, unsigned, JS::Value*);
}
