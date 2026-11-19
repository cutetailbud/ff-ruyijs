#include "document_model/state.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/Realm.h"
#include "js/StableStringChars.h"
#include "js/String.h"
namespace ruyijs::document_model {
static bool Error(JSContext* cx, JSExnType type, const char* message) {
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message));
  JS::RootedString file(cx, JS_NewStringCopyZ(cx, "ruyijs:document"));
  JS::Rooted<mozilla::Maybe<JS::Value>> cause(cx, mozilla::Nothing());
  JS::RootedValue value(cx);
  if (!text || !file || !JS::CreateError(cx, type, nullptr, file, 1,
      JS::ColumnNumberOneOrigin(), nullptr, text, cause, &value)) return false;
  JS_SetPendingException(cx, value);
  return false;
}
bool TypeError(JSContext* cx, const char* message) { return Error(cx, JSEXN_TYPEERR, message); }
bool RangeError(JSContext* cx, const char* message) { return Error(cx, JSEXN_RANGEERR, message); }
bool StandardError(JSContext* cx, const char* message) { return Error(cx, JSEXN_ERR, message); }
bool String(JSContext* cx, JS::HandleValue value, DomString* result, bool null_empty) {
  if (null_empty && value.isNullOrUndefined()) { result->clear(); return true; }
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  if (JS_GetStringLength(text) > 16 * 1024 * 1024)
    return RangeError(cx, "DOMString exceeds product limit");
  JS::AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, text)) return false;
  result->assign(chars.twoByteChars(), chars.length());
  return true;
}
bool Value(JSContext* cx, DomStringView text, JS::MutableHandleValue result) {
  JSString* value = JS_NewUCStringCopyN(cx, text.data(), text.size());
  if (!value) return false;
  result.setString(value); return true;
}
bool Value(JSContext* cx, const std::optional<DomString>& text, JS::MutableHandleValue result) {
  if (!text) { result.setNull(); return true; }
  return Value(cx, DomStringView(*text), result);
}
State* FunctionState(const JS::CallArgs& args) {
  return static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
}
int32_t FunctionSlot(const JS::CallArgs& args) {
  return js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
}
Wrapper* Receiver(State& state, const JS::CallArgs& args, NodeKind* expected) {
  Wrapper* entry = args.thisv().isObject() ? state.Find(&args.thisv().toObject()) : nullptr;
  if (!entry) { TypeError(state.cx, "Node receiver is incompatible"); return nullptr; }
  if (expected) {
    NodeSnapshot snapshot;
    if (!state.Check(state.model.Inspect(entry->key, &snapshot))) return nullptr;
    bool compatible = snapshot.kind == *expected ||
        (*expected == NodeKind::Text &&
         (snapshot.kind == NodeKind::Comment || snapshot.kind == NodeKind::ProcessingInstruction));
    if (!compatible) { TypeError(state.cx, "DOM interface receiver is incompatible"); return nullptr; }
  }
  return entry;
}
bool NodeArgument(State& state, JS::HandleValue value, NodeKey* key, bool nullable) {
  if (nullable && value.isNullOrUndefined()) { *key = {}; return true; }
  Wrapper* entry = value.isObject() ? state.Find(&value.toObject()) : nullptr;
  if (!entry) return TypeError(state.cx, "Argument is not a Node of this realm");
  *key = entry->key; return true;
}
bool State::Check(Status status) {
  // Internal committed-change consumers leave failures pending at this boundary.
  if (JS_IsExceptionPending(cx)) return false;
  // Every owner mutation funnels through here, so this is the single place
  // live collections are refreshed from the owner revision.
  if (status.code == ErrorCode::None) {
    if (syncing || model.StructureRevision() == syncedRevision) return true;
    JSAutoRealm realm(cx, page);
    return SyncCollections();
  }
  if (status.code == ErrorCode::OutOfMemory) { JS_ReportOutOfMemory(cx); return false; }
  if (status.code == ErrorCode::LimitExceeded) return RangeError(cx, status.message);
  const char* name = status.code == ErrorCode::NotFound ? "NotFoundError" :
      status.code == ErrorCode::HierarchyRequest ? "HierarchyRequestError" :
      status.code == ErrorCode::InvalidCharacter ? "InvalidCharacterError" :
      status.code == ErrorCode::IndexSize ? "IndexSizeError" :
      status.code == ErrorCode::NotSupported ? "NotSupportedError" :
  status.code == ErrorCode::Namespace ? "NamespaceError" :
  status.code == ErrorCode::NoModification ? "NoModificationAllowedError" :
      status.code == ErrorCode::Syntax ? "SyntaxError" : "InvalidStateError";
  return ThrowDOM(*this, name, status.message);
}
}
