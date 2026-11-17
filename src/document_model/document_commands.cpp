#include "document_model/editing.h"
#include "document_model/focus_internal.h"
#include "document_model/selection.h"
#include "document_model/state.h"
#include "interaction/realm.h"
#include "js/Conversions.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
enum Command { Exec, Enabled, Indeterm, StateQuery, Supported, ValueQuery, Clear };
constexpr const char* names[] = {"execCommand", "queryCommandEnabled", "queryCommandIndeterm",
    "queryCommandState", "queryCommandSupported", "queryCommandValue", "clear"};
// The editing commands this runtime runs (editing.cpp); Firefox fires a
// trusted, non-cancelable `input` (no beforeinput) after each that changed the DOM.
bool EditingCommand(const DomString& name, EditCommand* command, const char16_t** inputType) {
  const DomString lowered = focus_detail::Lower(name);
  if (lowered == u"inserttext") { *command = EditCommand::InsertText; *inputType = u"insertText"; }
  else if (lowered == u"delete") { *command = EditCommand::DeleteBackward; *inputType = u"deleteContentBackward"; }
  else if (lowered == u"forwarddelete") { *command = EditCommand::DeleteForward; *inputType = u"deleteContentForward"; }
  else if (lowered == u"insertparagraph") { *command = EditCommand::InsertParagraph; *inputType = u"insertParagraph"; }
  else if (lowered == u"insertlinebreak") { *command = EditCommand::InsertLineBreak; *inputType = u"insertLineBreak"; }
  else return false;
  return true;
}
// The editing host the selection is in, if any.
bool SelectionHost(State& state, NodeKey* host) {
  *host = {};
  Range* primary = SelectionPrimary(state);
  return !primary || EditHost(state, primary->start.node, host);
}
bool FireInput(State& state, NodeKey host, const char16_t* inputType, JS::HandleValue data) {
  JSContext* cx = state.cx;
  JS::RootedValue target(cx);
  if (!state.Wrap(host, &target) || !target.isObject()) return false;
  JS::RootedObject element(cx, &target.toObject()), init(cx, JS_NewPlainObject(cx));
  JS::RootedString type(cx, JS_NewUCStringCopyZ(cx, inputType));
  if (!init || !type || !JS_DefineProperty(cx, init, "inputType", type, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "data", data, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "bubbles", true, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "cancelable", false, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "composed", true, JSPROP_ENUMERATE)) return false;
  bool ignored = true;
  return interaction::DispatchTrusted(cx, element, "input", init, -1, &ignored);
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto method = static_cast<Command>(FunctionSlot(args));
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* entry = DocumentReceiver(cx, object, &args);
  if (!entry) return false;
  entry->owner->Observe("Document", names[method], int(ApiAccess::Call));
  if (method == Clear) {
    if (!ClearDocument(*entry)) return false;
    args.rval().setUndefined(); return true;
  }
  if (!argc) return TypeError(cx, "Document command requires one argument");
  DomString command;
  if (!String(cx, args[0], &command)) return false;
  DomString value;
  if (method == Exec && argc > 2 && !String(cx, args[2], &value)) return false;
  if (method == ValueQuery) return Value(cx, DomStringView(), args.rval());
  State& state = *entry->owner;
  EditCommand edit;
  const char16_t* inputType = nullptr;
  const bool editing = EditingCommand(command, &edit, &inputType);
  NodeKey host;
  if (method == Exec || method == Enabled) {
    // A command acts in the editing host the window's selection is in.
    if (!editing || !Same(entry->key, state.main) || !SelectionHost(state, &host) || !host.node) {
      if (JS_IsExceptionPending(cx)) return false;
      args.rval().setBoolean(false);
      return true;
    }
    if (method == Enabled) { args.rval().setBoolean(true); return true; }
    bool changed = false;
    if (!EditRun(state, host, edit, value, &changed)) return false;
    JS::RootedValue data(cx, JS::NullValue());
    if (edit == EditCommand::InsertText && !Value(cx, DomStringView(value), &data)) return false;
    if (changed && !FireInput(state, host, inputType, data)) return false;
    args.rval().setBoolean(true);
    return true;
  }
  args.rval().setBoolean(method == Supported && (editing || SupportedEditCommand(focus_detail::Lower(command))));
  return true;
}
}
bool InstallDocumentCommands(State& state) {
  for (int i = Exec; i <= Clear; ++i)
    if (!state.Method("Document", names[i], Call, i == Clear ? 0 : 1, i)) return false;
  return true;
}
}
