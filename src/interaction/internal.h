#ifndef RUYIJS_INTERACTION_INTERNAL_H
#define RUYIJS_INTERACTION_INTERNAL_H
#include "runtime/secure_slot.h"
#include "runtime/weak_object.h"
#include "interaction/realm.h"
#include "interaction/dispatch_path.h"
#include "domain/interaction/event_dispatch_service.h"
#include <chrono>
#include <vector>
#include "js/CallArgs.h"
#include "js/PropertySpec.h"
#include "js/PropertyDescriptor.h"
#include "jsapi.h"

namespace ruyijs::interaction {
namespace domain = ruyijs::domain::interaction;
// Event brands and installed interfaces share names for materialized events.
// Interfaces with no native factory have no Kind; IsA owns the explicit mapping.
enum class Kind : uint8_t { Event, Custom, UI, Mouse, Pointer, Keyboard,
                            Focus, Input, Wheel, Touch, Progress, Message,
                            Error, Close, Storage, PopState, HashChange,
                            OfflineAudioComplete, PageTransition,
                            PromiseRejection, Submit, Drag, Clipboard,
                            Composition, Transition, Animation,
                            PolicyViolation, MediaQueryList, BeforeUnload,
                            DeviceMotion, DeviceOrientation, RtcIce,
                            RtcDataChannel, RtcTrack, AnimationPlayback,
                            BlobPayload, ContentVisibility, FormDataPayload, Toggle,
                            Gamepad, TextInput, NavigationEntryChange, Speech, SpeechError,
                            // A trusted SpeechSynthesisEvent fired by the Speech Synthesis owner:
                            // a full Event, unlike the placeholder public construction above.
                            SpeechTrusted,
                            MidiConnection, MidiMessage, CookieChange, PictureInPicture, Navigate,
                            MediaEncrypted, MediaKeyMessage, MediaStream, MediaTrack,
                            RtcTone, RtcErrorPayload, RtcIceError,
                            AudioProcessing, FontLoad, GpuUncaptured, TaskPriority, IdbVersionChange,
                            PopupBlocked, TimeEvent, MouseScroll, ScrollArea, Command, MediaStreamTrack, MediaRecorderError,
                            Extendable, ExtendableMessage, Fetch, RtcTransform };
enum class Iface : uint8_t { Event, Custom, UI, Mouse, Pointer, Keyboard,
                             Focus, Input, Wheel, Touch, Progress, Message,
                             Error, Close, Storage, PopState, HashChange,
                             OfflineAudioComplete, PageTransition,
                             PromiseRejection, Submit, Drag, Clipboard,
                             Composition, Transition, Animation,
                             PolicyViolation, MediaQueryList, BeforeUnload,
                             DeviceMotion, DeviceOrientation, RtcIce,
                             RtcDataChannel, RtcTrack, AnimationPlayback,
                             BlobPayload, ContentVisibility, FormDataPayload, Toggle,
                             Gamepad, TextInput, NavigationEntryChange, Speech, SpeechError,
                             MidiConnection, MidiMessage, CookieChange, PictureInPicture, Navigate,
                             MediaEncrypted, MediaKeyMessage, MediaStream, MediaTrack,
                             RtcTone, RtcErrorPayload, RtcIceError,
                             AudioProcessing, FontLoad, GpuUncaptured, TaskPriority, IdbVersionChange,
                             DocumentPicture, PopupBlocked, TimeEvent, MouseScroll, ScrollArea, Command, MediaStreamTrack, MediaRecorderError,
                            Extendable, ExtendableMessage, Fetch, RtcTransform };
bool IsA(Kind actual, Iface required);
struct Event;
// True while any realm on this thread has a debug monitor bound. The hot
// event accessors test it once before the owner lookup, so a non-debug page
// keeps a single false branch instead of an always-on event scan.
bool MonitorBound();
// Install-time interface label of one event interface,
// e.g. Iface::Mouse -> "MouseEvent". Static names only.
const char* InterfaceName(Iface);
// Reports one access to an installed event member, after the accessor has
// validated its receiver. No-op when no monitor is bound or the event's
// realm has no port.
void ReportEventAccess(JSContext*, Event*, const char* iface, const char* member, int type);
// Reserved-slot layout of every event object. Subclass property values live
// in the slots so the traced object keeps them alive, and only the event
// kind lives in the C++ record used by the dispatch path.
enum EventSlot : uint32_t {
  kEventKind = 0, kEventUiDetail, kEventView, kEventCustomDetail,
  kEventClientX, kEventClientY, kEventScreenX, kEventScreenY,
  kEventMovementX, kEventMovementY, kEventButton, kEventButtons,
  kEventModifiers, kEventRelatedTarget,
  kEventKey, kEventCode, kEventLocation, kEventKeyboardFlags,
  kEventPointerId, kEventPointerType, kEventWidth, kEventHeight,
  kEventPressure, kEventTiltX, kEventTiltY, kEventTwist, kEventIsPrimary,
  kEventTangentialPressure, kEventAltitudeAngle, kEventAzimuthAngle,
  kEventInputData, kEventInputType, kEventInputFlags,
  kEventDeltaX, kEventDeltaY, kEventDeltaZ, kEventDeltaMode,
  kEventTouches, kEventTargetTouches, kEventChangedTouches,
  kEventLengthComputable, kEventLoaded, kEventTotal,
  kEventMessageData, kEventOrigin, kEventLastEventId, kEventMessageSource,
  kEventPorts,
  kEventErrorMessage, kEventErrorFilename, kEventLineno, kEventColno,
  kEventErrorValue,
  kEventCloseCode, kEventCloseReason, kEventWasClean,
  kEventStorageKey, kEventStorageOldValue, kEventStorageNewValue,
  kEventStorageUrl, kEventStorageArea,
  kEventPopState, kEventHashChangeOldUrl, kEventHashChangeNewUrl,
  kEventRenderedBuffer,
  kEventPagePersisted,
  kEventPromise, kEventRejectionReason,
  kEventSubmitter,
  kEventDragDataTransfer,
  kEventCompositionData,
  kEventTransitionProperty, kEventTransitionElapsed, kEventTransitionPseudo,
  kEventTransitionAnimation,
  kEventAnimationName, kEventAnimationElapsed, kEventAnimationPseudo,
  kEventAnimationObject,
  kEventPolicyDocumentUri, kEventPolicyReferrer, kEventPolicyBlockedUri,
  kEventPolicyViolatedDirective, kEventPolicyEffectiveDirective,
  kEventPolicyOriginalPolicy, kEventPolicySourceFile, kEventPolicySample,
  kEventPolicyDisposition, kEventPolicyStatusCode, kEventPolicyLineNumber,
  kEventPolicyColumnNumber,
  kEventMediaQueryMedia, kEventMediaQueryMatches,
  kEventBeforeUnloadText,
  kEventMotionAcceleration, kEventMotionGravity, kEventMotionRotation,
  kEventMotionInterval,
  kEventOrientationAlpha, kEventOrientationBeta, kEventOrientationGamma,
  kEventOrientationAbsolute,
  kEventIceCandidate,
  kEventChannel,
  kEventTrackReceiver, kEventTrack, kEventTrackStreams, kEventTrackTransceiver,
  kEventPublicFirst, kEventPublicSecond, kEventPublicThird, kEventPublicFourth, kEventPublicFifth,
  // MouseEvent's Mozilla extension shares pressure with PointerEvent but has
  // its own traced source value.
  kEventInputSource,
  // ExtendableEvent: the waitUntil promises of a trusted dispatched event (an
  // array only on events the Service Worker owner fires).
  kEventLifetimes,
  kEventSlotCount
};
// One EventTarget record. A realm-created object (`new EventTarget`, Window,
// Screen, FontFaceSet) is rooted here because no other owner keeps it alive. A
// DOM node passes its existing wrapper instead: that record must never extend
// the wrapper's lifetime, so its object is borrowed and the Document Model
// releases the record when the wrapper is collected (T41). `released` marks a
// borrowed object that died; prune() then removes the record and its listeners.
struct Target {
  JSObject* object;
  JS::PersistentRootedObject root;
  domain::EventTargetAggregate aggregate;
  const bool owned;
  bool released = false;
  Target(JSContext* cx, JS::HandleObject value, uint64_t id, bool owns)
      : object(value), root(cx), aggregate(id), owned(owns) {
    if (owns) root.set(value);
  }
  // The object the engine holds right now. An owned object is read back from
  // its persistent root, which every collection updates in place; a borrowed
  // node wrapper is refreshed by that wrapper's own trace hook, because a
  // realm-created record must never keep a node wrapper's object alive (T41).
  // A borrowed wrapper an incremental sweep already doomed reads as absent
  // until its finalizer releases this record.
  JSObject* live() const { return owned ? root.get() : WeakPeek(object); }
};
struct Event {
  JS::PersistentRootedObject object;
  JS::PersistentRootedString type;
  domain::DomEvent value;
  Kind kind = Kind::Event;
  // Borrowed only from this Event's active stack scope, in root-to-target order.
  PreparedPath* dispatchPath = nullptr;
  Event(JSContext* cx, JS::HandleObject wrapper, JS::HandleString text,
        std::string key, bool bubbles, bool cancelable)
      : object(cx, wrapper), type(cx, text),
        value(domain::EventType(std::move(key)), bubbles, cancelable) {}
};
struct Listener {
  JS::PersistentRootedObject callback;
  uint64_t target;
  domain::ListenerId id;
  std::string type;
  bool capture;
  Listener(JSContext* cx, JS::HandleObject value, uint64_t owner,
           domain::ListenerId key, std::string text, bool capturing)
      : callback(cx, value), target(owner), id(key), type(std::move(text)), capture(capturing) {}
};
struct CloseLifetime;
struct State {
  JSContext* cx;
  PageHelpers* helpers = nullptr;
  // FileList factory of the Web Platform owner (DataTransfer.files).
  bool (*fileList)(void*, JS::HandleObject, JS::MutableHandleObject) = nullptr;
  void* fileListContext = nullptr;
  State* previous;
  JS::PersistentRootedObject page, targetPrototype, eventPrototype, trustedGetter;
  // [SecureContext] derived events (MIDI*, DocumentPictureInPictureEvent).
  SecureSlots secure{cx};
  JS::PersistentRootedObject speechTrustedGetter, speechErrorTrustedGetter;
  JS::PersistentRootedObject pictureInPictureTrustedGetter;
  JS::PersistentRootedObject navigateTrustedGetter;
  JS::PersistentRootedObject mediaEncryptedTrustedGetter, mediaKeyTrustedGetter;
  JS::PersistentRootedObject mediaStreamTrustedGetter, mediaTrackTrustedGetter;
  JS::PersistentRootedObject rtcToneTrustedGetter, rtcErrorTrustedGetter, rtcIceErrorTrustedGetter;
  JS::PersistentRootedObject audioProcessingTrustedGetter, fontLoadTrustedGetter, taskPriorityTrustedGetter;
  bool publicWindowTrusted = false;
  JS::PersistentRootedObject mousePrototype, pointerPrototype;
  JS::PersistentRootedObject legacyPrototypes;
  JS::PersistentRootedObject touchPrototype, touchListPrototype;
  JS::PersistentRooted<JS::PropertyDescriptor> touchDescriptor, touchListDescriptor, touchEventDescriptor;
  bool touchInstalled = false, touchEnabled = false;
  JS::PersistentRootedObject transferPrototype, transferListPrototype, transferItemPrototype;
  // The two [LegacyNoInterfaceObject] helper shapes of DeviceMotionEvent
  // (DeviceAcceleration/DeviceRotationRate): no global constructor, one
  // prototype per realm, owned by event_supplemental.cpp.
  JS::PersistentRootedObject deviceAccelerationPrototype,
      deviceRotationRatePrototype;
  PathResolver pathResolver = nullptr;
  RetargetResolver retargetResolver = nullptr;
  void* pathContext = nullptr;
  ExceptionReporter exceptionReporter = nullptr;
  StorageBrand storageBrand = nullptr;
  void* exceptionContext = nullptr;
  ApiMonitorPort apiMonitor = nullptr;
  void* apiMonitorContext = nullptr;
  InputActivationPort inputActivation = nullptr;
  void* inputActivationContext = nullptr;
  JS::PersistentRootedObject closeFactory{cx},closeManager{cx};
  CloseLifetime* closeLifetimes = nullptr;
  CloseWatcherPorts closePorts;
  AbortPorts abortPorts;
  void* closeContext = nullptr;
  ReclaimPort reclaim = nullptr;
  void* reclaimContext = nullptr;
  // Reports one access to an install-registered member of this owner. A null
  // port (non-debug mode) skips without touching engine state.
  void Observe(const char* iface, const char* member, int type) const {
    if (apiMonitor) apiMonitor(apiMonitorContext, iface, member, type);
  }
  std::vector<std::unique_ptr<Target>> targets;
  std::vector<std::unique_ptr<Event>> events;
  std::vector<std::shared_ptr<Listener>> listeners;
  std::unordered_map<uint64_t, domain::EventTargetAggregate*> targetMap;
  // Borrowed node wrapper address to its Target. A node wrapper's trace hook
  // reports each move (RefreshBorrowed), so the key follows the object. Owned
  // targets (script EventTargets, XHR, Window...) live in a persistent root
  // that a collection updates without telling anyone, so they are not keyed
  // by address: they stay in `ownedTargets`, which is short.
  std::unordered_map<JSObject*, Target*> targetByObject;
  std::vector<Target*> ownedTargets;
  void refresh(Target* record, JSObject* object);
  std::unordered_map<domain::ListenerId, domain::ListenerCallback> callbacks;
  std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  State(JSContext* context, JS::HandleObject global);
  // An event's creation timestamp (Event::mEvent->mTimeStamp as a DOMHighRes time).
  double EventTime(double clock_ms = -1) const;
  ~State();
  Target* target(JSObject* object);
  Event* event(JSObject* object);
  Event* event(domain::DomEvent& value);
  JSObject* targetObject(uint64_t id);
  // `owns` roots the object for the realm (an object the realm created); a DOM
  // node passes false and is released through release() instead. `record`
  // receives the record that names the object.
  bool attach(JS::HandleObject object, bool owns, void** record);
  void release(void* record);
  // Drops every released record and the listeners that named it. Runs before the
  // next attach and after a borrowed wrapper was collected. The flag keeps the
  // common case - nothing released - from walking the registry.
  bool anyReleased = false;
  void prune();
  void invoke(std::shared_ptr<Listener> listener, domain::DomEvent& event, uint64_t target);
  // The DOM "current event" of this global while a listener runs (window.event):
  // the address of invoke()'s rooted argument, restored when the call returns.
  const JS::Value* currentEvent = nullptr;
  bool (*brandPort)(void*, int, JS::HandleValue, bool*) = nullptr;
  // The document clock (Scheduling): an event's creation time relative to the
  // document's time origin (`clock_ms` < 0: now; otherwise a Scheduling clock
  // time), and Event.timeStamp's precision reduction on that timeline.
  double (*eventClock)(void*, double clock_ms) = nullptr;
  double (*eventReduce)(void*, double ms) = nullptr;
  void* eventClockContext = nullptr;
  void (*microtaskCheckpoint)(void*) = nullptr;
  void* microtaskContext = nullptr;
  void* brandContext = nullptr;
  void cleanup();
};
bool NotifyInputActivation(State&, JS::HandleObject target, JS::HandleObject event, const char* type);
extern thread_local State* activeState;
// The realm state whose page global is the current realm global of `cx`, or
// null when no installed realm matches. The derived-constructor path and the
// T3c device adapters that materialize per-realm helper objects share it.
State* RealmState(JSContext* cx);
State* Owner(JSContext* cx, JSObject* object, bool event);
bool Error(JSContext* cx, const char* name, const char* message);
bool Text(JSContext* cx, JS::HandleValue value, JS::MutableHandleString text, std::string* key);
bool Option(JSContext* cx, JS::HandleObject object, const char* name, bool* value);
bool InstallEvent(State& state);
// Event instances expose isTrusted as an own enumerable accessor (ruyijs parity and
// monitor visibility), installed by one shared helper for every constructor.
bool InstallTrusted(State& state, JS::HandleObject object);
enum class Exposure : uint8_t { Window, Worker, ServiceWorker, DedicatedWorker };
bool InstallDerivedEvents(State& state, bool touch_enabled,
                          Exposure exposure = Exposure::Window);
bool InstallDataTransfer(State& state);
// event_fields.cpp owns the per-interface property/method tables and the
// KeyboardEvent/WheelEvent constant lists.
struct EventConstant { const char* name; int32_t value; };
extern const JSPropertySpec kEventProperties[];
extern const JSPropertySpec kCustomProperties[];
extern const JSPropertySpec kUiProperties[];
extern const JSPropertySpec kMouseProperties[];
extern const JSPropertySpec kPointerProperties[];
extern const JSPropertySpec kKeyboardProperties[];
extern const JSPropertySpec kFocusProperties[];
extern const JSPropertySpec kInputProperties[];
extern const JSPropertySpec kWheelProperties[];
extern const JSPropertySpec kTouchProperties[];
extern const JSPropertySpec kProgressProperties[];
extern const JSPropertySpec kMessageProperties[];
extern const JSPropertySpec kErrorProperties[];
extern const JSPropertySpec kCloseProperties[];
extern const JSPropertySpec kStorageProperties[];
// event_navigation.cpp owns the navigation-event payload tables (T9/T10), which
// stay out of event_fields.cpp so no table TU grows past its split budget.
extern const JSPropertySpec kPopStateProperties[];
extern const JSPropertySpec kHashChangeProperties[];
// event_offline_audio.cpp owns the Web Audio completion payload (T13c).
extern const JSPropertySpec kOfflineAudioCompleteProperties[];
// The T3c Event subclasses are split into four table TUs so no table TU grows
// past its split budget: event_supplemental.cpp owns the direct Event payload
// classes, event_input_extra.cpp the MouseEvent/UIEvent-derived classes with
// their legacy init*Event methods, event_device.cpp the two device sensor
// classes plus the [LegacyNoInterfaceObject] helper prototypes, and
// event_rtc.cpp the WebRTC payload classes.
extern const JSPropertySpec kPageTransitionProperties[];
extern const JSPropertySpec kPromiseRejectionProperties[];
extern const JSPropertySpec kSubmitProperties[];
extern const JSPropertySpec kDragProperties[];
extern const JSPropertySpec kClipboardProperties[];
extern const JSPropertySpec kCompositionProperties[];
extern const JSPropertySpec kTransitionProperties[];
extern const JSPropertySpec kAnimationProperties[];
extern const JSPropertySpec kPolicyViolationProperties[];
extern const JSPropertySpec kMediaQueryListProperties[];
extern const JSPropertySpec kBeforeUnloadProperties[];
extern const JSPropertySpec kDeviceMotionProperties[];
extern const JSPropertySpec kDeviceOrientationProperties[];
extern const JSPropertySpec kRtcIceProperties[];
extern const JSPropertySpec kRtcDataChannelProperties[];
extern const JSPropertySpec kRtcTrackProperties[];
extern const JSPropertySpec kAnimationPlaybackProperties[];
extern const JSPropertySpec kBlobPayloadProperties[];
extern const JSPropertySpec kContentVisibilityProperties[];
extern const JSPropertySpec kFormDataPayloadProperties[];
extern const JSPropertySpec kToggleProperties[];
extern const JSPropertySpec kGamepadProperties[];
extern const JSPropertySpec kTextInputProperties[];
extern const JSFunctionSpec kTextInputMethods[];
extern const JSPropertySpec kNavigationEntryChangeProperties[];
bool InitNavigationEntryChange(JSContext*, JS::HandleObject, JS::HandleObject);
extern const JSPropertySpec kSpeechProperties[], kSpeechErrorProperties[];
bool InitSpeechFields(JSContext*, Iface, JS::HandleObject, JS::HandleObject);
bool PublicEventString(JSContext*, JS::HandleValue, const char*, JS::MutableHandleString);
bool PublicFormattedEventType(JSContext*, JS::HandleValue, JS::MutableHandleString, std::string*);
bool InstallSpeechTrusted(State&, JS::HandleObject, Kind);
extern const JSPropertySpec kMidiConnectionProperties[], kMidiMessageProperties[];
extern const JSPropertySpec kCookieChangeProperties[], kPictureInPictureProperties[];
bool InitPeripheralFields(JSContext*, Iface, JS::HandleObject, JS::HandleObject);
bool InstallPictureInPictureTrusted(State&, JS::HandleObject);
bool IsPeripheralEvent(Iface);
extern const JSPropertySpec kNavigateProperties[];
extern const JSFunctionSpec kNavigateMethods[];
bool InitNavigateFields(JSContext*, JS::HandleObject, JS::HandleObject);
bool InstallNavigateTrusted(State&, JS::HandleObject);
extern const JSPropertySpec kMediaEncryptedProperties[], kMediaKeyMessageProperties[];
extern const JSPropertySpec kMediaStreamProperties[], kMediaTrackProperties[];
bool IsMediaPayloadEvent(Iface);
bool InitMediaPayloadFields(JSContext*, Iface, JS::HandleObject, JS::HandleObject);
bool InstallMediaEventTrusted(State&, JS::HandleObject, Kind);
extern const JSPropertySpec kRtcToneProperties[], kRtcErrorPayloadProperties[], kRtcIceErrorProperties[];
bool IsRtcExtraEvent(Iface);
bool InitRtcExtraFields(JSContext*, Iface, JS::HandleObject, JS::HandleObject);
bool RtcEventUnsignedShort(JSContext*, JS::HandleValue, uint16_t*);
bool InstallRtcExtraTrusted(State&, JS::HandleObject, Kind);
// [Exposed=DedicatedWorker] events (RTCTransformEvent): not Window, SharedWorker or ServiceWorker.
bool IsDedicatedWorkerEvent(Iface);
extern const JSPropertySpec kRtcTransformProperties[];
extern const JSPropertySpec kAudioProcessingProperties[], kFontLoadProperties[];
extern const JSPropertySpec kGpuUncapturedProperties[], kTaskPriorityProperties[];
bool IsProcessingEvent(Iface);
bool InitProcessingFields(JSContext*, Iface, JS::HandleObject, JS::HandleObject);
bool InstallProcessingTrusted(State&, JS::HandleObject, Kind);
bool CallProcessingEvent(JSContext*, const JS::CallArgs&, Iface);
bool ReadPublicWindowTrusted(JSContext*, const JS::CallArgs&, bool cross_realm);
extern const JSPropertySpec kIdbVersionChangeProperties[];
bool InitIdbVersionChange(JSContext*, JS::HandleObject, JS::HandleObject);
extern const JSPropertySpec kDocumentPictureProperties[];
bool DocumentPictureConstructor(JSContext*, unsigned, JS::Value*);
bool IsPublicEvent(Iface);
// event_firefox_payloads.cpp: WebIDL-exact Firefox event interfaces.
bool IsFirefoxPayloadEvent(Iface);
bool PopupBlockedConstructor(JSContext*, unsigned, JS::Value*);
bool CommandConstructor(JSContext*, unsigned, JS::Value*);
bool MediaStreamTrackConstructor(JSContext*, unsigned, JS::Value*);
bool MediaRecorderErrorConstructor(JSContext*, unsigned, JS::Value*);
bool RTCTransformConstructor(JSContext*, unsigned, JS::Value*);
// Service Worker events (event_extendable.cpp), exposed only in a ServiceWorkerGlobalScope.
bool IsServiceWorkerEvent(Iface);
bool InitFetch(JSContext*, JS::HandleObject, JS::HandleObject);
extern const JSFunctionSpec kExtendableMethods[], kFetchMethods[];
extern const JSPropertySpec kExtendableMessageProperties[], kFetchProperties[];
extern const JSPropertySpec kPopupBlockedProperties[], kCommandProperties[], kMediaStreamTrackProperties[],
    kMediaRecorderErrorProperties[], kTimeEventProperties[], kMouseScrollProperties[], kScrollAreaProperties[];
extern const JSFunctionSpec kTimeEventMethods[], kMouseScrollMethods[], kScrollAreaMethods[];
bool InitLegacyMouseEventFields(JSContext*, Event*, const JS::CallArgs&, bool extended);
// Brands the Interaction brand port answers (bound by the runtime to their owners).
enum EventBrand : int { kBrandElement = 0, kBrandMediaStreamTrack = 1, kBrandDomException = 2,
                       kBrandRtcScriptTransformer = 3 };
unsigned PublicEventArity(Iface);
bool CallPublicEvent(JSContext*, const JS::CallArgs&, Iface);
bool PublicEventType(JSContext*, JS::HandleValue, JS::MutableHandleString, std::string*);
bool PublicEventRead(JSContext*, JS::HandleObject, const char*, JS::MutableHandleValue,
                     bool* failed = nullptr);
bool ReadPublicEventBase(JSContext*, Iface, JS::HandleValue, bool*, bool*, bool*);
bool InitPublicEventFields(JSContext*, Iface, JS::HandleObject, JS::HandleObject);
bool InstallEventCoordinates(State&, Iface, JS::HandleObject);
extern const JSFunctionSpec kCustomMethods[];
extern const JSFunctionSpec kDragMethods[];
extern const JSFunctionSpec kCompositionMethods[];
extern const JSFunctionSpec kDeviceMotionMethods[];
extern const JSFunctionSpec kDeviceOrientationMethods[];
bool InstallSupplementalEventObjects(State& state);
// One dictionary adapter per T3c table TU (event_init.cpp maps each interface
// onto its owner); the Drag/Composition pair reuses the UIEvent/MouseEvent
// field writers declared below instead of copying them.
bool InitPagePayloadFields(JSContext*, Iface, JS::HandleObject dict,
                           JS::HandleObject object);
bool InitDrag(JSContext*, JS::HandleObject dict, JS::HandleObject object);
bool InitComposition(JSContext*, JS::HandleObject dict, JS::HandleObject object);
bool InitDeviceFields(JSContext*, Iface, JS::HandleObject dict,
                      JS::HandleObject object);
bool InitRtcFields(JSContext*, Iface, JS::HandleObject dict,
                   JS::HandleObject object);
extern const JSFunctionSpec kMouseMethods[];
extern const JSFunctionSpec kUiMethods[];
extern const JSFunctionSpec kMessageMethods[];
extern const JSFunctionSpec kStorageMethods[];
bool InitMouseEvent(JSContext*, unsigned, JS::Value*);
bool InitNSMouseEvent(JSContext*, unsigned, JS::Value*);
bool InitKeyboardEvent(JSContext*, unsigned, JS::Value*);
extern const JSFunctionSpec kPointerMethods[];
extern const JSFunctionSpec kKeyboardMethods[];
extern const JSFunctionSpec kInputMethods[];
extern const EventConstant kKeyboardConstants[];
extern const EventConstant kWheelConstants[];
extern const EventConstant kMouseScrollConstants[];
extern const EventConstant kMouseConstants[];
// event_init.cpp / event_fields.cpp: init dictionary reading and the traced
// slot materialization shared by the Event and derived constructors.
void SetSlotNull(JS::HandleObject, uint32_t);
void SetSlotBool(JS::HandleObject, uint32_t, bool);
void SetSlotInt(JS::HandleObject, uint32_t, int32_t);
void SetSlotNumber(JS::HandleObject, uint32_t, double);
bool SetSlotText(JSContext*, JS::HandleObject, uint32_t, const std::string&);
bool SetSlotNullable(JSContext*, JS::HandleObject, uint32_t, bool, const std::string&);
bool InitEmptyArray(JSContext*, JS::MutableHandleValue);
bool ReadInitBool(JSContext*, JS::HandleObject, const char*, bool*);
bool ReadInitInt(JSContext*, JS::HandleObject, const char*, int*);
bool ReadInitNumber(JSContext*, JS::HandleObject, const char*, double*);
bool ReadInitString(JSContext*, JS::HandleObject, const char*, std::string*);
bool ReadInitNullableString(JSContext*, JS::HandleObject, const char*, bool*, std::string*);
bool ReadInitValue(JSContext*, JS::HandleObject, const char*, JS::MutableHandleValue);
// WebIDL conversions of the interface-typed dictionary members the T3c payload
// classes carry. The nullable form takes null for a missing, undefined or null
// member and throws the conversion TypeError for a primitive; the required form
// throws the required-member TypeError for a missing member as well. An
// accepted object is carried as-is: the interface that would brand-check it
// (HTMLElement, DataTransfer, RTCDataChannel, CSSAnimation, ...) belongs to
// another owner.
bool ReadNullableObject(JSContext*, JS::HandleObject dict, const char* name,
                        JS::MutableHandleValue out);
bool ReadRequiredObject(JSContext*, JS::HandleObject dict, const char* name,
                        JS::MutableHandleValue out);
// Legacy init*Event support, shared by the four interfaces that expose one
// (DragEvent, CompositionEvent, DeviceMotionEvent, DeviceOrientationEvent).
// LegacyBase replaces type/bubbles/cancelable with Event::InitEvent semantics;
// the rest read one optional argument of the legacy signature with the spec
// defaults, and every one of them is a no-op while the event dispatches.
bool LegacyBase(JSContext*, Event*, const JS::CallArgs&);
bool LegacyInt(JSContext*, const JS::CallArgs&, unsigned index, int* out);
bool LegacyBool(const JS::CallArgs&, unsigned index, bool* out);
bool LegacyObject(JSContext*, const JS::CallArgs&, unsigned index,
                  JS::MutableHandleValue out);
bool LegacyString(JSContext*, const JS::CallArgs&, unsigned index, std::string* out);
bool LegacyDictionary(JSContext*, const JS::CallArgs&, unsigned index,
                      JS::MutableHandleObject out);
bool LegacyNullableDouble(JSContext*, const JS::CallArgs&, unsigned index,
                          bool* is_null, double* out);
bool ReadEventBase(JSContext*, JS::HandleValue, bool*, bool*, bool*);
bool InitEventFields(JSContext*, Iface, JS::HandleValue, JS::HandleObject);
// event_init.cpp shares the UIEvent/MouseEvent field writers with the T3c
// classes whose dictionaries extend them (DragEvent, CompositionEvent).
bool InitUiFields(JSContext*, JS::HandleObject, JS::HandleObject);
bool InitMouseFields(JSContext*, JS::HandleObject, JS::HandleObject);
bool InitPopState(JSContext*, JS::HandleObject, JS::HandleObject);
bool InitHashChange(JSContext*, JS::HandleObject, JS::HandleObject);
bool InitOfflineAudioComplete(JSContext*, JS::HandleObject, JS::HandleObject);
bool InstallTarget(State& state);
bool Tag(JSContext* cx, JS::HandleObject object, const char* value);
bool EventConstructor(JSContext*, unsigned, JS::Value*);
bool TargetConstructor(JSContext*, unsigned, JS::Value*);
bool Add(JSContext*, unsigned, JS::Value*);
bool Remove(JSContext*, unsigned, JS::Value*);
bool Dispatch(JSContext*, unsigned, JS::Value*);
Event* Receiver(JSContext* cx, const JS::CallArgs& args);
// Brand-checked receiver: the object must be an installed event whose kind
// implements the requested interface (a PointerEvent is a MouseEvent).
Event* Receiver(JSContext* cx, const JS::CallArgs& args, Iface required,
                bool cross_realm = false);
// Receiver of the three EventTarget members. A bare call hands a JSNative an
// undefined this value, and the donor's MaybeGlobalThisPolicy
// (dom/bindings/BindingUtils.cpp:2863) resolves null/undefined to the callee
// realm's global object - the Window this runtime already registers as a
// target. Any other primitive this value is rejected, and every resolved
// object must exist in the one target registry. `owner` receives the state
// that owns the resolved target, so callers never re-resolve the object.
Target* TargetReceiver(JSContext* cx, const JS::CallArgs& args,
                       JS::MutableHandleObject object, State** owner);
extern const JSClass kEventClass;
extern const JSClass kTargetClass;
}
#endif
