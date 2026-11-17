#include "document_model/selection.h"
#include "document_model/state.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const bool document = FunctionSlot(args) == 1;
  JS::RootedObject receiver(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (document) {
    Wrapper* entry = DocumentReceiver(cx, receiver);
    if (!entry) return false;
    state.Observe("Document", "getSelection", int(ApiAccess::Call));
    // Firefox returns the browsing context's Selection for the window's own
    // document (editors such as ProseMirror read root.getSelection()); a
    // document without one (DOMParser, createHTMLDocument) has none.
    if (entry->owner != &state || entry->key.document != state.main.document ||
        entry->key.node != state.main.node) {
      args.rval().setNull(); return true;
    }
  } else {
    // The page class is the shared runtime's realm identity, not a JS prototype.
    // Any genuine page Window is accepted; the factory belongs to the callee.
    if (!args.thisv().isNullOrUndefined() &&
        (!receiver || JS::GetClass(receiver) != JS::GetClass(state.page)))
      return TypeError(cx, "Window receiver is incompatible");
    state.Observe("Window", "getSelection", int(ApiAccess::Call));
  }
  if (!state.selection) {
    JS::RootedObject prototype(cx, state.Find("Selection")->prototype);
    state.selection = JS_NewObjectWithGivenProto(cx, &kSelectionClass, prototype);
    if (!state.selection) return false;
  }
  args.rval().setObject(*state.selection); return true;
}
}
bool InstallSelection(State& state) {
  if (!InstallSelectionMembers(state) ||
      !state.Method("Document", "getSelection", Get, 0, 1)) return false;
  JS::RootedObject method(state.cx, state.Function(Get, "getSelection", 0, 0));
  return method && JS_DefineProperty(state.cx, state.page, "getSelection", method, JSPROP_ENUMERATE) &&
      state.helpers.Member(method, "Window", "getSelection", "call");
}
}
