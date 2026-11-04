#include "document_model/state.h"
#include "runtime/weak_object.h"
#include "document_model/sanitizer.h"
#include "document_model/traversal.h"
#include "document_model/range.h"
#include "document_model/parsing.h"
#include "document_model/html_form.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/TracingAPI.h"
#include "js/Wrapper.h"
#include <cstring>
#include <new>
namespace ruyijs::document_model {
namespace {
// A collected node wrapper stops naming its node. The finalizer only marks the
// wrapper and raises the realm's reclaim flag: it may not allocate, call JSAPI
// or free the record. The next State::Wrap reclaims both the domain record (if
// the tree no longer holds the node) and the Interaction record (T41).
void FinalizeNode(JS::GCContext*, JSObject* object) {
  auto* wrapper = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (!wrapper) return;
  wrapper->dead = true;
  // Native Wrapper reclamation happens after sweeping. Clear its dead GC
  // edges now so later Heap destructors never barrier a swept collection.
  for (auto& collection : wrapper->collectionObjects)
    collection.unbarrieredSet(nullptr);
  State* owner = wrapper->owner;
  if (!owner) return;
  owner->reclaimPending = true;
  // The borrowed Interaction record stops resolving its target here, at the
  // same instant the wrapper dies: a dispatch between this collection and the
  // reclaim below must never hand the collected object to a listener (T41).
  // Slot 1 is the record AttachBorrowed returned, so this is one store instead
  // of a scan of the whole target registry for every collected node.
  if (wrapper->factory)
    wrapper->factory->interaction.ReleaseBorrowed(JS::GetReservedSlot(object, 1).toPrivate());
}
// A wrapper object's own slots name its native record, and the engine moves
// the object with its slots. The trace hook is therefore the one point where
// both the wrapper's cached identity and the borrowed Interaction record it
// names follow the object to its new address, so no live wrapper or target is
// ever left pointing at an abandoned copy (T41).
void TraceNode(JSTracer* tracer, JSObject* object) {
  auto* wrapper = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (!wrapper) return;
  wrapper->object = object;
  for (auto& collection : wrapper->collectionObjects)
    JS::TraceEdge(tracer, &collection, "DocumentModel node collection");
  State* owner = wrapper->owner;
  if (owner && wrapper->factory)
    wrapper->factory->interaction.RefreshBorrowed(JS::GetReservedSlot(object, 1).toPrivate(), object);
}
const JSClassOps kNodeOps = {nullptr, nullptr, nullptr, FormEnumerate, FormResolve, nullptr,
                             FinalizeNode, nullptr, nullptr, TraceNode};
}  // namespace
// Slot 0 names this wrapper's native record and slot 1 the borrowed Interaction
// record it must release; both are cleared before the records are freed.
const JSClass kNodeClass = {"Node",
    JSCLASS_HAS_RESERVED_SLOTS(13) | JSCLASS_FOREGROUND_FINALIZE, &kNodeOps};
State::State(JSContext* context, JS::HandleObject global, interaction::Realm& events,
             browser_environment::Environment& env, PageHelpers& trace)
    : cx(context), page(context, global), documentLocationGetter(context),
      documentLocationSetter(context),
      datasetPrototype(context), optionsIteratorValues(context), selection(context),
      functions(context, JS::GCVector<JSObject*>(context)),
      interaction(events), environment(env), helpers(trace),
      handlerAdd(context), handlerRemove(context), handlerPrevent(context), handlerInvokers(context) {}
State::~State() {
  OrphanImageRequests(*this);
  RetireFactoryWrappers();
  RetireNodeLifecycle();
  model.SetMutationSink(nullptr, nullptr);
  while (tokenViews) UnlinkTokenView(*tokenViews);
  // Cached live records belong to State, while JS objects are scope-owned.
  // Clear live objects' slots before freeing records; finalized objects have
  // already cleared their weak address and must never be touched here.
  while (collections) {
    auto current = std::move(collections);
    collections = std::move(current->next);
    if (current->object)
      JS::SetReservedSlot(current->object, 0, JS::PrivateValue(nullptr));
  }
  // The engine can still finalize these objects while the context is torn
  // down, so a cleared reserved slot turns every finalizer into a no-op before
  // the record it names is freed. A wrapper whose object the engine already
  // collected has no slots left to clear - its finalizer is what marked it,
  // and the next reclaim would have dropped it (T41) - so teardown must skip
  // it instead of writing through a dangling object pointer.
  for (Wrapper* entry = wrappers.get(); entry; entry = entry->next.get()) {
    if (entry->dead || !entry->object) continue;
    JS::SetReservedSlot(entry->object, 0, JS::PrivateValue(nullptr));
    JS::SetReservedSlot(entry->object, 1, JS::PrivateValue(nullptr));
  }
  for (ReflectedLink* entry = reflected.get(); entry; entry = entry->next.get()) {
    if (entry->style) JS::SetReservedSlot(entry->style, 2, JS::PrivateValue(nullptr));
    if (entry->dataset) JS::SetReservedSlot(entry->dataset, 2, JS::PrivateValue(nullptr));
    if (entry->animatedClass) JS::SetReservedSlot(entry->animatedClass, 2, JS::PrivateValue(nullptr));
  }
  // Linked caches are destroyed iteratively to bound the native stack.
  while (wrappers) {
    auto current = std::move(wrappers);
    wrappers = std::move(current->next);
  }
  while (interfaces) {
    auto current = std::move(interfaces);
    interfaces = std::move(current->next);
  }
  while (frames) {
    auto current = std::move(frames);
    frames = std::move(current->next);
  }
  while (reflected) {
    auto current = std::move(reflected);
    reflected = std::move(current->next);
  }
  while (walkers) {
    auto current = std::move(walkers);
    walkers = std::move(current->next);
  }
  RangeRetire(*this);
  while (ranges) {
    auto current = std::move(ranges);
    ranges = std::move(current->next);
  }
}
bool Same(NodeKey a, NodeKey b) { return a.document == b.document && a.node == b.node; }
Wrapper* State::Find(JSObject* object) const {
  if (!object) return nullptr;
  object = js::CheckedUnwrapStatic(object);
  for (auto* value = wrappers.get(); value; value = value->next.get())
    if (!value->dead && value->object == object) return value;
  return nullptr;
}
Wrapper* State::Find(NodeKey key) const {
  auto found = wrapperIndex.find(key);
  // A wrapper an incremental sweep already doomed is not this node's identity
  // any more: a fresh wrapper replaces it and its finalizer reaps the record.
  if (found == wrapperIndex.end() || found->second->dead || !WeakLive(found->second->object))
    return nullptr;
  return found->second;
}
void State::DropReflected(ReflectedLink* link) {
  if (!link) return;
  reflectedIndex.erase(link->key);
  auto* slot = &reflected;
  while (*slot) {
    if (slot->get() == link) {
      auto expired = std::move(*slot);
      *slot = std::move(expired->next);
      return;
    }
    slot = &(*slot)->next;
  }
}
ReflectedLink* State::Reflected(NodeKey key) {
  auto found = reflectedIndex.find(key);
  if (found != reflectedIndex.end()) {
    ReflectedLink* value = found->second;
    if (value->style || value->dataset || value->animatedClass) return value;
    DropReflected(value);
  }
  std::unique_ptr<ReflectedLink> entry(new (std::nothrow) ReflectedLink(cx, key));
  if (!entry) { JS_ReportOutOfMemory(cx); return nullptr; }
  ReflectedLink* raw = entry.get();
  entry->next = std::move(reflected);
  reflected = std::move(entry);
  reflectedIndex.emplace(key, raw);
  return raw;
}
Interface* State::Find(const char* name) const {
  for (auto* value = interfaces.get(); value; value = value->next.get())
    if (!strcmp(value->name, name)) return value;
  return nullptr;
}
Realm::Realm(JSContext* cx, JS::HandleObject page, interaction::Realm& events,
             browser_environment::Environment& env, PageHelpers& helpers)
    : state_(std::make_unique<State>(cx, page, events, env, helpers)) {
  state_->sourceRealm = this;
}
Realm::~Realm() = default;
static bool DocumentGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = FunctionState(args);
  if (!args.thisv().isObject() || &args.thisv().toObject() != state->page)
    return TypeError(cx, "Window receiver is incompatible");
  return state->Wrap(state->main, args.rval());
}
bool Realm::Install() {
  State& state = *state_;
  JSAutoRealm realm(state.cx, state.page);
  // 256 documents, 100000 live nodes and 16 MiB of text per page realm. The
  // node cap bounds tree memory for one page; it is a product limit, not a
  // spec value. Firefox has none; real pages (a 529 KB job-site home page
  // parses to ~12900 nodes) exceeded the former 9999.
  if (!state.Check(state.model.Initialize({256, 100000, 16 * 1024 * 1024})) ||
      !InstallInterfaces(state) || !InstallNodeFilter(state) ||
      !InstallTreeWalker(state) || !InstallNodeIterator(state) || !InstallRange(state) || !InstallParsing(state) ||
      !InstallException(state) ||
      !InstallNode(state) || !InstallElement(state) ||
      !InstallCharacterData(state) || !InstallProcessingInstruction(state) || !InstallDocument(state) ||
      !InstallDocumentPresentation(state) || !InstallPresentationInterfaces(state) || !InstallXsltProcessor(state) || !InstallSanitizer(state) ||
      !InstallIframe(state) || !InstallCollections(state) ||
      !InstallTokenList(state) || !InstallAttr(state) || !InstallAttributeMap(state) ||
      !InstallAttrNode(state) ||
      !InstallQuery(state) || !InstallStyleDeclaration(state) || !InstallStyleProperties(state) ||
      !InstallStringMap(state) || !InstallHTMLElement(state) || !InstallMetaElement(state) ||
      !InstallAnchorElement(state) || !InstallImageElement(state) ||
      !InstallAudioElement(state) ||
      !InstallMediaElement(state) ||
      !InstallTableElements(state) || !InstallSelectElements(state) ||
      !InstallButtonElement(state) || !InstallFormElements(state) || !InstallInputElement(state) ||
      !InstallTextAreaElement(state) || !InstallFormAux(state) || !InstallGaugeElements(state) ||
      !InstallHeadElements(state) || !InstallContentElements(state) || !InstallDisclosureElements(state) ||
      !InstallTemplateElement(state) || !InstallEmbeddedElements(state) ||
      !InstallBodyFrameset(state) ||
      !InstallCustomElements(state) || !InstallEventHandlers(state)) return false;
  // The wrapper lifetime follows tree membership; the same change point the
  // Model already reports to its consumers, with both sides of the edit.
  state.model.SetTreeSink(&NotifyTree, &state);
  state.model.SetReclaimSink(&ReclaimNow, &state);
  for (auto* entry = state.interfaces.get(); entry; entry = entry->next.get())
    if (!state.TraceInterface(entry->name)) return false;
  Draft draft;
  if (!state.Check(state.model.CreateDocument(DocumentInit::MainHtml, &draft))) return false;
  JS::RootedValue value(state.cx);
  if (!state.Commit(draft, &value)) return false;
  state.main = draft.root;
  JS::RootedObject getter(state.cx, state.Function(DocumentGetter, "get document", 0, 0));
  JS::RootedObject setter(state.cx);
  return getter && JS_DefineProperty(state.cx, state.page, "document", getter, setter,
                                    JSPROP_ENUMERATE | JSPROP_PERMANENT) &&
         state.helpers.Accessor("Window", state.page, "document");
}
JSObject* Realm::canvasPrototype() {
  Interface* entry = state_->Find("HTMLCanvasElement");
  return entry ? entry->prototype.get() : nullptr;
}
JSObject* Realm::shadowPrototype() {
  Interface* entry = state_->Find("ShadowRoot");
  return entry ? entry->prototype.get() : nullptr;
}
void Realm::Reclaim() { ReclaimNow(state_.get()); }
void Realm::SetApiMonitor(ApiMonitorPort port, void* context) {
  state_->apiMonitor = port;
  state_->apiMonitorContext = context;
}
void Realm::SetFileListPort(FileListPort port, void* context) {
  state_->fileList = port;
  state_->fileListContext = context;
}
void Realm::SetMediaKeysCheckPort(MediaKeysCheckPort port, void* context) {
  state_->mediaKeysCheck = port;
  state_->mediaKeysCheckContext = context;
}
void Realm::SetTextTrackPorts(TextTrackPorts ports) { state_->textTracks = ports; }
void Realm::SetStyleDeclarationPort(StyleDeclarationPort port) { state_->styleDeclarations = port; }
void Realm::SetOffsetGeometryPort(OffsetGeometryPort port, void* context) {
  state_->offsetGeometry = port;
  state_->offsetGeometryContext = context;
}
void Realm::SetElementScrollPort(const ElementScrollPort& port) {
  state_->elementScroll = port;
  state_->scrollPending.clear();
}
void Realm::SetImageLoadPort(ImageLoadPort port, void* context) {
  state_->imageLoad = port;
  state_->imageLoadContext = context;
}
void Realm::SetCanvasDimensionPort(CanvasDimensionPort port, void* context) {
  state_->canvasDimensions = port;
  state_->canvasDimensionsContext = context;
}
bool Realm::ImageLoadsPending() const { return state_->imageLoadBlockers != 0; }
bool Realm::RunAutofocus() { return document_model::RunAutofocus(*state_); }
void Realm::SetDocumentReferrer(const std::string& referrer) {
  // Referer values are ASCII (a serialized URL).
  state_->referrer.assign(referrer.begin(), referrer.end());
}
bool Realm::ActiveElement(JS::MutableHandleObject element) {
  DocumentSnapshot document;
  JS::RootedValue value(state_->cx);
  element.set(nullptr);
  if (!state_->Check(state_->model.InspectDocument(state_->main, &document))) return false;
  if (!document.active_element.node) return true;
  if (!state_->Wrap(document.active_element, &value)) return false;
  element.set(&value.toObject());
  return true;
}
void Realm::RetireImageLoads() {
  OrphanImageRequests(*state_, true);
  state_->imageLoad = nullptr;
  state_->imageLoadContext = nullptr;
}
void Realm::SetMutationRecording(bool enabled) { state_->model.SetMutationRecording(enabled); }
uint64_t Realm::MutationSequence() { return state_->model.MutationSequence(); }
uint64_t Realm::DroppedMutations() { return state_->model.DroppedMutations(); }
bool Realm::MutationsSince(uint64_t sequence, std::vector<MutationNotice>* result) {
  return state_->Check(state_->model.MutationsSince(sequence, result));
}
bool Realm::AncestorOrSelf(NodeKey ancestor, NodeKey node, bool* result) {
  return state_->Check(state_->model.AncestorOrSelf(ancestor, node, result));
}
bool Realm::WrapNode(NodeKey key, JS::MutableHandleObject result) {
  State& state = *state_;
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedValue value(state.cx);
  if (!state.Wrap(key, &value)) return false;
  // A released identity resolves to null, never to a fresh record: the observer
  // owner turns it into a dropped slot exactly like a null key.
  result.set(value.isObject() ? &value.toObject() : nullptr);
  return true;
}
bool Realm::NodeListOf(const std::vector<NodeKey>& nodes, JS::MutableHandleObject result, Realm* factory) {
  State& state = *state_;
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedObject list(state.cx);
  if (!state.NodeListOwned(nodes, &list, CollectionKind::NodeList,
                           factory ? factory->state_.get() : nullptr)) return false;
  result.set(list);
  return true;
}
bool Realm::NodeIdentity(JS::HandleValue value, NodeKey* result) {
  State& state = *state_;
  JSAutoRealm realm(state.cx, state.page);
  return NodeArgument(state, value, result);
}
bool Realm::ThrowTypeError(const char* message) {
  State& state = *state_;
  JSAutoRealm realm(state.cx, state.page);
  return TypeError(state.cx, message);
}
}
namespace ruyijs::document_model {
bool Realm::ConfigureExposure() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.page);
  JS::RootedValue secure(s.cx);
  return JS_GetProperty(s.cx, s.page, "isSecureContext", &secure) && s.secure.Apply(secure.isTrue());
}
}  // namespace ruyijs::document_model
