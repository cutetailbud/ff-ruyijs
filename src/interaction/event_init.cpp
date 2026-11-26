// Event initialization: reading the JS init dictionary and materializing the
// domain init structs into the traced reserved slots of an event object. The
// spec defaults and modifier semantics live in domain/interaction/dom_event.h;
// this adapter only moves values, and JavaScript values stay JavaScript values.
#include "interaction/internal.h"
#include "domain/interaction/pointer_angles.h"
#include "interaction/event_keyboard.h"
#include "interaction/touch.h"
#include "domain/interaction/dom_event.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Object.h"
#include "js/String.h"
#include <cmath>

namespace ruyijs::interaction {
void SetSlotNull(JS::HandleObject object, uint32_t slot) {
  JS::SetReservedSlot(object, slot, JS::NullValue());
}
void SetSlotBool(JS::HandleObject object, uint32_t slot, bool value) {
  JS::SetReservedSlot(object, slot, JS::BooleanValue(value));
}
void SetSlotInt(JS::HandleObject object, uint32_t slot, int32_t value) {
  JS::SetReservedSlot(object, slot, JS::Int32Value(value));
}
void SetSlotNumber(JS::HandleObject object, uint32_t slot, double value) {
  JS::SetReservedSlot(object, slot, JS::DoubleValue(value));
}
bool SetSlotText(JSContext* cx, JS::HandleObject object, uint32_t slot,
                 const std::string& text) {
  JS::RootedString value(cx, JS_NewStringCopyUTF8N(
      cx, JS::UTF8Chars(text.data(), text.size())));
  if (!value) return false;
  JS::SetReservedSlot(object, slot, JS::StringValue(value));
  return true;
}
bool SetSlotNullable(JSContext* cx, JS::HandleObject object, uint32_t slot,
                     bool is_null, const std::string& text) {
  if (is_null) { SetSlotNull(object, slot); return true; }
  return SetSlotText(cx, object, slot, text);
}
bool InitEmptyArray(JSContext* cx, JS::MutableHandleValue out) {
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  out.setObject(*array);
  return true;
}
bool InitFrozenPorts(JSContext* cx, JS::HandleValue input, JS::MutableHandleValue out) {
  JS::RootedObject result(cx, JS::NewArrayObject(cx, 0));
  if (!result) return false;
  if (input.isObject()) {
    bool is_array = false;
    if (!JS::IsArrayObject(cx, input, &is_array)) return false;
    if (!is_array) return Error(cx, "TypeError", "MessageEvent ports must be an array");
    JS::RootedObject source(cx, &input.toObject());
    uint32_t length = 0;
    if (!JS::GetArrayLength(cx, source, &length) || length > 10000) return false;
    JS::RootedValue value(cx);
    for (uint32_t index = 0; index < length; ++index) {
      if (!JS_GetElement(cx, source, index, &value) ||
          !JS_DefineElement(cx, result, index, value, JSPROP_ENUMERATE)) return false;
    }
  }
  if (!JS_FreezeObject(cx, result)) return false;
  out.setObject(*result);
  return true;
}
bool ReadInitBool(JSContext* cx, JS::HandleObject options, const char* name, bool* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, options, name, &value)) return false;
  if (!value.isUndefined()) *out = JS::ToBoolean(value);
  return true;
}
bool ReadInitInt(JSContext* cx, JS::HandleObject options, const char* name, int* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, options, name, &value)) return false;
  if (value.isUndefined()) return true;
  int32_t number = 0;
  if (!JS::ToInt32(cx, value, &number)) return false;
  *out = static_cast<int>(number);
  return true;
}
bool ReadInitNumber(JSContext* cx, JS::HandleObject options, const char* name, double* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, options, name, &value)) return false;
  if (value.isUndefined()) return true;
  return JS::ToNumber(cx, value, out);
}
bool ReadInitString(JSContext* cx, JS::HandleObject options, const char* name,
                    std::string* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, options, name, &value)) return false;
  if (value.isUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  JS::UniqueChars encoded = JS_EncodeStringToUTF8(cx, text);
  if (!encoded) return false;
  out->assign(encoded.get());
  return true;
}
bool ReadInitNullableString(JSContext* cx, JS::HandleObject options, const char* name,
                            bool* is_null, std::string* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, options, name, &value)) return false;
  if (value.isUndefined()) return true;
  if (value.isNull()) { *is_null = true; out->clear(); return true; }
  *is_null = false;
  return ReadInitString(cx, options, name, out);
}
bool ReadInitValue(JSContext* cx, JS::HandleObject options, const char* name,
                   JS::MutableHandleValue out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, options, name, &value)) return false;
  if (!value.isUndefined()) out.set(value);
  return true;
}
bool ReadEventBase(JSContext* cx, JS::HandleValue options, bool* bubbles,
                   bool* cancelable, bool* composed) {
  *bubbles = *cancelable = *composed = false;
  if (options.isNullOrUndefined()) return true;
  if (!options.isObject()) return Error(cx, "TypeError", "Event options require an object");
  JS::RootedObject object(cx, &options.toObject());
  return ReadInitBool(cx, object, "bubbles", bubbles) &&
         ReadInitBool(cx, object, "cancelable", cancelable) &&
         ReadInitBool(cx, object, "composed", composed);
}

