#pragma once
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "js/RootingAPI.h"
#include "document_model/svg_points_types.h"
namespace ruyijs { class PageHelpers; }
namespace ruyijs::document_model {
struct NodeKey;
struct ChangedElement;
struct GeometryNode;
struct LayoutHeadSource;
struct LayoutNodeSource;
struct OffsetGeometry;
struct SvgViewBoxValue;
}
namespace ruyijs::web_styling { struct ComputedStyle; class ComputedStyleReader; }
namespace ruyijs::web_geometry {
struct State;
struct SvgPorts {
  bool (*viewBoxCheck)(void*, JS::HandleValue, unsigned) = nullptr;
  bool (*viewBoxRead)(void*, JS::HandleValue, bool*, document_model::SvgViewBoxValue*, void**) = nullptr;
  bool (*viewBoxWrite)(void*, JS::HandleValue, unsigned, float) = nullptr;
  bool (*viewBoxView)(void*, JS::HandleValue, unsigned, JS::HandleObject, JS::MutableHandleObject) = nullptr;
  // SVG points/lengths: values and weak views stay with the original DocumentModel Element/Wrapper.
  bool (*elementCheck)(void*, JS::HandleValue, const char* iface, const char* member) = nullptr;
  bool (*pointsRead)(void*, JS::HandleValue, document_model::SvgPointsSnapshot*, void**) = nullptr;
  bool (*pointsMutate)(void*, JS::HandleValue, unsigned, uint32_t, const document_model::SvgPointRef&,
                       document_model::SvgPointRef*) = nullptr;
  bool (*pointsWrite)(void*, JS::HandleValue, const document_model::SvgPointRef&, unsigned, float) = nullptr;
  bool (*pointsView)(void*, JS::HandleValue, unsigned, JS::HandleObject, JS::MutableHandleObject) = nullptr;
  bool (*lengthCheck)(void*, JS::HandleValue, unsigned) = nullptr;
  bool (*lengthRead)(void*, JS::HandleValue, unsigned, document_model::SvgLengthValue*, void**) = nullptr;
  bool (*lengthWrite)(void*, JS::HandleValue, unsigned, const document_model::SvgLengthValue&) = nullptr;
  bool (*lengthView)(void*, JS::HandleValue, unsigned, unsigned, JS::HandleObject, JS::MutableHandleObject) = nullptr;
  bool (*lengthRoot)(void*, JS::HandleValue, JS::MutableHandleObject) = nullptr;
  // Layout viewport of the node's original factory realm, for vw/vh/vmin/vmax.
  bool (*lengthViewport)(void*, JS::HandleValue, double* width, double* height) = nullptr;
  bool (*node)(void*, JS::HandleValue, document_model::GeometryNode*, JS::MutableHandleObject,
                JS::MutableHandleObjectVector) = nullptr;
  bool (*style)(void*, JS::HandleObject, web_styling::ComputedStyle*) = nullptr;
  void* context = nullptr;
};
struct BoxPorts {
  std::unique_ptr<web_styling::ComputedStyleReader> (*styles)(void*) = nullptr;
  bool (*node)(void*, JS::HandleValue, document_model::LayoutNodeSource*, JS::MutableHandleObject,
               JS::MutableHandleObjectVector) = nullptr;
  bool (*head)(void*, JS::HandleValue, document_model::LayoutHeadSource*, JS::MutableHandleObject) = nullptr;
  bool (*changedElements)(void*, JS::HandleValue, const std::vector<document_model::NodeKey>&,
                          const std::vector<char>&, const std::vector<uint64_t>&,
                          const std::vector<std::u16string>&, bool,
                          std::vector<document_model::ChangedElement>*) = nullptr;
  bool (*wrap)(void*, JS::HandleValue, document_model::NodeKey, JS::MutableHandleObject) = nullptr;
  bool (*viewport)(void*, double*, double*, double*, double*) = nullptr;
  bool (*stamp)(void*, JS::HandleObject, uint64_t*) = nullptr;  // Cascade fingerprint.
  void* context = nullptr;
};
// The environment's "fonts" group port (font_config.cpp); `context` is the JSContext.
bool ConfigureProfileGroup(void* context, JS::HandleObject group);
class Realm final {
 public:
  Realm(JSContext*, JS::HandleObject, PageHelpers&);
  ~Realm();
  bool Install();
  bool InstallValues();
  bool InstallWorkerValues();
  JSObject* ReadOnlyRectPrototype() const;
  void SetExceptionPort(bool (*)(void*, const char*, const char*), void*);
  bool MatrixFrom2D(const double components[6], JS::MutableHandleObject);
  // Range.getClientRects/getBoundingClientRect: the port lists the elements the
  // range contains (Document Model); each is measured like Element geometry.
  bool InstallRangeProjection(JS::HandleObject,
                              bool (*)(void*, JS::HandleValue, JS::MutableHandleObjectVector), void*);
  bool InstallElementProjection(JS::HandleObject, bool (*)(void*, JS::HandleValue), void*);
  void ClearProjections();
  bool InstallSvgProjection(JS::HandleObject graphics, JS::HandleObject text, const SvgPorts&);
  bool NewSvgRect(const float*, JS::HandleObject parent, JS::MutableHandleValue);
  bool InstallSvgViewBoxProjection(JS::HandleObject, unsigned index);
  bool SvgViewBoxView(JS::HandleValue node, unsigned role, JS::MutableHandleValue);
  // SVGPoint/SVGPointList (svg_point*.cpp) and SVGLength/SVGAnimatedLength (svg_length*.cpp).
  // Projection index: 0 SVGSVGElement.createSVGPoint, 1 polyline, 2 polygon; length
  // field: kSvgLengthFields index, past the end = SVGSVGElement.createSVGLength.
  bool InstallSvgPointsProjection(JS::HandleObject, unsigned index);
  bool SvgPointsView(JS::HandleValue node, unsigned role, JS::MutableHandleValue);
  bool NewSvgPointValue(float x, float y, JS::MutableHandleValue);
  bool InstallSvgLengthProjection(JS::HandleObject, unsigned field);
  // SVGSVGElement.createSVGNumber/createSVGAngle/createSVGMatrix/createSVGTransform.
  bool InstallSvgDomTypesProjection(JS::HandleObject);
  bool SvgLengthView(JS::HandleValue node, unsigned field, unsigned role, JS::MutableHandleValue);
  bool NewSvgLength(JS::HandleObject, JS::MutableHandleValue);
  bool SvgLengthViewportSize(double* width, double* height);
  void SetBoxPorts(const BoxPorts&);
  bool MeasureElement(JS::HandleObject, std::array<double, 4>*, bool* present, bool content = false);
  bool MeasureOffset(JS::HandleObject, document_model::OffsetGeometry*);
  // An element scroll container's position (box_scroll.cpp): read, or with
  // |write| clamp and store (*changed when it moved). 0,0 for a non-container.
  bool ElementScroll(JS::HandleObject, bool write, double* x, double* y, bool* changed);
  bool NewElementRects(const std::array<double, 4>&, bool present, bool list, JS::MutableHandleValue);
 private:
  std::unique_ptr<State> state_;
};
}
