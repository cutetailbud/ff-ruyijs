// Event subclass property surface: the read-only accessors, the modifier query
// and the per-interface property tables installed by event_derived.cpp. The
// values were already materialized into the traced reserved slots by
// event_init.cpp, so a getter only re-wraps the stored JavaScript value and
// never recomputes it. Each accessor belongs to one interface and rejects a
// receiver that does not implement it, so MouseEvent.prototype.clientX.get
// throws for a plain Event and works for a PointerEvent.
#include "interaction/internal.h"
#include "interaction/event_keyboard.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Object.h"
#include "js/String.h"
#include <string>

namespace ruyijs::interaction {
namespace {

JSObject* Self(const JS::CallArgs& args) { return &args.thisv().toObject(); }

bool Slot(JSContext* cx, const JS::CallArgs& args, const char* member, Iface iface,
          uint32_t slot) {
  Event* event = Receiver(cx, args, iface, iface == Iface::UI);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), member, int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(event->object, slot));
  return JS_WrapValue(cx, args.rval());
}

bool Flag(JSContext* cx, const JS::CallArgs& args, const char* member, Iface iface,
          uint32_t slot, uint32_t bit) {
  Event* event = Receiver(cx, args, iface, iface == Iface::UI);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), member, int(ApiAccess::Read));
  const int32_t mask = JS::GetReservedSlot(event->object, slot).toInt32();
  args.rval().setBoolean((static_cast<uint32_t>(mask) & bit) != 0);
  return true;
}

bool EmptyArray(JSContext* cx, const JS::CallArgs& args, const char* member, Iface iface) {
  Event* event = Receiver(cx, args, iface, iface == Iface::UI);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), member, int(ApiAccess::Read));
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  args.rval().setObject(*array);
  return true;
}
}  // namespace

#define EVENT_SLOT_GETTER(name, member, iface, slot)                   \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {              \
    return Slot(cx, JS::CallArgsFromVp(argc, vp), member, iface, slot); \
  }
#define EVENT_FLAG_GETTER(name, member, iface, slot, bit)                   \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {                  \
    return Flag(cx, JS::CallArgsFromVp(argc, vp), member, iface, slot, bit); \
  }
#define EVENT_CONST_GETTER(name, member, iface, body)              \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {         \
    auto args = JS::CallArgsFromVp(argc, vp);                      \
    Event* event = Receiver(cx, args, iface, iface == Iface::UI);                      \
    if (!event) return false;                                      \
    ReportEventAccess(cx, event, InterfaceName(iface), member,    \
                      int(ApiAccess::Read));                       \
    body;                                                          \
    return true;                                                   \
  }

