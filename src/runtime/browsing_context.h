#ifndef RUYIJS_BROWSING_CONTEXT_H
#define RUYIJS_BROWSING_CONTEXT_H
#include <memory>
#include <vector>
#include "js/RootingAPI.h"
#include "runtime/page_helpers.h"
#include "interaction/realm.h"
#include "browser_environment/environment.h"
#include "document_model/realm.h"
#include "web_audio/realm.h"
#include "web_console/realm.h"
#include "font_loading/realm.h"
#include "web_platform/realm.h"
#include "web_crypto/realm.h"
#include "web_trusted_types/realm.h"
#include "web_security/realm.h"
#include "web_permissions/realm.h"
#include "web_geolocation/realm.h"
#include "web_speech/realm.h"
#include "web_broadcast/realm.h"
#include "web_locks/realm.h"
#include "web_wake_lock/realm.h"
#include "web_midi/realm.h"
#include "web_gamepad/realm.h"
#include "web_animation/realm.h"
#include "web_service_worker/realm.h"
#include "runtime/broadcast_ports.h"
#include "web_messaging/realm.h"
#include "web_clipboard/realm.h"
#include "web_credentials/realm.h"
#include "web_geometry/realm.h"
#include "web_canvas/realm.h"
#include "web_graphics/realm.h"
#include "web_styling/realm.h"
#include "runtime/page_reference.h"
#include "web_rtc/realm.h"
#include "web_media/realm.h"
#include "web_indexeddb/realm.h"
#include "web_gpu/realm.h"
#include "web_observers/realm.h"
#include "web_workers/realm.h"
#include "resource_network/realm.h"

