// The window Selection (Selection.webidl), measured against Firefox 160
// (ci/selection_check.py). Its ranges are ordinary live Ranges of the realm,
// kept in document order: addRange inserts the caller's own Range object, the
// replacing methods (collapse, setBaseAndExtent, selectAllChildren, extend's
// new range) create one, and getRangeAt returns those same objects. The most
// recently added range is the primary one anchor/focus describe; the
// direction belongs to it. A range whose boundary left the document selects
// nothing; like every Range of this runtime its boundaries are not moved by
// later tree or character-data changes (offsets are clamped where read).
#include "document_model/selection.h"
#include "document_model/editing.h"
#include "document_model/focus_internal.h"
#include "document_model/state.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <optional>
#include <string>
#include <utility>

namespace ruyijs::document_model {
using namespace focus_detail;
const JSClass kSelectionClass = {"Selection", 0, nullptr};
namespace {
enum Field { Anchor, AnchorOffset, Focus, FocusOffset, Collapsed, Count, Type, Direction, BidiLevel };
constexpr const char* fields[] = {"anchorNode", "anchorOffset", "focusNode", "focusOffset",
                                  "isCollapsed", "rangeCount", "type", "direction", "caretBidiLevel"};
enum Method { GetRange, AddRange, RemoveRange, RemoveAll, Empty, Composed, Collapse, Position,
              Start, End, Extend, BaseExtent, AllChildren, Modify, Delete, Contains, Stringify };
constexpr const char* methods[] = {"getRangeAt", "addRange", "removeRange", "removeAllRanges", "empty",
  "getComposedRanges", "collapse", "setPosition", "collapseToStart", "collapseToEnd", "extend",
  "setBaseAndExtent", "selectAllChildren", "modify", "deleteFromDocument", "containsNode", "toString"};
constexpr unsigned lengths[] = {1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 4, 1, 0, 0, 1, 0};

Range* Native(JSObject* object) {
  return object ? static_cast<Range*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
}
// GCVector has no insert: append, then rotate the new last element into place.
bool InsertAt(JSContext* cx, JS::GCVector<JSObject*>& list, size_t position, JSObject* object) {
  if (!list.append(object)) { JS_ReportOutOfMemory(cx); return false; }
  for (size_t i = list.length() - 1; i > position; --i) std::swap(list[i], list[i - 1]);
  return true;
}
bool Receiver(JSContext* cx, JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  return (object && JS::GetClass(object) == &kSelectionClass) ||
      TypeError(cx, "Selection receiver is incompatible");
}
uint32_t Length(const NodeSnapshot& value) {
  return value.kind == NodeKind::Text || value.kind == NodeKind::Comment ||
         value.kind == NodeKind::ProcessingInstruction ? uint32_t(value.data_length)
                                                       : uint32_t(value.child_count);
}
bool IsCollapsed(const Range& range) {
  return Same(range.start.node, range.end.node) && range.start.offset == range.end.offset;
}
// Whether `node` is in the selection's document. Firefox ignores boundaries in
// another document or a detached tree and leaves the selection as it was.
bool InDocument(State& state, NodeKey node, bool* result) {
  NodeSnapshot value;
  if (!state.Check(state.model.Inspect(node, &value))) return false;
  *result = value.is_connected && state.main.node && value.root.document == state.main.document &&
            Same(value.root, state.main);
  return true;
}
// A boundary point argument: a Node of this realm and an offset within it.
bool Point(State& state, JS::HandleValue node, JS::HandleValue offset, BoundaryPoint* point) {
  if (!node.isObject()) return TypeError(state.cx, "Selection boundary is not a Node");
  if (!NodeArgument(state, node, &point->node)) return false;
  if (!JS::ToUint32(state.cx, offset, &point->offset)) return false;
  NodeSnapshot value;
  if (!state.Check(state.model.Inspect(point->node, &value))) return false;
  if (value.kind == NodeKind::DocumentType)
    return ThrowDOM(state, "InvalidNodeTypeError", "Selection boundary is a DocumentType");
  if (point->offset > Length(value))
    return ThrowDOM(state, "IndexSizeError", "Selection offset is greater than the node length");
  return true;
}
bool Queue(State& state);
// A new selection Range [start, end]: the unexposed scratch Range when the
// selection keeps no other use of it (every range is being replaced, or the
// scratch is the `replacing` one), else a new one.
Range* Fresh(State& state, const BoundaryPoint& start, const BoundaryPoint& end, bool all,
             JSObject* replacing) {
  Range* range = Native(state.selectionScratch);
  if (range && !all && state.selectionScratch != replacing)
    for (JSObject* object : state.selectionRanges.get())
      if (object == state.selectionScratch) { range = nullptr; break; }
  if (!range || &RangeTree(*range) != &state) {
    JS::RootedValue object(state.cx);
    range = NewRange(state, &object);
    if (!range) return nullptr;
    state.selectionScratch = range->object;
  }
  range->document = state.main;
  range->start = start;
  range->end = end;
  return range;
}
// Replaces every range with one Range [start, end].
bool Replace(State& state, const BoundaryPoint& start, const BoundaryPoint& end, bool backward) {
  Range* range = Fresh(state, start, end, true, nullptr);
  return range && SelectionAssign(state, range, backward);
}
// A base/focus pair in either order (setBaseAndExtent, extend).
bool Order(State& state, const BoundaryPoint& base, const BoundaryPoint& focus,
           BoundaryPoint* start, BoundaryPoint* end, bool* backward) {
  NodeSnapshot a, b;
  if (!state.Check(state.model.Inspect(base.node, &a)) || !state.Check(state.model.Inspect(focus.node, &b)))
    return false;
  *backward = false;
  if (!Same(a.root, b.root)) { *start = *end = focus; return true; }
  int order = 0;
  if (!RangeComparePoints(state, base, focus, &order)) return false;
  *backward = order > 0;
  *start = *backward ? focus : base;
  *end = *backward ? base : focus;
  return true;
}
bool Wrap(State& state, NodeKey key, JS::MutableHandleValue result) {
  if (!key.node) { result.setNull(); return true; }
  return state.Wrap(key, result);
}
Range* Argument(State& state, const JS::Value& value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != &kRangeClass) return nullptr;
  Range* range = Native(object);
  return range && range->owner == &state ? range : nullptr;
}

bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const auto field = Field(FunctionSlot(args));
  if (!Receiver(cx, args.thisv())) return false;
  state.Observe("Selection", fields[field], int(ApiAccess::Read));
  if (field == BidiLevel) { args.rval().set(state.caretBidiLevel); return true; }
  std::vector<Range*> ranges;
  if (!SelectionRanges(state, &ranges)) return false;
  Range* range = SelectionPrimary(state);
  const bool backward = state.selectionBackward;
  const BoundaryPoint* anchor = range ? (backward ? &range->end : &range->start) : nullptr;
  const BoundaryPoint* focus = range ? (backward ? &range->start : &range->end) : nullptr;
  bool collapsed = true;
  for (Range* each : ranges) collapsed = collapsed && IsCollapsed(*each);
  switch (field) {
    case Anchor: return Wrap(state, anchor ? anchor->node : NodeKey{}, args.rval());
    case Focus: return Wrap(state, focus ? focus->node : NodeKey{}, args.rval());
    case AnchorOffset: args.rval().setNumber(anchor ? anchor->offset : 0); return true;
    case FocusOffset: args.rval().setNumber(focus ? focus->offset : 0); return true;
    case Collapsed: args.rval().setBoolean(!range || IsCollapsed(*range)); return true;
    case Count: args.rval().setInt32(int32_t(ranges.size())); return true;
    case Type:
      return Value(cx, DomStringView(ranges.empty() ? u"None" : collapsed ? u"Caret" : u"Range"), args.rval());
    case Direction:
      return Value(cx, DomStringView(!range || IsCollapsed(*range) ? u"none" : backward ? u"backward" : u"forward"),
                   args.rval());
    default: return false;
  }
}
// caretBidiLevel: a nullable short, stored as given (null resets it).
bool WriteBidi(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  if (!Receiver(cx, args.thisv())) return false;
  state.Observe("Selection", "caretBidiLevel", int(ApiAccess::Write));
  if (args.get(0).isNullOrUndefined()) state.caretBidiLevel.setNull();
  else {
    int32_t level = 0;
    if (!JS::ToInt32(cx, args.get(0), &level)) return false;
    state.caretBidiLevel.setInt32(int16_t(level));
  }
  args.rval().setUndefined();
  return true;
}

