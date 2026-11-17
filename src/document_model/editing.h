#pragma once
// contenteditable editing at the window Selection (editing.cpp), measured
// against Firefox 160 (ci/selection_check.py): the default actions of typed
// text, Backspace and Enter in an editing host, and document.execCommand's
// insertText / delete / forwardDelete / insertParagraph / insertLineBreak.
#include "document_model/range.h"

namespace ruyijs::document_model {
struct State;
enum class EditCommand { InsertText, DeleteBackward, DeleteForward, InsertParagraph, InsertLineBreak };
// The editing host of `node` (contenteditable / designMode), or a null key.
bool EditHost(State&, NodeKey node, NodeKey* host);
// Puts the caret at the host's first position when the selection is not
// already inside it (focusing an editing host, Firefox's focused window).
bool EditFocusCaret(State&, NodeKey host);
// Runs one command in `host` at the selection. `changed` reports a DOM change.
bool EditRun(State&, NodeKey host, EditCommand, DomStringView text, bool* changed);
}  // namespace ruyijs::document_model
