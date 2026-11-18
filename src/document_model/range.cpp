#include "document_model/range.h"

#include <algorithm>
#include <new>

#include "document_model/state.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "runtime/web_kit.h"
#include <string>

namespace ruyijs::document_model {
const JSClass kRangeClass = {"Range", JSCLASS_HAS_RESERVED_SLOTS(1), nullptr};

Range* RangeOf(State& state, const JS::CallArgs& args) {
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kRangeClass) {
    // The WebIDL brand message Firefox reports ("'m' called on an object ...").
    std::string text;
    if (WebKit::BrandText(state.cx, args, "Range", &text)) TypeError(state.cx, text.c_str());
    return nullptr;
  }
  auto* range = static_cast<Range*>(
      JS::GetReservedSlot(&args.thisv().toObject(), 0).toPrivate());
  if (!range || range->owner != &state) {
    TypeError(state.cx, "Range receiver is incompatible with this realm");
    return nullptr;
  }
  return range;
}

void RangeRehome(Range& range, State& target, const BoundaryPoint& point) {
  State* next = &target == range.owner ? nullptr : &target;
  if (next != range.tree) {
    if (range.tree) std::erase(range.tree->guests, &range);
    if (next) next->guests.push_back(&range);
    range.tree = next;
  }
  range.start = range.end = point;
}

void RangeRetire(State& state) noexcept {
  for (Range* range = state.ranges.get(); range; range = range->next.get())
    if (range->tree) { std::erase(range->tree->guests, range); range->tree = nullptr; }
  for (Range* guest : state.guests) {
    guest->tree = nullptr;
    guest->start = guest->end = BoundaryPoint{guest->document, 0};
  }
  state.guests.clear();
}

bool RangePrototype(State& state, const char* name, const JS::CallArgs* args,
                    JS::MutableHandleObject prototype) {
  Interface* iface = state.Find(name);
  if (!iface) {
    TypeError(state.cx, "Range interface is not installed");
    return false;
  }
  prototype.set(iface->prototype);
  if (args) {
    JS::RootedObject target(state.cx, &args->newTarget().toObject());
    JS::RootedValue value(state.cx);
    if (!JS_GetProperty(state.cx, target, "prototype", &value)) return false;
    if (value.isObject()) prototype.set(&value.toObject());
  }
  return true;
}

Range* NewRange(State& state, JS::MutableHandleValue result, const JS::CallArgs* args) {
  JS::RootedObject prototype(state.cx);
  if (!RangePrototype(state, "Range", args, &prototype)) return nullptr;
  JS::RootedObject object(state.cx,
      JS_NewObjectWithGivenProto(state.cx, &kRangeClass, prototype));
  std::unique_ptr<Range> entry(new (std::nothrow) Range(state.cx));
  if (!object || !entry) {
    JS_ReportOutOfMemory(state.cx);
    return nullptr;
  }
  entry->owner = &state;
  entry->object = object;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(entry.get()));
  Range* raw = entry.get();
  entry->next = std::move(state.ranges);
  state.ranges = std::move(entry);
  result.setObject(*object);
  return raw;
}

bool CreateRange(State& state, NodeKey document, JS::MutableHandleValue result,
                 const JS::CallArgs* args) {
  Range* range = NewRange(state, result, args);
  if (!range) return false;
  range->document = document;
  range->start = range->end = BoundaryPoint{document, 0};
  return true;
}

namespace {
void AdjustBoundaryForReplace(BoundaryPoint& bp, NodeKey node, uint32_t offset,
                              uint32_t count, uint32_t inserted) noexcept {
  if (bp.node.document != node.document || bp.node.node != node.node) return;
  const uint64_t end = uint64_t(offset) + uint64_t(count);
  if (bp.offset > offset && uint64_t(bp.offset) <= end) {
    bp.offset = offset;
  } else if (uint64_t(bp.offset) > end) {
    bp.offset = uint32_t(uint64_t(bp.offset) - count + inserted);
  }
}

void AdjustBoundaryForSplit(BoundaryPoint& bp, NodeKey node, NodeKey new_node,
                            NodeKey parent, uint32_t parent_index_after,
                            uint32_t offset) noexcept {
  if (bp.node.document == node.document && bp.node.node == node.node) {
    if (bp.offset > offset) {
      if (parent.node) {
        bp.node = new_node;
        bp.offset -= offset;
      } else {
        bp.offset = offset;
      }
    }
  } else if (parent.node && bp.node.document == parent.document &&
             bp.node.node == parent.node && bp.offset >= parent_index_after) {
    ++bp.offset;
  }
}

void AdjustBoundaryBeforeRemove(State& state, BoundaryPoint& bp, NodeKey parent,
                                NodeKey child, uint32_t old_index) noexcept {
  if (bp.node.document != parent.document) return;
  bool inside = false;
  if (child.node && state.model.AncestorOrSelf(child, bp.node, &inside).code == ErrorCode::None && inside) {
    bp = BoundaryPoint{parent, old_index};
  } else if (bp.node.node == parent.node && bp.offset > old_index) {
    --bp.offset;
  }
}

void AdjustBoundaryAfterInsert(BoundaryPoint& bp, NodeKey parent, uint32_t index,
                               uint32_t count) noexcept {
  if (bp.node.document == parent.document && bp.node.node == parent.node && bp.offset > index) {
    bp.offset += count;
  }
}

void AdjustBoundaryMergeText(BoundaryPoint& bp, NodeKey kept_text, uint32_t kept_old_length,
                             NodeKey merged_sibling, NodeKey parent, uint32_t sibling_index) noexcept {
  if (bp.node.document == merged_sibling.document && bp.node.node == merged_sibling.node) {
    bp.node = kept_text;
    bp.offset += kept_old_length;
  } else if (parent.node && bp.node.document == parent.document &&
             bp.node.node == parent.node && bp.offset == sibling_index) {
    bp = BoundaryPoint{kept_text, kept_old_length};
  }
}

template <typename Fn>
void ForEachLiveRange(State& state, Fn&& fn) noexcept {
  if (!state.ranges && state.guests.empty()) return;
  for (Range* r = state.ranges.get(); r; r = r->next.get()) {
    if (&RangeTree(*r) != &state) continue;
    fn(*r);
  }
  for (Range* r : state.guests) {
    if (r) fn(*r);
  }
}
}  // namespace

