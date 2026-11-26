#ifndef RUYIJS_INTERACTION_REALM_H
#define RUYIJS_INTERACTION_REALM_H
#include "interaction/geometry_port.h"
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "interaction/close_watcher_ports.h"
#include "interaction/abort_ports.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction {
enum class PathResult { NotOwned, Ready, Failure };
using PathResolver = PathResult (*)(void*, JS::HandleObject, JS::HandleString, bool,
                                   JS::MutableHandleObjectVector, std::vector<uint8_t>*);
using RetargetResolver = bool (*)(void*, JS::HandleObject, JS::HandleObject,
                                  JS::MutableHandleObject, bool*);
using ExceptionReporter = bool (*)(void*, const char*, const char*);
// ResourceNetwork supplies only its native Storage instance identity. No data
// or owner pointer crosses this stateless port.
using StorageBrand = bool (*)(JSObject*);
// Debug-session port, mirroring document_model::ApiMonitorPort. The owner
// reports accesses to members its installer registered, and the access kind
// travels as a plain int so Interaction keeps no observability dependency.
// Null outside debug mode, and every call site then skips.
enum class ApiAccess : int { Read = 0, Write = 1, Call = 2, Construct = 3 };
using ApiMonitorPort = void (*)(void* context, const char* iface,
                                const char* member, int type);
// Reclaim port, mirroring the Document Model's own reclaim sink. The realm's
// 100000 target quota bounds targets that are still reachable, exactly like that
// owner's 100000 node quota, so when this quota is about to fail the owner runs
// the same real collection the node quota runs (T41): the class finalizer marks
// every collected node wrapper's borrowed record released, and attach() then
// drops them. Bound by the shared coordinator after installation, cleared
// before teardown; a null port reports the quota failure directly.
using ReclaimPort = void (*)(void* context);
// Original runtime notification before activation-triggering trusted input.
using InputActivationPort = bool (*)(void* context, JS::HandleObject target);
struct State;
// Host input uses the target's realm and the existing event factory/dispatch.
bool DispatchInput(JSContext*, JS::HandleObject target, const char* type,
                   JS::HandleObject init, double time_ms);
// Host user input (runtime/user_input.cpp): one trusted Mouse/Pointer,
// Keyboard, Input or Focus event; `allowed` reports it was not cancelled.
bool DispatchTrusted(JSContext*, JS::HandleObject target, const char* type,
                     JS::HandleObject init, double time_ms, bool* allowed);
