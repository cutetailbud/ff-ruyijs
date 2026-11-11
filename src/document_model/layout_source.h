#pragma once
#include "document_model/model.h"
#include "js/TypeDecls.h"

namespace ruyijs::document_model {
// A short-lived projection of the existing composed tree, never a node store.
struct LayoutNodeSource {
  bool element = false, text = false, renderedDocument = false;
  DomString tag, data;
  std::vector<AttributeSnapshot> attributes;
  void* factory = nullptr;
  void* layoutOwner = nullptr;
  // The node's identity and the owning tree's state when read: the Model
  // revision and the main document's focused element (Web Geometry's cache).
  NodeKey key, focused;
  uint64_t revision = 0;
  // Model::StructureRevision: unchanged when only inline styles changed.
  uint64_t structure = 0;
  // Model::TreeRevision: unchanged when only attributes changed.
  uint64_t tree = 0;
};
// Fast layout root & revision probe without copying child lists or attributes.
struct LayoutHeadSource {
  bool renderedDocument = false, ancestorBudgetExceeded = false;
  void* factory = nullptr;
  void* layoutOwner = nullptr;
  NodeKey key, root, focused;
  uint64_t revision = 0, structure = 0, tree = 0;
};
// HTMLElement offset* as measured by the layout owner (Web Geometry), in CSS
// pixels before rounding. No offset parent for body, the root or fixed boxes.
struct OffsetGeometry {
  bool present = false, parent = false;
  double left = 0, top = 0, width = 0, height = 0;
  // Element.clientWidth/clientHeight: the padding box (0 for a non-replaced
  // inline box); `root` marks the layout root, whose client size is the viewport.
  double client_width = 0, client_height = 0;
  // Element.scrollWidth/scrollHeight: the scrollable overflow area, at least
  // the padding box (0 for a non-replaced inline box).
  double scroll_width = 0, scroll_height = 0;
  // The root element's own box overflow (its scroll size when the viewport is
  // scrolled by <body>, as in a quirks-mode document).
  double own_scroll_width = 0, own_scroll_height = 0;
  // Element.clientLeft/clientTop: the left/top border widths (0 for an inline box).
  double client_left = 0, client_top = 0;
  bool scroller = false;  // A scroll container (overflow hidden/auto/scroll).
  bool root = false;
  NodeKey parentKey;
};
using OffsetGeometryPort = bool (*)(void* context, JS::HandleObject element, OffsetGeometry* out);
// Element scroll containers (the Geometry owner keeps the position): read or
// move it, and queue the scroll notification for a container that moved.
struct ElementScrollPort {
  bool (*position)(void* context, JS::HandleObject element, bool write, double* x, double* y,
                   bool* changed) = nullptr;
  bool (*notify)(void* context, JS::HandleObject element) = nullptr;
  void* context = nullptr;
};
}