// Install-time interface label of one event interface. Static per interface,
// never script-supplied, so a watch entry names the installer's own member.
const char* InterfaceName(Iface iface) {
  switch (iface) {
    case Iface::Event: return "Event";
    case Iface::Custom: return "CustomEvent";
    case Iface::UI: return "UIEvent";
    case Iface::Mouse: return "MouseEvent";
    case Iface::Pointer: return "PointerEvent";
    case Iface::Keyboard: return "KeyboardEvent";
    case Iface::Focus: return "FocusEvent";
    case Iface::Input: return "InputEvent";
    case Iface::Wheel: return "WheelEvent";
    case Iface::Touch: return "TouchEvent";
    case Iface::Progress: return "ProgressEvent";
    case Iface::Message: return "MessageEvent";
    case Iface::Error: return "ErrorEvent";
    case Iface::Close: return "CloseEvent";
    case Iface::Storage: return "StorageEvent";
    case Iface::PopState: return "PopStateEvent";
    case Iface::HashChange: return "HashChangeEvent";
    case Iface::OfflineAudioComplete: return "OfflineAudioCompletionEvent";
    case Iface::PageTransition: return "PageTransitionEvent";
    case Iface::PromiseRejection: return "PromiseRejectionEvent";
    case Iface::Submit: return "SubmitEvent";
    case Iface::Drag: return "DragEvent";
    case Iface::Clipboard: return "ClipboardEvent";
    case Iface::Composition: return "CompositionEvent";
    case Iface::Transition: return "TransitionEvent";
    case Iface::Animation: return "AnimationEvent";
    case Iface::PolicyViolation: return "SecurityPolicyViolationEvent";
    case Iface::MediaQueryList: return "MediaQueryListEvent";
    case Iface::BeforeUnload: return "BeforeUnloadEvent";
    case Iface::DeviceMotion: return "DeviceMotionEvent";
    case Iface::DeviceOrientation: return "DeviceOrientationEvent";
    case Iface::RtcIce: return "RTCPeerConnectionIceEvent";
    case Iface::RtcDataChannel: return "RTCDataChannelEvent";
    case Iface::RtcTrack: return "RTCTrackEvent";
    case Iface::AnimationPlayback: return "AnimationPlaybackEvent";
    case Iface::BlobPayload: return "BlobEvent";
    case Iface::ContentVisibility: return "ContentVisibilityAutoStateChangeEvent";
    case Iface::FormDataPayload: return "FormDataEvent";
    case Iface::Toggle: return "ToggleEvent";
    case Iface::Gamepad: return "GamepadEvent";
    case Iface::TextInput: return "TextEvent";
    case Iface::NavigationEntryChange: return "NavigationCurrentEntryChangeEvent";
    case Iface::Speech: return "SpeechSynthesisEvent";
    case Iface::SpeechError: return "SpeechSynthesisErrorEvent";
    case Iface::MidiConnection: return "MIDIConnectionEvent";
    case Iface::MidiMessage: return "MIDIMessageEvent";
    case Iface::CookieChange: return "CookieChangeEvent";
    case Iface::PictureInPicture: return "PictureInPictureEvent";
    case Iface::Navigate: return "NavigateEvent";
    case Iface::MediaEncrypted: return "MediaEncryptedEvent";
    case Iface::MediaKeyMessage: return "MediaKeyMessageEvent";
    case Iface::MediaStream: return "MediaStreamEvent";
    case Iface::MediaTrack: return "TrackEvent";
    case Iface::RtcTone: return "RTCDTMFToneChangeEvent";
    case Iface::RtcErrorPayload: return "RTCErrorEvent";
    case Iface::RtcIceError: return "RTCPeerConnectionIceErrorEvent";
    case Iface::AudioProcessing: return "AudioProcessingEvent";
    case Iface::FontLoad: return "FontFaceSetLoadEvent";
    case Iface::GpuUncaptured: return "GPUUncapturedErrorEvent";
    case Iface::TaskPriority: return "TaskPriorityChangeEvent";
    case Iface::IdbVersionChange: return "IDBVersionChangeEvent";
    case Iface::DocumentPicture: return "DocumentPictureInPictureEvent";
    case Iface::PopupBlocked: return "PopupBlockedEvent";
    case Iface::TimeEvent: return "TimeEvent";
    case Iface::MouseScroll: return "MouseScrollEvent";
    case Iface::ScrollArea: return "ScrollAreaEvent";
    case Iface::Command: return "CommandEvent";
    case Iface::MediaStreamTrack: return "MediaStreamTrackEvent";
    case Iface::MediaRecorderError: return "MediaRecorderErrorEvent";
    case Iface::Extendable: return "ExtendableEvent";
    case Iface::ExtendableMessage: return "ExtendableMessageEvent";
    case Iface::Fetch: return "FetchEvent";
    case Iface::RtcTransform: return "RTCTransformEvent";
  }
  return "";
}

// One installed member access of an event object, reported after the accessor
// validated its receiver. The cheap thread-local guard keeps the non-debug path
// to a single false branch, so no page pays for an event scan it never uses.
void ReportEventAccess(JSContext* cx, Event* event, const char* iface,
                       const char* member, int type) {
  if (!MonitorBound()) return;
  State* state = Owner(cx, event->object, true);
  if (state) state->Observe(iface, member, type);
}

// CustomEvent
EVENT_SLOT_GETTER(GetCustomDetail, "detail", Iface::Custom, kEventCustomDetail)

// UIEvent
EVENT_SLOT_GETTER(GetView, "view", Iface::UI, kEventView)
EVENT_SLOT_GETTER(GetUiDetail, "detail", Iface::UI, kEventUiDetail)

