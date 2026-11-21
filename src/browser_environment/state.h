#ifndef RUYIJS_BROWSER_ENVIRONMENT_STATE_H
#define RUYIJS_BROWSER_ENVIRONMENT_STATE_H
#include "runtime/secure_slot.h"
#include "browser_environment/environment.h"
#include "browser_environment/cross_origin.h"
#include "browser_environment/model.h"
#include "browser_environment/schema.h"
#include "browser_environment/storage_opfs.h"
#include "domain/browser_environment/context_security.h"
#include "domain/browser_environment/user_activation.h"
#include "jsapi.h"
#include <memory>
#include <map>
#include <string>
#include "js/GCVector.h"
#include "js/GlobalObject.h"

namespace ruyijs::browser_environment {
struct State final {
  using ViewportPort = bool (*)(void*, double*, double*);
  State(JSContext* context, JS::HandleObject global, Model* document = nullptr)
      : cx(context), page(context, global), controller(context, JS::CurrentGlobalOrNull(context)),
        languages(context), topWindow(context, global), parentWindow(context, global),
        frameElement(context), navigatorPrototype(context), stringListPrototype(context),
        pluginArray(context), mimeTypeArray(context), external(context),
        storageManager(context), screenOrientation(context), visualViewport(context),
        instances(context, context), functions(context, context), name(context),
        webglGroup(context), gpuGroup(context), permissionsGroup(context),
        geolocationGroup(context), speechGroup(context), mediaGroup(context),
        model(document ? *document : own) {
    for (auto& owner : owners) owner.group.init(context);
    RegisterWindow(this);  // cross_origin.cpp: global -> State for the same-origin checks
  }
  ~State();
  JSContext* cx;
  PageHelpers* helpers = nullptr;
  JS::PersistentRootedObject page;
  JS::PersistentRootedObject controller;
  JS::PersistentRootedObject languages;
  // An iframe context reads Navigator, Screen, History and locale from the
  // parent's context-scoped state; its about:blank Location is its own.
  State* shared = nullptr;
  // HTML "historical agent cluster key map" of the browsing context group: the
  // first document of an origin decides, from its Origin-Agent-Cluster response
  // header, whether that origin is origin-keyed; later same-origin documents of
  // the group keep the decision. One map per group, shared by the top-level
  // successor documents and every frame.
  std::shared_ptr<std::map<std::string, bool>> agentClusters = std::make_shared<std::map<std::string, bool>>();
  // This document's own answer (window.originAgentCluster).
  bool originKeyed = false;
  // The document whose buffered owner groups (webgl, webgpu, permissions,
  // geolocation, speech, media and kOwnerGroups) this one starts with: the
  // creator of an iframe, or the document a later top-level document replaces.
  // Inherit() clones them into this realm once and clears it.
  const State* groupSource = nullptr;
  // Runtime document state; never delegated through shared or fingerprint Model.
  domain::browser_environment::UserActivation activation;
  double (*activationClock)(void*) = nullptr;
  void* activationClockContext = nullptr;
  // The child browsing context's own document URL, committed by the runtime
  // frame owner through the one page-load coordinator. Empty while the frame
  // is about:blank; after a load the child's Location components and origin
  // come from this URL while every other environment field still reads the
  // creator's shared model.
  // Worker mode reuses this realm URL for its immutable startup location.
  std::string frameUrl;
  bool worker = false;
  bool dedicatedWorker = false;  // a DedicatedWorkerGlobalScope (FileSystemSyncAccessHandle)
  ruyijs::domain::browser_environment::WorkerContext workerContext;
  JS::PersistentRootedObject topWindow, parentWindow, frameElement;
  // This window's cross-origin WindowProxy and Location views (cross_origin.cpp).
  std::unique_ptr<JS::PersistentRootedObject> crossWindow, crossLocation;
  // The Navigator prototype the Navigator instance is built from. The WebGPU
  // owner appends its one "gpu" accessor here; the environment owns the object
  // and nothing else writes to it.
  JS::PersistentRootedObject navigatorPrototype;
  // [SecureContext] members of this owner (Navigator.registerProtocolHandler,
  // Navigator.storage, window.StorageManager, window.onpointerrawupdate),
  // applied by Environment::ConfigureSecureMembers.
  SecureSlot protocolHandlerSlot{cx, "registerProtocolHandler"}, storageSlot{cx, "storage"},
      storageManagerSlot{cx, "StorageManager"}, rawUpdateSlot{cx, "onpointerrawupdate"},
      pipInterfaceSlot{cx, "DocumentPictureInPicture"}, pipWindowSlot{cx, "documentPictureInPicture"};
  // The DOMStringList prototype the Location.ancestorOrigins instances are
  // built from. Browser Environment owns both the interface and the ancestry
  // it reports, so there is no second environment layer for it.
  JS::PersistentRootedObject stringListPrototype;
  // The HTML PDF viewing surface: the two array-likes whose own properties hold
  // the plugin and MIME type instances (plugins.cpp). Also the one External
  // instance behind window.external (external.cpp). All are realm-local and are
  // created by this realm's installer.
  JS::PersistentRootedObject pluginArray;
  JS::PersistentRootedObject mimeTypeArray;
  JS::PersistentRootedObject external;
  // The realm's one StorageManager behind navigator.storage and the in-memory
  // OPFS store its getDirectory() exposes (storage_manager.cpp/storage_opfs.cpp).
  // Both are created by the shared installer and owned by this State; no other
  // layer keeps a second navigator.storage surface.
  JS::PersistentRootedObject storageManager;
  OpfsStore* opfsStore = nullptr;
  // The realm's own ScreenOrientation (behind Screen.prototype.orientation) and
  // VisualViewport (behind window.visualViewport), created by their installers
  // (screen_orientation.cpp / visual_viewport.cpp). Both are EventTarget
  // subclasses registered with the interaction owner; their event handler
  // attributes live in each instance's own reserved slots, not here.
  JS::PersistentRootedObject screenOrientation;
  JS::PersistentRootedObject visualViewport;
  // The realm's Navigation API (navigation_api.cpp), created by its installer.
  std::shared_ptr<struct NavigationState> navigation;
  // Firefox Window legacy members and documentPictureInPicture (window_legacy.cpp).
  std::shared_ptr<struct WindowLegacyState> legacy;
  // Root scroll content extent (right, bottom) in viewport coordinates, from the
  // Geometry layout of the document element; bound by runtime/geometry_ports.cpp.
  // The viewport's scroll range (window.scrollMaxX/Y) from Geometry layout.
  bool (*scrollExtent)(void*, double* maxX, double* maxY) = nullptr;
  void* scrollExtentContext = nullptr;
  JS::PersistentRooted<JS::GCVector<JSObject*>> instances;
  JS::PersistentRooted<JS::GCVector<JSObject*>> functions;
  // Window facade state that is not environment-profile data: the replaceable
  // name, the simulated scroll offsets, the closed flag and the headless alert
  // counter. Content sizes read the profile/parent geometry port, never a copy.
  JS::PersistentRooted<JS::Value> name;
  double scrollX = 0.0, scrollY = 0.0;
  // Window scroll notification (window_scroll.cpp): a position change marks it
  // pending and posts one task that fires scroll then scrollend at the document
  // through the runtime-bound Interaction port.
  bool scrollPending = false;
  bool (*scrollEvent)(void*, const char* type) = nullptr;
  void* scrollEventContext = nullptr;
  bool closed = false;
  uint32_t alertCount = 0;
  // Frame-count port bound by the runtime coordinator after the Document Model
  // is installed (runtime/browsing_context.cpp). It reads the unique DOM owner,
  // so this adapter never keeps an iframe list of its own.
  size_t (*frameCount)(void*) = nullptr;
  void* frameCountContext = nullptr;
  WindowNamePort windowNamePort = nullptr;
  void* windowNameContext = nullptr;
  // Content viewport source. Top-level reads the profile; child realms use the
  // runtime's parent Geometry port and never store an iframe size here.
  ViewportPort viewportPort = nullptr;
  void* viewportPortContext = nullptr;
  // Sole DOMException thrower, the Document Model factory, bound by the same
  // runtime coordinator (runtime/browsing_context.cpp). Only used by members
  // whose spec failure is a DOMException, such as btoa/atob/structuredClone.
  bool (*throwException)(void*, const char*, const char*) = nullptr;
  void* exceptionContext = nullptr;
  // Scheduling port: enqueues a handler as one macrotask on the unique task
  // queue, so postMessage delivery is asynchronous. Bound by the runtime
  // coordinator after the Scheduling owner is installed.
  bool (*postTask)(void*, JS::HandleObject) = nullptr;
  void* postTaskContext = nullptr;
  bool (*postMessageTask)(void*, JS::HandleObject, JS::HandleObject, JS::HandleObject, uint64_t, uint32_t) = nullptr;
  void* postMessageTaskContext = nullptr;
  uint64_t messageSequence = 0;
  // Interaction port: dispatches the trusted MessageEvent for a delivered
  // message. Bound by the same coordinator after the Interaction owner exists.
  bool (*deliverMessage)(void*, JS::HandleObject, JS::HandleValue, const char*, JS::HandleObject, JS::HandleObject) = nullptr;
  bool (*clonePort)(void*, JS::HandleValue, JS::HandleValue, JS::HandleObject,
                    JS::MutableHandleValue, JS::MutableHandleObject) = nullptr;
  void* cloneContext = nullptr;
  void* deliverMessageContext = nullptr;
  bool messageActive = true;
  // Interaction port: dispatches the trusted PopStateEvent for a committed
  // history traversal. Bound by the same coordinator after the Interaction
  // owner exists (runtime/runtime.cpp).
  bool (*deliverPopState)(void*, JS::HandleObject, JS::HandleValue) = nullptr;
  void* deliverPopStateContext = nullptr;
  // Navigation port: the runtime's single PageLoad coordinator performs a full
  // document navigation (fetch through the one resource owner, parse, run
  // scripts, fire the lifecycle events). Bound by runtime/runtime.cpp after the
  // coordinator exists and cleared before it is destroyed.
  bool (*navigate)(void*, const char*, bool replace, bool reload) = nullptr;
  void* navigateContext = nullptr;
  // Interaction port: dispatches the trusted hashchange for the one
  // same-document navigation the Location hash setter performs. Bound next to
  // the popstate port and cleared with it.
  bool (*deliverHashChange)(void*, JS::HandleObject, const char*, const char*) = nullptr;
  void* deliverHashChangeContext = nullptr;
  // Interaction port: dispatches the trusted "error" event window.reportError
  // fires. Bound next to the message ports by the runtime coordinator and
  // cleared with them before the Interaction owner is destroyed.
  bool (*fireEvent)(void*, JS::HandleObject, const char*) = nullptr;
  void* fireEventContext = nullptr;
  // Navigator owns the method. The shared runtime routes the receiver's
  // browsing context to its original document and Resource Network owners.
  BeaconPort sendBeacon = nullptr;
  void* sendBeaconContext = nullptr;
  // Debug-session port, bound by the shared coordinator in debug mode. Reports
  // one access to an install-registered member of this owner; a null port
  // (non-debug mode) skips without touching engine state.
  ApiMonitorPort apiMonitor = nullptr;
  void* apiMonitorContext = nullptr;
  void Observe(const char* iface, const char* member, int type) const {
    if (apiMonitor) apiMonitor(apiMonitorContext, iface, member, type);
  }
  // Graphics port: the WebGL owner's "webgl" group. Configure changes are
  // merged into webglGroup and pushed to the port; before the port exists they
  // stay buffered in webglGroup (see PushWebGL).
  bool (*graphicsPort)(void*, JS::HandleObject) = nullptr;
  void* graphicsPortContext = nullptr;
  bool (*graphicsDescribe)(void*, JS::MutableHandleValue) = nullptr;
  void* graphicsDescribeContext = nullptr;
  JS::PersistentRootedObject webglGroup;
  // WebGPU port: the WebGPU owner's "webgpu" group. Configure changes are
  // merged into gpuGroup and pushed to the port; before the port exists they
  // stay buffered (see PushWebGpu).
  bool (*gpuPort)(void*, JS::HandleObject) = nullptr;
  void* gpuPortContext = nullptr;
  bool (*gpuDescribe)(void*, JS::MutableHandleValue) = nullptr;
  void* gpuDescribeContext = nullptr;
  JS::PersistentRootedObject gpuGroup;
  // Permissions port: the Permissions owner's "permissions" group, buffered and
  // pushed exactly like the webgpu group.
  bool (*permissionsPort)(void*, JS::HandleObject) = nullptr;
  void* permissionsPortContext = nullptr;
  bool (*permissionsDescribe)(void*, JS::MutableHandleValue) = nullptr;
  void* permissionsDescribeContext = nullptr;
  JS::PersistentRootedObject permissionsGroup;
  // Geolocation port: the Geolocation owner's "geolocation" group, buffered
  // and pushed like the permissions group (docs/owners/GEOLOCATION.md).
  bool (*geolocationPort)(void*, JS::HandleObject) = nullptr;
  void* geolocationPortContext = nullptr;
  JS::PersistentRootedObject geolocationGroup;
  // Speech port: the Speech Synthesis owner's "speech" group, buffered and
  // pushed like the geolocation group (docs/owners/SPEECH_SYNTHESIS.md).
  bool (*speechPort)(void*, JS::HandleObject) = nullptr;
  void* speechPortContext = nullptr;
  JS::PersistentRootedObject speechGroup;
  // Media port: the Web Media owner's "media" group (device enumeration and
  // decoding answers), buffered and pushed like the webgl group.
  bool (*mediaPort)(void*, JS::HandleObject) = nullptr;
  void* mediaPortContext = nullptr;
  bool (*mediaDescribe)(void*, JS::MutableHandleValue) = nullptr;
  void* mediaDescribeContext = nullptr;
  JS::PersistentRootedObject mediaGroup;
  // Configure-only owner groups (model.h kOwnerGroups, owner_groups.cpp): one
  // buffered group and one bound port per owner, pushed whenever either changes.
  struct OwnerSlot {
    bool (*port)(void*, JS::HandleObject) = nullptr;
    void* context = nullptr;
    JS::PersistentRootedObject group;
  };
  OwnerSlot owners[kOwnerGroupCount];
  bool MergeOwner(size_t index, const std::vector<Change>& changes);
  bool PushOwner(size_t index);
  // The top-level traversable's profile, session history and URL. The first
  // top-level document owns it; every later top-level document reads and
  // writes that same Model (`adopted`), so a navigation keeps one history and
  // one profile while the Window and its realm-local objects are new.
  Model own;
  Model& model;
  bool adopted = false;
  bool ready = false;
  bool installed = false;
  bool Baseline();
  bool SpeechBaseline(JS::MutableHandleObject result);
  bool MergeWebGL(const std::vector<Change>& changes);
  bool PushWebGL();
  bool MergeWebGpu(const std::vector<Change>& changes);
  bool PushWebGpu();
  bool MergePermissions(const std::vector<Change>& changes);
  bool PushPermissions();
  bool MergeGeolocation(const std::vector<Change>& changes);
  bool PushGeolocation();
  bool MergeSpeech(const std::vector<Change>& changes);
  bool MergeMedia(const std::vector<Change>& changes);
  bool PushMedia();
  // The effective "speech" group object: the configured one, else the model
  // baseline list in kSpeechFields shape. Describe() reports it, PushSpeech()
  // hands it to the Speech Synthesis owner.
  bool EffectiveSpeech(JS::MutableHandleObject result);
  bool PushSpeech();
  bool Inherit();
  bool InheritGroups(const State& source);
  bool Apply(JS::HandleObject profile, bool complete);
  bool ApplyChanges(std::vector<Change>& changes);
  bool ApplyWorkerProfile(std::string_view profile);
  bool ApplyWorkerContext(JS::HandleObject input);
  bool ExportWorkerContext(JS::HandleObject output);
  bool ExportWorkerGroups(JS::HandleObject output);   // worker_groups.cpp
  bool ImportWorkerGroups(JS::HandleObject profile);
  bool InstallWorkerContext(JS::HandleObject prototype);
  // btoa/atob/structuredClone of WindowOrWorkerGlobalScope, from the
  // original Window method table and the same encoding/clone algorithms.
  bool InstallWorkerEncoding(JS::HandleObject prototype);
  bool ReadOrigin(JS::MutableHandleValue result);
  bool ReadOriginValue(OriginValue* result);
  bool ReadSecureContext(bool* result);
  bool IsCrossOriginIsolated() const;
  bool ValidateLocale(std::vector<Change>* changes);
  void ApplyLocale(const std::vector<Change>& changes);
  bool Read(Kind kind, uint32_t field, ValueType type, JS::MutableHandleValue result);
  bool ReadViewport(double* width, double* height);
  // Existing Navigator/CSS capability and the independent Window exposure input.
  // Both read the original Model shared by parent and successor contexts.
  bool TouchCapable();
  bool TouchEventsEnabled(bool* result);
  // Windows content legacy default, independent of modern hardware exposure.
  bool LegacyTouchEventsEnabled() const;
  bool Instance(Kind kind, JS::HandleObject parent);
  bool Globals();
  bool WindowMembers();
  bool NavigatorPlatformMembers();
  bool NavigatorLegacyMembers();
  friend bool ThrowDOM(State& state, const char* name, const char* message);
  bool Window(JS::HandleObject eventPrototype);
  JSObject* Function(JSNative callback, const char* name, unsigned length,
                     int32_t slot, unsigned flags = 0);
  bool Accessor(JS::HandleObject target, const char* name, int32_t slot,
                 unsigned attrs, JSNative getter, JSNative setter = nullptr);
};
const JSClass* ClassFor(Kind kind, bool worker = false);
// The receiver of a member of one of this owner's interfaces: |this|, unwrapped,
// when it is an object of |clasp| whose slot 0 holds its owner State; null
// otherwise. The member then works on that State, the receiver's own data, from
// whichever same-origin realm the member function came: a getter taken from a
// frame's Navigator.prototype reads the top window's navigator, as a Firefox
// binding works on its receiver's native object rather than the caller's realm.
State* ReceiverState(const JS::CallArgs& args, const JSClass* clasp);
// Install-time interface and member names of one registered instance accessor,
// e.g. InterfaceName(Kind::Navigator) -> "Navigator" and
// MemberName(Kind::Navigator, 2) -> "appCodeName". Static names only, so the
// debug monitor never sees a script-supplied string.
const char* InterfaceName(Kind kind);
const char* MemberName(Kind kind, uint32_t index);
bool Getter(JSContext* cx, unsigned argc, JS::Value* vp);
bool LocationString(JSContext* cx, unsigned argc, JS::Value* vp);
bool ParseBaseline(JSContext* cx, JS::MutableHandleValue result);
}
#endif
