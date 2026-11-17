#ifndef RUYIJS_BROWSER_ENVIRONMENT_H
#define RUYIJS_BROWSER_ENVIRONMENT_H
#include <string>
#include <cstdint>
#include <string_view>
#include <vector>
#include "browser_environment/source_snapshot.h"
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment {
struct State;
using BeaconPort = bool (*)(void*, JSContext*, const std::string&, JS::HandleValue, bool*);
using WindowNamePort = bool (*)(void*, JS::HandleString, JS::MutableHandleValue);
// Debug-session port, mirroring document_model::ApiMonitorPort. The owner
// reports accesses to members its installer registered, and the access kind
// travels as a plain int so Browser Environment keeps no observability
// dependency. Null outside debug mode, and every call site then skips.
enum class ApiAccess : int { Read = 0, Write = 1, Call = 2, Construct = 3 };
using ApiMonitorPort = void (*)(void* context, const char* iface,
                                const char* member, int type);
class Environment final {
 public:
  Environment(JSContext* cx, JS::HandleObject page);
  // An iframe's about:blank browsing context in its own realm (child.cpp).
  Environment(JSContext* cx, JS::HandleObject page, Environment& parent,
              JS::HandleObject frameElement);
  // The next top-level document of the same traversable, in a new realm
  // (child.cpp). It shares `previous`'s profile, session history and URL Model
  // and builds its own Window, Navigator, Location and History objects.
  struct Succession {};
  Environment(JSContext* cx, JS::HandleObject page, Environment& previous, Succession);
  ~Environment();
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;
  bool Configure(JS::HandleObject profile);
  // Original UserActivation runtime value and original Scheduling clock input.
  void SetActivationClock(double (*read)(void*), void* context);
  void NotifyActivation(double now);
  bool HasHistoryActivation() const;
  void ConsumeHistoryActivation();
  void ConsumeTransientActivation();
  void ResetActivation();
  bool SameOrigin(Environment& other, bool* result);
  bool Install(interaction::Realm& events, PageHelpers& helpers);
  bool InstallWorker(PageHelpers&, JS::HandleObject globalScopePrototype,
                     const std::string& scriptUrl, std::string_view profile, bool dedicated);
  // The File System interfaces (storage_opfs.cpp). They extend the Resource
  // Network WritableStream and build Web Platform Files, so the assembly runs
  // this after those owners installed; InstallWorker runs it itself.
  bool InstallFileSystem();
  bool ExportWorkerProfile(std::string* result);
  // Runtime frame port: the connected iframe count of this browsing context,
  // read from the unique DOM owner. Bound after Document installation.
  void SetFrameCount(size_t (*port)(void*), void* context);
  // Dynamic Window named access, resolved by the sole Document owner.
  void SetWindowNamePort(WindowNamePort port, void* context);
  void SetViewportPort(bool (*port)(void*, double*, double*), void* context);
  void SetScrollExtentPort(bool (*port)(void*, double*, double*), void* context);
  // Interaction port: fires the trusted scroll/scrollend UIEvent at this
  // context's document. Bound next to the post-task port, cleared with it.
  void SetScrollEventPort(bool (*port)(void*, const char* type), void* context);
  // Graphics port: the WebGL owner's "webgl" profile group. Bound by the
  // runtime coordinator after the WebGL realm is installed; the group is
  // buffered until then, so a profile file applied before Finish still reaches
  // the one WebGL owner.
  void SetGraphicsPort(bool (*port)(void*, JS::HandleObject), void* context);
  void SetGraphicsDescribe(bool (*port)(void*, JS::MutableHandleValue), void* context);
  // WebGPU port: the WebGPU owner's "webgpu" profile group. Bound by the
  // runtime coordinator after the WebGPU realm is installed; the group is
  // buffered until then, so a profile applied before Finish still reaches the
  // one WebGPU owner. navigatorPrototype() exposes the Navigator prototype the
  // instance uses so that owner can append its single "gpu" accessor.
  void SetGpuPort(bool (*port)(void*, JS::HandleObject), void* context);
  void SetGpuDescribe(bool (*port)(void*, JS::MutableHandleValue), void* context);
  // Permissions port: the Permissions owner's "permissions" group, buffered
  // until the owner binds it, like the webgpu group.
  void SetPermissionsPort(bool (*port)(void*, JS::HandleObject), void* context);
  void SetPermissionsDescribe(bool (*port)(void*, JS::MutableHandleValue), void* context);
  // Geolocation port: the Geolocation owner's "geolocation" group, buffered
  // until the owner binds it, like the permissions group.
  void SetGeolocationPort(bool (*port)(void*, JS::HandleObject), void* context);
  // Speech port: the Speech Synthesis owner's "speech" group, likewise buffered.
  void SetSpeechPort(bool (*port)(void*, JS::HandleObject), void* context);
  // The Web Media owner's "media" group: the device list enumerateDevices()
  // reports and the per-content-type MediaCapabilities decoding answers.
  void SetMediaPort(bool (*port)(void*, JS::HandleObject), void* context);
  void SetMediaDescribe(bool (*port)(void*, JS::MutableHandleValue), void* context);
  // A configure-only owner group ("canvas", "audio", "mediaFeatures", "rtc",
  // "fonts"; model.h kOwnerGroups), buffered until the owner binds its port.
  void SetOwnerPort(std::string_view group, bool (*port)(void*, JS::HandleObject),
                    void* context);
  JSObject* navigatorPrototype() const;
  // The content area's screen origin (mozInnerScreenX/Y): host input adds it
  // to client coordinates for MouseEvent.screenX/Y.
  bool InnerScreenOrigin(double* x, double* y);
  // Re-defines Navigator.prototype members in Firefox binding order; called
  // once by the runtime coordinator after every Navigator-extending installer.
  bool OrderNavigator();
  // Removes or restores this owner's [SecureContext] members for the realm's
  // current secure-context state (runtime/secure_exposure.cpp).
  bool ConfigureSecureMembers();
  // Media owner supplies its sole SameObject instance to the existing factory.
  void SetNavigatorMediaCapabilities(JS::HandleObject value);
  // HTML IsPlatformObjectSameOrigin for other owners (docs/owners/BROWSER_ENVIRONMENT.md):
  // `target` (a window global) as the current realm may see it - itself when
  // same origin, else its cross-origin WindowProxy - and the plain check.
  static bool WindowView(JSContext*, JS::HandleObject target, JS::MutableHandleValue out);
  static bool SameOriginWithCurrent(JSContext*, JS::HandleObject target, bool* same);
  static bool ReadNavigatorMediaCapabilities(JSContext*, JS::HandleValue,
                                            JS::MutableHandleValue);
  // Realm-local DOMStringList factory; the caller supplies an internal array.
  JSObject* StringList(JS::HandleObject values);
  // Runtime port to the Document Model's sole DOMException factory. The
  // signature matches Realm::ThrowException; bound after Document installation.
  void SetExceptionPort(bool (*port)(void*, const char*, const char*), void* context);
  // Runtime ports for the Window facade's postMessage task, bound after the
  // Scheduling and Interaction owners are installed and cleared before either
  // is destroyed (runtime/runtime.cpp).
  void SetPostTaskPort(bool (*port)(void*, JS::HandleObject), void* context);
  bool PostTask(JS::HandleObject task);
  void SetPostMessageTaskPort(bool (*port)(void*, JS::HandleObject, JS::HandleObject,
                                           JS::HandleObject, uint64_t, uint32_t), void* context);
  void RetireMessages();
  void SetMessagePort(bool (*port)(void*, JS::HandleObject, JS::HandleValue, const char*, JS::HandleObject, JS::HandleObject),
                      void* context);
  using ClonePort = bool (*)(void*, JS::HandleValue, JS::HandleValue, JS::HandleObject,
                            JS::MutableHandleValue, JS::MutableHandleObject);
  void SetClonePort(ClonePort, void*);
  // Runtime port for the History owner's traversal notification, bound next to
  // the message port and cleared before the Interaction owner is destroyed.
  void SetPopStatePort(bool (*port)(void*, JS::HandleObject, JS::HandleValue), void* context);
  // Runtime port to the single page-load coordinator. A JS-driven navigation
  // (Location.assign/href/component setters, replace, reload) commits the URL
  // and one session-history entry here and then loads the target document
  // through the same parse/execute path as the `page` RPC.
  // `reload` is location.reload(): the coordinator reloads the document as it was loaded.
  void SetNavigatePort(bool (*port)(void*, const char*, bool replace, bool reload), void* context);
  // Runtime port for the Location hash setter's trusted hashchange delivery.
  void SetHashChangePort(bool (*port)(void*, JS::HandleObject, const char*, const char*),
                         void* context);
  // Runtime port for the Window facade's reportError: one trusted event on the
  // interaction owner's dispatch tree. Bound next to the message ports.
  void SetEventPort(bool (*port)(void*, JS::HandleObject, const char*), void* context);
  void SetSendBeaconPort(BeaconPort port, void* context);
  // Debug-session port: reports accesses to the Window, Navigator, Screen,
  // Location and History members this owner registered. Bound by the shared
  // coordinator in debug mode and cleared before teardown.
  void SetApiMonitor(ApiMonitorPort port, void* context);
  bool Describe(JS::MutableHandleValue result);
  bool Defaults(JS::MutableHandleValue result);
  // Existing Navigator-based device/CSS capability, separate from Touch exposure.
  bool TouchCapable() const;
  // Private owner preparation and independent Window touch exposure decision.
  // Both propagate initialization failure; neither installs browser properties.
  bool Prepare();
  bool TouchEventsEnabled(bool* result);
  // Geometry reads the same private viewport/scroll source as Window scalars.
  bool ReadLayoutViewport(double* width, double* height, double* scrollX, double* scrollY);
  // The document's scrolling element (Document Model) scrolls the viewport
  // through the window's one scroll operation: same clamp, same events.
  bool ScrollViewportTo(double scrollX, double scrollY);
  JSObject* location() const;
  bool URL(JS::MutableHandleValue result);
  bool Origin(JS::MutableHandleValue result);
  // Typed source read from this original owner, independent of public globals.
  bool ReadSourceSnapshot(SourceSnapshot* result);
  // The profile's navigator.language (the browser locale), "" when unset.
  bool ReadLanguage(std::string* result);
  // The profile's navigator.platform ("Win32", "MacIntel", "Linux x86_64"...),
  // "" when unset. Selection/innerText serialization picks its line break by it.
  bool ReadPlatform(std::string* result);
  // locale.osLocale: the OS locale Firefox uses for Intl and untagged text
  // ("" when the host's own OS locale applies).
  bool ReadOsLocale(std::string* result);
  // The profile's window.devicePixelRatio (1 when unset).
  bool ReadDevicePixelRatio(double* result);
  // Commits a page-load navigation to an absolute HTTP(S) URL (RangeError otherwise).
  bool Navigate(const std::string& url);
  // Commits a JS-driven navigation (Location.assign/href/component setters,
  // replace, reload) to an absolute HTTP(S) URL: a full navigation appends a
  // document history entry, a replace navigation keeps the length. A
  // non-canonical target is a RangeError, matching the profile URL rule.
  bool CommitNavigation(const std::string& url, bool replace);
  // Commits the document URL of an iframe browsing context loaded by the
  // runtime frame owner through the one page-load coordinator. A child holds
  // no profile model of its own, so this is the child's Location source until
  // it loads again; the top-level Navigate/CommitNavigation stay the paths
  // that also write the profile model and the session history.
  bool SetFrameLocation(const std::string& url);
  // Commits the document's agent cluster (window.originAgentCluster) after its
  // URL committed: `header` is its Origin-Agent-Cluster response field ("" when
  // absent), honoured only in a secure context.
  bool CommitAgentCluster(const std::string& header);
 private:
  bool ReadNavigatorText(const char* name, std::string* result);
  State* state_;
};
}
#endif