// MouseEvent
// MouseEvent screen/client coordinates: integers except on a fractional
// PointerEvent (DOMEventShouldUseFractionalCoords).
bool GetScreenX(JSContext* cx, unsigned argc, JS::Value* vp) { return ClientCoordinate(cx, argc, vp, "screenX", true, kEventScreenX); }
bool GetScreenY(JSContext* cx, unsigned argc, JS::Value* vp) { return ClientCoordinate(cx, argc, vp, "screenY", false, kEventScreenY); }
bool GetClientX(JSContext* cx, unsigned argc, JS::Value* vp) { return ClientCoordinate(cx, argc, vp, "clientX", true, kEventClientX); }
bool GetClientY(JSContext* cx, unsigned argc, JS::Value* vp) { return ClientCoordinate(cx, argc, vp, "clientY", false, kEventClientY); }
EVENT_SLOT_GETTER(GetMovementX, "movementX", Iface::Mouse, kEventMovementX)
EVENT_SLOT_GETTER(GetMovementY, "movementY", Iface::Mouse, kEventMovementY)
EVENT_SLOT_GETTER(GetMozPressure, "mozPressure", Iface::Mouse, kEventPressure)
EVENT_SLOT_GETTER(GetMozInputSource, "mozInputSource", Iface::Mouse, kEventInputSource)
EVENT_SLOT_GETTER(GetButton, "button", Iface::Mouse, kEventButton)
EVENT_SLOT_GETTER(GetButtons, "buttons", Iface::Mouse, kEventButtons)
EVENT_SLOT_GETTER(GetMouseRelatedTarget, "relatedTarget", Iface::Mouse, kEventRelatedTarget)
EVENT_FLAG_GETTER(GetMouseCtrl, "ctrlKey", Iface::Mouse, kEventModifiers, domain::kControlMask)
EVENT_FLAG_GETTER(GetMouseShift, "shiftKey", Iface::Mouse, kEventModifiers, domain::kShiftMask)
EVENT_FLAG_GETTER(GetMouseAlt, "altKey", Iface::Mouse, kEventModifiers, domain::kAltMask)
EVENT_FLAG_GETTER(GetMouseMeta, "metaKey", Iface::Mouse, kEventModifiers, domain::kMetaMask)

// PointerEvent
EVENT_SLOT_GETTER(GetPointerId, "pointerId", Iface::Pointer, kEventPointerId)
EVENT_SLOT_GETTER(GetPointerWidth, "width", Iface::Pointer, kEventWidth)
EVENT_SLOT_GETTER(GetPointerHeight, "height", Iface::Pointer, kEventHeight)
EVENT_SLOT_GETTER(GetPressure, "pressure", Iface::Pointer, kEventPressure)
EVENT_SLOT_GETTER(GetTangentialPressure, "tangentialPressure", Iface::Pointer, kEventTangentialPressure)
EVENT_SLOT_GETTER(GetTiltX, "tiltX", Iface::Pointer, kEventTiltX)
EVENT_SLOT_GETTER(GetTiltY, "tiltY", Iface::Pointer, kEventTiltY)
EVENT_SLOT_GETTER(GetTwist, "twist", Iface::Pointer, kEventTwist)
EVENT_SLOT_GETTER(GetAltitudeAngle, "altitudeAngle", Iface::Pointer, kEventAltitudeAngle)
EVENT_SLOT_GETTER(GetAzimuthAngle, "azimuthAngle", Iface::Pointer, kEventAzimuthAngle)
EVENT_SLOT_GETTER(GetPointerType, "pointerType", Iface::Pointer, kEventPointerType)
EVENT_SLOT_GETTER(GetIsPrimary, "isPrimary", Iface::Pointer, kEventIsPrimary)

// FocusEvent
EVENT_SLOT_GETTER(GetFocusRelatedTarget, "relatedTarget", Iface::Focus, kEventRelatedTarget)

// InputEvent
EVENT_SLOT_GETTER(GetInputData, "data", Iface::Input, kEventInputData)
EVENT_SLOT_GETTER(GetInputType, "inputType", Iface::Input, kEventInputType)
EVENT_FLAG_GETTER(GetInputIsComposing, "isComposing", Iface::Input, kEventInputFlags, 1)
EVENT_CONST_GETTER(GetDataTransfer, "dataTransfer", Iface::Input, args.rval().setNull())