// containsNode(node, allowPartialContainment) against one range (Firefox
// Selection::ContainsNode): the node spans (parent, index)..(parent, index + 1);
// a range overlapping that span holds it partly, and a Text node counts as
// held whenever it is partly held.
bool ContainsNode(State& state, Range& range, NodeKey node, bool partial, bool* result) {
  *result = false;
  NodeSnapshot value, start;
  if (!state.Check(state.model.Inspect(node, &value)) ||
      !state.Check(state.model.Inspect(range.start.node, &start))) return false;
  if (!Same(value.root, start.root)) return true;
  BoundaryPoint low{node, 0}, high{node, Length(value)};
  if (value.parent.node) {
    uint32_t index = 0;
    if (!RangeIndex(state, value.parent, node, &index)) return false;
    low = {value.parent, index};
    high = {value.parent, index + 1};
  }
  int startToHigh = 0, endToLow = 0;
  if (!RangeComparePoints(state, range.start, high, &startToHigh) ||
      !RangeComparePoints(state, range.end, low, &endToLow)) return false;
  const bool overlaps = startToHigh < 0 && endToLow > 0;
  if (!overlaps) return true;
  if (partial || value.kind == NodeKind::Text) { *result = true; return true; }
  int startToLow = 0, endToHigh = 0;
  if (!RangeComparePoints(state, range.start, low, &startToLow) ||
      !RangeComparePoints(state, range.end, high, &endToHigh)) return false;
  *result = startToLow <= 0 && endToHigh >= 0;
  return true;
}
// addRange: the caller's Range joins in document order and becomes primary.
// The same object again, or a collapsed range at an existing caret, is ignored.
// A range the added one overlaps gives way (Firefox's
// StyledRanges::MaybeAddRangeAndTruncateOverlaps): what it selects outside the
// added range stays selected as new Range objects, before and after it.
bool Add(State& state, Range* added) {
  JSContext* cx = state.cx;
  JS::RootedObject object(cx, added->object);
  auto& list = state.selectionRanges.get();
  for (size_t i = 0; i < list.length(); ++i) {
    Range* other = Native(list[i]);
    if (other == added) return true;
    if (IsCollapsed(*added) && IsCollapsed(*other) && Same(other->start.node, added->start.node) &&
        other->start.offset == added->start.offset) return true;
  }
  JS::RootedObjectVector kept(cx);
  bool placed = false;
  auto place = [&]() { placed = true; return kept.append(object); };
  for (size_t i = 0; i < list.length(); ++i) {
    JS::RootedObject each(cx, list[i]);
    Range* other = Native(each);
    int startToEnd = 0, endToStart = 0, starts = 0, ends = 0;
    if (!RangeComparePoints(state, other->start, added->end, &startToEnd) ||
        !RangeComparePoints(state, other->end, added->start, &endToStart)) return false;
    if (endToStart <= 0) { if (!kept.append(each)) return false; continue; }  // wholly before
    if (startToEnd >= 0) {                                                      // wholly after
      if (!placed && !place()) return false;
      if (!kept.append(each)) return false;
      continue;
    }
    if (!RangeComparePoints(state, other->start, added->start, &starts) ||
        !RangeComparePoints(state, other->end, added->end, &ends)) return false;
    if (starts < 0) {
      JS::RootedValue value(cx);
      Range* head = NewRange(state, &value);
      if (!head) return false;
      head->document = state.main; head->start = other->start; head->end = added->start;
      if (!kept.append(head->object.get())) return false;
    }
    if (!placed && !place()) return false;
    if (ends > 0) {
      JS::RootedValue value(cx);
      Range* tail = NewRange(state, &value);
      if (!tail) return false;
      tail->document = state.main; tail->start = added->end; tail->end = other->end;
      if (!kept.append(tail->object.get())) return false;
    }
  }
  if (!placed && !place()) return false;
  list.clear();
  for (JSObject* each : kept)
    if (!list.append(each)) { JS_ReportOutOfMemory(cx); return false; }
  state.selectionPrimary = object;
  return Queue(state);
}
bool Remove(State& state, Range* removed) {
  auto& list = state.selectionRanges.get();
  for (size_t i = 0; i < list.length(); ++i) {
    if (Native(list[i]) != removed) continue;
    list.erase(list.begin() + i);
    if (state.selectionPrimary == removed->object)
      state.selectionPrimary = list.empty() ? nullptr : list.back();
    if (list.empty()) state.selectionBackward = false;
    return Queue(state);
  }
  return ThrowDOM(state, "NotFoundError", "Range is not in the selection");
}

