#pragma once
#include <cstdint>
#include <vector>

#include "document_model/range.h"
#include "js/TypeDecls.h"

namespace ruyijs::document_model {
struct State;

extern const JSClass kSelectionClass;
bool InstallSelection(State&);
bool InstallSelectionMembers(State&);

// The window Selection (selection.cpp, selection_text.cpp). Its ranges are
// ordinary Ranges of the realm (State::selectionRanges, document order).
// The anchor/focus come from
// the most recently added range; `selectionBackward` puts the anchor at its end.

// The selection's ranges in document order, without those whose boundaries
// left the main document (they select nothing).
bool SelectionRanges(State&, std::vector<Range*>* result);
// The range anchor/focus describe (the last one added), or null.
Range* SelectionPrimary(State&);
// Replaces the whole selection with `range` (null clears it) and queues
// selectionchange.
bool SelectionAssign(State&, Range*, bool backward);
// Replaces the selection with a new collapsed range at `point` (the caret);
// a null node clears it.
bool SelectionCollapse(State&, const BoundaryPoint& point);
// The text the selection serializes to (selection_text.cpp).
bool SelectionText(State&, DomString* result);
}  // namespace ruyijs::document_model