// WheelEvent
EVENT_SLOT_GETTER(GetDeltaX, "deltaX", Iface::Wheel, kEventDeltaX)
EVENT_SLOT_GETTER(GetDeltaY, "deltaY", Iface::Wheel, kEventDeltaY)
EVENT_SLOT_GETTER(GetDeltaZ, "deltaZ", Iface::Wheel, kEventDeltaZ)
EVENT_SLOT_GETTER(GetDeltaMode, "deltaMode", Iface::Wheel, kEventDeltaMode)


// ProgressEvent
EVENT_SLOT_GETTER(GetLengthComputable, "lengthComputable", Iface::Progress, kEventLengthComputable)
EVENT_SLOT_GETTER(GetLoaded, "loaded", Iface::Progress, kEventLoaded)
EVENT_SLOT_GETTER(GetTotal, "total", Iface::Progress, kEventTotal)

// MessageEvent
EVENT_SLOT_GETTER(GetMessageData, "data", Iface::Message, kEventMessageData)
EVENT_SLOT_GETTER(GetOrigin, "origin", Iface::Message, kEventOrigin)
EVENT_SLOT_GETTER(GetLastEventId, "lastEventId", Iface::Message, kEventLastEventId)
EVENT_SLOT_GETTER(GetMessageSource, "source", Iface::Message, kEventMessageSource)
EVENT_SLOT_GETTER(GetPorts, "ports", Iface::Message, kEventPorts)

// ErrorEvent
EVENT_SLOT_GETTER(GetErrorMessage, "message", Iface::Error, kEventErrorMessage)
EVENT_SLOT_GETTER(GetErrorFilename, "filename", Iface::Error, kEventErrorFilename)
EVENT_SLOT_GETTER(GetLineno, "lineno", Iface::Error, kEventLineno)
EVENT_SLOT_GETTER(GetColno, "colno", Iface::Error, kEventColno)
EVENT_SLOT_GETTER(GetErrorValue, "error", Iface::Error, kEventErrorValue)

// CloseEvent
EVENT_SLOT_GETTER(GetCloseCode, "code", Iface::Close, kEventCloseCode)
EVENT_SLOT_GETTER(GetCloseReason, "reason", Iface::Close, kEventCloseReason)
EVENT_SLOT_GETTER(GetWasClean, "wasClean", Iface::Close, kEventWasClean)

// StorageEvent
EVENT_SLOT_GETTER(GetStorageKey, "key", Iface::Storage, kEventStorageKey)
EVENT_SLOT_GETTER(GetStorageOldValue, "oldValue", Iface::Storage, kEventStorageOldValue)
EVENT_SLOT_GETTER(GetStorageNewValue, "newValue", Iface::Storage, kEventStorageNewValue)
EVENT_SLOT_GETTER(GetStorageUrl, "url", Iface::Storage, kEventStorageUrl)
EVENT_SLOT_GETTER(GetStorageArea, "storageArea", Iface::Storage, kEventStorageArea)

#undef EVENT_SLOT_GETTER
#undef EVENT_FLAG_GETTER
#undef EVENT_CONST_GETTER

// --- methods ---
namespace {
bool ModifierState(JSContext* cx, const JS::CallArgs& args, const char* member,
                   Iface iface) {
  Event* event = Receiver(cx, args, iface, iface == Iface::UI);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), member, int(ApiAccess::Call));
  JS::RootedString key(cx, JS::ToString(cx, args.get(0)));
  if (!key) return false;
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, key);
  if (!utf8) return false;
  const uint32_t mask = static_cast<uint32_t>(
      JS::GetReservedSlot(Self(args), kEventModifiers).toInt32());
  const std::string name(utf8.get());
  args.rval().setBoolean((name == "Alt" && (mask & domain::kAltMask)) ||
                         (name == "Control" && (mask & domain::kControlMask)) ||
                         (name == "Meta" && (mask & domain::kMetaMask)) ||
                         (name == "Shift" && (mask & domain::kShiftMask)));
  return true;
}
}  // namespace

