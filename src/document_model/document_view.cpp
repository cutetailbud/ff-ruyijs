#include "document_model/state.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"
#include <cmath>
#include <vector>

namespace ruyijs::document_model {
namespace {
constexpr const char* fields[] = {"scrollingElement", "rootElement", "applets", "plugins"};
constexpr const char* methods[] = {"elementFromPoint", "elementsFromPoint",
                                  "caretPositionFromPoint", "caretRangeFromPoint"};
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const int field = FunctionSlot(args);
  JS::RootedObject receiver(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!DocumentReceiver(cx, receiver)) return false;
  state.Observe("Document", fields[field], int(ApiAccess::Read));
  args.rval().setNull();
  if (field == 0) {
    // Standards mode scrolls the root element, quirks mode the body.
    JS::RootedValue doctype(cx);
    if (!JS_GetProperty(cx, receiver, "doctype", &doctype) ||
        !JS_GetProperty(cx, receiver, doctype.isNull() ? "body" : "documentElement", args.rval()))
      return false;
  } else if (field == 2) {
    JS::RootedValue method(cx), self(cx, JS::ObjectValue(*receiver));
    JS::RootedValueArray<1> name(cx);
    JS::RootedString tag(cx, JS_NewStringCopyZ(cx, "applet"));
    if (!tag || !JS_GetProperty(cx, receiver, "getElementsByTagName", &method)) return false;
    name[0].setString(tag);
    if (!JS::Call(cx, self, method, name, args.rval())) return false;
  } else if (field == 3) {
    if (!JS_GetProperty(cx, receiver, "embeds", args.rval())) return false;
  }
  return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const int method = FunctionSlot(args);
  JS::RootedObject receiver(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!DocumentReceiver(cx, receiver)) return false;
  state.Observe("Document", methods[method], int(ApiAccess::Call));
  if (argc < 2) return TypeError(cx, "Document point query requires two arguments");
  double x, y;
  // Public methods convert both coordinates, accept non-finite values and
  // ignore options. Conversion failures propagate instead of public crashes.
  if (!JS::ToNumber(cx, args[0], &x) || !JS::ToNumber(cx, args[1], &y)) return false;

  if (method == 0 || method == 1) {
    auto* docEntry = static_cast<Wrapper*>(JS::GetReservedSlot(receiver, 0).toPrivate());
    std::vector<NodeKey> filtered;
    if (docEntry && !docEntry->dead && docEntry->owner && std::isfinite(x) && std::isfinite(y)) {
      State& docState = *docEntry->owner;
      if (docState.pointQuery && Same(docEntry->key, docState.main)) {
        std::vector<NodeKey> rawHits;
        {
          JSAutoRealm realm(cx, docState.page);
          DocumentSnapshot doc;
          if (!docState.Check(docState.model.InspectDocument(docEntry->key, &doc))) return false;
          if (doc.document_element.node) {
            JS::RootedValue rootVal(cx);
            if (!docState.Wrap(doc.document_element, &rootVal)) return false;
            if (rootVal.isObject()) {
              JS::RootedObject rootObj(cx, &rootVal.toObject());
              if (!docState.pointQuery(docState.pointQueryContext, rootObj, x, y, method == 1, &rawHits))
                return false;
            }
          }
          for (const NodeKey& k : rawHits) {
            NodeKey target;
            bool inShadow = false;
            if (!docState.Check(docState.model.Retarget(k, &docState.model, docEntry->key, &target, &inShadow)))
              return false;
            NodeKind kind = NodeKind::Document;
            if (!target.node || !docState.Check(docState.model.Kind(target, &kind)) || kind != NodeKind::Element)
              continue;
            bool dup = false;
            for (const NodeKey& existing : filtered) {
              if (Same(existing, target)) { dup = true; break; }
            }
            if (!dup) {
              filtered.push_back(target);
              if (method == 0) break;
            }
          }
        }
        if (method == 0) {
          JS::RootedValue elemVal(cx, JS::NullValue());
          if (!filtered.empty()) {
            JSAutoRealm realm(cx, docState.page);
            if (!docState.Wrap(filtered.front(), &elemVal)) return false;
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
            JSAutoRealm realm(cx, docState.page);
            if (!docState.Wrap(filtered[i], &elemVal)) return false;
          }
          if (!JS_WrapValue(cx, &elemVal) || !JS_DefineElement(cx, array, uint32_t(i), elemVal, JSPROP_ENUMERATE))
            return false;
        }
        args.rval().setObject(*array);
        return true;
      }
    }
  }

  if (method == 1) {
    JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
    if (!array) return false;
    args.rval().setObject(*array);
  } else args.rval().setNull();
  return true;
}
}
bool InstallDocumentView(State& state) {
  for (int field = 0; field < int(std::size(fields)); ++field)
    if (!state.Accessor("Document", fields[field], Read, field)) return false;
  for (int method = 0; method < int(std::size(methods)); ++method)
    if (!state.Method("Document", methods[method], Call, method == 2 ? 2 : 0, method)) return false;
  return true;
}
}
