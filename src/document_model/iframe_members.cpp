#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
struct Member { const char* name; const char16_t* attribute; };
constexpr Member kMembers[] = {
  {"name", u"name"}, {"width", u"width"}, {"height", u"height"},
  {"allow", u"allow"}, {"referrerPolicy", u"referrerpolicy"}, {"loading", u"loading"},
  {"allowFullscreen", u"allowfullscreen"}, {"sandbox", u"sandbox"},
  // The obsolete partial interface (HTMLIFrameElement.webidl): plain reflection,
  // longDesc as a URL, the two margins [LegacyNullToEmptyString].
  {"align", u"align"}, {"scrolling", u"scrolling"}, {"frameBorder", u"frameborder"},
  {"longDesc", u"longdesc"}, {"marginHeight", u"marginheight"}, {"marginWidth", u"marginwidth"}
};
enum Field { kAllowFullscreen = 6, kSandbox = 7, kLongDesc = 11, kMarginHeight = 12, kMarginWidth = 13 };
// [SameObject]: one list per element, kept on the wrapper (as Element.part).
bool Sandbox(State& state, Wrapper& entry, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedObject node(cx, entry.object);
  result.set(JS::GetReservedSlot(node, kNodeSandboxSlot));
  if (!result.isUndefined()) return true;
  if (!state.TokenListOwned(entry, u"sandbox", result)) return false;
  JSAutoRealm realm(cx, node);
  if (!JS_WrapValue(cx, result)) return false;
  JS::SetReservedSlot(node, kNodeSandboxSlot, result);
  return true;
}
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto field = FunctionSlot(args);
  const auto& member = kMembers[field];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLIFrameElement", ("get " + std::string(member.name)).c_str());
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLIFrameElement", member.name, int(ApiAccess::Read));
    if (field == kSandbox) {
      if (!Sandbox(state, *entry, &result)) return false;
    } else {
      std::optional<DomString> raw;
      if (!state.Check(state.model.GetAttribute(entry->key, member.attribute, &raw))) return false;
      if (field == kAllowFullscreen) result.setBoolean(raw.has_value());
      else if (field == kLongDesc) { if (!ReflectUrl(state, entry->key, raw, &result)) return false; }
      else if (!Value(cx, DomStringView(raw.value_or(u"")), &result)) return false;
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto field = FunctionSlot(args);
  const auto& member = kMembers[field];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLIFrameElement", ("set " + std::string(member.name)).c_str());
  if (!entry) return false;
  if (field == kSandbox) {
    JS::RootedValue list(cx);
    {
      auto& state = *entry->owner;
      JSAutoRealm realm(cx, state.page);
      state.Observe("HTMLIFrameElement", member.name, int(ApiAccess::Write));
      if (!Sandbox(state, *entry, &list)) return false;
    }
    if (!JS_WrapValue(cx, &list)) return false;
    JS::RootedObject tokens(cx, &list.toObject());
    if (!JS_SetProperty(cx, tokens, "value", args.get(0))) return false;
    args.rval().setUndefined(); return true;
  }
  DomString text;
  const bool nullToEmpty = (field == kMarginHeight || field == kMarginWidth) && args.get(0).isNull();
  // The obsolete attributes convert with plain WebIDL DOMString rules.
  const bool plain = field > kSandbox;
  if (field != kAllowFullscreen && !nullToEmpty &&
      !(plain ? String(cx, args.get(0), &text) : ControlString(cx, args.get(0), &text))) return false;
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLIFrameElement", member.name, int(ApiAccess::Write));
    const auto status = field == kAllowFullscreen && !JS::ToBoolean(args.get(0))
        ? state.model.RemoveAttribute(entry->key, member.attribute)
        : state.model.SetAttribute(entry->key, member.attribute, text);
    if (!state.Check(status)) return false;
  }
  args.rval().setUndefined(); return true;
}
bool SvgDocument(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLIFrameElement", "getSVGDocument");
  if (!entry) return false;
  entry->owner->Observe("HTMLIFrameElement", "getSVGDocument", int(ApiAccess::Call));
  args.rval().setNull(); return true;
}
}
bool InstallIframeMembers(State& state) {
  for (size_t i = 0; i < std::size(kMembers); ++i)
    if (!state.Accessor("HTMLIFrameElement", kMembers[i].name, Read, int32_t(i), Write)) return false;
  return state.Method("HTMLIFrameElement", "getSVGDocument", SvgDocument, 0, 0);
}
}
