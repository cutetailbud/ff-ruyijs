#include "document_model/state.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <cmath>
#include <vector>

namespace ruyijs::document_model {
namespace {
constexpr const char* members[] = {"mode", "host", "delegatesFocus", "slotAssignment",
  "clonable", "serializable", "innerHTML", "activeElement", "fullscreenElement",
  "styleSheets", "adoptedStyleSheets", "pointerLockElement", "pictureInPictureElement",
  "onslotchange", "getAnimations", "getHTML", "setHTML", "elementFromPoint", "elementsFromPoint",
  "setHTMLUnsafe", "getElementById"};
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = ShadowReceiver(cx, args.thisv());
  if (!entry) return false;
  State& state = *entry->owner;
  const int slot = FunctionSlot(args);
  state.Observe("ShadowRoot", members[slot], int(ApiAccess::Read));
  ShadowSnapshot root;
  if (!state.Check(state.model.InspectShadow(entry->key, &root))) return false;
  switch (slot) {
    case 0: return Value(cx, DomStringView(root.options.closed ? u"closed" : u"open"), args.rval());
    case 1: return state.Wrap(root.host, args.rval());
    case 2: args.rval().setBoolean(root.options.delegates_focus); return true;
    case 3: return Value(cx, DomStringView(root.options.manual_slots ? u"manual" : u"named"), args.rval());
    case 4: args.rval().setBoolean(root.options.clonable); return true;
    case 5: args.rval().setBoolean(root.options.serializable); return true;
    case 6: {
      DomString html;
      return state.Check(state.model.InnerHtml(entry->key, &html)) && Value(cx, DomStringView(html), args.rval());
    }
    case 7: case 8: case 11: case 12: args.rval().setNull(); return true;
  }
  return ThrowDOM(state, "NotSupportedError", "ShadowRoot capability awaits its original subsystem port");
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = ShadowReceiver(cx, args.thisv());
  if (!entry) return false;
  State& state = *entry->owner;
  const int slot = FunctionSlot(args);
  state.Observe("ShadowRoot", members[slot], int(ApiAccess::Write));
  if (slot != 6) return ThrowDOM(state, "NotSupportedError", "ShadowRoot capability awaits its original subsystem port");
  if (!argc) return TypeError(cx, "innerHTML setter requires a value");
  DomString html;
  if (!String(cx, args[0], &html, true) || !state.Check(state.model.SetInnerHtml(entry->key, html))) return false;
  args.rval().setUndefined(); return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = ShadowReceiver(cx, args.thisv());
  if (!entry) return false;
  State& state = *entry->owner;
  const int slot = FunctionSlot(args);
  state.Observe("ShadowRoot", members[slot], int(ApiAccess::Call));
  if (slot == 17 || slot == 18) {
    if (argc < 2) return TypeError(cx, "ShadowRoot point query requires two arguments");
    double x = 0, y = 0;
    if (!JS::ToNumber(cx, args[0], &x) || !JS::ToNumber(cx, args[1], &y)) return false;
    std::vector<NodeKey> filtered;
    if (std::isfinite(x) && std::isfinite(y) && state.pointQuery && state.main.node) {
      JSAutoRealm realm(cx, state.page);
      NodeSnapshot snap;
      if (!state.Check(state.model.Inspect(entry->key, &snap))) return false;
      if (snap.is_connected && Same(snap.composed_root, state.main)) {
        DocumentSnapshot doc;
        if (!state.Check(state.model.InspectDocument(state.main, &doc))) return false;
        std::vector<NodeKey> rawHits;
        if (doc.document_element.node) {
          JS::RootedValue rootVal(cx);
          if (!state.Wrap(doc.document_element, &rootVal)) return false;
          if (rootVal.isObject()) {
            JS::RootedObject rootObj(cx, &rootVal.toObject());
            if (!state.pointQuery(state.pointQueryContext, rootObj, x, y, true, &rawHits)) return false;
          }
        }
        for (const NodeKey& k : rawHits) {
          NodeKey target;
          bool inShadow = false;
          if (!state.Check(state.model.Retarget(k, &state.model, entry->key, &target, &inShadow)))
            return false;
          bool inside = false;
          if (!target.node || Same(target, entry->key) ||
              !state.Check(state.model.AncestorOrSelf(entry->key, target, &inside)))
            return false;
          if (!inside) continue;
          NodeKind kind = NodeKind::Document;
          if (!state.Check(state.model.Kind(target, &kind)) || kind != NodeKind::Element) continue;
          bool dup = false;
          for (const NodeKey& existing : filtered) {
            if (Same(existing, target)) { dup = true; break; }
          }
          if (!dup) {
            filtered.push_back(target);
            if (slot == 17) break;
          }
        }
      }
    }
    if (slot == 17) {
      JS::RootedValue elemVal(cx, JS::NullValue());
      if (!filtered.empty()) {
        JSAutoRealm realm(cx, state.page);
        if (!state.Wrap(filtered.front(), &elemVal)) return false;
      }
      if (!JS_WrapValue(cx, &elemVal)) return false;
      args.rval().set(elemVal);
      return true;
    }
    JS::RootedObject array(cx, JS::NewArrayObject(cx, filtered.size()));
    if (!array) return false;
    for (size_t i = 0; i < filtered.size(); ++i) {
      JS::RootedValue elemVal(cx);
      {
        JSAutoRealm realm(cx, state.page);
        if (!state.Wrap(filtered[i], &elemVal)) return false;
      }
      if (!JS_WrapValue(cx, &elemVal) || !JS_DefineElement(cx, array, uint32_t(i), elemVal, JSPROP_ENUMERATE))
        return false;
    }
    args.rval().setObject(*array);
    return true;
  }
  if (slot == 20) {
    if (!argc) return TypeError(cx, "getElementById requires an ID");
    DomString id;
    std::vector<NodeKey> found;
    if (!String(cx, args[0], &id) || !state.Check(state.model.Query(entry->key, QueryKind::Id, id, &found))) return false;
    return state.Wrap(found.empty() ? NodeKey{} : found.front(), args.rval());
  }
  if (slot == 15) {
    if (!args.get(0).isNullOrUndefined() && !args[0].isObject()) return TypeError(cx, "getHTML options require a dictionary");
    DomString html;
    return state.Check(state.model.InnerHtml(entry->key, &html)) && Value(cx, DomStringView(html), args.rval());
  }
  if (slot == 19) {
    if (!argc) return TypeError(cx, "setHTMLUnsafe requires markup");
    DomString html;
    if (!String(cx, args[0], &html) || !state.Check(state.model.SetInnerHtml(entry->key, html))) return false;
    args.rval().setUndefined(); return true;
  }
  return ThrowDOM(state, "NotSupportedError", "ShadowRoot capability awaits its original subsystem port");
}
}
bool InstallShadowRootMembers(State& state) {
  struct Property { const char* name; bool writable; };
  static const Property properties[] = {
    {"mode",false},{"host",false},{"delegatesFocus",false},{"slotAssignment",false},
    {"clonable",false},{"serializable",false},{"innerHTML",true},{"activeElement",false},
    {"fullscreenElement",false},
    {"pointerLockElement",false},{"pictureInPictureElement",false},{"onslotchange",true}
  };
  for (int i = 0; i < int(std::size(properties)); ++i)
    if (!state.Accessor("ShadowRoot", properties[i].name, Read, i < 9 ? i : i + 2,
        properties[i].writable ? Write : nullptr)) return false;
  static const struct { const char* name; unsigned length; } methods[] = {
    {"getAnimations",0},{"getHTML",0},{"setHTML",1},{"elementFromPoint",2},{"elementsFromPoint",2},{"setHTMLUnsafe",1},
    {"getElementById",1}
  };
  for (int i = 0; i < int(std::size(methods)); ++i)
    if (!state.Method("ShadowRoot", methods[i].name, Call, methods[i].length, i + 14)) return false;
  return true;
}
}