// Ranges whose boundaries left the document select nothing and no reader
// sees them (SelectionRanges): a method that changes the selection drops them
// first, so the list it orders holds only comparable boundaries.
bool Prune(State& state) {
  auto& list = state.selectionRanges.get();
  for (size_t i = 0; i < list.length();) {
    Range* range = Native(list[i]);
    bool inside = false;
    if (range && &RangeTree(*range) == &state && !InDocument(state, range->start.node, &inside)) return false;
    if (inside) { ++i; continue; }
    if (state.selectionPrimary == list[i]) state.selectionPrimary = nullptr;
    list.erase(list.begin() + i);
  }
  if (!state.selectionPrimary && !list.empty()) state.selectionPrimary = list.back();
  if (list.empty()) state.selectionBackward = false;
  return true;
}
// A selection script moved wholly into one editing host gives that host the
// focus (Firefox, Selection::NotifySelectionListeners for a call from script);
// moving it out again leaves the focus where it is.
bool FocusHost(State& state) {
  std::vector<Range*> ranges;
  if (!SelectionRanges(state, &ranges) || ranges.empty()) return !JS_IsExceptionPending(state.cx);
  NodeKey host;
  for (Range* range : ranges) {
    NodeKey start, end;
    if (!EditHost(state, range->start.node, &start) || !EditHost(state, range->end.node, &end)) return false;
    if (!start.node || !Same(start, end) || (host.node && !Same(host, start))) return true;
    host = start;
  }
  NodeSnapshot value;
  if (!state.Check(state.model.Inspect(host, &value))) return false;
  // designMode's host is the body: the document, not an element, is editable.
  std::optional<DomString> editable;
  if (!state.Check(state.model.GetAttribute(host, u"contenteditable", &editable))) return false;
  if (!editable) return true;
  return MoveFocus(state, state.main, host);
}
bool Change(JSContext* cx, State& state, int method, const JS::CallArgs& args) {
  const unsigned argc = args.length();
  (void)argc;
  std::vector<Range*> ranges;
  if (!SelectionRanges(state, &ranges)) return false;
  Range* primary = SelectionPrimary(state);
  args.rval().setUndefined();
  switch (method) {
    case GetRange: {
      uint32_t index = 0;
      if (!JS::ToUint32(cx, args[0], &index)) return false;
      if (index >= ranges.size()) return ThrowDOM(state, "IndexSizeError", "Invalid range index");
      if (state.selectionScratch == ranges[index]->object) state.selectionScratch = nullptr;  // exposed
      args.rval().setObject(*ranges[index]->object);
      return JS_WrapValue(cx, args.rval());
    }
    case AddRange: case RemoveRange: {
      Range* range = Argument(state, args[0]);
      if (!range) {
        std::string text = std::string("Selection.") + methods[method] +
                           ": Argument 1 does not implement interface Range.";
        return TypeError(cx, text.c_str());
      }
      if (method == RemoveRange) return Remove(state, range);
      bool inside = false;
      if (&RangeTree(*range) != &state || !InDocument(state, range->start.node, &inside)) return !JS_IsExceptionPending(cx);
      return !inside || Add(state, range);
    }
    case RemoveAll: case Empty:
      return ranges.empty() || SelectionAssign(state, nullptr, false);
    case Composed: {
      // Without shadow roots in the options every range is already composed:
      // each becomes a StaticRange over the same boundaries.
      JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
      if (!list) return false;
      Interface* iface = state.Find("StaticRange");
      if (!iface) return TypeError(cx, "StaticRange interface is not installed");
      for (uint32_t i = 0; i < ranges.size(); ++i) {
        JS::RootedObject init(cx, JS_NewPlainObject(cx));
        JS::RootedValue start(cx), end(cx), value(cx);
        if (!init || !state.Wrap(ranges[i]->start.node, &start) || !state.Wrap(ranges[i]->end.node, &end) ||
            !JS_DefineProperty(cx, init, "startContainer", start, JSPROP_ENUMERATE) ||
            !JS_DefineProperty(cx, init, "startOffset", double(ranges[i]->start.offset), JSPROP_ENUMERATE) ||
            !JS_DefineProperty(cx, init, "endContainer", end, JSPROP_ENUMERATE) ||
            !JS_DefineProperty(cx, init, "endOffset", double(ranges[i]->end.offset), JSPROP_ENUMERATE)) return false;
        JS::RootedValue constructor(cx, JS::ObjectValue(*iface->constructor));
        JS::RootedValueArray<1> argv(cx);
        argv[0].setObject(*init);
        JS::RootedObject created(cx);
        if (!JS::Construct(cx, constructor, JS::HandleValueArray(argv), &created)) return false;
        value.setObject(*created);
        if (!JS_SetElement(cx, list, i, value)) return false;
      }
      args.rval().setObject(*list);
      return true;
    }
    case Collapse: case Position: {
      if (args[0].isNullOrUndefined()) return ranges.empty() || SelectionAssign(state, nullptr, false);
      BoundaryPoint point;
      if (!Point(state, args[0], args.get(1), &point)) return false;
      bool inside = false;
      if (!InDocument(state, point.node, &inside)) return false;
      return !inside || Replace(state, point, point, false);
    }
    case Start: case End: {
      if (ranges.empty()) return ThrowDOM(state, "InvalidStateError", "There is no selection to collapse");
      const BoundaryPoint point = method == Start ? ranges.front()->start : ranges.back()->end;
      return Replace(state, point, point, false);
    }
    case Extend: {
      if (!primary) return ThrowDOM(state, "InvalidStateError", "There is no selection to extend");
      BoundaryPoint focus, start, end;
      if (!Point(state, args[0], args.get(1), &focus)) return false;
      bool inside = false, backward = false;
      if (!InDocument(state, focus.node, &inside)) return false;
      if (!inside) return true;
      const BoundaryPoint anchor = state.selectionBackward ? primary->end : primary->start;
      if (!Order(state, anchor, focus, &start, &end, &backward)) return false;
      // A new Range takes the primary one's place; the others stay.
      JS::RootedObject old(cx, primary->object);
      Range* extended = Fresh(state, start, end, false, old);
      if (!extended) return false;
      state.selectionBackward = backward;
      auto& list = state.selectionRanges.get();
      JS::RootedObject object(cx, extended->object);
      for (size_t i = 0; i < list.length(); ++i)
        if (list[i] == old) { list.erase(list.begin() + i); break; }
      state.selectionPrimary = object;
      size_t position = list.length();
      for (size_t i = 0; i < list.length(); ++i) {
        int order = 0;
        if (!RangeComparePoints(state, start, Native(list[i])->start, &order)) return false;
        if (order < 0) { position = i; break; }
      }
      if (!InsertAt(cx, list, position, object)) return false;
      return Queue(state);
    }
    case BaseExtent: {
      if (!args[0].isObject() || !args[2].isObject())
        return TypeError(cx, "Selection.setBaseAndExtent: Argument is not an object.");
      BoundaryPoint base, focus, start, end;
      if (!Point(state, args[0], args[1], &base) || !Point(state, args[2], args[3], &focus)) return false;
      bool inside = false, other = false, backward = false;
      if (!InDocument(state, base.node, &inside) || !InDocument(state, focus.node, &other)) return false;
      if (!inside || !other) return true;
      return Order(state, base, focus, &start, &end, &backward) && Replace(state, start, end, backward);
    }
    case AllChildren: {
      NodeKey node;
      if (!args[0].isObject()) return TypeError(cx, "Selection.selectAllChildren: Argument 1 is not an object.");
      if (!NodeArgument(state, args[0], &node)) return false;
      NodeSnapshot value;
      if (!state.Check(state.model.Inspect(node, &value))) return false;
      if (value.kind == NodeKind::DocumentType)
        return ThrowDOM(state, "InvalidNodeTypeError", "Selection boundary is a DocumentType");
      bool inside = false;
      if (!InDocument(state, node, &inside)) return false;
      return !inside || Replace(state, {node, 0}, {node, Length(value)}, false);
    }
    case Modify: {
      // Caret movement needs layout (lines, words, bidi); a character step
      // within one Text node is the movement without it.
      DomString alter, direction, granularity;
      if (!String(cx, args.get(0), &alter) || !String(cx, args.get(1), &direction) ||
          !String(cx, args.get(2), &granularity)) return false;
      alter = Lower(alter); direction = Lower(direction); granularity = Lower(granularity);
      if (!primary || (alter != u"move" && alter != u"extend") || granularity != u"character") return true;
      const bool forward = direction == u"forward" || direction == u"right";
      if (!forward && direction != u"backward" && direction != u"left") return true;
      const BoundaryPoint anchor = state.selectionBackward ? primary->end : primary->start;
      BoundaryPoint focus = state.selectionBackward ? primary->start : primary->end;
      NodeSnapshot value;
      if (!state.Check(state.model.Inspect(focus.node, &value))) return false;
      if (value.kind != NodeKind::Text || (forward ? focus.offset >= value.data_length : !focus.offset)) return true;
      focus.offset = forward ? focus.offset + 1 : focus.offset - 1;
      if (alter == u"move") return Replace(state, focus, focus, false);
      BoundaryPoint start, end;
      bool backward = false;
      return Order(state, anchor, focus, &start, &end, &backward) && Replace(state, start, end, backward);
    }
    case Delete: {
      if (ranges.empty()) return true;
      for (Range* range : ranges)
        if (!RangeDeleteContents(RangeTree(*range), *range)) return false;
      const BoundaryPoint caret = primary ? primary->start : ranges.back()->start;
      return Replace(state, caret, caret, false);
    }
    case Contains: {
      if (!args[0].isObject()) return TypeError(cx, "Selection.containsNode: Argument 1 is not an object.");
      NodeKey node;
      if (!NodeArgument(state, args[0], &node)) return false;
      bool result = false;
      for (Range* range : ranges) {
        if (result) break;
        if (!ContainsNode(state, *range, node, JS::ToBoolean(args.get(1)), &result)) return false;
      }
      args.rval().setBoolean(result);
      return true;
    }
    case Stringify: {
      DomString text;
      if (!SelectionText(state, &text)) return false;
      return Value(cx, DomStringView(text), args.rval());
    }
  }
  return false;
}

bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const auto method = Method(FunctionSlot(args));
  if (!Receiver(cx, args.thisv())) return false;
  state.Observe("Selection", methods[method], int(ApiAccess::Call));
  if (argc < lengths[method]) {
    const std::string text = std::string("Selection.") + methods[method] + ": At least " +
        std::to_string(lengths[method]) + (lengths[method] > 1 ? " arguments" : " argument") +
        " required, but only " + std::to_string(argc) + " passed";
    return TypeError(cx, text.c_str());
  }
  JSAutoRealm realm(cx, state.page);
  const bool changes = method != GetRange && method != Composed && method != Contains && method != Stringify;
  if (changes && !Prune(state)) return false;
  if (!Change(cx, state, method, args)) return false;
  // The selection moved by script into an editing host focuses it.
  const bool moves = method == AddRange || method == Collapse || method == Position || method == Extend ||
                     method == BaseExtent || method == AllChildren;
  if (!moves) return true;
  JS::RootedValue result(cx, args.rval());
  if (!FocusHost(state)) return false;
  args.rval().set(result);
  return true;
}

// selectionchange is queued once per task at the document (Firefox).
bool FireSelectionChange(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  args.rval().setUndefined();
  if (!state) return true;
  state->selectionChangePending = false;
  JS::RootedValue document(cx);
  if (!state->main.node || !state->Wrap(state->main, &document) || !document.isObject()) return true;
  JS::RootedObject target(cx, &document.toObject());
  return state->interaction.Fire(target, "selectionchange", false, false);
}
bool Queue(State& state) {
  if (state.selectionChangePending) return true;
  JSContext* cx = state.cx;
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, FireSelectionChange, 0, 0, "Selection change task"));
  if (!fn) return false;
  JS::RootedObject callback(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(callback, 0, JS::PrivateValue(&state));
  state.selectionChangePending = state.environment.PostTask(callback);
  return true;
}
}  // namespace

