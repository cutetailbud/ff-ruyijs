#ifndef RUYIJS_DOCUMENT_REALM_H
#define RUYIJS_DOCUMENT_REALM_H
#include <memory>
#include <string>
#include "js/RootingAPI.h"
#include "document_model/model.h"
#include "document_model/form_submission.h"
#include "document_model/node_lifecycle.h"
#include "document_model/node_source.h"
#include "document_model/style_source.h"
#include "document_model/style_port.h"
#include "document_model/layout_source.h"
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; enum class PathResult; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::document_model {
struct State;
class Realm;
using ScriptPort = bool (*)(void*, Realm&, interaction::Realm&,
    browser_environment::Environment&, JS::HandleObject page, JS::HandleObject script, bool external);
struct GeometryNode;
struct LayoutNodeSource;
// Runtime ports for an iframe's browsing context. Create assembles the child
// realm with the shared installers and returns its window and document; it runs
// at the structural change that connected the element, never at a later
// contentWindow read. Discard retires the context when the element leaves the
// tree. Both are bound by the runtime coordinator after installation and
// cleared before teardown.
using FrameCreate = bool (*)(void* context, JS::HandleObject element, NodeKey key,
                             JS::MutableHandleObject window, JS::MutableHandleObject document);
using FrameDiscard = void (*)(void* context, NodeKey key);
// A committed change to the `src` of an element that owns a live frame
// context. The runtime frame owner reads the new value through Attribute and
// navigates that existing context through the one page-load coordinator; a
// removed attribute arrives as an element without `src`. It runs at the
// attribute change point, recording or not. The structure notice also calls it
// once with `initial` set, after the element -> context link exists, for the
// first load of a newly connected frame (including its about:blank completion).
using FrameNavigate = bool (*)(void* context, JS::HandleObject element, NodeKey key,
                               bool initial);
// Debug-session port. The owner reports accesses to the members it registered
// at install time; it never hands out script-supplied names. Null when the
// Context is not in debug mode, and every call site is then skipped. The kind
// travels as a plain int so this owner keeps no observability dependency; the
// runtime coordinator maps it onto the observability domain's CallType.
enum class ApiAccess : int { Read = 0, Write = 1, Call = 2, Construct = 3 };
using ApiMonitorPort = void (*)(void* context, const char* iface,
                                const char* member, int type);
using MediaKeysCheckPort = bool (*)(void*, JS::HandleValue);
// Brand check for HTMLMediaElement (an HTMLAudioElement/HTMLVideoElement node
// wrapper of any realm), for other owners' WebIDL overload resolution.
bool IsMediaElementValue(JS::HandleValue value);
// Brand check for AbstractRange (a Range or StaticRange of any realm), for the
// CSS Highlight API (Web Styling) through its range brand port.
bool IsAbstractRangeValue(JS::HandleValue value);
// Brand check for Document (a document node wrapper of any realm).
bool IsDocumentValue(JS::HandleValue value);
// WebMedia's TextTrack factories for HTMLMediaElement: |list| creates an empty
// TextTrackList in the current realm; |append| creates a TextTrack in that
// list's realm and appends it. Both are bound after WebMedia installs.
struct TextTrackPorts {
  bool (*list)(JSContext*, JS::MutableHandleObject) = nullptr;
  bool (*append)(JSContext*, JS::HandleObject list, const char* kind, JS::HandleString label,
                 JS::HandleString language, JS::MutableHandleObject track) = nullptr;
};
// Resource Network owns transport; Document Model reports only the resolved
// URL/referrer and receives the HTTP success decision. The port returns at
// once; `done(request, outcome)` runs later on this thread at a task boundary
// (ARCHITECTURE.md, image fetch). An empty URL is a failed resolution.
struct ImageLoadOutcome {
  bool loaded = false;
  uint32_t width = 0, height = 0;
  std::string mime;
  std::string bytes;
};
using ImageLoadDone = void (*)(void* request, const ImageLoadOutcome& outcome);
using ImageLoadPort = void (*)(void* context, const std::string& url, const std::string& referer,
                               ImageLoadDone done, void* request);
// Canvas owner (docs/owners/CANVAS.md): a canvas element's width or height
// content attribute was set or removed. `width`/`height` are the current
// attribute values (string or null) read from this owner's tree, so the
// Canvas owner calls no page method inside the mutation.
using CanvasDimensionPort = bool (*)(void* context, JS::HandleObject canvas,
                                     JS::HandleValue width, JS::HandleValue height);
// Content Security owner (docs/owners/CONTENT_SECURITY.md). `meta` receives the
// content of a <meta http-equiv="Content-Security-Policy"> of the main document
// when it connects inside <head>; `handler` decides whether an event handler
// content attribute of `element` may compile (and reports a violation).
using CspMetaPort = void (*)(void* context, const std::u16string& policy);
using CspInlinePort = bool (*)(void* context, JS::HandleObject element, JS::HandleString source,
                               bool* allowed);
// Host user input (runtime/user_input.cpp; ARCHITECTURE.md, host user input).
// Each acts in the node's own realm; elements may come from any realm.
bool FocusElement(JSContext*, JS::HandleObject element);
bool BlurElement(JSContext*, JS::HandleObject element);
// mousedown's default action: focus the nearest focusable inclusive ancestor,
// or clear the document's focus.
bool PointerFocus(JSContext*, JS::HandleObject target);
bool InsertText(JSContext*, JS::HandleObject element, DomStringView text, bool* inserted);
bool DeleteBackward(JSContext*, JS::HandleObject element, bool* deleted);
bool Multiline(JSContext*, JS::HandleObject element, bool* multiline);
// `target`'s element ancestors, inclusive, from the root element down.
bool ElementPath(JSContext*, JS::HandleObject target, JS::MutableHandleObjectVector path);
// The click's activation behavior: form control activation or hyperlink.
bool Activate(JSContext*, JS::HandleObject target);
// Enter in a text input: click the default button, else submit the form.
bool ImplicitSubmit(JSContext*, JS::HandleObject element, double time_ms);
class Realm final {
 public:
  Realm(JSContext*, JS::HandleObject, interaction::Realm&,
        browser_environment::Environment&, PageHelpers&);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  // Removes or restores this owner's [SecureContext] members for the realm's
  // current secure-context state (runtime/secure_exposure.cpp).
  bool ConfigureExposure();

  bool ThrowException(const char* name, const char* message);
  // Prepared native topology, target-to-outer; flags bit 0 is a closed root.
  // The caller passes empty path/flags and keeps their object edges rooted.
  interaction::PathResult EventPath(JS::HandleObject target, JS::HandleString type,
      bool composed, JS::MutableHandleObjectVector path, std::vector<uint8_t>* flags);
  bool RetargetEventTarget(JS::HandleObject original, JS::HandleObject against,
                           JS::MutableHandleObject result, bool* inShadowTree);
  // Runs the same real collection the node quota runs (T41). The Interaction
  // owner binds this as its reclaim port so the target quota also bounds only
  // reachable objects; it stays the document owner's single collection entry.
  void Reclaim();
  // Bound by the shared coordinator after installation; cleared before teardown.
  // The element lifecycle of every iframe of this realm's main document is
  // driven by the unique tree's own structural-change notices.
  void SetFrameLifecycle(FrameCreate create, FrameDiscard discard, FrameNavigate navigate,
                         void* context);
  // Bound by the shared coordinator after installation; cleared before teardown.
  void SetApiMonitor(ApiMonitorPort port, void* context);
  // HTMLMediaElement stores its MediaKeys attachment in the Document Model;
  // web_media supplies only the brand check through this port.
  void SetMediaKeysCheckPort(MediaKeysCheckPort port, void* context);
  // FileList factory of the Web Platform owner (null `files`: an empty list).
  using FileListPort = bool (*)(void*, JS::HandleObject files, JS::MutableHandleObject result);
  void SetFileListPort(FileListPort port, void* context);
  void SetTextTrackPorts(TextTrackPorts ports);
  // Rule and computed declarations (Styling) answer the CSSStyleDeclaration
  // and CSSStyleProperties members this owner installs once (style_port.h).
  void SetStyleDeclarationPort(StyleDeclarationPort port);
  void SetImageLoadPort(ImageLoadPort port, void* context);
  // Bound by the shared coordinator after the Canvas owner installs; cleared before teardown.
  void SetCanvasDimensionPort(CanvasDimensionPort port, void* context);
  // HTMLElement offset* read the layout owner through this port (bound after install).
  void SetOffsetGeometryPort(OffsetGeometryPort port, void* context);
  void SetElementScrollPort(const ElementScrollPort& port);
  // The queued scroll notification of |element| is being delivered.
  void ElementScrollDelivered(JS::HandleObject element);
  // Bound by the shared coordinator after installation; cleared before teardown.
  void SetSecurityPorts(CspMetaPort meta, CspInlinePort handler, void* context);
  // Whether `node` is connected to this realm's main document, read from the
  // unique tree; false for a non-node.
  bool NodeConnected(JS::HandleObject node);
  // Image fetches that still delay this realm's main document's load event.
  bool ImageLoadsPending() const;
  // Document retirement: cancels in-flight image fetches (complete, no event)
  // and loads no further image in this realm.
  void RetireImageLoads();
  // Focuses the main document's first focusable `autofocus` element when
  // nothing has focus (top-level PageLoad, after DOMContentLoaded).
  bool RunAutofocus();
  // The main document's active element (the focused element, else body).
  bool ActiveElement(JS::MutableHandleObject element);
  // document.referrer of this realm's window document (the loading request's Referer).
  void SetDocumentReferrer(const std::string& referrer);
  // Page-load port for the shared runtime coordinator (runtime/page_load.cpp).
  // The parser, readyState and currentScript stay in this owner.
  JSObject* document();  // Main document wrapper.
  bool ReadBaseReference(JS::MutableHandleValue);  // First base[href] in the original tree.
  // The HTMLCanvasElement prototype, so the Canvas owner adds the canvas
  // members without this owner creating a second interface for the element.
  JSObject* canvasPrototype();
  JSObject* shadowPrototype();  // Original DOM factory, for the Styling installer.
  // Geometry owns Range's rect projections; these ports expose the existing
  // prototype and validate its original native receiver without copying state.
  JSObject* rangePrototype();
  // Range geometry source: every element and text node a live Range contains, in
  // tree order (Firefox PreContentIterator), as wrappers of the realm whose tree
  // holds the boundaries; a TypeError for a non-Range receiver. Partially
  // selected boundary text nodes are not reported.
  bool RangeContents(JS::HandleValue value, JS::MutableHandleObjectVector elements);
  JSObject* elementPrototype();
  // Web Animations ports (animation_port.cpp, docs/owners/WEB_ANIMATION.md).
  // Read the unique tree only; no page method runs. Kind: 1 Element, 2 Document, 0 other.
  JSObject* animationDocumentPrototype();
  int AnimationNodeKind(JS::HandleValue node);
  bool AnimationNodeDocument(JS::HandleObject node, JS::MutableHandleObject document);
  bool AnimationInScope(JS::HandleObject root, JS::HandleObject node, bool subtree, bool* result);
  bool CheckElementReceiver(JS::HandleValue);
  bool IsElementValue(JS::HandleValue, bool* result);  // non-throwing brand check
  JSObject* svgPrototype(bool text);
  JSObject* svgViewBoxPrototype(unsigned index);
  bool CheckSvgViewBoxReceiver(JS::HandleValue, unsigned index);
  bool ReadSvgViewBox(JS::HandleValue, bool* supported, SvgViewBoxValue*, void** factory);
  bool WriteSvgViewBoxField(JS::HandleValue, unsigned field, float number);
  bool SvgViewBoxView(JS::HandleValue, unsigned role, JS::HandleObject replacement, JS::MutableHandleObject);
  // SVG point and length sources for WebGeometry (svg_points_source.cpp,
  // svg_length_source.cpp); `CheckSvgInterface` is the generic SVG receiver brand check.
  bool CheckSvgInterface(JS::HandleValue, const char* iface, const char* member);
  JSObject* svgPointsPrototype(unsigned index);  // 0 SVGSVGElement, 1 polyline, 2 polygon
  bool ReadSvgPoints(JS::HandleValue, SvgPointsSnapshot*, void** factory);
  bool MutateSvgPoints(JS::HandleValue, unsigned operation, uint32_t index, const SvgPointRef& input,
                       SvgPointRef* out);
  bool WriteSvgPointField(JS::HandleValue, const SvgPointRef& point, unsigned field, float number);
  bool SvgPointsView(JS::HandleValue, unsigned role, JS::HandleObject replacement, JS::MutableHandleObject);
  JSObject* svgLengthPrototype(unsigned field);
  bool CheckSvgLengthReceiver(JS::HandleValue, unsigned field);
  bool ReadSvgLength(JS::HandleValue, unsigned field, SvgLengthValue*, void** factory);
  bool WriteSvgLength(JS::HandleValue, unsigned field, const SvgLengthValue&);
  bool SvgLengthRoot(JS::HandleValue, JS::MutableHandleObject);
  bool SvgLengthView(JS::HandleValue, unsigned field, unsigned role, JS::HandleObject replacement,
                     JS::MutableHandleObject);
  void SetGeometryFactory(void* value) { geometryFactory_ = value; }
  bool ReadGeometryNode(JS::HandleValue, GeometryNode*, JS::MutableHandleObject parent,
                        JS::MutableHandleObjectVector children);
  bool ReadLayoutNode(JS::HandleValue, LayoutNodeSource*, JS::MutableHandleObject parent,
                      JS::MutableHandleObjectVector children);
  bool ReadLayoutHead(JS::HandleValue, LayoutHeadSource*, JS::MutableHandleObject root);
  bool ReadChangedElements(JS::HandleValue, const std::vector<NodeKey>&, const std::vector<char>&,
                           const std::vector<uint64_t>&, const std::vector<DomString>&,
                           bool style_only, std::vector<ChangedElement>*);
  bool WrapLayoutNode(JS::HandleValue, NodeKey, JS::MutableHandleObject);
  // Connected iframe elements of the main document, from the unique tree via
  // the existing Model::Query. The runtime frame port reads this for
  // window.length/window.frames; it is a snapshot, not a second index.
  size_t IframeCount();
  // The WindowProxy of the `index`-th connected iframe in tree order (the same
  // view contentWindow returns); undefined when it has no browsing context.
  bool FrameWindowAt(size_t index, JS::MutableHandleValue window);
  // WindowProperties reads this realm's original connected tree and wrappers.
  bool ResolveWindowName(JS::HandleString name, JS::MutableHandleValue result);
  bool BeginLoad(JS::HandleString markup);
  // Next parser-inserted script element; null once the markup is consumed.
  bool NextScript(JS::MutableHandleObject script);
  bool SetReadyState(ReadyState state);
  // True once the main document left readyState "loading" (its DOMContentLoaded
  // point); the Service Worker container enables its client message queue then.
  bool DocumentParsed();
  bool SetCurrentScript(JS::HandleObject script);  // null clears
  // Attribute value or null; inline source is the element's text content.
  bool ScriptAttribute(JS::HandleObject script, const char16_t* name, JS::MutableHandleValue result);
  // The same attribute read for any node of this realm (null when absent).
  bool Attribute(JS::HandleObject element, const char16_t* name, JS::MutableHandleValue result);
  bool ScriptText(JS::HandleObject script, JS::MutableHandleValue result);
  // Where the parser found this script's text in the document (0, 0 if not).
  bool ScriptPosition(JS::HandleObject script, uint32_t* line, uint32_t* column);
  void SetScriptPort(ScriptPort, void*);
  void CopyScriptPortFrom(const Realm&);
  void SetFormPorts(FormDataPort, FormNavigatePort, void*);
  void CopyFormPortsFrom(const Realm&);
  // Collects the successful controls through the existing Model owner for the
  // WebPlatform FormData(form[, submitter]) adapter.
  bool FormFields(JS::HandleObject form, JS::HandleValue submitter,
                  std::vector<FormField>* out);
  bool FormNavigationCurrent(JS::HandleObject, uint64_t version);
  uint64_t ScriptGeneration() const;
  bool CurrentScript(JS::MutableHandleObject);
  // Style-source port for the Styling / CSSOM owner (T17): the cascade
  // input of one element and the document's ordered <style> sources, both
  // read from the unique DOM tree. The element side sets is_element=false
  // for a non-Element so the caller can raise the DOM TypeError.
  bool ReadStyleChain(JS::HandleObject element, std::vector<StyleFrame>* chain,
                     bool* is_element);
  bool ReadStyleRelation(JS::HandleObject,uint64_t,uint64_t,StyleRelationFrame*);
  bool ReadStyleSheets(std::vector<StyleSheetSource>* out);
  bool ReadStyleNode(uint64_t document, uint64_t key, JS::MutableHandleValue out);
  bool ReadStyleScope(uint64_t document, uint64_t key, std::vector<StyleSheetSource>*);
  bool AssociateStyleSheet(JS::HandleObject, JS::HandleValue expected, JS::HandleValue value);
  void SetStyleElementPort(StyleElementPort, void* context);
  void SetStyleDocumentListPort(StyleDocumentListPort, void*);
  bool ResolveStyleSheets(JS::HandleValue, JS::MutableHandleValue, bool shadow);
  void SetAdoptedStylesListPort(StyleDocumentListPort, void*);
  void SetStyleNotifyPort(StyleNotifyPort, void*);
  bool ResolveAdoptedStyles(JS::HandleValue, JS::MutableHandleValue, bool shadow);
  bool ReadStyleScopeDocument(JS::HandleObject, JS::MutableHandleObject);
  bool ReadCascadeScope(JS::HandleObject, JS::MutableHandleObject, bool* shadow);
  bool ReadDocumentStyleSources(JS::HandleObject, StyleSheetScope*, std::vector<StyleSheetSource>*);
  // Real-change ports for the observer owner (src/web_observers): the bounded
  // notice log of the unique tree, its subtree test, the realm's single wrapper
  // for a node identity and one static NodeList over explicit nodes. Every
  // policy decision (filters, batching, delivery) belongs to that owner.
  void SetMutationRecording(bool enabled);
  uint64_t MutationSequence();
  uint64_t DroppedMutations();
  bool MutationsSince(uint64_t sequence, std::vector<MutationNotice>* result);
  // Synchronous for ordinary mutations; transfers notify after the original
  // wrapper identities are published, before returning from the DOM call.
  void SetMutationSink(void (*sink)(void*), void* context);
  // Native identity/lifetime events, including when mutation recording is off.
  // Rekeyed precedes mutation publication; wrapper GC alone is not Released.
  void SetNodeLifecycleSink(NodeLifecycleSink sink, void* context);
  bool AncestorOrSelf(NodeKey ancestor, NodeKey node, bool* result);
  bool WrapNode(NodeKey key, JS::MutableHandleObject result);
  bool NodeListOf(const std::vector<NodeKey>& nodes, JS::MutableHandleObject result,
                  Realm* factory = nullptr);
  // The Node identity behind a script value, so observe(target) rejects a
  // non-Node through the document model's own TypeError instead of a second
  // node registry. The realm wrapper list stays the only identity source.
  bool NodeIdentity(JS::HandleValue value, NodeKey* result);
  // Cross-realm consumers need both the original owner and its current local
  // key. The narrower NodeIdentity contract above is intentionally unchanged.
  bool ResolveNodeSource(JS::HandleValue value, NodeSource* result);
  // Original current Document behind a trusted input target. Non-Nodes and
  // retained nodes of replaced documents return null without a script error.
  Realm* InputDocument(JS::HandleObject target);
  // A plain engine TypeError in this realm, for the observer owner's option
  // validation; DOM errors keep their own factory.
  bool ThrowTypeError(const char* message);
 private:
  void* geometryFactory_ = nullptr;
  std::unique_ptr<State> state_;
};
}
#endif




