// Public part policy on the existing DOMTokenList adapter and attribute owner.
#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
bool Convert(JSContext* cx, JS::HandleValue value, DomString* text, bool* failed) {
  if (value.isBigInt()) { text->clear(); return true; }
  if (value.isNumber()) return ControlString(cx, value, text);
  JS::RootedString converted(cx, JS::ToString(cx, value));
  if (!converted) {
    if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
    *text = value.isObject() ? u"[object Object]" : u"";
    *failed = true; return true;
  }
  JS::RootedValue string(cx, JS::StringValue(converted));
  return String(cx, string, text);
}
}
bool PartTokenSet(State& function, CollectionRecord& record, const JS::CallArgs& args) {
  JSContext* cx = function.cx;
  if (!args.length()) return TypeError(cx, "DOMTokenList setter requires a value");
  DomString text;
  bool failed = false;
  if (!Convert(cx, args[0], &text, &failed)) return false;
  JS::AutoSaveExceptionState pending(cx);
  State& owner = *record.weakOwner;
  {
    JSAutoRealm realm(cx, owner.page);
    if (!owner.Check(owner.model.SetAttribute(record.scope, u"part", text))) return false;
  }
  args.rval().setUndefined(); return !failed;
}
bool PartTokenCall(State& function, CollectionRecord& record, const JS::CallArgs& args, TokenOperation op) {
  JSContext* cx = function.cx;
  const unsigned argc = args.length();
  if (argc < (op == TokenOperation::Replace ? 2u : 1u)) return TypeError(cx, "DOMTokenList argument is required");
  if (op == TokenOperation::TokenItem) {
    uint32_t index;
    if (!JS::ToUint32(cx, args[0], &index)) return false;
    if (index >= CollectionCount(record)) { args.rval().setNull(); return true; }
    return CollectionIndex(function, record, index, args.rval());
  }
  if (op == TokenOperation::Supports) {
    DomString ignored;
    bool failed = false;
    if (!Convert(cx, args[0], &ignored, &failed) || failed) return false;
    return NoSupportedTokens(cx, record, u"part");
  }
  const unsigned count = op == TokenOperation::Add || op == TokenOperation::Remove ? argc : op == TokenOperation::Replace ? 2u : 1u;
  std::vector<DomString> tokens(count);
  for (unsigned i = 0; i < count; ++i) {
    bool failed = false;
    if (!Convert(cx, args[i], &tokens[i], &failed)) return false;
    // Every failed conversion has an invalid fixed token. Public token
    // validation supersedes that exception after all arguments and the edit.
    if (failed) JS_ClearPendingException(cx);
  }
  const auto action = op == TokenOperation::Contains ? TokenAction::Contains : op == TokenOperation::Add ? TokenAction::Add
      : op == TokenOperation::Remove ? TokenAction::Remove : op == TokenOperation::Toggle ? TokenAction::Toggle : TokenAction::Replace;
  bool result = false;
  Status status;
  State& owner = *record.weakOwner;
  {
    JSAutoRealm realm(cx, owner.page);
    status = owner.model.EditPartTokens(record.scope, action, tokens,
        argc > 1 && JS::ToBoolean(args[1]), argc > 1, &result);
    // Invalid tokens can commit: refresh original views before reporting the
    // error in the borrowed method's realm, not the data owner's realm.
    if (!owner.Check({})) return false;
  }
  if (!function.Check(status)) return false;
  if (op == TokenOperation::Add || op == TokenOperation::Remove) args.rval().setUndefined();
  else args.rval().setBoolean(result);
  return true;
}
}
