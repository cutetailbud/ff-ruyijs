#ifndef RUYIJS_DOCUMENT_STATE_H
#define RUYIJS_DOCUMENT_STATE_H
#include "runtime/secure_slot.h"
#include "document_model/realm.h"
#include "document_model/model.h"
#include "document_model/collections.h"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "jsapi.h"
#include "js/CallArgs.h"
namespace ruyijs::document_model {
constexpr uint32_t kNodeStyleAssociationSlot = 8;
constexpr uint32_t kNodeAdoptedStylesSlot = 11;
constexpr uint32_t kNodeShadowAssociationSlot = 9;
constexpr uint32_t kNodeShadowScopeSlot = 10;
constexpr uint32_t kNodeFragmentDirectiveSlot = 12;
struct Walker;
struct Range;
struct State;
// The registry is a realm-owned DOM service.  Definitions root the
// user-supplied constructors for the lifetime of the realm; the DOM tree and
// element wrappers remain the only node identity source.
struct NodeKeyLess {
  bool operator()(const NodeKey& a, const NodeKey& b) const {
    return a.document != b.document ? a.document < b.document : a.node < b.node;
  }
};
struct NodeKeyHash {
  size_t operator()(const NodeKey& key) const {
    return std::hash<uint64_t>{}(key.document) ^ (std::hash<uint64_t>{}(key.node) << 1);
  }
};
struct NodeKeyEqual {
  bool operator()(const NodeKey& a, const NodeKey& b) const {
    return a.document == b.document && a.node == b.node;
  }
};
struct CustomElementDefinition {
  std::u16string name;
  std::unique_ptr<JS::PersistentRootedObject> constructor;
  // Read once by define() in Firefox's order (custom_element_reactions.cpp).
  std::unique_ptr<JS::PersistentRootedObject> prototype;
  std::unique_ptr<JS::PersistentRootedValue> connected, disconnected, adopted, attributeChanged;
  std::vector<std::u16string> observed;
  bool formAssociated = false, disableInternals = false, disableShadow = false;
  CustomElementDefinition(JSContext* cx, std::u16string value, JS::HandleObject ctor)
      : name(std::move(value)),
        constructor(std::make_unique<JS::PersistentRootedObject>(cx, ctor)),
        prototype(std::make_unique<JS::PersistentRootedObject>(cx)),
        connected(std::make_unique<JS::PersistentRootedValue>(cx)),
        disconnected(std::make_unique<JS::PersistentRootedValue>(cx)),
        adopted(std::make_unique<JS::PersistentRootedValue>(cx)),
        attributeChanged(std::make_unique<JS::PersistentRootedValue>(cx)) {}
};
// One element's custom element state (HTML "custom element state").
struct CustomElementState {
  enum Kind { Undefined, Failed, Custom } kind = Undefined;
  CustomElementDefinition* definition = nullptr;
  bool connected = false;  // last connectedness the callbacks reported
  NodeKey document;        // owner document, for adoptedCallback(old, new)
};
struct CustomElementPending {
  std::u16string name;
  std::unique_ptr<JS::PersistentRootedObject> promise;
  CustomElementPending(JSContext* cx, std::u16string value, JS::HandleObject pending)
      : name(std::move(value)),
        promise(std::make_unique<JS::PersistentRootedObject>(cx, pending)) {}
};
struct CustomElementsRegistry {
  JS::PersistentRootedObject object;
  JS::PersistentRootedObject internals;  // WeakMap element -> ElementInternals
  // The construction stack: the element an upgrade is constructing, if any.
  std::vector<std::pair<NodeKey, CustomElementDefinition*>> constructing;
  std::map<NodeKey, CustomElementState, NodeKeyLess> states;
  std::vector<std::pair<NodeKey, std::pair<NodeKey, NodeKey>>> adopted;  // element, old/new document
  std::vector<NodeKey> deferredAdded, deferredRemoved;  // scopes seen during an identity transfer
  int depth = 0;
  std::vector<std::unique_ptr<CustomElementDefinition>> definitions;
  std::vector<std::unique_ptr<CustomElementPending>> pending;
  explicit CustomElementsRegistry(JSContext* cx) : object(cx), internals(cx) {}
};
// One cached node wrapper. `object` is this realm's one JS object for `key`.
// The unique tree owns a connected node, so its wrapper is a strong root and
// is never reaped; a detached node's wrapper is not rooted, so it lives only
// while script references it (T41). The class finalizer marks a collected
// wrapper dead; `dead` also keeps Find() from matching a recycled address.
// `listed` is true only while this record is linked from its owner's
// `wrappers` list, so Find can trust the object's slot 0 and still miss a
// record that exists but is not on that list yet.
struct ImageRequest;
struct SvgViewCache;
struct Wrapper {
  NodeKey key;
  JSObject* object = nullptr;
  State* owner = nullptr;
  bool listed = false;
  State* factory = nullptr;
  Wrapper* factoryPrevious = nullptr;
  Wrapper* factoryNext = nullptr;
  JS::PersistentRootedObject root;
  JS::PersistentRootedValue mediaKeys;
  // Same-object collection edges are owned by this node's GC trace, not State.
  std::vector<JS::Heap<JSObject*>> collectionObjects;
  std::unique_ptr<SvgViewCache> svgViews;
  bool rooted = false;
  bool dead = false;
  bool formSubmitting = false, formConstructing = false, formResetting = false, clickInProgress = false;
  uint64_t formNavigationVersion = 0;
  uint32_t imageGeneration = 0;
  bool imageComplete = true;
  uint32_t imageNaturalWidth = 0;
  uint32_t imageNaturalHeight = 0;
  std::string imageMime;
  std::string imageBytes;
  std::unique_ptr<Wrapper> next;
  Wrapper(JSContext* cx, NodeKey id, JSObject* value);
  ~Wrapper();
  void LinkFactory(State&);
  // The tree holds the node: the wrapper must outlive every collection. The
  // persistent root's own `initialized()` only reports that it is registered
  // with the engine's root list (a `(cx)` root is registered but empty), so the
  // wrapper tracks the real state itself. Re-registering after a reset is what
  // keeps a reconnected node rooted rather than silently unrooted.
  void Root(JSContext* cx, bool connected) {
    if (connected == rooted) return;
    if (connected) {
      root.reset();
      root.init(cx, object);
    } else {
      root.reset();
    }
    rooted = connected;
  }
};
struct Interface {
  const char* name;
  JS::PersistentRootedObject prototype, constructor;
  std::unique_ptr<Interface> next;
  Interface(JSContext* cx, const char* label)
      : name(label), prototype(cx), constructor(cx) {}
};
// An iframe element's browsing context, assembled by the runtime frame port.
struct FrameLink {
  NodeKey frame;
  JS::PersistentRootedObject window, document;
  std::unique_ptr<FrameLink> next;
  FrameLink(JSContext* cx, NodeKey key, JS::HandleObject view, JS::HandleObject content)
      : frame(key), window(cx, view), document(cx, content) {}
};
// One element's reflected facade objects (inline style, dataset). The realm
// keeps the single instance per element; the facade slots hold the element and
// owner. The facade holds the element object, so it must not be rooted for the
// realm: each facade clears its slot here when it is collected (T41).
struct ReflectedLink {
  NodeKey key;
  JSObject* style = nullptr;
  JSObject* dataset = nullptr;
  JSObject* animatedClass = nullptr;
  std::unique_ptr<ReflectedLink> next;
  ReflectedLink(JSContext*, NodeKey id) : key(id) {}
};
struct State {
  Realm* sourceRealm = nullptr;  // The containing Realm; no independent lifetime.
  JSContext* cx;
  JS::PersistentRootedObject page, documentLocationGetter, documentLocationSetter,
      datasetPrototype, optionsIteratorValues;
  JS::PersistentRootedObject selection;  // Window.getSelection empty singleton per realm
  // HTMLInputElement.files: element -> FileList (html_input_files.cpp); the
  // FileList factory belongs to the Web Platform owner.
  JS::PersistentRootedObject inputFiles{cx};
  // Document -> {last, sets} of the presentation members (document_presentation.cpp).
  JS::PersistentRootedObject presentation{cx};
  Realm::FileListPort fileList = nullptr;
  void* fileListContext = nullptr;
  JS::PersistentRooted<JS::GCVector<JSObject*>> functions;
  std::unique_ptr<CustomElementsRegistry> customElements;
  interaction::Realm& interaction;
  browser_environment::Environment& environment;
  PageHelpers& helpers;
  Model model;
  NodeKey main;
  // The window document's referrer: the Referer of the navigation that
  // loaded it (runtime page_load / page_navigation); other documents report "".
  std::u16string referrer;
  // The original mutation consumer reads the Model log only after wrapper
  // identities have been published. No notices or node identities live here.
  void (*mutationConsumer)(void*) = nullptr;
  void* mutationConsumerContext = nullptr;
  bool mutationNotificationPending = false, publishingMutation = false;
  void NotifyMutation();
  void FlushMutationNotification();
  ScriptPort scriptPort = nullptr;
  FormDataPort formDataPort = nullptr;
  FormNavigatePort formNavigatePort = nullptr;
  void* formContext = nullptr;
  void* scriptContext = nullptr;
  uint64_t scriptGeneration = 0;
  uint32_t scriptDepth = 0;
  std::vector<NodeKey> deferredScriptScopes;
  void FlushScriptChanges();
  NodeLifecycleSink nodeLifecycleConsumer = nullptr;
  void* nodeLifecycleContext = nullptr;
  void NotifyNodeLifecycle(const NodeLifecycleNotice&) noexcept;
  void RetireNodeLifecycle() noexcept;
  FrameCreate frameCreate = nullptr;
  FrameDiscard frameDiscard = nullptr;
  FrameNavigate frameNavigate = nullptr;
  void* frameContext = nullptr;
  ApiMonitorPort apiMonitor = nullptr;
  void* apiMonitorContext = nullptr;
  StyleElementPort styleElement = nullptr;
  StyleDocumentListPort styleDocumentList = nullptr;
  StyleDocumentListPort adoptedStylesList = nullptr;
  void* adoptedStylesContext = nullptr;
  void* styleDocumentContext = nullptr;
  void* styleContext = nullptr;
  StyleNotifyPort styleNotify = nullptr;
  void* styleNotifyContext = nullptr;
  // EME validation belongs to web_media; attachment identity remains in this
  // Document Model wrapper. The callback is bound after both owners install.
  MediaKeysCheckPort mediaKeysCheck = nullptr;
  void* mediaKeysCheckContext = nullptr;
  // TextTrackList/TextTrack objects belong to web_media; which list belongs to
  // which element stays in this wrapper's media map.
  TextTrackPorts textTracks;
  // Declaration blocks Styling stores (rule and computed style) behind the
  // one CSSStyleDeclaration/CSSStyleProperties prototype of this owner.
  StyleDeclarationPort styleDeclarations;
  ImageLoadPort imageLoad = nullptr;
  void* imageLoadContext = nullptr;
  CanvasDimensionPort canvasDimensions = nullptr;
  void* canvasDimensionsContext = nullptr;
  OffsetGeometryPort offsetGeometry = nullptr;
  void* offsetGeometryContext = nullptr;
  ElementScrollPort elementScroll;
  // Element scroll containers with a queued scroll notification: one per
  // container until it is delivered (scroll events coalesce per task).
  std::set<NodeKey, NodeKeyLess> scrollPending;
  // Content Security owner ports (content_security_ports.cpp): policy
  // delivery from a <meta http-equiv> in <head> and the event handler
  // content-attribute check. The policy itself lives in web_security.
  CspMetaPort cspMeta = nullptr;
  CspInlinePort cspHandler = nullptr;
  void* cspContext = nullptr;
  // In-flight image fetches of this realm (html_image_load.cpp); ~State orphans
  // them. The blocker count covers those whose node document is `main`.
  std::vector<ImageRequest*> imageRequests;
  std::vector<NodeKey> pendingImageKeys;
  uint32_t imageLoadBlockers = 0;
  // Reports one access to an install-registered member of this owner.
  void Observe(const char* iface, const char* member, int type) const {
    if (apiMonitor) apiMonitor(apiMonitorContext, iface, member, type);
  }
  // Event handler attributes: the original EventTarget/Event natives saved at
  // install time and one invoker per event type (event_handlers.cpp).
  JS::PersistentRootedObject handlerAdd, handlerRemove, handlerPrevent, handlerInvokers;
  // [SecureContext] members (onpointerrawupdate on the handler interfaces).
  SecureSlots secure{cx};
  std::unique_ptr<FrameLink> frames;
  std::vector<NodeKey> deferredFrameScopes; // Unpublished insertions, cleared after Rekey.
  void FlushFrameLifecycle();
  void DiscardFramesIn(const std::vector<NodeKey>& removed);
  std::unique_ptr<ReflectedLink> reflected;
  std::unordered_map<NodeKey, ReflectedLink*, NodeKeyHash, NodeKeyEqual> reflectedIndex;
  void DropReflected(ReflectedLink* link);
  std::unique_ptr<Wrapper> wrappers;
  Wrapper* factoryWrappers = nullptr;  // Borrowed original factory lifetime links.
  // Key lookup over the same wrapper list; never a second identity source.
  std::map<NodeKey, Wrapper*, NodeKeyLess> wrapperIndex;
  // Reclaim worklist, not an identity source: the roots of the detached
  // subtrees this realm may still release (T41). A node that never had a
  // wrapper is unreachable by definition and is only findable through the
  // change that detached it, so the walk starts from these roots.
  std::map<NodeKey, bool, NodeKeyLess> detached;
  // Set by a wrapper finalizer; the next safe point reclaims the records.
  bool reclaimPending = false;
  std::unique_ptr<Interface> interfaces;
  // Live collection objects. Records are rebuilt from the owner
  // revision, never from a second tree or a JS-side value cache.
  std::unique_ptr<Collection> collections;
  CollectionRecord* tokenViews = nullptr;  // Weak intrusive links; JS owns records.
  // Traversal objects (TreeWalker/NodeIterator), rooted for the realm life.
  std::unique_ptr<Walker> walkers;
  // Live ranges (document.createRange), realm-owned for the same reason.
  std::unique_ptr<Range> ranges;
  // Other realms' ranges whose boundaries are in this tree (Range::tree).
  std::vector<Range*> guests;
  bool syncing = false;
  uint64_t syncedRevision = 0;
  // The style sources of a scope as last read (style_source.cpp): a pure
  // function of the tree, the main document and the document URL, reused while
  // none changed (layout reads them on every geometry query).
  struct StyleSourceMemo {
    NodeKey scope, main;
    uint64_t revision = 0;
    std::string url;
    std::vector<StyleSheetSource> sources;
  };
  std::vector<StyleSourceMemo> styleSourceMemo;
  State(JSContext*, JS::HandleObject, interaction::Realm&,
        browser_environment::Environment&, PageHelpers&);
  ~State();
  Wrapper* Find(JSObject*) const;
  Wrapper* Find(NodeKey) const;
  Interface* Find(const char*) const;
  // T41 wrapper lifetime. The class finalizer only marks the wrapper dead and
  // raises reclaimPending; ReclaimPending runs at the next Wrap, where the
  // engine is quiescent and the owner may allocate and call JSAPI.
  void ReclaimPending();
  // The tree-membership half: after a change the wrapper of every node below
  // `scope` follows whether the Document still holds that node, so a subtree
  // inserted into a detached parent stays unrooted (T41).
  void SyncMembership(NodeKey scope, bool retireStyle = true);
  std::vector<NodeKey> deferredStyleScopes; // Unpublished notifications, never DOM/CSS state.
  bool Pinned(NodeKey scope) const;
  // Releases one detached tree (its root) when no live wrapper reaches any node
  // of it. One tree is one decision, because parentNode from any live wrapper
  // reaches every other node of the same detached root.
  void ReleaseDetached(NodeKey root, bool connected);
  Collection* FindCollection(CollectionKind, NodeKey, CollectionFilter, DomStringView,
                             bool live = true) const;
  ReflectedLink* Reflected(NodeKey);
  Collection* NewCollection(CollectionKind, NodeKey, CollectionFilter, DomStringView, bool live);
  // A static NodeList over explicit node keys (a MutationRecord's addedNodes
  // and removedNodes). The record is owned by the object and freed by the
  // collection class finalizer.
  bool NodeListOwned(const std::vector<NodeKey>&, JS::MutableHandleObject,
                     CollectionKind kind = CollectionKind::NodeList, State* factory = nullptr,
                     bool defer = false);
  bool TokenListOwned(Wrapper&, DomStringView attribute, JS::MutableHandleValue);
  bool SyncTokenViews(uint64_t revision);
  bool SyncCollections();
  bool Check(Status);
  // Repoints cached wrappers, reflected facades, frame links and collection keys
  // after an adoption changed a subtree's document identity in place. Runs before
  // the revision-driven collection sync so a live collection resolves the
  // caller's existing objects instead of fresh duplicates.
  void Rekey(const std::vector<std::pair<NodeKey, NodeKey>>& pairs);
  void TakeAdoptedIdentities(State&, const std::vector<std::pair<NodeKey, NodeKey>>&);
  void FinishAdoptionPublication();
  void RetireFactoryWrappers();
  bool Wrap(NodeKey, JS::MutableHandleValue, const JS::CallArgs* = nullptr, State* factory = nullptr);
  bool WrapLocal(State&, NodeKey, JS::MutableHandleValue, const JS::CallArgs*);
  bool Commit(Draft, JS::MutableHandleValue, const JS::CallArgs* = nullptr, State* factory = nullptr);
  JSObject* Function(JSNative, const char*, unsigned, int32_t, unsigned = 0);
  bool Method(const char*, const char*, JSNative, unsigned, int32_t);
  bool Accessor(const char*, const char*, JSNative, int32_t, JSNative = nullptr);
  bool DefineInterface(const char*, JS::HandleObject, int32_t, bool inherit_constructor = true);
  bool TraceInterface(const char*);
};
extern const JSClass kNodeClass, kExceptionClass;
Wrapper* ShadowReceiver(JSContext*, JS::HandleValue);
bool ReadShadowOptions(JSContext*, JS::HandleValue, ShadowOptions*);
bool CacheShadowNode(State&, const NodeSnapshot&, JS::HandleObject);
bool RefreshShadowScope(State&, NodeKey, JS::HandleObject);
Wrapper* NodeReceiver(JSContext*, JS::HandleValue, const JS::CallArgs* args = nullptr);
Wrapper* NodeReceiver(JSContext*, JS::HandleValue, NodeKind);
bool InstallDocumentType(State&);
bool CreateDocumentType(JSContext*, unsigned, JS::Value*);
bool CreateXMLDocument(JSContext*, unsigned, JS::Value*);
bool PublicDocumentString(JSContext*, JS::HandleValue, DomString*);
bool ParsedDocumentTypeGetter(JSContext*, unsigned, JS::Value*);
bool CacheParsedDocumentType(State&, NodeKey, NodeKind, JS::HandleObject);
Wrapper* ElementReceiver(JSContext*, JS::HandleValue);
bool InstallElementHTML(State&);
bool InstallDocumentNodes(State&);
bool Same(NodeKey, NodeKey);
// The unique tree-change notice this owner binds on Model; it carries both the
// keys that entered and the keys that left the tree (T41).
void NotifyTree(void*, DocumentId, const std::vector<NodeKey>&, const std::vector<NodeKey>&);
// The node-cap collection request this owner binds on Model (T41).
void ReclaimNow(void*);
State* FunctionState(const JS::CallArgs&);
int32_t FunctionSlot(const JS::CallArgs&);
Wrapper* Receiver(State&, const JS::CallArgs&, NodeKind* = nullptr);
Wrapper* DocumentReceiver(JSContext*, JS::HandleObject, const JS::CallArgs* args = nullptr);
bool DocumentImport(JSContext*, unsigned, JS::Value*);
bool ClearDocument(Wrapper&);
bool NodeArgument(State&, JS::HandleValue, NodeKey*, bool nullable = false);
bool String(JSContext*, JS::HandleValue, DomString*, bool null_empty = false);
bool Value(JSContext*, DomStringView, JS::MutableHandleValue);
bool Value(JSContext*, const std::optional<DomString>&, JS::MutableHandleValue);
// A reflected URL content attribute of `key` (url_reflection.cpp).
bool ReflectUrl(State&, NodeKey key, const std::optional<DomString>& raw, JS::MutableHandleValue);
bool TypeError(JSContext*, const char*);
bool RangeError(JSContext*, const char*);
bool StandardError(JSContext*, const char*);
bool ThrowDOM(State&, const char*, const char*);
bool InstallException(State&);
bool InstallNode(State&);
bool InstallElement(State&);
bool InstallElementARIA(State&);
bool InstallElementView(State&);
bool InstallHTMLPresentation(State&);
bool InstallShadowDOM(State&);
bool InstallShadowRootMembers(State&);
bool InstallCharacterData(State&);
bool InstallTextMembers(State&);
bool InstallProcessingInstruction(State&);
bool InstallDocumentPresentation(State&);
bool InstallPresentationInterfaces(State&);
bool InstallXsltProcessor(State&);
bool NewXsltProcessor(State&, const JS::CallArgs&);
bool InstallParentMembers(State&);
bool InstallDocument(State&);
bool CreateElementNS(JSContext*, unsigned, JS::Value*);
bool InstallForeignElementInterfaces(State&);
bool InstallForeignElementMembers(State&);
bool InstallAnimatedString(State&);
bool AnimatedClassObject(Wrapper&, JS::MutableHandleValue);
bool InstallDocumentCreateEvent(State&);
bool InstallDocumentView(State&);
bool InstallDocumentStream(State&);
bool InstallDocumentMetadata(State&);
bool InstallDocumentCommands(State&);
bool InstallIframe(State&);
bool InstallInterfaces(State&);
bool InstallStyleDeclaration(State&);
bool InstallStyleProperties(State&);
bool InstallStringMap(State&);
bool InstallHTMLElement(State&);
bool InstallCustomElements(State&);
// Custom element upgrades and reactions (custom_element_reactions.cpp).
CustomElementDefinition* FindDefinition(State&, DomStringView name);
CustomElementDefinition* DefinitionFor(State&, JSObject* constructor);
bool ReadDefinition(State&, CustomElementDefinition&);  // define(): prototype, callbacks, statics
bool ConstructCustomElement(State&, const JS::CallArgs& args);  // HTMLElement [[Construct]]
bool CreateDefinedElement(State&, CustomElementDefinition&, JS::MutableHandleValue result);
bool UpgradeElement(State&, NodeKey element);  // "try to upgrade"; false only on OOM
bool UpgradeTree(State&, NodeKey root, bool connectedOnly);
void CustomElementsTreeChanged(State&, const std::vector<NodeKey>& added, const std::vector<NodeKey>& removed);
void CustomElementAttributeChanged(State&, NodeKey, DomStringView name, const DomString* old);
void CustomElementsRekey(State&, NodeKey from, NodeKey to);
void FlushCustomElementChanges(State&);  // scopes deferred during an identity transfer
bool InstallElementInternals(State&);  // element_internals.cpp
bool InstallCustomStateSet(State&);  // custom_state_set.cpp
JSObject* NewCustomStateSet(State&);
// ElementInternals validity: flags for a ValidityState owned by an internals object.
bool InternalsValidityFlags(JSObject* internals, uint32_t* flags);
bool NewInternalsValidity(State&, JS::HandleObject internals, JS::MutableHandleObject out);
bool InstallMetaElement(State&);
bool InstallAnchorElement(State&);
bool InstallHyperlinkMembers(State&);
bool InstallImageElement(State&);
bool InstallImageMembers(State&);
bool InstallAudioElement(State&);
bool InstallMediaElement(State&);
bool InstallTableElements(State&);
bool InstallSelectElements(State&);
bool InstallButtonElement(State&);
bool InstallFormElements(State&);
bool InstallInputElement(State&);
bool InstallTextAreaElement(State&);
bool InstallFormAux(State&);
bool InstallGaugeElements(State&);
bool InstallHeadElements(State&);
bool InstallContentElements(State&);
bool InstallDisclosureElements(State&);
bool InstallTemplateElement(State&);
bool InstallEmbeddedElements(State&);
bool InstallBodyFrameset(State&);
bool DocumentReachability(State&, std::set<NodeKey, NodeKeyLess>*, std::vector<NodeKey>*);
// Event handler IDL/content attributes (event_handlers.cpp,
// event_handler_attributes.cpp). A value is null, an object, or the uncompiled
// source string of a content attribute.
bool InstallEventHandlers(State&);
bool WriteEventHandler(State&, JS::HandleObject wrapper, const char* name, JS::HandleValue value);
bool ReadEventHandler(State&, JS::HandleObject wrapper, const char* name, JS::MutableHandleValue result);
bool HasEventHandler(JS::HandleObject wrapper, const char* name, JSContext*, bool* present);
bool CompileEventHandler(State&, JS::HandleObject wrapper, const char* name, JS::HandleString source,
                         JS::MutableHandleObject result);
void EventHandlerAttributeChanged(State&, NodeKey, DomStringView name);
void EventHandlerAttributesInserted(State&, const std::vector<NodeKey>& inserted);
// Content Security ports (content_security_ports.cpp).
void SecurityMetaInserted(State&, const std::vector<NodeKey>& inserted);
bool SecurityAllowsHandler(State&, JS::HandleObject wrapper, JS::HandleString source, bool* allowed);
bool NotifyStyleAssociations(State&, Wrapper&, bool migration = false);
void NotifyStyleMutation(void*, NodeKey, MutationKind, DomStringView, DomStringView,
                         const DomString*);
void ImageSourceChanged(State&, NodeKey);
void QueueImageSource(State&, NodeKey);
void CanvasDimensionChanged(State&, NodeKey, DomStringView name);
// Releases the element roots of in-flight image fetches; a later completion
// only frees its request.
void OrphanImageRequests(State&, bool settle = false);
bool RunAutofocus(State&);  // focus.cpp
bool StyleObject(State&, JS::HandleObject, JS::MutableHandleObject);
bool DataObject(State&, JS::HandleObject, JS::MutableHandleObject);
bool IsDataObject(JSObject*);
bool Constructor(JSContext*, unsigned, JS::Value*);
enum ConstructorKind { Illegal, NewDocument, NewFragment, NewText, NewComment,
                        NewRangeObject, NewStaticRange, NewXslt, NewSanitizerKind,
                        NewDOMParser, NewXMLSerializer, NewDOMImplementation, NewXPathEvaluator,
                        NewOptionElement, NewOptGroupElement, NewButtonElement, NewInputElement, NewTextAreaElement,
                        NewLabelElement, NewDataListElement, NewMeterElement, NewProgressElement,
                        NewLinkElement, NewBaseElement, NewScriptElement, NewAreaElement,
                        NewDetailsElement, NewDialogElement, NewTemplateElement, NewSourceElement, NewMarqueeElement,
                        NewIFrameElement,
                        NewStyleElement, NewBRElement, NewDivElement, NewParagraphElement,
                        NewHeadingElement, NewDataElement, NewDListElement, NewDirectoryElement,
                        NewOListElement, NewLIElement, NewHtmlElement, NewCustomElement };
}
#endif
