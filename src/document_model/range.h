// Live Range (DOM Standard select-a-range) over the unique Document Model tree.
// A range stores only two boundary points (node identity + offset); it never
// keeps a node list or a cached order, so every query re-derives from the
// DocumentAggregate. Owner: the existing document_model tree. Realm-owned like
// Walker, so the rooted object dies with the realm (docs/DEVIATIONS.md D22).
#ifndef RUYIJS_DOCUMENT_RANGE_H
#define RUYIJS_DOCUMENT_RANGE_H

#include <cstdint>
#include <memory>
#include <vector>

#include "document_model/model.h"
#include "js/CallArgs.h"
#include "js/Class.h"
#include "js/RootingAPI.h"

namespace ruyijs::document_model {
struct State;

// One native class behind every Range. The reserved slot holds the realm-owned
// Range below.
extern const JSClass kRangeClass;
extern const JSClass kStaticRangeClass;

struct BoundaryPoint {
  NodeKey node;
  uint32_t offset = 0;
};

// A live range. `document` is the associated document that createRange was
// called on; fragment and text creation use it, so no document id is assumed.
struct Range {
  JS::PersistentRootedObject object;
  State* owner = nullptr;
  // The realm whose tree holds the boundaries when it is not the owner's
  // (a boundary was set to another realm's node); listed in its `guests`.
  State* tree = nullptr;
  NodeKey document;
  BoundaryPoint start, end;
  std::unique_ptr<Range> next;
  explicit Range(JSContext* cx) : object(cx) {}
};

// Native reserved-slot ids shared by the installer and the member table.
enum class RangeField {
  StartContainer, StartOffset, EndContainer, EndOffset, Collapsed, CommonAncestor
};
enum class RangeMethod {
  SetStart, SetEnd, SetStartBefore, SetStartAfter, SetEndBefore, SetEndAfter,
  SelectNode, SelectNodeContents, Collapse, CloneContents, ExtractContents,
  DeleteContents, InsertNode, CloneRange, Detach, ToString
};

// The range behind a receiver, or a TypeError for a foreign/other-realm one.
Range* RangeOf(State&, const JS::CallArgs&);
// The State whose Model holds the range's boundaries.
inline State& RangeTree(Range& range) { return range.tree ? *range.tree : *range.owner; }
// Moves the boundaries into `target`'s tree at `point` (collapsed), keeping the
// guest lists of both trees exact.
void RangeRehome(Range&, State& target, const BoundaryPoint& point);
// State teardown: own ranges leave other trees; guests collapse back home.
void RangeRetire(State&) noexcept;
// Creates a bare realm-owned Range; boundaries default to (document, 0).
// Returns null with a pending exception on allocation/interface failure.
Range* NewRange(State&, JS::MutableHandleValue result, const JS::CallArgs* args = nullptr);
// document.createRange(): start and end are (document, 0).
bool CreateRange(State&, NodeKey document, JS::MutableHandleValue result,
                 const JS::CallArgs* args = nullptr);
bool InstallRange(State&);
bool InstallAbstractRange(State&);
bool InstallRangeStubs(State&);
bool CreateStaticRange(State&, const JS::CallArgs&);
bool RangePrototype(State&, const char*, const JS::CallArgs*, JS::MutableHandleObject);

// Boundary geometry over the unique tree (range_geom.cpp). Every helper expects
// the nodes to share a root; the setters enforce that before the first write.
bool RangeIndex(State&, NodeKey parent, NodeKey child, uint32_t* result);
bool RangeIsAncestor(State&, NodeKey ancestor, NodeKey node, bool* result);
bool RangeComparePoints(State&, const BoundaryPoint& a, const BoundaryPoint& b, int* result);
bool RangeCommonAncestor(State&, const BoundaryPoint& a, const BoundaryPoint& b,
                         NodeKey* result);
bool RangeContainsNode(State&, const BoundaryPoint& start, const BoundaryPoint& end,
                       NodeKey node, bool* result);
// Collects the contained nodes whose parent is not contained, in tree order.
bool RangeTopLevelContained(State&, const BoundaryPoint& start, const BoundaryPoint& end,
                            NodeKey root, std::vector<NodeKey>* result);

// Mutations (range_ops.cpp).
bool RangeDeleteContents(State&, Range&);
bool RangeExtractContents(State&, Range&, JS::MutableHandleValue result);
bool RangeCloneContents(State&, Range&, JS::MutableHandleValue result);
bool RangeInsertNode(State&, Range&, JS::HandleValue node);
bool RangeText(State&, Range&, JS::MutableHandleValue result);
void RangeReplaceData(State&, NodeKey node, uint32_t offset, uint32_t count,
                      uint32_t inserted) noexcept;
void RangeSplitText(State&, NodeKey node, NodeKey new_node, NodeKey parent,
                    uint32_t parent_index_after, uint32_t offset) noexcept;
void RangeBeforeRemove(State&, NodeKey parent, NodeKey child, uint32_t old_index) noexcept;
void RangeAfterInsert(State&, NodeKey parent, uint32_t index, uint32_t count) noexcept;
void RangeMergeText(State&, NodeKey kept_text, uint32_t kept_old_length,
                    NodeKey merged_sibling, NodeKey parent, uint32_t sibling_index) noexcept;
Model::RangeSink MakeRangeSink(State&) noexcept;
}  // namespace ruyijs::document_model
#endif