bool GetMouseModifierState(JSContext* cx, unsigned argc, JS::Value* vp) {
  return ModifierState(cx, JS::CallArgsFromVp(argc, vp), "getModifierState",
                       Iface::Mouse);
}
bool GetCoalescedEvents(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Pointer);
  if (!event) return false;
  ReportEventAccess(cx, event, "PointerEvent", "getCoalescedEvents",
                    int(ApiAccess::Call));
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  // Firefox coalesces only trusted pointermove/pointerrawupdate events; every
  // other pointer event (and a constructed one) has an empty list.
  if (event->value.is_trusted() &&
      (EventTypeIs(*event, "pointermove") || EventTypeIs(*event, "pointerrawupdate"))) {
    JS::RootedValue self(cx, JS::ObjectValue(*Self(args)));
    if (!JS_DefineElement(cx, array, 0, self, JSPROP_ENUMERATE)) return false;
  }
  args.rval().setObject(*array);
  return true;
}
bool GetPredictedEvents(JSContext* cx, unsigned argc, JS::Value* vp) {
  return EmptyArray(cx, JS::CallArgsFromVp(argc, vp), "getPredictedEvents",
                    Iface::Pointer);
}
bool GetTargetRanges(JSContext* cx, unsigned argc, JS::Value* vp) {
  return EmptyArray(cx, JS::CallArgsFromVp(argc, vp), "getTargetRanges",
                    Iface::Input);
}

