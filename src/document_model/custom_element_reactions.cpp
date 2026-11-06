// Custom element upgrades, the construction stack and the four lifecycle
// callbacks (HTML §4.13.5-4.13.6, docs/owners/DOCUMENT_MODEL.md 2026-10-21).
// Callbacks run at the same committed change points that run inserted scripts;
// a callback's exception is reported, never thrown into the mutating caller.
#include "document_model/custom_element_reactions.h"
#include "runtime/exception_capture.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
constexpr int kDepthLimit = 32;
}  // namespace
namespace reactions {
void Settle(State& s) {
  if (JS_IsExceptionPending(s.cx) && !JS_IsThrowingOutOfMemory(s.cx)) {
    CaptureOperationFailure(s.cx);
    JS_ClearPendingException(s.cx);
  }
}
bool Usable(State& s) {
  return s.customElements && !JS_IsExceptionPending(s.cx) && s.customElements->depth < kDepthLimit;
}
// Invokes a stored callback with `this` = the element; exceptions are reported.
void Invoke(State& s, NodeKey key, JS::HandleValue callback, JS::HandleValueArray args) {
  if (!callback.isObject()) return;
  JSAutoRealm realm(s.cx, s.page);
  JS::RootedValue element(s.cx), ignored(s.cx);
  if (!s.Wrap(key, &element) || !element.isObject()) return Settle(s);
  ++s.customElements->depth;
  if (!JS::Call(s.cx, element, callback, args, &ignored)) Settle(s);
  --s.customElements->depth;
}
CustomElementState* StateOf(State& s, NodeKey key) {
  auto found = s.customElements->states.find(key);
  return found == s.customElements->states.end() ? nullptr : &found->second;
}
bool Connected(State& s, NodeKey key) {
  NodeSnapshot node;
  return s.model.PeekElement(key, &node).code == ErrorCode::None && node.is_connected;
}
// The element's definition when it is an upgrade candidate of this realm.
CustomElementDefinition* Candidate(State& s, NodeKey key, NodeSnapshot* node) {
  if (s.model.PeekElement(key, node).code != ErrorCode::None || node->kind != NodeKind::Element ||
      !node->is_html_element || !node->local_name || node->owner_document.document != s.main.document ||
      node->owner_document.node != s.main.node)
    return nullptr;
  return FindDefinition(s, DomStringView(*node->local_name));
}
}  // namespace reactions
using namespace reactions;

// HTML "upgrade an element": construct with the element on the construction
// stack, then the attributeChanged reactions for present observed attributes,
// then connectedCallback when connected.
bool UpgradeElement(State& s, NodeKey key) {
  if (!Usable(s)) return true;
  NodeSnapshot node;
  CustomElementDefinition* definition = Candidate(s, key, &node);
  CustomElementState* current = StateOf(s, key);
  if (!definition || (current && current->kind != CustomElementState::Undefined)) return true;
  CustomElementState& state = s.customElements->states[key];
  state.kind = CustomElementState::Failed;  // until the constructor returns the element
  state.definition = definition;
  state.document = node.owner_document;
  JSAutoRealm realm(s.cx, s.page);
  JS::RootedValue element(s.cx), result(s.cx);
  JS::RootedValue constructor(s.cx, JS::ObjectValue(*definition->constructor->get()));
  JS::RootedObject created(s.cx);
  if (!s.Wrap(key, &element)) return false;
  s.customElements->constructing.push_back({key, definition});
  ++s.customElements->depth;
  const bool built = JS::Construct(s.cx, constructor, JS::HandleValueArray::empty(), &created);
  --s.customElements->depth;
  s.customElements->constructing.pop_back();
  if (!built) { Settle(s); return true; }
  if (created != &element.toObject()) {
    TypeError(s.cx, "Custom element constructor returned a different element");
    Settle(s);
    return true;
  }
  StateOf(s, key)->kind = CustomElementState::Custom;
  std::vector<AttributeSnapshot> attributes;
  if (definition->attributeChanged->isObject() && s.model.Attributes(key, &attributes).code == ErrorCode::None)
    for (const auto& attribute : attributes) {
      if (!attribute.namespace_uri.empty()) continue;
      bool observed = false;
      for (const auto& name : definition->observed) observed |= name == attribute.name;
      if (!observed) continue;
      JS::RootedValueArray<4> args(s.cx);
      if (!Value(s.cx, DomStringView(attribute.name), args[0]) ||
          !Value(s.cx, DomStringView(attribute.value), args[2])) return false;
      args[1].setNull();
      args[3].setNull();
      Invoke(s, key, *definition->attributeChanged, args);
    }
  if (node.is_connected) {
    StateOf(s, key)->connected = true;
    Invoke(s, key, *definition->connected, JS::HandleValueArray::empty());
  }
  return true;
}