namespace {
// Spec defaults live in domain/interaction/dom_event.h; this adapter only
// copies them into the traced slots, and JavaScript-value fields stay JS values.
bool ReadModifiers(JSContext* cx, JS::HandleObject dict, domain::EventModifiers* out) {
  if (!dict) return true;
  return ReadInitBool(cx, dict, "ctrlKey", &out->ctrl_key) &&
         ReadInitBool(cx, dict, "shiftKey", &out->shift_key) &&
         ReadInitBool(cx, dict, "altKey", &out->alt_key) &&
         ReadInitBool(cx, dict, "metaKey", &out->meta_key);
}
bool InitCustom(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  JS::RootedValue detail(cx, JS::NullValue());
  if (dict && !ReadInitValue(cx, dict, "detail", &detail)) return false;
  JS::SetReservedSlot(object, kEventCustomDetail, detail);
  return true;
}
// Whether `name` is present (not undefined) in the init dictionary.
bool InitPresent(JSContext* cx, JS::HandleObject dict, const char* name, bool* present) {
  *present = false;
  if (!dict) return true;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, dict, name, &value)) return false;
  *present = !value.isUndefined();
  return true;
}
bool InitPointer(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::PointerEventInit init;
  bool hasTiltX = false, hasTiltY = false, hasAltitude = false, hasAzimuth = false;
  if (!InitPresent(cx, dict, "tiltX", &hasTiltX) || !InitPresent(cx, dict, "tiltY", &hasTiltY) ||
      !InitPresent(cx, dict, "altitudeAngle", &hasAltitude) ||
      !InitPresent(cx, dict, "azimuthAngle", &hasAzimuth)) return false;
  if (dict && (!ReadInitInt(cx, dict, "pointerId", &init.pointer_id) ||
               !ReadInitNumber(cx, dict, "width", &init.width) ||
               !ReadInitNumber(cx, dict, "height", &init.height) ||
               !ReadInitNumber(cx, dict, "pressure", &init.pressure) ||
               !ReadInitNumber(cx, dict, "tangentialPressure", &init.tangential_pressure) ||
               !ReadInitInt(cx, dict, "tiltX", &init.tilt_x) ||
               !ReadInitInt(cx, dict, "tiltY", &init.tilt_y) ||
               !ReadInitInt(cx, dict, "twist", &init.twist) ||
               !ReadInitNumber(cx, dict, "altitudeAngle", &init.altitude_angle) ||
               !ReadInitNumber(cx, dict, "azimuthAngle", &init.azimuth_angle) ||
               !ReadInitString(cx, dict, "pointerType", &init.pointer_type) ||
               !ReadInitBool(cx, dict, "isPrimary", &init.is_primary))) return false;
  SetSlotInt(object, kEventPointerId, init.pointer_id);
  SetSlotNumber(object, kEventWidth, init.width);
  SetSlotNumber(object, kEventHeight, init.height);
  SetSlotNumber(object, kEventPressure, init.pressure);
  SetSlotNumber(object, kEventTangentialPressure, init.tangential_pressure);
  // The missing tilt or angle values derive from the given ones
  // (WidgetPointerHelper); with none given a pointer reads altitude pi/2.
  ::ruyijs::domain::interaction::PointerAngles angles;
  if (hasTiltX) angles.x = init.tilt_x;
  if (hasTiltY) angles.y = init.tilt_y;
  if (hasAltitude) angles.altitude = init.altitude_angle;
  if (hasAzimuth) angles.azimuth = init.azimuth_angle;
  angles.Complete();
  SetSlotInt(object, kEventTiltX, *angles.x);
  SetSlotInt(object, kEventTiltY, *angles.y);
  SetSlotInt(object, kEventTwist, init.twist);
  SetSlotNumber(object, kEventAltitudeAngle, *angles.altitude);
  SetSlotNumber(object, kEventAzimuthAngle, *angles.azimuth);
  SetSlotBool(object, kEventIsPrimary, init.is_primary);
  int32_t input_source = 0;
  if (init.pointer_type == "mouse") input_source = 1;
  else if (init.pointer_type == "pen") input_source = 2;
  else if (init.pointer_type == "touch") input_source = 5;
  SetSlotInt(object, kEventInputSource, input_source);
  return SetSlotText(cx, object, kEventPointerType, init.pointer_type);
}
bool InitWheel(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::WheelEventInit init;
  if (dict && (!ReadInitNumber(cx, dict, "deltaX", &init.delta_x) ||
               !ReadInitNumber(cx, dict, "deltaY", &init.delta_y) ||
               !ReadInitNumber(cx, dict, "deltaZ", &init.delta_z) ||
               !ReadInitInt(cx, dict, "deltaMode", &init.delta_mode))) return false;
  SetSlotNumber(object, kEventDeltaX, init.delta_x);
  SetSlotNumber(object, kEventDeltaY, init.delta_y);
  SetSlotNumber(object, kEventDeltaZ, init.delta_z);
  SetSlotInt(object, kEventDeltaMode, init.delta_mode);
  return true;
}
bool InitFocus(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  JS::RootedValue related(cx, JS::NullValue());
  if (dict && !ReadInitValue(cx, dict, "relatedTarget", &related)) return false;
  JS::SetReservedSlot(object, kEventRelatedTarget, related);
  return true;
}
bool InitInput(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::InputEventInit init;
  JS::RootedValue data(cx, JS::NullValue());
  if (dict && (!ReadInitValue(cx, dict, "data", &data) ||
               !ReadInitString(cx, dict, "inputType", &init.input_type) ||
               !ReadInitBool(cx, dict, "isComposing", &init.is_composing))) return false;
  JS::SetReservedSlot(object, kEventInputData, data);
  SetSlotInt(object, kEventInputFlags, init.is_composing ? 1 : 0);
  return SetSlotText(cx, object, kEventInputType, init.input_type);
}
bool InitProgress(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::ProgressEventInit init;
  if (dict && (!ReadInitBool(cx, dict, "lengthComputable", &init.length_computable) ||
               !ReadInitNumber(cx, dict, "loaded", &init.loaded) ||
               !ReadInitNumber(cx, dict, "total", &init.total))) return false;
  SetSlotBool(object, kEventLengthComputable, init.length_computable);
  SetSlotNumber(object, kEventLoaded, init.loaded);
  SetSlotNumber(object, kEventTotal, init.total);
  return true;
}
bool InitMessage(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::MessageEventInit init;
  JS::RootedValue data(cx, JS::NullValue());
  JS::RootedValue source(cx, JS::NullValue());
  JS::RootedValue ports(cx);
  if (!InitEmptyArray(cx, &ports)) return false;
  if (dict && (!ReadInitValue(cx, dict, "data", &data) ||
               !ReadInitString(cx, dict, "origin", &init.origin) ||
               !ReadInitString(cx, dict, "lastEventId", &init.last_event_id) ||
               !ReadInitValue(cx, dict, "source", &source) ||
               !ReadInitValue(cx, dict, "ports", &ports))) return false;
  JS::RootedValue frozen_ports(cx);
  if (!InitFrozenPorts(cx, ports, &frozen_ports)) return false;
  JS::SetReservedSlot(object, kEventMessageData, data);
  JS::SetReservedSlot(object, kEventMessageSource, source);
  JS::SetReservedSlot(object, kEventPorts, frozen_ports);
  return SetSlotText(cx, object, kEventOrigin, init.origin) &&
         SetSlotText(cx, object, kEventLastEventId, init.last_event_id);
}
bool InitErrorEvent(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::ErrorEventInit init;
  JS::RootedValue error(cx, JS::NullValue());
  if (dict && (!ReadInitString(cx, dict, "message", &init.message) ||
               !ReadInitString(cx, dict, "filename", &init.filename) ||
               !ReadInitInt(cx, dict, "lineno", &init.lineno) ||
               !ReadInitInt(cx, dict, "colno", &init.colno) ||
               !ReadInitValue(cx, dict, "error", &error))) return false;
  JS::SetReservedSlot(object, kEventErrorValue, error);
  SetSlotInt(object, kEventLineno, init.lineno);
  SetSlotInt(object, kEventColno, init.colno);
  return SetSlotText(cx, object, kEventErrorMessage, init.message) &&
         SetSlotText(cx, object, kEventErrorFilename, init.filename);
}
bool InitClose(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::CloseEventInit init;
  if (dict && (!ReadInitInt(cx, dict, "code", &init.code) ||
               !ReadInitString(cx, dict, "reason", &init.reason) ||
               !ReadInitBool(cx, dict, "wasClean", &init.was_clean))) return false;
  SetSlotInt(object, kEventCloseCode, init.code);
  SetSlotBool(object, kEventWasClean, init.was_clean);
  return SetSlotText(cx, object, kEventCloseReason, init.reason);
}
bool InitStorage(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::StorageEventInit init;
  JS::RootedValue area(cx, JS::NullValue());
  if (dict && (!ReadInitNullableString(cx, dict, "key", &init.key_is_null, &init.key) ||
               !ReadInitNullableString(cx, dict, "oldValue", &init.old_value_is_null,
                                       &init.old_value) ||
               !ReadInitNullableString(cx, dict, "newValue", &init.new_value_is_null,
                                       &init.new_value) ||
               !ReadInitString(cx, dict, "url", &init.url) ||
               !ReadInitValue(cx, dict, "storageArea", &area))) return false;
  if (!SetSlotNullable(cx, object, kEventStorageKey, init.key_is_null, init.key) ||
      !SetSlotNullable(cx, object, kEventStorageOldValue, init.old_value_is_null,
                       init.old_value) ||
      !SetSlotNullable(cx, object, kEventStorageNewValue, init.new_value_is_null,
                       init.new_value) ||
      !SetSlotText(cx, object, kEventStorageUrl, init.url)) return false;
  JS::SetReservedSlot(object, kEventStorageArea, area);
  return true;
}
}  // namespace