// --- property tables ---
extern const JSPropertySpec kCustomProperties[] = {
  JS_PSG("detail", GetCustomDetail, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kUiProperties[] = {
  JS_PSG("view", GetView, JSPROP_ENUMERATE),
  JS_PSG("detail", GetUiDetail, JSPROP_ENUMERATE),
  JS_PSG("which", GetUiWhich, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kMouseProperties[] = {
  JS_PSG("screenX", GetScreenX, JSPROP_ENUMERATE),
  JS_PSG("screenY", GetScreenY, JSPROP_ENUMERATE),
  JS_PSG("clientX", GetClientX, JSPROP_ENUMERATE),
  JS_PSG("clientY", GetClientY, JSPROP_ENUMERATE),
  JS_PSG("movementX", GetMovementX, JSPROP_ENUMERATE),
  JS_PSG("movementY", GetMovementY, JSPROP_ENUMERATE),
  JS_PSG("mozPressure", GetMozPressure, JSPROP_ENUMERATE),
  JS_PSG("mozInputSource", GetMozInputSource, JSPROP_ENUMERATE),
  JS_PSG("button", GetButton, JSPROP_ENUMERATE),
  JS_PSG("buttons", GetButtons, JSPROP_ENUMERATE),
  JS_PSG("relatedTarget", GetMouseRelatedTarget, JSPROP_ENUMERATE),
  JS_PSG("ctrlKey", GetMouseCtrl, JSPROP_ENUMERATE),
  JS_PSG("shiftKey", GetMouseShift, JSPROP_ENUMERATE),
  JS_PSG("altKey", GetMouseAlt, JSPROP_ENUMERATE),
  JS_PSG("metaKey", GetMouseMeta, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kPointerProperties[] = {
  JS_PSG("pointerId", GetPointerId, JSPROP_ENUMERATE),
  JS_PSG("width", GetPointerWidth, JSPROP_ENUMERATE),
  JS_PSG("height", GetPointerHeight, JSPROP_ENUMERATE),
  JS_PSG("pressure", GetPressure, JSPROP_ENUMERATE),
  JS_PSG("tangentialPressure", GetTangentialPressure, JSPROP_ENUMERATE),
  JS_PSG("tiltX", GetTiltX, JSPROP_ENUMERATE),
  JS_PSG("tiltY", GetTiltY, JSPROP_ENUMERATE),
  JS_PSG("twist", GetTwist, JSPROP_ENUMERATE),
  JS_PSG("altitudeAngle", GetAltitudeAngle, JSPROP_ENUMERATE),
  JS_PSG("azimuthAngle", GetAzimuthAngle, JSPROP_ENUMERATE),
  JS_PSG("pointerType", GetPointerType, JSPROP_ENUMERATE),
  JS_PSG("isPrimary", GetIsPrimary, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kFocusProperties[] = {
  JS_PSG("relatedTarget", GetFocusRelatedTarget, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kInputProperties[] = {
  JS_PSG("data", GetInputData, JSPROP_ENUMERATE),
  JS_PSG("inputType", GetInputType, JSPROP_ENUMERATE),
  JS_PSG("isComposing", GetInputIsComposing, JSPROP_ENUMERATE),
  JS_PSG("dataTransfer", GetDataTransfer, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kWheelProperties[] = {
  JS_PSG("deltaX", GetDeltaX, JSPROP_ENUMERATE),
  JS_PSG("deltaY", GetDeltaY, JSPROP_ENUMERATE),
  JS_PSG("deltaZ", GetDeltaZ, JSPROP_ENUMERATE),
  JS_PSG("deltaMode", GetDeltaMode, JSPROP_ENUMERATE),
  JS_PS_END
};

extern const JSPropertySpec kProgressProperties[] = {
  JS_PSG("lengthComputable", GetLengthComputable, JSPROP_ENUMERATE),
  JS_PSG("loaded", GetLoaded, JSPROP_ENUMERATE),
  JS_PSG("total", GetTotal, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kMessageProperties[] = {
  JS_PSG("data", GetMessageData, JSPROP_ENUMERATE),
  JS_PSG("origin", GetOrigin, JSPROP_ENUMERATE),
  JS_PSG("lastEventId", GetLastEventId, JSPROP_ENUMERATE),
  JS_PSG("source", GetMessageSource, JSPROP_ENUMERATE),
  JS_PSG("ports", GetPorts, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kErrorProperties[] = {
  JS_PSG("message", GetErrorMessage, JSPROP_ENUMERATE),
  JS_PSG("filename", GetErrorFilename, JSPROP_ENUMERATE),
  JS_PSG("lineno", GetLineno, JSPROP_ENUMERATE),
  JS_PSG("colno", GetColno, JSPROP_ENUMERATE),
  JS_PSG("error", GetErrorValue, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kCloseProperties[] = {
  JS_PSG("code", GetCloseCode, JSPROP_ENUMERATE),
  JS_PSG("reason", GetCloseReason, JSPROP_ENUMERATE),
  JS_PSG("wasClean", GetWasClean, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kStorageProperties[] = {
  JS_PSG("key", GetStorageKey, JSPROP_ENUMERATE),
  JS_PSG("oldValue", GetStorageOldValue, JSPROP_ENUMERATE),
  JS_PSG("newValue", GetStorageNewValue, JSPROP_ENUMERATE),
  JS_PSG("url", GetStorageUrl, JSPROP_ENUMERATE),
  JS_PSG("storageArea", GetStorageArea, JSPROP_ENUMERATE),
  JS_PS_END
};

// --- method tables ---
extern const JSFunctionSpec kMouseMethods[] = {
  JS_FN("initMouseEvent", InitMouseEvent, 0, JSPROP_ENUMERATE),
  JS_FN("getModifierState", GetMouseModifierState, 1, JSPROP_ENUMERATE),
  JS_FN("initNSMouseEvent", InitNSMouseEvent, 1, JSPROP_ENUMERATE),
  JS_FS_END
};
extern const JSFunctionSpec kPointerMethods[] = {
  JS_FN("getCoalescedEvents", GetCoalescedEvents, 0, JSPROP_ENUMERATE),
  JS_FN("getPredictedEvents", GetPredictedEvents, 0, JSPROP_ENUMERATE),
  JS_FS_END
};
extern const JSFunctionSpec kInputMethods[] = {
  JS_FN("getTargetRanges", GetTargetRanges, 0, JSPROP_ENUMERATE),
  JS_FS_END
};

// --- constants ---
extern const EventConstant kWheelConstants[] = {
  {"DOM_DELTA_PIXEL", 0x00},
  {"DOM_DELTA_LINE", 0x01},
  {"DOM_DELTA_PAGE", 0x02},
  {nullptr, 0}
};
extern const EventConstant kMouseConstants[] = {
  {"MOZ_SOURCE_UNKNOWN", 0},
  {"MOZ_SOURCE_MOUSE", 1},
  {"MOZ_SOURCE_PEN", 2},
  {"MOZ_SOURCE_ERASER", 3},
  {"MOZ_SOURCE_CURSOR", 4},
  {"MOZ_SOURCE_TOUCH", 5},
  {"MOZ_SOURCE_KEYBOARD", 6},
  {nullptr, 0}
};
}  // namespace ruyijs::interaction