bool UpgradeTree(State& s, NodeKey root, bool connectedOnly) {
  std::vector<NodeKey> keys;
  if (s.model.Subtree(root, &keys).code != ErrorCode::None) return true;
  keys.insert(keys.begin(), root);
  for (NodeKey key : keys) {
    if (connectedOnly && !Connected(s, key)) continue;
    if (!UpgradeElement(s, key)) return false;
  }
  return true;
}

// Inserted scopes: an element entering the document is a connected reaction (or
// an upgrade); a parser-created element (no wrapper yet) is upgraded wherever it
// lands. Removed scopes: custom elements leaving the document disconnect.
void CustomElementsTreeChanged(State& s, const std::vector<NodeKey>& added, const std::vector<NodeKey>& removed) {
  if (!s.customElements) return;
  if (s.model.IdentityTransferPending()) {  // replayed by FlushCustomElementChanges, like scripts
    auto& r = *s.customElements;
    r.deferredAdded.insert(r.deferredAdded.end(), added.begin(), added.end());
    r.deferredRemoved.insert(r.deferredRemoved.end(), removed.begin(), removed.end());
    return;
  }
  auto adopted = std::move(s.customElements->adopted);
  s.customElements->adopted.clear();
  for (const auto& [key, documents] : adopted) {
    CustomElementState* state = StateOf(s, key);
    if (!Usable(s) || !state || state->kind != CustomElementState::Custom) continue;
    JS::RootedValueArray<2> args(s.cx);
    JSAutoRealm realm(s.cx, s.page);
    if (!s.Wrap(documents.first, args[0]) || !s.Wrap(documents.second, args[1])) return Settle(s);
    Invoke(s, key, *state->definition->adopted, args);
  }
  for (NodeKey scope : removed) {
    std::vector<NodeKey> keys;
    if (s.model.Subtree(scope, &keys).code != ErrorCode::None) keys.clear();
    keys.insert(keys.begin(), scope);
    for (NodeKey key : keys) {
      CustomElementState* state = StateOf(s, key);
      if (!Usable(s) || !state || state->kind != CustomElementState::Custom || !state->connected ||
          Connected(s, key)) continue;
      state->connected = false;
      Invoke(s, key, *state->definition->disconnected, JS::HandleValueArray::empty());
    }
  }
  for (NodeKey scope : added) {
    std::vector<NodeKey> keys;
    if (s.model.Subtree(scope, &keys).code != ErrorCode::None) keys.clear();
    keys.insert(keys.begin(), scope);
    for (NodeKey key : keys) {
      if (!Usable(s)) return;
      CustomElementState* state = StateOf(s, key);
      const bool connected = Connected(s, key);
      if (state && state->kind == CustomElementState::Custom) {
        if (connected && !state->connected) {
          state->connected = true;
          Invoke(s, key, *state->definition->connected, JS::HandleValueArray::empty());
        }
        continue;
      }
      if (state || (!connected && s.Find(key))) continue;  // created by script, not yet in the document
      if (!UpgradeElement(s, key)) return Settle(s);
    }
  }
}

void FlushCustomElementChanges(State& s) {
  if (!s.customElements || s.model.IdentityTransferPending()) return;
  auto added = std::move(s.customElements->deferredAdded);
  auto removed = std::move(s.customElements->deferredRemoved);
  s.customElements->deferredAdded.clear();
  s.customElements->deferredRemoved.clear();
  if (!added.empty() || !removed.empty() || !s.customElements->adopted.empty())
    CustomElementsTreeChanged(s, added, removed);
}

}  // namespace ruyijs::document_model