void RangeReplaceData(State& state, NodeKey node, uint32_t offset, uint32_t count,
                      uint32_t inserted) noexcept {
  ForEachLiveRange(state, [&](Range& r) {
    AdjustBoundaryForReplace(r.start, node, offset, count, inserted);
    AdjustBoundaryForReplace(r.end, node, offset, count, inserted);
  });
}

void RangeSplitText(State& state, NodeKey node, NodeKey new_node, NodeKey parent,
                    uint32_t parent_index_after, uint32_t offset) noexcept {
  ForEachLiveRange(state, [&](Range& r) {
    AdjustBoundaryForSplit(r.start, node, new_node, parent, parent_index_after, offset);
    AdjustBoundaryForSplit(r.end, node, new_node, parent, parent_index_after, offset);
  });
}

void RangeBeforeRemove(State& state, NodeKey parent, NodeKey child, uint32_t old_index) noexcept {
  ForEachLiveRange(state, [&](Range& r) {
    AdjustBoundaryBeforeRemove(state, r.start, parent, child, old_index);
    AdjustBoundaryBeforeRemove(state, r.end, parent, child, old_index);
  });
}

void RangeAfterInsert(State& state, NodeKey parent, uint32_t index, uint32_t count) noexcept {
  if (count == 0) return;
  ForEachLiveRange(state, [&](Range& r) {
    AdjustBoundaryAfterInsert(r.start, parent, index, count);
    AdjustBoundaryAfterInsert(r.end, parent, index, count);
  });
}

void RangeMergeText(State& state, NodeKey kept_text, uint32_t kept_old_length,
                    NodeKey merged_sibling, NodeKey parent, uint32_t sibling_index) noexcept {
  ForEachLiveRange(state, [&](Range& r) {
    AdjustBoundaryMergeText(r.start, kept_text, kept_old_length, merged_sibling, parent, sibling_index);
    AdjustBoundaryMergeText(r.end, kept_text, kept_old_length, merged_sibling, parent, sibling_index);
  });
}

Model::RangeSink MakeRangeSink(State& state) noexcept {
  Model::RangeSink sink;
  sink.context = &state;
  sink.before_remove = [](void* ctx, NodeKey parent, NodeKey child, uint32_t old_index) {
    RangeBeforeRemove(*static_cast<State*>(ctx), parent, child, old_index);
  };
  sink.after_insert = [](void* ctx, NodeKey parent, uint32_t index, uint32_t count) {
    RangeAfterInsert(*static_cast<State*>(ctx), parent, index, count);
  };
  sink.replace_data = [](void* ctx, NodeKey node, uint32_t offset, uint32_t count, uint32_t inserted) {
    RangeReplaceData(*static_cast<State*>(ctx), node, offset, count, inserted);
  };
  sink.split_text = [](void* ctx, NodeKey node, NodeKey new_node, NodeKey parent,
                       uint32_t parent_index_after, uint32_t offset) {
    RangeSplitText(*static_cast<State*>(ctx), node, new_node, parent, parent_index_after, offset);
  };
  sink.merge_text = [](void* ctx, NodeKey kept_text, uint32_t kept_old_length,
                       NodeKey merged_sibling, NodeKey parent, uint32_t sibling_index) {
    RangeMergeText(*static_cast<State*>(ctx), kept_text, kept_old_length, merged_sibling, parent,
                   sibling_index);
  };
  return sink;
}
}  // namespace ruyijs::document_model
