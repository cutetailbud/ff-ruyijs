#include "web_styling/cssom.h"
#include "js/Realm.h"
#include "js/Symbol.h"

namespace ruyijs::web_styling {
namespace {
enum Member { Item, Delete, Append, String, Text, Length };
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int member = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kMediaListClass, &self)) return false;
  JS::RootedObject sheet(cx, &js::GetProxyReservedSlot(self, ListSheet).toObject());
  const auto& value = ContainerMedia(sheet);
  if (member == Length) { args.rval().setNumber(value.entries().size()); return true; }
  if (member == Item) {
    if (!argc) return WebKit::Fail(cx, "MediaList.item requires an index");
    uint32_t index;
    if (!JS::ToUint32(cx, args[0], &index) || !CssListAt(cx, self, index, args.rval())) return false;
    if (args.rval().isUndefined()) args.rval().setNull();
    return true;
  }
  auto text = value.text();
  JS::RootedString out(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size())));
  if (!out) return false;
  args.rval().setString(out); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int member = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kMediaListClass, &self)) return false;
  if (!argc) return WebKit::Fail(cx, "MediaList member requires a value");
  std::string text;
  if (!(member == Text && args[0].isNull()) && !CssString(cx, args[0], &text)) return false;
  JS::RootedObject sheet(cx, &js::GetProxyReservedSlot(self, ListSheet).toObject());
  State& state = ContainerOwnerOf(sheet);
  JSAutoRealm realm(cx, state.page);
  auto& value = ContainerMedia(sheet);
  if (member == Text) value.set(text);
  else if (member == Append) {
    if (text.empty()) return state.Throw("NotFoundError", "Empty medium");
    value.append(text);
  } else if (!value.remove(text)) return state.Throw("NotFoundError", "Medium not in list");
  ++state.cssomGeneration;
  args.rval().setUndefined(); return true;
}
}
bool InstallMediaList(State& state) {
  auto& kit = state.kit;auto& face = state.mediaList;
  JS::RootedObject plain(kit.cx, JS::GetRealmObjectPrototype(kit.cx));
  if (!kit.Define("MediaList", &kMediaListClass, plain, WebKit::Reject, &face)) return false;
  JS::ObjectOpResult deleted;
  JS::RootedId tagId(kit.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(kit.cx, JS::SymbolCode::toStringTag)));
  if (!JS_DeleteProperty(kit.cx, face.prototype, "constructor", deleted) ||
      !JS_DeletePropertyById(kit.cx, face.prototype, tagId, deleted) ||
      !kit.Method(face, "MediaList", "item", Get, 1, Item) ||
      !kit.Method(face, "MediaList", "deleteMedium", Set, 1, Delete) ||
      !kit.Method(face, "MediaList", "appendMedium", Set, 1, Append) ||
      !kit.Method(face, "MediaList", "toString", Get, 0, String) ||
      !kit.Accessor(face, "MediaList", "mediaText", Get, Set, Text) ||
      !kit.Accessor(face, "MediaList", "length", Get, nullptr, Length) ||
      !OrderConstructor(kit, face)) return false;
  JS::RootedObject array(kit.cx, JS::GetRealmArrayPrototype(kit.cx));
  JS::RootedValue values(kit.cx);
  JS::RootedId iterator(kit.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(kit.cx, JS::SymbolCode::iterator)));
  JS::RootedString tag(kit.cx, JS_NewStringCopyZ(kit.cx, "MediaList"));
  return array && tag && JS_GetProperty(kit.cx, array, "values", &values) &&
      JS_DefinePropertyById(kit.cx, face.prototype, iterator, values, 0) &&
      JS_DefinePropertyById(kit.cx, face.prototype, tagId, tag, JSPROP_READONLY);
}
}  // namespace ruyijs::web_styling