// The UIEvent and MouseEvent field writers stay at namespace scope because the
// T3c dictionaries that extend them (DragEventInit, CompositionEventInit) reuse
// them through their own table TU - one adapter per parent interface.
bool InitUiFields(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::UIEventInit init;
  JS::RootedValue view(cx, JS::NullValue());
  if (dict && (!ReadInitInt(cx, dict, "detail", &init.detail) ||
               !ReadInitValue(cx, dict, "view", &view))) return false;
  SetSlotInt(object, kEventUiDetail, init.detail);
  JS::SetReservedSlot(object, kEventView, view);
  return true;
}
bool InitMouseFields(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  domain::MouseEventInit init;
  JS::RootedValue related(cx, JS::NullValue());
  if (dict && (!ReadModifiers(cx, dict, &init) ||
               !ReadInitNumber(cx, dict, "screenX", &init.screen_x) ||
               !ReadInitNumber(cx, dict, "screenY", &init.screen_y) ||
               !ReadInitNumber(cx, dict, "clientX", &init.client_x) ||
               !ReadInitNumber(cx, dict, "clientY", &init.client_y) ||
               !ReadInitInt(cx, dict, "movementX", &init.movement_x) ||
               !ReadInitInt(cx, dict, "movementY", &init.movement_y) ||
               !ReadInitInt(cx, dict, "button", &init.button) ||
               !ReadInitInt(cx, dict, "buttons", &init.buttons) ||
               !ReadInitValue(cx, dict, "relatedTarget", &related))) return false;
  if (!std::isfinite(init.screen_x) || !std::isfinite(init.screen_y) ||
      !std::isfinite(init.client_x) || !std::isfinite(init.client_y))
    return Error(cx, "TypeError", "Mouse coordinates must be finite doubles");
  SetSlotInt(object, kEventModifiers, static_cast<int32_t>(init.modifier_mask()));
  SetSlotNumber(object, kEventScreenX, init.screen_x);
  SetSlotNumber(object, kEventScreenY, init.screen_y);
  SetSlotNumber(object, kEventClientX, init.client_x);
  SetSlotNumber(object, kEventClientY, init.client_y);
  SetSlotInt(object, kEventMovementX, init.movement_x);
  SetSlotInt(object, kEventMovementY, init.movement_y);
  SetSlotInt(object, kEventButton, init.button);
  SetSlotInt(object, kEventButtons, init.buttons);
  SetSlotNumber(object, kEventPressure, 0.0);
  SetSlotInt(object, kEventInputSource, 0);
  JS::SetReservedSlot(object, kEventRelatedTarget, related);
  return true;
}

