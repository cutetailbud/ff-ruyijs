#include "web_styling/cssom.h"
#include "js/Realm.h"

namespace ruyijs::web_styling {
namespace {
const JSClass baseClass = {"StyleSheet", 0};
enum Member { Type, Href, Node, Parent, Title, Media, Disabled };
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int member = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kSheetClass, &self)) return false;
  if (!RefreshSheetSource(cx, self)) return false;
  const auto& cache = SheetDataOf(self);
  if (member == Disabled) { args.rval().setBoolean(cache.disabled); return true; }
  if (member == Media) {
    args.rval().set(JS::GetReservedSlot(self, SheetMedia));
    return JS_WrapValue(cx, args.rval());
  }
  if (member == Node) {
    args.rval().set(JS::GetReservedSlot(self, SheetOwnerNode));
    return JS_WrapValue(cx, args.rval());
  }
  if (member == Parent || (member == Href && cache.href.empty()) ||
      (member == Title && cache.title.empty())) {
    args.rval().setNull(); return true;
  }
  const std::string text = member == Type ? "text/css" : member == Title ? cache.title : cache.href;
  JS::RootedString out(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size())));
  if (!out) return false;
  args.rval().setString(out); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int member = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kSheetClass, &self)) return false;
  if (member == Disabled) SheetDataOf(self).disabled = JS::ToBoolean(args.get(0));
  else {
    std::string text;
    if (!args.get(0).isNull() && !CssString(cx, args.get(0), &text)) return false;
    SheetDataOf(self).media.set(text);
  }
  ++SheetOwnerOf(self).cssomGeneration;
  args.rval().setUndefined(); return true;
}
}
bool OrderConstructor(WebKit& kit, WebFace& face) {
  JS::ObjectOpResult deleted;
  return JS_DeleteProperty(kit.cx,face.prototype,"constructor",deleted)&&
      deleted.checkStrictModeError(kit.cx,face.prototype,true)&&
      JS_DefineProperty(kit.cx, face.prototype, "constructor", face.constructor, 0);
}
bool InstallBaseSheet(State& state) {
  auto& kit = state.kit;
  JS::RootedObject plain(kit.cx, JS::GetRealmObjectPrototype(kit.cx));
  if (!kit.Define("StyleSheet", &baseClass, plain, WebKit::Reject, &state.baseSheet)) return false;
  JS::ObjectOpResult deleted;
  if (!JS_DeleteProperty(kit.cx, state.baseSheet.prototype, "constructor", deleted)) return false;
  static const char* const names[] = {"type", "href", "ownerNode", "parentStyleSheet", "title", "media", "disabled"};
  for (int i = 0; i < 7; ++i)
    if (!kit.Accessor(state.baseSheet, "StyleSheet", names[i], Get,
                       i >= Media ? Set : nullptr, i)) return false;
  return OrderConstructor(kit, state.baseSheet);
}
}  // namespace ruyijs::web_styling
