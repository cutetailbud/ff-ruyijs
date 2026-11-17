// Focus (ARCHITECTURE.md, host user input). The focused element lives in the
// domain DocumentAggregate; this file fires the trusted FocusEvents of a change
// (blur, focusout, focus, focusin, as Firefox's nsFocusManager), runs autofocus,
// and backs HTMLElement.prototype.focus/blur and the pointer's mousedown focus.
// Editing and activation default actions are in user_actions.cpp.
#include "document_model/html_element_internal.h"
#include "document_model/focus_internal.h"
#include "document_model/editing.h"
#include "interaction/realm.h"
#include "runtime/runtime.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <vector>

namespace ruyijs::document_model {
namespace focus_detail {
bool Has(State& s, NodeKey key, const char16_t* name, DomString* value) {
  std::optional<DomString> found;
  if (!s.Check(s.model.GetAttribute(key, name, &found)) || !found) return false;
  if (value) *value = *found;
  return true;
}
DomString Lower(DomString text) {
  for (auto& c : text) if (c >= u'A' && c <= u'Z') c = char16_t(c - u'A' + u'a');
  return text;
}
DomString Tag(const NodeSnapshot& node) {
  return node.is_html_element && node.local_name ? *node.local_name : DomString();
}
bool Fire(State& s, NodeKey key, const char* type, bool bubbles, JS::HandleValue related) {
  JSContext* cx = s.cx;
  JS::RootedValue target(cx);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  if (!init || !s.Wrap(key, &target) ||
      !JS_DefineProperty(cx, init, "bubbles", bubbles, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "cancelable", false, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "relatedTarget", related, JSPROP_ENUMERATE)) return false;
  JS::RootedObject element(cx, &target.toObject());
  return interaction::DispatchTrusted(cx, element, type, init, -1, nullptr);
}
bool Focusable(State& s, NodeKey key) {
  NodeSnapshot node;
  if (!s.Check(s.model.Inspect(key, &node)) || node.kind != NodeKind::Element || !node.is_connected)
    return false;
  const DomString tag = Tag(node);
  if (tag == u"input" || tag == u"textarea" || tag == u"select" || tag == u"button") {
    bool disabled = false, submit = false;
    DomString type;
    if (!s.Check(s.model.FormControlInfo(key, &disabled, &submit)) || disabled) return false;
    return !(tag == u"input" && Has(s, key, u"type", &type) && Lower(type) == u"hidden");
  }
  if ((tag == u"a" || tag == u"area") && Has(s, key, u"href")) return true;
  if (tag == u"iframe" || tag == u"summary") return true;
  if ((tag == u"audio" || tag == u"video") && Has(s, key, u"controls")) return true;
  DomString editable;
  if (Has(s, key, u"contenteditable", &editable) && Lower(editable) != u"false") return true;
  return Has(s, key, u"tabindex");
}
bool MoveFocus(State& s, NodeKey document, NodeKey next) {
  NodeKey previous;
  if (!s.Check(s.model.FocusedElement(document, &previous))) return false;
  if (Same(previous, next)) return true;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.page);
  JS::RootedValue before(cx, JS::NullValue()), after(cx, JS::NullValue());
  if ((previous.node && !s.Wrap(previous, &before)) || (next.node && !s.Wrap(next, &after))) return false;
  if (previous.node) {
    if (!s.Check(s.model.SetFocusedElement(document, NodeKey{document.document, 0})) ||
        !Fire(s, previous, "blur", false, after) || !Fire(s, previous, "focusout", true, after)) return false;
  }
  if (!next.node) return true;
  if (!s.Check(s.model.SetFocusedElement(document, next))) return false;
  // Focusing an editing host places the caret in it (when the window has focus,
  // which a headless page always has: the selection is not elsewhere in it).
  NodeKey host;
  if (Same(document, s.main) && EditHost(s, next, &host) && Same(host, next) && !EditFocusCaret(s, host))
    return false;
  if (JS_IsExceptionPending(cx)) return false;
  return Fire(s, next, "focus", false, before) && Fire(s, next, "focusin", true, before);
}
NodeKey OwnerDocument(State& s, NodeKey key) {
  NodeSnapshot node;
  return s.Check(s.model.Inspect(key, &node)) ? node.owner_document : NodeKey{};
}
// The pointer's default focus target: the nearest focusable inclusive ancestor.
NodeKey PointerFocusTarget(State& s, NodeKey key) {
  for (unsigned depth = 0; key.node && depth < 1024; ++depth) {
    if (Focusable(s, key)) return key;
    NodeSnapshot node;
    if (!s.Check(s.model.Inspect(key, &node))) break;
    key = node.parent_element;
  }
  return NodeKey{};
}
Wrapper* Element(JSContext* cx, JS::HandleObject object) {
  JS::RootedValue value(cx, JS::ObjectValue(*object));
  return NodeReceiver(cx, value, NodeKind::Element);
}
}  // namespace focus_detail
using namespace focus_detail;


bool FocusElement(JSContext* cx, JS::HandleObject element) {
  Wrapper* entry = Element(cx, element);
  if (!entry) return false;
  State& s = *entry->owner;
  return !Focusable(s, entry->key) || MoveFocus(s, OwnerDocument(s, entry->key), entry->key);
}
bool BlurElement(JSContext* cx, JS::HandleObject element) {
  Wrapper* entry = Element(cx, element);
  if (!entry) return false;
  State& s = *entry->owner;
  const NodeKey document = OwnerDocument(s, entry->key);
  NodeKey focused;
  if (!s.Check(s.model.FocusedElement(document, &focused))) return false;
  return !Same(focused, entry->key) || MoveFocus(s, document, NodeKey{document.document, 0});
}
bool PointerFocus(JSContext* cx, JS::HandleObject target) {
  Wrapper* entry = Element(cx, target);
  if (!entry) return false;
  State& s = *entry->owner;
  const NodeKey document = OwnerDocument(s, entry->key), next = PointerFocusTarget(s, entry->key);
  return MoveFocus(s, document, next.node ? next : NodeKey{document.document, 0});
}
// The first connected, focusable `autofocus` element of the realm's main
// document gets focus when nothing has it yet (Firefox's autofocus candidates).
bool RunAutofocus(State& s) {
  NodeKey focused;
  DocumentSnapshot document;
  if (!s.Check(s.model.FocusedElement(s.main, &focused)) || focused.node ||
      !s.Check(s.model.InspectDocument(s.main, &document)) || !document.document_element.node) return true;
  std::vector<NodeKey> nodes;
  if (!s.Check(s.model.Subtree(document.document_element, &nodes))) return false;
  for (const NodeKey& key : nodes) {
    NodeSnapshot node;
    if (!s.Check(s.model.Inspect(key, &node))) return false;
    if (node.kind == NodeKind::Element && Has(s, key, u"autofocus") && Focusable(s, key))
      return MoveFocus(s, s.main, key);
  }
  return true;
}
// The receiver check reads the callee, which shares its slot with rval: it
// runs first and leaves rval undefined.
bool FocusMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, args.thisv().isObject() ? &args.thisv().toObject() : nullptr);
  return InertElement(cx, argc, vp) && FocusElement(cx, self);
}
// The receiver check reads the callee, which shares its slot with rval: it
// runs first and leaves rval undefined.
bool BlurMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, args.thisv().isObject() ? &args.thisv().toObject() : nullptr);
  return InertElement(cx, argc, vp) && BlurElement(cx, self);
}
}  // namespace ruyijs::document_model
