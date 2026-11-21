#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
// HTMLIFrameElement::sSupportedSandboxTokens (IframeSandboxKeywordList.inc).
constexpr const char16_t* kSandboxTokens[] = {
  u"allow-same-origin", u"allow-forms", u"allow-scripts", u"allow-top-navigation",
  u"allow-top-navigation-by-user-activation", u"allow-pointer-lock", u"allow-orientation-lock",
  u"allow-popups", u"allow-modals", u"allow-popups-to-escape-sandbox", u"allow-presentation",
  u"allow-storage-access-by-user-activation", u"allow-downloads",
  u"allow-top-navigation-to-custom-protocols"
};
bool SupportedSandboxToken(DomStringView token) {
  for (DomStringView supported : kSandboxTokens) {
    if (supported.size() != token.size()) continue;
    bool same = true;
    for (size_t i = 0; same && i < token.size(); ++i) {
      char16_t unit = token[i];
      if (unit >= u'A' && unit <= u'Z') unit = char16_t(unit + 32);
      same = unit == supported[i];
    }
    if (same) return true;
  }
  return false;
}
DomStringView Attribute(const CollectionRecord& record) {
  return record.argument.empty() ? DomStringView(u"class") : DomStringView(record.argument);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = CollectionReceiver(cx, args.thisv(), CollectionKind::TokenList);
  if (!record) return false;
  State* state = record->dataOwner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state->page);
    std::optional<DomString> value;
    if (!state->Check(state->model.GetAttribute(record->scope, Attribute(*record), &value)) ||
        !Value(cx, DomStringView(value.value_or(u"")), &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = CollectionReceiver(cx, args.thisv(), CollectionKind::TokenList);
  if (!record) return false;
  if (Attribute(*record) == u"part") return PartTokenSet(*FunctionState(args), *record, args);
  DomString value;
  // DOMString value: null and undefined stringify ("null"), as in Firefox.
  if (Attribute(*record) == u"sandbox" ? !ControlString(cx, args.get(0), &value)
                                      : !String(cx, args.get(0), &value)) return false;
  {
    State* state = record->dataOwner;
    JSAutoRealm realm(cx, state->page);
    if (!state->Check(state->model.SetAttribute(record->scope, Attribute(*record), value))) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = CollectionReceiver(cx, args.thisv(), CollectionKind::TokenList);
  if (!record) return false;
  const auto op = TokenOperation(FunctionSlot(args));
  if (Attribute(*record) == u"part") return PartTokenCall(*FunctionState(args), *record, args, op);
  const bool sandbox = Attribute(*record) == u"sandbox";
  if (sandbox && argc < (op == TokenOperation::Replace ? 2u : 1u)) return TypeError(cx, "DOMTokenList argument is required");
  if (op == TokenOperation::TokenItem) {
    uint32_t index = 0;
    if (!JS::ToUint32(cx, args.get(0), &index)) return false;
    if (index >= CollectionCount(*record)) { args.rval().setNull(); return true; }
    return CollectionIndex(*record->dataOwner, *record, index, args.rval());
  }
  if (op == TokenOperation::Supports) {
    if (!sandbox) return NoSupportedTokens(cx, *record, Attribute(*record));
    DomString token;
    if (!ControlString(cx, args.get(0), &token)) return false;
    args.rval().setBoolean(SupportedSandboxToken(token)); return true;
  }
  auto convert = [&](JS::HandleValue v, DomString* out) {
    return sandbox ? ControlString(cx, v, out) : String(cx, v, out);
  };
  if (op == TokenOperation::Add || op == TokenOperation::Remove) {
    for (unsigned i = 0; i < argc; ++i) {
      DomString token; bool ignored;
      if (!convert(args[i], &token)) return false;
      State* state = record->dataOwner;
      JSAutoRealm realm(cx, state->page);
      if (!state->Check(state->model.EditTokens(record->scope, Attribute(*record),
          op == TokenOperation::Add ? TokenAction::Add : TokenAction::Remove, token, u"", false, false, &ignored))) return false;
    }
    args.rval().setUndefined(); return true;
  }
  DomString first, second;
  if (!convert(args.get(0), &first) || (op == TokenOperation::Replace && !convert(args.get(1), &second))) return false;
  const bool hasForce = argc > 1, force = hasForce && JS::ToBoolean(args[1]);
  bool result = false;
  const auto action = op == TokenOperation::Contains ? TokenAction::Contains : op == TokenOperation::Toggle ? TokenAction::Toggle : TokenAction::Replace;
  State* state = record->dataOwner;
  JSAutoRealm realm(cx, state->page);
  if (!state->Check(state->model.EditTokens(record->scope, Attribute(*record), action,
      first, second, force, hasForce, &result))) return false;
  args.rval().setBoolean(result); return true;
}
}
namespace {
// Element and attribute names for the message (unpaired surrogates as U+FFFD).
std::string Utf8(DomStringView text) {
  std::string out;
  for (size_t i = 0; i < text.size(); ++i) {
    uint32_t c = text[i];
    if (c >= 0xd800 && c <= 0xdbff && i + 1 < text.size() && text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff)
      c = 0x10000 + ((c - 0xd800) << 10) + (text[++i] - 0xdc00);
    else if (c >= 0xd800 && c <= 0xdfff) c = 0xfffd;
    if (c < 0x80) out += char(c);
    else if (c < 0x800) { out += char(0xc0 | (c >> 6)); out += char(0x80 | (c & 0x3f)); }
    else if (c < 0x10000) { out += char(0xe0 | (c >> 12)); out += char(0x80 | ((c >> 6) & 0x3f)); out += char(0x80 | (c & 0x3f)); }
    else { out += char(0xf0 | (c >> 18)); out += char(0x80 | ((c >> 12) & 0x3f)); out += char(0x80 | ((c >> 6) & 0x3f)); out += char(0x80 | (c & 0x3f)); }
  }
  return out;
}
}  // namespace
bool NoSupportedTokens(JSContext* cx, CollectionRecord& record, DomStringView attribute) {
  State& owner = *record.dataOwner;
  NodeSnapshot snapshot;
  {
    JSAutoRealm realm(cx, owner.page);
    if (!owner.Check(owner.model.Inspect(record.scope, &snapshot))) return false;
  }
  // Firefox's MSG_TOKENLIST_NO_SUPPORTED_TOKENS, arguments in its order.
  const std::string message = "DOMTokenList.supports: " + Utf8(snapshot.local_name.value_or(u"")) +
      " attribute of <" + Utf8(attribute) + "> does not define any supported tokens";
  return TypeError(cx, message.c_str());
}
bool InstallTokenList(State& state) {
  struct Operation { const char* name; TokenOperation op; unsigned length; };
  static const Operation operations[] = {
    {"add", TokenOperation::Add, 0}, {"remove", TokenOperation::Remove, 0}, {"toggle", TokenOperation::Toggle, 1},
    {"contains", TokenOperation::Contains, 1}, {"replace", TokenOperation::Replace, 2}, {"supports", TokenOperation::Supports, 1}, {"item", TokenOperation::TokenItem, 1}
  };
  if (!state.Accessor("DOMTokenList", "length", CollectionLength, int32_t(CollectionKind::TokenList)) ||
      !state.Accessor("DOMTokenList", "value", Get, 0, Set) ||
      !state.Method("DOMTokenList", "toString", Get, 0, 0)) return false;
  for (const auto& operation : operations)
    if (!state.Method("DOMTokenList", operation.name, Call, operation.length, int32_t(operation.op))) return false;
  return state.Method("DOMTokenList", "forEach", CollectionForEach, 1, int32_t(CollectionKind::TokenList)) &&
      InstallCollectionMethods(state, "DOMTokenList");
}
}