// Interface-typed dictionary members of the T3c payloads (T3c): the value
// conversion rule of WebIDL dictionary members, with the brand check left to
// the owner of the referenced interface (none of them is installed yet).
bool ReadNullableObject(JSContext* cx, JS::HandleObject dict, const char* name,
                        JS::MutableHandleValue out) {
  out.setNull();
  if (!dict) return true;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, dict, name, &value)) return false;
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject()) {
    std::string message = std::string("Event dictionary member '") + name +
                          "' requires an object";
    return Error(cx, "TypeError", message.c_str());
  }
  out.set(value);
  return true;
}
bool ReadRequiredObject(JSContext* cx, JS::HandleObject dict, const char* name,
                        JS::MutableHandleValue out) {
  out.setNull();
  JS::RootedValue value(cx);
  if (dict && !JS_GetProperty(cx, dict, name, &value)) return false;
  if (value.isObject()) {
    out.set(value);
    return true;
  }
  std::string message = std::string("Event dictionary member '") + name +
                        "' is required";
  return Error(cx, "TypeError", message.c_str());
}

bool InitEventFields(JSContext* cx, Iface iface, JS::HandleValue options,
                     JS::HandleObject object) {
  JS::RootedObject dict(cx);
  if (options.isObject()) dict.set(&options.toObject());
  JS::RootedObject none(cx);
  switch (iface) {
    case Iface::PopupBlocked:
    case Iface::TimeEvent:
    case Iface::MouseScroll:
    case Iface::ScrollArea:
    case Iface::Command:
    case Iface::MediaStreamTrack:
    case Iface::MediaRecorderError:
      return true;  // event_firefox_payloads.cpp reads and validates these dictionaries
    case Iface::DocumentPicture:
      return Error(cx, "TypeError", "DocumentPictureInPictureEvent has no native instance factory");
    case Iface::IdbVersionChange:
      return InitIdbVersionChange(cx, dict, object);
    case Iface::Extendable:
      return true;
    case Iface::ExtendableMessage:
      return InitMessage(cx, dict, object);
    case Iface::Fetch:
      return InitFetch(cx, dict, object);
    case Iface::AudioProcessing:
    case Iface::FontLoad:
    case Iface::GpuUncaptured:
    case Iface::TaskPriority:
      return InitProcessingFields(cx, iface, dict, object);
    case Iface::RtcTone:
    case Iface::RtcErrorPayload:
    case Iface::RtcIceError:
      return InitRtcExtraFields(cx, iface, dict, object);
    case Iface::MediaEncrypted:
    case Iface::MediaKeyMessage:
    case Iface::MediaStream:
    case Iface::MediaTrack:
      return InitMediaPayloadFields(cx, iface, dict, object);
    case Iface::Navigate:
      return InitNavigateFields(cx, dict, object);
    case Iface::MidiConnection:
    case Iface::MidiMessage:
    case Iface::CookieChange:
    case Iface::PictureInPicture:
      return InitPeripheralFields(cx, iface, dict, object);
    case Iface::Speech:
    case Iface::SpeechError:
      return InitSpeechFields(cx, iface, dict, object);
    case Iface::NavigationEntryChange:
      return InitNavigationEntryChange(cx, dict, object);
    case Iface::Event:
      return true;
    case Iface::Custom:
      return InitCustom(cx, dict, object);
    case Iface::UI:
      return InitUiFields(cx, dict, object);
    case Iface::Mouse:
      return InitUiFields(cx, dict, object) && InitMouseFields(cx, dict, object);
    case Iface::Pointer:
      return InitUiFields(cx, dict, object) && InitMouseFields(cx, dict, object) &&
             InitPointer(cx, dict, object);
    case Iface::Wheel:
      return InitUiFields(cx, dict, object) && InitMouseFields(cx, dict, object) &&
             InitWheel(cx, dict, object);
    case Iface::Keyboard:
      return InitKeyboardFields(cx, dict, object);
    case Iface::Focus:
      return InitUiFields(cx, dict, object) && InitFocus(cx, dict, object);
    case Iface::Input:
      return InitUiFields(cx, dict, object) && InitInput(cx, dict, object);
    case Iface::Touch:
      return InitTouchFields(cx, dict, object);
    case Iface::Progress:
      return InitProgress(cx, dict, object);
    case Iface::Message:
      return InitMessage(cx, dict, object);
    case Iface::Error:
      return InitErrorEvent(cx, dict, object);
    case Iface::Close:
      return InitClose(cx, dict, object);
    case Iface::Storage:
      return InitStorage(cx, dict, object);
    case Iface::PopState:
      return InitPopState(cx, dict, object);
    case Iface::HashChange:
      return InitHashChange(cx, dict, object);
    case Iface::OfflineAudioComplete:
      return InitOfflineAudioComplete(cx, dict, object);
    // The T3c payload classes: each table TU owns its own dictionary adapter.
    case Iface::PageTransition:
    case Iface::PromiseRejection:
    case Iface::Submit:
    case Iface::Clipboard:
    case Iface::Transition:
    case Iface::Animation:
    case Iface::PolicyViolation:
    case Iface::MediaQueryList:
    case Iface::BeforeUnload:
      return InitPagePayloadFields(cx, iface, dict, object);
    case Iface::Drag:
      return InitUiFields(cx, dict, object) && InitMouseFields(cx, dict, object) &&
             InitDrag(cx, dict, object);
    case Iface::Composition:
      return InitUiFields(cx, dict, object) && InitComposition(cx, dict, object);
    case Iface::DeviceMotion:
    case Iface::DeviceOrientation:
      return InitDeviceFields(cx, iface, dict, object);
    case Iface::RtcIce:
    case Iface::RtcDataChannel:
    case Iface::RtcTrack:
      return InitRtcFields(cx, iface, dict, object);
    case Iface::AnimationPlayback:
    case Iface::BlobPayload:
    case Iface::ContentVisibility:
    case Iface::FormDataPayload:
    case Iface::Toggle:
    case Iface::Gamepad:
    case Iface::TextInput:
      return InitPublicEventFields(cx, iface, dict, object);
  }
  return true;
}
}  // namespace ruyijs::interaction