// Firefox UserActivation::IsHandlingUserInput: true while a trusted host
// mousedown/mouseup/pointerdown/pointerup/click/auxclick/dblclick/contextmenu/
// keydown/keyup/keypress/touchstart/touchend is being delivered, including the
// microtask checkpoints that follow its listeners. Process-wide (any realm).
bool HandlingUserInput();
class Realm {
 public:
  Realm(JSContext* cx, JS::HandleObject page);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install(PageHelpers& helpers);
  // Second install phase, run by the shared coordinator after the main page's
  // profile is applied and before the Browser Environment installs: the
  // pref-dependent derived interface table. Firefox exposes TouchEvent only on
  // touch-capable configurations, so the capability comes from the Browser
  // Environment owner. Still exactly one production installer per interface.
  bool InstallDerived(bool touch_enabled);
  // Original touch interfaces are created once; visibility restores their saved
  // descriptors and identities from the independent Environment decision.
  bool ConfigureTouchExposure(bool enabled);
  // Removes or restores the [SecureContext] derived event interfaces for the
  // realm's current secure-context state (runtime/secure_exposure.cpp).
  bool ConfigureExposure();
  // Worker exposure reuses the same derived-interface table with Firefox's
  // Worker-only filter; it never installs Window-only helpers and does not
  // create a second constructor owner.
  // service: ServiceWorkerGlobalScope events; dedicated: [Exposed=DedicatedWorker]
  // events (RTCErrorEvent, RTCTransformEvent). Neither: a SharedWorker.
  bool InstallWorkerDerived(bool service = false, bool dedicated = false);
  JSObject* prototype() const;
  // Document.createEvent uses this realm's original event prototypes and state.
  // No public constructor call or independent event factory is installed.
  bool CreateLegacyEvent(std::u16string_view type, JS::HandleObject view,
                         JS::MutableHandleValue result);
  // Realm-created EventTarget (new EventTarget, Window, Screen, FontFaceSet):
  // the record roots the object.
  bool Attach(JS::HandleObject object);
  // A DOM node's wrapper: the record borrows the object and never roots it, so
  // the node's lifetime stays with the Document Model wrapper (T41). `record`
  // receives the one record that names the object, so the owner can release it
  // from the class finalizer without scanning the target registry.
  bool AttachBorrowed(JS::HandleObject object, void** record);
  // Marks one record returned by AttachBorrowed as released. It stores one flag
  // and allocates nothing, so the Document Model class finalizer may call it
  // while the engine is collecting the wrapper; the next prune() frees the
  // record and its listeners. A null record is ignored.
  void ReleaseBorrowed(void* record);
  // Follows the engine when it moves a borrowed wrapper object: the Document
  // Model's wrapper trace hook calls this so a node record keeps naming the
  // live object for its whole life, without ever rooting it (T41).
  void RefreshBorrowed(void* record, JSObject* object);
  // Fires a trusted event created by the runtime (page load lifecycle) at an
  // attached target. Listener exceptions are reported, not propagated.
  bool Fire(JS::HandleObject target, const char* type, bool bubbles, bool cancelable,
            bool* allowed = nullptr);
  // Fires the trusted ErrorEvent used by parser/module failures.  The
  // exception value is retained as ErrorEvent.error and the textual fields
  // are supplied by the runtime's exception adapter.
  bool FireError(JS::HandleObject target, JS::HandleValue error,
                 const char* message, const char* filename,
                 int32_t line = 0, int32_t column = 0,
                 bool* allowed = nullptr);
  bool FireClick(JS::HandleObject target, bool* allowed);
  bool FireSubmission(JS::HandleObject target, JS::HandleObject submitter,
                      JS::HandleObject data, bool* allowed);
  // Fires a trusted MessageEvent carrying an already-structured-cloned payload.
  // Used by the Window facade's postMessage task; delivery stays on the one
  // interaction dispatch tree.
  bool FireMessage(JS::HandleObject target, JS::HandleValue data, const char* origin,
                   JS::HandleObject source, JS::HandleObject ports = nullptr);
  // SharedWorker owner: a trusted MessageEvent of type "connect" carrying the
  // newly-entangled worker-side ports (data null, origin "", source null).
  bool FireConnect(JS::HandleObject target, JS::HandleObject ports);
  // EventSource (resource network): a trusted MessageEvent of the stream's event
  // type carrying data, origin and lastEventId; no source, no ports.
  bool FireNamedMessage(JS::HandleObject target, const std::u16string& type, JS::HandleValue data,
                        const char* origin, const std::u16string& lastEventId);
  // Service Worker owner: fires a trusted ExtendableEvent (or, with `message`, an
  // ExtendableMessageEvent from the init's data/origin/source/ports) and returns
  // the array of promises its listeners passed to waitUntil (event_extendable.cpp).
  bool FireExtendable(JS::HandleObject target, const char* type, JS::HandleObject init, bool message,
                      JS::MutableHandleObject lifetimes);
  // Fires a trusted PopStateEvent carrying the session-history state. Used by
  // the History owner's traversal (T9); delivery stays on the one dispatch tree.
  bool FirePopState(JS::HandleObject target, JS::HandleValue state);
  // Fires a trusted HashChangeEvent carrying the old and new fragment URLs.
  // Used by the Location owner's hash setter (T10).
  bool FireHashChange(JS::HandleObject target, const char* oldURL, const char* newURL);
  // The trusted scroll/scrollend UIEvent the window scroll owner requests after
  // the viewport moved (runtime_scroll.cpp). `target` is the document.
  // A document's scroll bubbles to its window; an element's does not.
  bool FireScroll(JS::HandleObject target, const char* type, bool bubbles = true);
  // The unload sequence of a top-level navigation (docs/owners/TOP_LEVEL.md):
  // a trusted cancelable BeforeUnloadEvent when the navigation starts and a
  // trusted PageTransitionEvent (pagehide) when its document is unloaded.
  bool FireBeforeUnload(JS::HandleObject target);
  bool FirePageTransition(JS::HandleObject target, const char* type, bool persisted);
  // Fires a trusted OfflineAudioCompletionEvent carrying the rendered
  // AudioBuffer. Used by the Web Audio owner's startRendering (T13c).
  bool FireOfflineAudioComplete(JS::HandleObject target, JS::HandleValue buffer);
  // Fires a trusted SecurityPolicyViolationEvent ("securitypolicyviolation")
  // from a SecurityPolicyViolationEventInit dictionary built by the Content
  // Security owner (docs/owners/CONTENT_SECURITY.md); bubbles/composed come
  // from that dictionary.
  bool FirePolicyViolation(JS::HandleObject target, JS::HandleObject init);
  // Fires a trusted SpeechSynthesisEvent (or, with `error`, a
  // SpeechSynthesisErrorEvent) for the Speech Synthesis owner
  // (docs/owners/SPEECH_SYNTHESIS.md). `init` carries utterance, charIndex,
  // charLength, elapsedTime, name (and error); the getters read it.
  bool FireSpeech(JS::HandleObject target, const char* type, JS::HandleObject init, bool error);
  // The Navigation owner's trusted navigate / currententrychange events.
  bool FireNavigate(JS::HandleObject target, JS::HandleObject init, bool* allowed);
  bool FireNavigationEntryChange(JS::HandleObject target, JS::HandleObject init);
  // The WebRTC owner's trusted RTCPeerConnectionIceEvent ("icecandidate").
  bool FireIceCandidate(JS::HandleObject target, JS::HandleValue candidate);
  bool FireTaskPriority(JS::HandleObject target,JS::HandleObject prototype,const char* previous,double time_ms);
  // Fires the trusted CookieChangeEvent ("change") at a CookieStore object for
  // the Resource Network owner: `changed` and `deleted` are the lists the jar
  // reported, already built in this realm.
  bool FireCookieChange(JS::HandleObject target, JS::HandleValue changed, JS::HandleValue deleted);
  // Dispatches an event built through the installed interface factory (T4's
  // HTMLElement.click). Delivery stays on the one dispatch tree.
  bool Dispatch(JS::HandleObject target, JS::HandleObject event);
  // window.event: the event whose listener is running in this realm, else undefined.
  bool CurrentEvent(JS::MutableHandleValue out);
  // Brand checks of other owners' objects for event init dictionaries:
  // brand 0 Element, 1 MediaStreamTrack, 2 DOMException. Bound by the runtime.
  void SetBrandPort(bool (*port)(void*, int brand, JS::HandleValue, bool* matches), void* context);
  // The document clock for Event.timeStamp: creation time relative to the
  // document's time origin, and its content precision reduction.
  // MouseEvent pageX/offsetX/layerX and their Y counterparts read the target
  // document's layout through this port (bound by the runtime coordinator).
  void SetGeometryPort(GeometryPort port, void* context);
  void SetEventClockPort(double (*now)(void*, double clock_ms), double (*reduce)(void*, double),
                         void* context);
  // The Scheduling owner's microtask checkpoint. HTML "clean up after running
  // script": a listener invoked with an empty JavaScript execution context
  // stack (native dispatch: user input, tasks) is followed by a checkpoint, so
  // its promise reactions run before the next listener; a script's
  // dispatchEvent keeps them until that script returns.
  void SetMicrotaskCheckpointPort(void (*port)(void*), void* context);
  // FileList factory (Web Platform) for DataTransfer.files.
  void SetFileListPort(bool (*port)(void*, JS::HandleObject, JS::MutableHandleObject), void* context);
  // Independent ports, bound by the shared coordinator after owner installation.
  // Clear both before their context owners are destroyed, after the final drain.
  void SetPathResolver(PathResolver callback, RetargetResolver retarget, void* context);
  void SetExceptionReporter(ExceptionReporter callback, void* context);
  void SetStorageBrand(StorageBrand callback);
  void SetApiMonitor(ApiMonitorPort callback, void* context);
  void SetReclaimPort(ReclaimPort callback, void* context);
  void SetInputActivationPort(InputActivationPort callback, void* context);
  void SetCloseWatcherPorts(const CloseWatcherPorts&,void* context);
  void SetAbortPorts(const AbortPorts&);
  void NotifyCloseWatcherActivation();
  bool ProcessCloseWatchers();
  bool PrepareCloseWatcherDocument(JS::MutableHandleObject);
  void CommitCloseWatcherDocument(JS::HandleObject);
  void RetireCloseWatchers();
 private:
  std::unique_ptr<State> state_;
};
}
#endif
