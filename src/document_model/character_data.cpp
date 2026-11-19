#include "document_model/state.h"
#include "js/Conversions.h"
#include <limits>
namespace ruyijs::document_model {
enum Method { Substring, Append, Insert, Delete, Replace };
static bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Text);
  if (!entry) return false;
  State& state = *entry->owner;
  if (FunctionSlot(args) == 0) {
    std::optional<DomString> text;
    if (!state.Check(state.model.NodeValue(entry->key, &text))) return false;
    return Value(cx, text, args.rval());
  }
  size_t length = 0;
  if (!state.Check(state.model.DataLength(entry->key, &length))) return false;
  args.rval().setNumber(double(length)); return true;
}
static bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Text);
  if (!entry) return false;
  DomString value;
  if (!String(cx, args.get(0), &value, args.get(0).isNull())) return false;
  State& state = *entry->owner;
  if (!state.Check(state.model.SetNodeValue(entry->key, value))) return false;
  args.rval().setUndefined(); return true;
}
static bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Text);
  if (!entry) return false;
  auto op = Method(FunctionSlot(args));
  unsigned required = op == Append ? 1 : op == Replace ? 3 : 2;
  if (argc < required) return TypeError(cx, "CharacterData method has too few arguments");
  uint32_t offset = 0, count = 0;
  DomString text;
  if (op != Append && !JS::ToUint32(cx, args[0], &offset)) return false;
  if ((op == Substring || op == Delete || op == Replace) &&
      !JS::ToUint32(cx, args[1], &count)) return false;
  if (op == Append || op == Insert || op == Replace) {
    unsigned position = op == Append ? 0 : op == Insert ? 1 : 2;
    if (!String(cx, args[position], &text)) return false;
  }
  State& state = *entry->owner;
  if (op == Append) {
    size_t length = 0;
    if (!state.Check(state.model.DataLength(entry->key, &length))) return false;
    offset = uint32_t(length);
  }
  if (op == Substring) {
    if (!state.Check(state.model.SubstringData(entry->key, offset, count, &text))) return false;
    return Value(cx, DomStringView(text), args.rval());
  }
  if (!state.Check(state.model.ReplaceData(entry->key, offset, count, text))) return false;
  args.rval().setUndefined(); return true;
}
bool InstallCharacterData(State& state) {
  if (!state.Accessor("CharacterData", "data", Get, 0, Set) ||
      !state.Accessor("CharacterData", "length", Get, 1)) return false;
  struct Operation { const char* name; Method method; unsigned length; };
  static const Operation methods[] = {
    {"substringData", Substring, 2}, {"appendData", Append, 1}, {"insertData", Insert, 2},
    {"deleteData", Delete, 2}, {"replaceData", Replace, 3}
  };
  for (const auto& method : methods)
    if (!state.Method("CharacterData", method.name, Call, method.length, method.method)) return false;
  return InstallTextMembers(state);
}
}
