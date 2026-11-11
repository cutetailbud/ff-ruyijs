#include "runtime/browsing_context.h"
#include "runtime/geometry_ports.h"
#include "runtime/element_scroll_ports.h"
#include "document_model/geometry_source.h"
#include "web_styling/computed_source.h"
#include "domain/document_model/svg_length_fields.h"
#include <iterator>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ruyijs {
bool BindGeometryPorts(BrowsingContext& context) {
  context.document->SetGeometryFactory(context.geometry.get());
  web_geometry::SvgPorts ports;
  ports.context = &context;
  ports.node = [](void* raw, JS::HandleValue value, document_model::GeometryNode* out,
                  JS::MutableHandleObject parent, JS::MutableHandleObjectVector children) {
    return static_cast<BrowsingContext*>(raw)->document->ReadGeometryNode(value, out, parent, children);
  };
  ports.style = [](void* raw, JS::HandleObject element, web_styling::ComputedStyle* out) {
    return static_cast<BrowsingContext*>(raw)->styling->ReadComputed(element, out);
  };
  ports.viewBoxCheck = [](void* raw, JS::HandleValue value, unsigned index) {
    return static_cast<BrowsingContext*>(raw)->document->CheckSvgViewBoxReceiver(value, index);
  };
  ports.viewBoxRead = [](void* raw, JS::HandleValue value, bool* supported,
                         document_model::SvgViewBoxValue* out, void** factory) {
    return static_cast<BrowsingContext*>(raw)->document->ReadSvgViewBox(value, supported, out, factory);
  };
  ports.viewBoxWrite = [](void* raw, JS::HandleValue value, unsigned field, float number) {
    return static_cast<BrowsingContext*>(raw)->document->WriteSvgViewBoxField(value, field, number);
  };
  ports.viewBoxView = [](void* raw, JS::HandleValue value, unsigned role,
                         JS::HandleObject replacement, JS::MutableHandleObject out) {
    return static_cast<BrowsingContext*>(raw)->document->SvgViewBoxView(value, role, replacement, out);
  };
  using document_model::SvgPointRef;
  using document_model::SvgLengthValue;
  ports.elementCheck = [](void* raw, JS::HandleValue value, const char* iface, const char* member) {
    return static_cast<BrowsingContext*>(raw)->document->CheckSvgInterface(value, iface, member);
  };
  ports.pointsRead = [](void* raw, JS::HandleValue value, document_model::SvgPointsSnapshot* out, void** factory) {
    return static_cast<BrowsingContext*>(raw)->document->ReadSvgPoints(value, out, factory);
  };
  ports.pointsMutate = [](void* raw, JS::HandleValue value, unsigned operation, uint32_t index,
                          const SvgPointRef& input, SvgPointRef* out) {
    return static_cast<BrowsingContext*>(raw)->document->MutateSvgPoints(value, operation, index, input, out);
  };
  ports.pointsWrite = [](void* raw, JS::HandleValue value, const SvgPointRef& point, unsigned field, float number) {
    return static_cast<BrowsingContext*>(raw)->document->WriteSvgPointField(value, point, field, number);
  };
  ports.pointsView = [](void* raw, JS::HandleValue value, unsigned role, JS::HandleObject replacement,
                        JS::MutableHandleObject out) {
    return static_cast<BrowsingContext*>(raw)->document->SvgPointsView(value, role, replacement, out);
  };
  ports.lengthCheck = [](void* raw, JS::HandleValue value, unsigned field) {
    return static_cast<BrowsingContext*>(raw)->document->CheckSvgLengthReceiver(value, field);
  };
  ports.lengthRead = [](void* raw, JS::HandleValue value, unsigned field, SvgLengthValue* out, void** factory) {
    return static_cast<BrowsingContext*>(raw)->document->ReadSvgLength(value, field, out, factory);
  };
  ports.lengthWrite = [](void* raw, JS::HandleValue value, unsigned field, const SvgLengthValue& length) {
    return static_cast<BrowsingContext*>(raw)->document->WriteSvgLength(value, field, length);
  };
  ports.lengthView = [](void* raw, JS::HandleValue value, unsigned field, unsigned role,
                        JS::HandleObject replacement, JS::MutableHandleObject out) {
    return static_cast<BrowsingContext*>(raw)->document->SvgLengthView(value, field, role, replacement, out);
  };
  ports.lengthRoot = [](void* raw, JS::HandleValue value, JS::MutableHandleObject out) {
    return static_cast<BrowsingContext*>(raw)->document->SvgLengthRoot(value, out);
  };
  ports.lengthViewport = [](void* raw, JS::HandleValue value, double* width, double* height) {
    auto& owner = *static_cast<BrowsingContext*>(raw);
    document_model::GeometryNode node;
    JS::RootedObject parent(owner.cx);
    JS::RootedObjectVector children(owner.cx);
    if (!owner.document->ReadGeometryNode(value, &node, &parent, &children)) return false;
    *width = *height = 0;
    auto* factory = static_cast<web_geometry::Realm*>(node.factory);
    return !node.connected || !factory || factory->SvgLengthViewportSize(width, height);
  };
  // window.scrollMaxX/Y: the viewport scroll frame's range, the root's scroll
  // size less its client (scrollport) size.
  context.environment->SetScrollExtentPort([](void* raw, double* maxX, double* maxY) {
    auto& owner = *static_cast<BrowsingContext*>(raw);
    *maxX = *maxY = 0;
    // A frame whose embedding element (or an ancestor) is display:none has no
    // rendered viewport and no scroll frame: Firefox reads 0/0.
    if (owner.parent) {
      JS::RootedObject frame(owner.cx);
      if (!owner.parent->document->WrapNode(owner.frame_element, &frame)) return false;
      std::array<double, 4> rect{};
      bool present = false;
      if (!owner.parent->geometry->MeasureElement(frame, &rect, &present, true)) return false;
      if (!present) return true;
    }
    JSObject* document = owner.document->document();
    if (!document) return true;
    JS::RootedValue value(owner.cx, JS::ObjectValue(*document));
    JS::RootedObject root(owner.cx);
    if (!owner.document->SvgLengthRoot(value, &root)) return false;
    if (!root) return true;
    document_model::OffsetGeometry geometry;
    if (!owner.geometry->MeasureOffset(root, &geometry)) return false;
    if (geometry.present) {
      *maxX = std::max(0.0, std::round(geometry.scroll_width) - std::round(geometry.client_width));
      *maxY = std::max(0.0, std::round(geometry.scroll_height) - std::round(geometry.client_height));
    }
    return true;
  }, &context);
  web_geometry::BoxPorts boxes;
  boxes.context = &context;
  boxes.styles = [](void* raw) { return static_cast<BrowsingContext*>(raw)->styling->BeginComputedRead(); };
  boxes.node = [](void* raw, JS::HandleValue value, document_model::LayoutNodeSource* out,
                  JS::MutableHandleObject parent, JS::MutableHandleObjectVector children) {
    return static_cast<BrowsingContext*>(raw)->document->ReadLayoutNode(value, out, parent, children);
  };
  boxes.head = [](void* raw, JS::HandleValue value, document_model::LayoutHeadSource* out,
                  JS::MutableHandleObject root) {
    return static_cast<BrowsingContext*>(raw)->document->ReadLayoutHead(value, out, root);
  };
  boxes.changedElements = [](void* raw, JS::HandleValue value,
                             const std::vector<document_model::NodeKey>& keys,
                             const std::vector<char>& elements,
                             const std::vector<uint64_t>& previous_attributes,
                             const std::vector<std::u16string>& previous_styles, bool style_only,
                             std::vector<document_model::ChangedElement>* changed) {
    return static_cast<BrowsingContext*>(raw)->document->ReadChangedElements(
        value, keys, elements, previous_attributes, previous_styles, style_only, changed);
  };
  boxes.wrap = [](void* raw, JS::HandleValue value, document_model::NodeKey key,
                  JS::MutableHandleObject out) {
    return static_cast<BrowsingContext*>(raw)->document->WrapLayoutNode(value, key, out);
  };
  boxes.stamp = [](void* raw, JS::HandleObject element, uint64_t* stamp) {
    return static_cast<BrowsingContext*>(raw)->styling->CascadeStamp(element, stamp);
  };
  boxes.viewport = [](void* raw, double* width, double* height, double* x, double* y) {
    auto& owner = *static_cast<BrowsingContext*>(raw);
    return owner.environment->ReadLayoutViewport(width,height,x,y);
  };
  if (context.parent) context.environment->SetViewportPort([](void* raw, double* width, double* height) {
    auto& owner = *static_cast<BrowsingContext*>(raw);
    JS::RootedObject frame(owner.cx);
    if (!owner.parent->document->WrapNode(owner.frame_element, &frame)) return false;
    std::array<double, 4> rect{}; bool present = false;
    if (!owner.parent->geometry->MeasureElement(frame, &rect, &present, true)) return false;
    const double limit = std::numeric_limits<int32_t>::max();
    *width = present ? std::clamp(std::round(rect[2]), 0.0, limit) : 0;
    *height = present ? std::clamp(std::round(rect[3]), 0.0, limit) : 0;
    return true;
  }, &context);
  context.geometry->SetBoxPorts(boxes);
  context.document->SetOffsetGeometryPort([](void* raw, JS::HandleObject element,
                                             document_model::OffsetGeometry* out) {
    auto& owner = *static_cast<BrowsingContext*>(raw);
    // A frame document whose embedding element (or an ancestor) is
    // display:none has no pres shell: every element reads as frameless.
    if (owner.parent) {
      *out = {};
      JS::RootedObject frame(owner.cx);
      if (!owner.parent->document->WrapNode(owner.frame_element, &frame)) return false;
      std::array<double, 4> rect{};
      bool present = false;
      if (!owner.parent->geometry->MeasureElement(frame, &rect, &present, true)) return false;
      if (!present) return true;
    }
    return owner.geometry->MeasureOffset(element, out);
  }, &context);
  BindElementScroll(context);
  JS::RootedObject graphics(context.cx, context.document->svgPrototype(false));
  JS::RootedObject text(context.cx, context.document->svgPrototype(true));
  if (!graphics || !text || !context.geometry->InstallSvgProjection(graphics, text, ports)) return false;
  for (unsigned index = 0; index < 5; ++index) {
    JS::RootedObject prototype(context.cx, context.document->svgViewBoxPrototype(index));
    if (!prototype || !context.geometry->InstallSvgViewBoxProjection(prototype, index)) return false;
  }
  for (unsigned index = 0; index < 3; ++index) {
    JS::RootedObject prototype(context.cx, context.document->svgPointsPrototype(index));
    if (!prototype || !context.geometry->InstallSvgPointsProjection(prototype, index)) return false;
  }
  // SVG length attributes: one accessor per (interface, member); createSVGLength last.
  const size_t fields = std::size(domain::document_model::kSvgLengthFields);
  for (unsigned field = 0; field <= fields; ++field) {
    JS::RootedObject prototype(context.cx, field < fields ? context.document->svgLengthPrototype(field)
                                                          : context.document->svgPointsPrototype(0));
    if (!prototype) continue;  // an SVG interface this realm does not expose
    if (!context.geometry->InstallSvgLengthProjection(prototype, field)) return false;
  }
  {
    JS::RootedObject svg(context.cx, context.document->svgPointsPrototype(0));
    if (!svg || !context.geometry->InstallSvgDomTypesProjection(svg)) return false;
  }
  return true;
}
}