bool SelectionRanges(State& state, std::vector<Range*>* result) {
  result->clear();
  const auto& list = state.selectionRanges.get();
  for (JSObject* object : list) {
    Range* range = Native(object);
    if (!range || &RangeTree(*range) != &state) continue;
    // A range whose boundaries left the document selects nothing.
    bool inside = false;
    if (!InDocument(state, range->start.node, &inside)) return false;
    if (inside) result->push_back(range);
  }
  return true;
}

Range* SelectionPrimary(State& state) {
  Range* range = Native(state.selectionPrimary);
  if (!range || &RangeTree(*range) != &state) return nullptr;
  bool inside = false;
  if (!InDocument(state, range->start.node, &inside)) { JS_ClearPendingException(state.cx); return nullptr; }
  return inside ? range : nullptr;
}

bool SelectionAssign(State& state, Range* range, bool backward) {
  auto& list = state.selectionRanges.get();
  list.clear();
  state.selectionPrimary = range ? range->object.get() : nullptr;
  if (range && !list.append(range->object.get())) { JS_ReportOutOfMemory(state.cx); return false; }
  state.selectionBackward = range && backward;
  return Queue(state);
}

bool SelectionCollapse(State& state, const BoundaryPoint& point) {
  if (!point.node.node) return SelectionAssign(state, nullptr, false);
  JSAutoRealm realm(state.cx, state.page);
  return Replace(state, point, point, false);
}

bool InstallSelectionMembers(State& state) {
  for (int field = Anchor; field <= BidiLevel; ++field)
    if (!state.Accessor("Selection", fields[field], Read, field, field == BidiLevel ? WriteBidi : nullptr))
      return false;
  for (int method = GetRange; method <= Stringify; ++method)
    if (!state.Method("Selection", methods[method], Call, lengths[method], method)) return false;
  state.caretBidiLevel.setNull();
  return true;
}
}  // namespace ruyijs::document_model
