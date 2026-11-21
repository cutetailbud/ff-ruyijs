#include "document_model/iframe_attributes.h"
#include "document_model/state.h"
#include "browser_environment/environment.h"
#include "js/Wrapper.h"
#include <cstring>
#include <memory>
#include <new>
#include <vector>

namespace ruyijs::document_model {
namespace {
// The realm's single element -> browsing-context link list. A link exists while
// the element is connected AND the runtime assembled its child context: this
// owner keeps only the identity mapping the getters read, never a tree or a
// second connectivity flag.
FrameLink* FindFrame(State& state, NodeKey key) {
  for (FrameLink* link = state.frames.get(); link; link = link->next.get())
    if (Same(link->frame, key)) return link;
  return nullptr;
}
bool RegisterFrame(State& state, NodeKey key, JS::HandleObject window,
                   JS::HandleObject document) {
  std::unique_ptr<FrameLink> created(
      new (std::nothrow) FrameLink(state.cx, key, window, document));
  if (!created) { JS_ReportOutOfMemory(state.cx); return false; }
  created->next = std::move(state.frames);
  state.frames = std::move(created);
  return true;
}
// The child browsing context's name follows the element's `name` content
// attribute (absent: the empty string), at creation and on every change, as
// Firefox's frame loader names it; window.name of the child reads it.
bool SyncFrameName(State& state, NodeKey key, JS::HandleObject window) {
  std::optional<DomString> value;
  if (!state.Check(state.model.GetAttribute(key, u"name", &value))) return false;
  const DomString text = value ? *value : DomString();
  JS::RootedString name(state.cx, JS_NewUCStringCopyN(state.cx, text.data(), text.size()));
  return name && browser_environment::Environment::SetBrowsingContextName(state.cx, window, name);
}
void DropFrame(State& state, NodeKey key) {
  std::unique_ptr<FrameLink>* slot = &state.frames;
  while (*slot && !Same((*slot)->frame, key)) slot = &(*slot)->next;
  if (*slot) *slot = std::move((*slot)->next);
}
// Reads of the unique tree that this owner needs for the lifecycle. They go
// through Model, so there is no second index and no cached connectivity.
bool Connected(State& state, NodeKey key, bool* result) {
  NodeSnapshot snapshot;
  *result = false;
  if (state.model.Inspect(key, &snapshot).code != ErrorCode::None) return false;
  *result = snapshot.is_connected;
  return true;
}
// Read the unique ownership subtree, including closed/nested shadow roots.
// Public DOM queries exclude those roots and miss frames when a host connects.
bool IframesIn(State& state, NodeKey scope, std::vector<NodeKey>* result) {
  std::vector<NodeKey> descendants;
  Status status = state.model.Subtree(scope, &descendants);
  if (status.code != ErrorCode::None) return state.Check(status);
  for (NodeKey key : descendants) {
    NodeSnapshot snapshot;
    status = state.model.Inspect(key, &snapshot);
    if (status.code != ErrorCode::None) return state.Check(status);
    if (snapshot.kind == NodeKind::Element &&
        !std::strcmp(snapshot.interface_name, "HTMLIFrameElement")) result->push_back(key);
  }
  return true;
}
// The Document Model's structural-change notice for this realm. The runtime
// port assembles exactly one child browsing context per connected iframe of the
// main document, at the change that connected it, and retires the context of an
// element that left the tree. The first load runs only after the link exists,
// so child scripts and the element's `load` listeners already read
// contentWindow/contentDocument. No policy lives here: creation, loading, the
// product bound and the retirement lifetime all belong to the runtime owner.
void Lifecycle(void* context, DocumentId document, const std::vector<NodeKey>& inserted) {
  State& state = *static_cast<State*>(context);
  if (document != state.main.document) return;
  // A meta-delivered CSP applies before any later script of the document runs.
  SecurityMetaInserted(state, inserted);
  // Content-attribute handlers of inserted (parser-created or moved) elements.
  EventHandlerAttributesInserted(state, inserted);
  std::vector<NodeKey> gone;
  for (FrameLink* link = state.frames.get(); link; link = link->next.get()) {
    if (link->frame.document != document) continue;
    bool connected = false;
    if (!Connected(state, link->frame, &connected) || !connected) gone.push_back(link->frame);
  }
  for (NodeKey key : gone) {
    if (state.frameDiscard) state.frameDiscard(state.frameContext, key);
    DropFrame(state, key);
  }
  if (!state.frameCreate) return;
  // Transfer commits before the original wrappers receive their new keys.
  // Defer discovery until Rekey so frameElement keeps the caller's object.
  if (state.model.IdentityTransferPending()) {
    state.deferredFrameScopes.insert(state.deferredFrameScopes.end(), inserted.begin(), inserted.end());
    return;
  }
  for (NodeKey scope : inserted) {
    if (scope.document != document) continue;
    std::vector<NodeKey> candidates;
    if (!IframesIn(state, scope, &candidates)) return;
    for (NodeKey key : candidates) {
      bool connected = false;
      if (!Connected(state, key, &connected) || !connected) continue;
      if (FindFrame(state, key)) continue;
      JS::RootedValue value(state.cx);
      if (!state.Wrap(key, &value)) return;
      if (!value.isObject()) continue;
      JS::RootedObject element(state.cx, &value.toObject());
      JS::RootedObject window(state.cx), content(state.cx);
      if (!state.frameCreate(state.frameContext, element, key, &window, &content)) {
        return;  // A pending exception carries the reason (for example the bound).
      }
      if (!RegisterFrame(state, key, window, content) || !SyncFrameName(state, key, window)) return;
      if (state.frameNavigate && !state.frameNavigate(state.frameContext, element, key, true))
        return;
    }
  }
}
// The attribute-change notice of this realm's main document. An element that
// already owns a live frame context and whose `src` or `srcdoc` changed hands
// that element to the runtime frame owner, which reads both values through
// Attribute and runs the one page-load coordinator; no URL or precedence
// policy lives here. An element without a link is ignored: the structure
// notice runs the first load itself once the context is built.
void SourceChanged(void* context, DocumentId document, NodeKey key, DomStringView name,
                   DomStringView namespace_uri, const DomString* old) {
  State& state = *static_cast<State*>(context);
  EventHandlerAttributeChanged(state, key, name);
  if (name == DomStringView(u"src")) QueueImageSource(state, key);
  if (namespace_uri.empty()) CanvasDimensionChanged(state, key, name);
  if (namespace_uri.empty()) CustomElementAttributeChanged(state, key, name, old);
  if (document != state.main.document) return;
  if (name == DomStringView(u"name") && namespace_uri.empty()) {
    if (FrameLink* link = FindFrame(state, key)) {
      JS::RootedObject window(state.cx, link->window);
      SyncFrameName(state, key, window);
    }
    return;
  }
  if (name != DomStringView(u"src") && name != DomStringView(u"srcdoc")) return;
  if (!state.frameNavigate || !FindFrame(state, key)) return;
  JS::RootedValue value(state.cx);
  if (!state.Wrap(key, &value) || !value.isObject()) return;
  JS::RootedObject element(state.cx, &value.toObject());
  state.frameNavigate(state.frameContext, element, key, false);
}
// contentWindow/contentDocument of a connected iframe of the main document. The
// link is created by the structure notice, so a read never assembles a context
// as a side effect.
static bool Frame(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const bool content = FunctionSlot(args) == 1;  // Read before rval() replaces the callee.
  Wrapper* entry = NodeReceiver(cx, args.thisv());
  if (!entry) return false;
  State& state = *entry->owner;
  NodeSnapshot node;
  if (!state.Check(state.model.Inspect(entry->key, &node))) return false;
  if (strcmp(node.interface_name, "HTMLIFrameElement"))
    return TypeError(cx, "HTMLIFrameElement member called on an incompatible receiver");
  args.rval().setNull();
  if (!node.is_connected || entry->key.document != state.main.document) return true;
  FrameLink* link = FindFrame(state, entry->key);
  if (!link) return true;
  // HTML: contentDocument is null and contentWindow a cross-origin WindowProxy
  // unless the caller is same origin with the frame's document (Browser Environment).
  JS::RootedObject window(cx, link->window);
  if (!content) return browser_environment::Environment::WindowView(cx, window, args.rval());
  bool same = true;
  if (!browser_environment::Environment::SameOriginWithCurrent(cx, window, &same)) return false;
  if (!same) return true;
  args.rval().setObject(*link->document);
  return JS_WrapValue(cx, args.rval());
}
}  // namespace
void State::FlushFrameLifecycle() {
  auto scopes = std::move(deferredFrameScopes);
  deferredFrameScopes.clear();
  if (!scopes.empty()) Lifecycle(this, main.document, scopes);
}
void State::DiscardFramesIn(const std::vector<NodeKey>& removed) {
  // replace-all removes and reinserts even a surviving iframe. Retire its old
  // browsing context through the original lifecycle port before reconnection.
  for (NodeKey key : removed) {
    if (!FindFrame(*this, key)) continue;
    if (frameDiscard) frameDiscard(frameContext, key);
    DropFrame(*this, key);
  }
}
bool InstallIframe(State& state) {
  // Bound before any markup reaches this realm, so an iframe that exists in the
  // tree always has its context before script can read contentWindow.
  state.model.SetStructureSink(&Lifecycle, &state);
  // The same change points drive an iframe `src`/`srcdoc` navigation: the
  // structure sink builds the context, the attribute sink navigates the built
  // one.
  state.model.SetAttributeSink(&SourceChanged, &state);
  // The two reflected content attributes are installed by their own TU; the
  // identity accessors stay with the link table this file owns.
  return InstallIframeAttributes(state) &&
         state.Accessor("HTMLIFrameElement", "contentWindow", Frame, 0) &&
         state.Accessor("HTMLIFrameElement", "contentDocument", Frame, 1);
}
size_t Realm::IframeCount() {
  std::vector<NodeKey> keys;
  State& state = *state_;
  if (state.model.Query(state.main, QueryKind::TagName, u"iframe", &keys).code !=
      ErrorCode::None) return 0;
  return keys.size();
}
bool Realm::FrameWindowAt(size_t index, JS::MutableHandleValue result) {
  result.setUndefined();
  State& state = *state_;
  std::vector<NodeKey> keys;
  if (state.model.Query(state.main, QueryKind::TagName, u"iframe", &keys).code != ErrorCode::None ||
      index >= keys.size()) return true;
  FrameLink* link = FindFrame(state, keys[index]);
  if (!link || !link->window) return true;
  JS::RootedObject window(state.cx, link->window);
  return browser_environment::Environment::WindowView(state.cx, window, result);
}
void Realm::SetFrameLifecycle(FrameCreate create, FrameDiscard discard, FrameNavigate navigate,
                              void* context) {
  state_->frameCreate = create;
  state_->frameDiscard = discard;
  state_->frameNavigate = navigate;
  state_->frameContext = context;
}
}  // namespace ruyijs::document_model