namespace ruyijs {
namespace execution_scheduling {class Realm;}
class ModuleSession;
class Frames;
class TopLevel;
// One browsing context's realm-local owners. The main page and every iframe
// run the same ordered installers: PageHelpers -> Interaction (base Event/
// EventTarget in Begin, the pref-dependent derived interfaces at the start of
// Finish) -> Browser Environment -> Document Model -> Window attach -> event
// ports -> frame port.
struct BrowsingContext {
  BrowsingContext(JSContext* context, JS::HandleObject global);
  ~BrowsingContext();  // Unbinds the ports before the owners are destroyed.
  BrowsingContext(const BrowsingContext&) = delete;
  BrowsingContext& operator=(const BrowsingContext&) = delete;
  // The main page applies its profile between Begin and Finish; Finish then
  // installs Interaction's derived interfaces against that profile.
  bool Begin(BrowsingContext* parent = nullptr, JS::HandleObject frameElement = nullptr);
  // A later top-level document of the same traversable (docs/owners/TOP_LEVEL.md):
  // the same ordered installers in a new realm, with Context-scoped state shared
  // from `previous` exactly as an iframe shares it from its parent.
  bool BeginSuccessor(BrowsingContext& previous);
  bool Finish(Frames& frames);
  JSContext* cx;
  JS::PersistentRootedObject page;
  // The current PageLoad operation itself; no copy of Document readyState.
  JS::PersistentRootedObject loadCompletion;
  // Script text the current document has compiled in full (PageLoad::ClaimFullParse);
  // a new document in this context starts again at 0.
  size_t parseBudget = 0;
  std::unique_ptr<PageHelpers> helpers;
  std::unique_ptr<interaction::Realm> interaction;
  // The Console namespace: one object per realm over the Context's one bounded
  // log, installed before any document exists so a parser-inserted script can
  // log while parsing. See docs/owners/CONSOLE.md.
  std::unique_ptr<web_console::Realm> console;
  std::unique_ptr<browser_environment::Environment> environment;
  std::unique_ptr<document_model::Realm> document;
  std::unique_ptr<web_audio::Realm> audio;
  std::unique_ptr<web_platform::Realm> platform;
  std::unique_ptr<web_geometry::Realm> geometry;
  std::unique_ptr<web_canvas::Realm> canvas;
  std::unique_ptr<web_graphics::Realm> graphics;
  std::unique_ptr<web_styling::Realm> styling;
  std::unique_ptr<web_rtc::Realm> rtc;
  std::unique_ptr<web_media::Realm> media;
  std::unique_ptr<web_indexeddb::Realm> indexeddb;
  std::unique_ptr<web_gpu::Realm> gpu;
  // Trusted Types policy factory and value interfaces (docs/owners/TRUSTED_TYPES.md).
  std::unique_ptr<web_trusted_types::Realm> trusted;
  // Content Security Policy of this context's document (docs/owners/
  // CONTENT_SECURITY.md); bound to the Document Model, destroyed before it.
  std::unique_ptr<web_security::Realm> security;
  // navigator.permissions; the main context owns the permission-state table.
  std::unique_ptr<web_permissions::Realm> permissions;
  std::unique_ptr<web_geolocation::Realm> geolocation;
  std::unique_ptr<web_speech::Realm> speech;
  // BroadcastChannel over the Context's one channel registry.
  std::unique_ptr<web_broadcast::Realm> broadcast;
  std::unique_ptr<web_locks::Realm> locks;
  std::unique_ptr<web_wake_lock::Realm> wakeLock;
  std::unique_ptr<web_midi::Realm> midi;
  std::unique_ptr<web_gamepad::Realm> gamepad;
  std::unique_ptr<web_animation::Realm> animation;
  std::unique_ptr<web_service_worker::Realm> serviceWorker;
  // BroadcastChannel between this Context's Window realms and worker processes.
  std::unique_ptr<BroadcastRoute> broadcastRoute;
  std::unique_ptr<web_messaging::Realm> messaging;
  // ClipboardItem (docs/owners/CLIPBOARD.md); realm-local, no shared state.
  std::unique_ptr<web_clipboard::Realm> clipboard;
  // Credential / PublicKeyCredential / Authenticator*Response interfaces
  // (docs/owners/CREDENTIALS.md); realm-local, no shared state.
  std::unique_ptr<web_credentials::Realm> credentials;
  std::unique_ptr<web_observers::Realm> observers;
  // Dedicated Worker owner. It creates realm/process state for this browsing
  // context and is destroyed before the page's event owner.
  std::unique_ptr<web_workers::Realm> workers;
  // The child realm's resource surface: its own Headers/Response/Request/Abort
  // and fetch/XHR/Storage globals, over the main realm's one resource catalog,
  // one NetLog and one Web Storage owner. Declared last so it is destroyed
  // before the Interaction owner its AbortSignal state points at. Null on the
  // main page, whose realm owns the shared state instead.
  std::unique_ptr<resource_network::Realm> resources;
  // Parser module records are scoped to this document generation. A reload in
  // an iframe reuses its BrowsingContext, so prior sessions stay rooted here
  // until the realm itself is destroyed while the active pointer advances.
  std::unique_ptr<ModuleSession> moduleSession;
  std::vector<std::unique_ptr<ModuleSession>> retiredModuleSessions;
  resource_network::Realm* navigatorNetwork = nullptr;  // Non-owning per-context port.
  // This context's resource realm serving its Workers' fetch; bound with the
  // object URL network port and cleared before the resource realm dies.
  resource_network::Realm* workerNetwork = nullptr;
  execution_scheduling::Realm* scheduling = nullptr;  // Borrowed assembly port, no clock state.
  Frames* frames = nullptr;
  // window[i] (runtime/page_reference.h): bound in Finish, cleared on teardown.
  PageFrames pageFrames;
  // Owning frame's context (null for the main page) and this frame's iframe
  // element in the parent document, for the frame-lifecycle bookkeeping.
  BrowsingContext* parent = nullptr;
  document_model::NodeKey frame_element;
  // This top-level document was loaded by a navigation the browser started
  // (host "navigate", or a reload of such a document); location.reload()
  // repeats it the same way (PageLoad::RequestNavigation).
  bool browserInitiated = false;
  // The top-level document this one replaced (null for the first document and
  // for an iframe). Context-scoped owners are shared from it.
  BrowsingContext* predecessor = nullptr;
};
// Owns the iframe child contexts; live ones are destroyed before the main
// page's realm, retired ones after it.
class Frames final {
 public:
  explicit Frames(JSContext* cx) : cx_(cx) {}
  ~Frames();
  // Runtime frame-load port: runs the one page-load coordinator against a
  // child context's owners. The runtime coordinator binds it after the
  // coordinator exists and clears it before that coordinator is destroyed.
  using LoadPort = bool (*)(void* context, BrowsingContext& target, const std::string& url);
  // Runtime inline-document port for iframe `srcdoc`: the same coordinator
  // runs the same parse/execute/lifecycle path with the markup handed in
  // directly instead of fetched from the resource owner.
  using LoadHtmlPort = bool (*)(void* context, BrowsingContext& target, const std::string& html);
  // Runtime port for a newly connected frame whose first load has no target:
  // the same coordinator completes its about:blank document, which in Firefox
  // fires the element's `load` during insertion.
  using BlankPort = bool (*)(void* context, BrowsingContext& target);
  void SetLoadPort(LoadPort port, void* context);
  void SetLoadHtmlPort(LoadHtmlPort port, void* context);
  void SetBlankPort(BlankPort port, void* context);
  // Runtime port: installs this Context's one Scheduling surface (timers, rAF,
  // queueMicrotask, performance) into a child realm. The clock, task queues and
  // counters stay the single Context-level owner; only the realm-local function
  // objects are new, so a child never opens a second time source.
  using SchedulingPort = bool (*)(void* context, BrowsingContext& target);
  void SetSchedulingPort(SchedulingPort port, void* context);
  // Runtime port: installs this Context's one resource surface (fetch, XHR,
  // Storage, Headers/Response/Request/Abort) into a child realm. The child gets
  // its own realm-local objects over the same catalog, NetLog and Storage area,
  // so there is one resource owner for the whole Context.
  using ResourcePort = bool (*)(void* context, BrowsingContext& target);
  void SetResourcePort(ResourcePort port, void* context);
  // Runtime port: drops a retired child's scheduled work (its timers, frames
  // and tasks stop, as in Firefox once the iframe's document is gone).
  using RetirePort = void (*)(void* context, BrowsingContext& target);
  void SetRetirePort(RetirePort port, void* context);
  // Retires every frame inside `top`'s document when that top-level document
  // is itself retired by a navigation.
  void RetireUnder(BrowsingContext& top);
  BrowsingContext* FindDocument(const document_model::Realm& document) const;
  // True while `context` is the live context of its frame element: not yet
  // retired by the element leaving the tree.
  bool Live(const BrowsingContext& context) const;
  // Borrow the original traversable owner; source identity may come from a
  // wrapper created in a retired realm and later adopted into a current one.
  void SetTopLevel(TopLevel* owner) { topLevel_ = owner; }
  BrowsingContext* FindCurrentDocument(const document_model::Realm&) const;
  // Activation visits this original frame tree; no second topology owner.
  bool NotifyActivation(BrowsingContext& source, double now);
  void ConsumeHistoryActivation(BrowsingContext& source);
  void UnbindActivation();
  void UnbindResourceCallbacks();
  bool LoadEventsPending(const BrowsingContext& parent,
                         bool (*pending)(void*, JS::HandleObject), void* context) const;
  // Runtime frame-lifecycle ports (document_model::FrameCreate/FrameDiscard).
  // Create runs at the structural change that connected the element and only
  // assembles the context, so it exists before any script can read
  // contentWindow; the first load follows through Navigate. Discard retires
  // the context when the element leaves the tree. An element inserted again
  // gets a fresh context, and a script that still holds the retired window
  // keeps a valid detached realm instead of a dangling one.
  static bool Create(void* parent, JS::HandleObject element, document_model::NodeKey key,
                     JS::MutableHandleObject window, JS::MutableHandleObject document);
  static void Discard(void* context, document_model::NodeKey key);
  // Runtime frame-navigation port (document_model::FrameNavigate): the first
  // load of a newly linked frame (`initial`) or a committed change to its
  // `src`/`srcdoc` navigates the context that element already has. This is the
  // one place that reads both attributes; an initial frame without a load target
  // completes its about:blank document instead.
  static bool Navigate(void* context, JS::HandleObject element, document_model::NodeKey key,
                       bool initial);
 private:
  // Product bound on child contexts per Context. This engine has no
  // GC-driven realm reclamation, so a retired realm is kept alive on purpose
  // and the same budget covers live and retired contexts together.
  static constexpr size_t kLimit = 64;
  // True when `child` is `ancestor` or was created inside its frame subtree.
  static bool DescendsFrom(const BrowsingContext* child, const BrowsingContext* ancestor);
  // Resolves a frame `src` against the parent document URL and loads it into
  // the child's existing context through the one page-load coordinator. A
  // reference that is missing or does not resolve to an absolute HTTP(S) URL
  // leaves the child at about:blank and `*loaded` false; a failed child load
  // never propagates into the parent document (see docs/DEVIATIONS.md D8).
  bool LoadChild(BrowsingContext& parent, BrowsingContext& child, const std::string& reference,
                 bool* loaded);
  // Produces an inline `srcdoc` document in the child's own context through the
  // one coordinator; a failure is dropped here for the same reason.
  bool LoadChildHtml(BrowsingContext& child, const std::string& html);
  // Completes an initial about:blank child through the one coordinator; a
  // failure is dropped for the same reason.
  bool CompleteBlank(BrowsingContext& child);
  JSContext* cx_;
  TopLevel* topLevel_ = nullptr;  // Original borrowed current-document source.
  LoadPort load_ = nullptr;
  void* loadContext_ = nullptr;
  LoadHtmlPort loadHtml_ = nullptr;
  void* loadHtmlContext_ = nullptr;
  BlankPort blank_ = nullptr;
  void* blankContext_ = nullptr;
  SchedulingPort scheduling_ = nullptr;
  void* schedulingContext_ = nullptr;
  ResourcePort resource_ = nullptr;
  void* resourceContext_ = nullptr;
  RetirePort retire_ = nullptr;
  void* retireContext_ = nullptr;
  // Moves every live context inside `ancestor`'s frame subtree to retired_.
  void Retire(const BrowsingContext* ancestor);
  std::vector<std::unique_ptr<BrowsingContext>> children_;  // Live, creation order.
  std::vector<std::unique_ptr<BrowsingContext>> retired_;   // Detached, kept alive.
};
}
#endif



