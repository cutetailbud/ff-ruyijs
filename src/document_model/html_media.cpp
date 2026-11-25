// HTMLMediaElement's common surface.  The node and attribute state remains in
// DocumentModel; this adapter supplies Firefox's inert media defaults used by
// Audio() and by HTML audio/video elements.
#include "document_model/html_control.h"
#include "document_model/model_hyperlink.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "runtime/web_kit.h"
#include "runtime/runtime.h"
#include "domain/web_media/can_play.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Exception.h"
#include "js/String.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace ruyijs::document_model {
namespace {
enum Field { Src, CurrentSrc, Preload, Autoplay, Loop, Controls, Muted,
             DefaultMuted, Volume, CurrentTime, Paused, Ended, ReadyState, NetworkState };
struct Spec { const char* name; const char16_t* attr; int kind; };
constexpr Spec kFields[] = {
  {"src",u"src",0},{"currentSrc",u"src",1},{"preload",u"preload",0},
  {"autoplay",u"autoplay",2},{"loop",u"loop",2},{"controls",u"controls",2},
  {"muted",u"muted",2},{"defaultMuted",u"defaultmuted",2},
  {"volume",nullptr,3},{"currentTime",nullptr,4},{"paused",nullptr,5},
  {"ended",nullptr,6},{"readyState",nullptr,7},{"networkState",nullptr,8}
};
bool Resolved(JSContext* cx, State& state, DomStringView raw, JS::MutableHandleValue out) {
  JS::RootedValue base(cx), source(cx);
  if (!state.environment.URL(&base) || !Value(cx, raw, &source)) return false;
  JS::RootedString baseString(cx, base.toString()), sourceString(cx, source.toString());
  std::string baseText, sourceText, result;
  if (!ToUTF8(cx, baseString, &baseText) || !ToUTF8(cx, sourceString, &sourceText) ||
      !state.Check(ReadHyperlink(sourceText, baseText, HyperlinkField::Href, &result))) return false;
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(result.data(), result.size())));
  if (!text) return false; out.setString(text); return true;
}
bool Get(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); int field=FunctionSlot(args); const Spec& s=kFields[field];
  Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement"); if(!entry)return false;
  State& state=*entry->owner; std::optional<DomString> text;
  if(s.kind<=2) {
    if(!state.Check(state.model.GetAttribute(entry->key,s.attr,&text)))return false;
    if(s.kind==2) args.rval().setBoolean(text.has_value());
    else if(field==Src || field==CurrentSrc) return text ? Resolved(cx,state,DomStringView(*text),args.rval()) : (args.rval().setString(JS_NewStringCopyZ(cx,"")),true);
    else if(text) return Value(cx,DomStringView(*text),args.rval());
    else return Value(cx,field==Preload?DomStringView(u"auto"):DomStringView(),args.rval());
    return true;
  }
  switch(s.kind) {
    case 3: args.rval().setNumber(1); break; case 4: args.rval().setNumber(0); break;
    case 5: args.rval().setBoolean(true); break; case 6: args.rval().setBoolean(false); break;
    case 7: case 8: args.rval().setInt32(0); break;
  }
  return true;
}
bool Set(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); const Spec& s=kFields[FunctionSlot(args)];
  Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement"); if(!entry)return false;
  State& state=*entry->owner; DomString text;
  if(s.kind==2) {
    if(!JS::ToBoolean(args.get(0))) return state.Check(state.model.RemoveAttribute(entry->key,s.attr));
    return state.Check(state.model.SetAttribute(entry->key,s.attr,u""));
  }
  if(s.kind==3) { double value; if(!JS::ToNumber(cx,args.get(0),&value))return false;
    if(!std::isfinite(value)||value<0||value>1)return WebKit::FailKind(cx,"volume is outside the valid range",JSEXN_RANGEERR);
    args.rval().setUndefined(); return true;
  }
  if(s.kind==4) { double value; if(!JS::ToNumber(cx,args.get(0),&value))return false; args.rval().setUndefined(); return true; }
  if(!ControlString(cx,args.get(0),&text))return false;
  args.rval().setUndefined(); return state.Check(state.model.SetAttribute(entry->key,s.attr,text));
}
bool VoidMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); if(!ControlReceiver(cx,args.thisv(),"HTMLMediaElement"))return false;
  args.rval().setUndefined(); return true;
}
// The answer comes from the web_media domain's decoder rules (Firefox
// DecoderTraits); this adapter only converts the DOMString argument.
bool CanPlayType(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); if(!ControlReceiver(cx,args.thisv(),"HTMLMediaElement"))return false;
  if(!argc) return WebKit::Fail(cx,"HTMLMediaElement.canPlayType: At least 1 argument required, but only 0 passed");
  JS::RootedString type(cx,JS::ToString(cx,args[0])); std::string text;
  if(!type || !ToUTF8(cx,type,&text)) return false;
  const char* answer=domain::web_media::CanPlayText(domain::web_media::CanPlayType(text));
  JSString* result=JS_NewStringCopyZ(cx,answer); if(!result)return false;
  args.rval().setString(result); return true;
}
bool Play(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); if(!ControlReceiver(cx,args.thisv(),"HTMLMediaElement"))return false;
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr)); if(!promise)return false;
  if(!JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue))return false; args.rval().setObject(*promise); return true;
}
bool MediaKeysGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement");
  if(!entry)return false; JS::RootedValue value(cx,entry->mediaKeys);
  if(!JS_WrapValue(cx,&value))return false; args.rval().set(value); return true;
}
bool HandlerGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement");
  if(!entry)return false;
  const int index=FunctionSlot(args);
  {
    JSAutoRealm realm(cx,entry->owner->page);
    JS::RootedObject object(cx,entry->object);
    JS::RootedValue map(cx,JS::GetReservedSlot(object,2));
    args.rval().setNull();
    if(map.isObject()) { JS::RootedObject values(cx,&map.toObject());
      if(!JS_GetElement(cx,values,index,args.rval()))return false;
    }
    if(args.rval().isUndefined())args.rval().setNull();
  }
  return JS_WrapValue(cx,args.rval());
}
bool HandlerSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement");
  if(!entry)return false; State& state=*entry->owner; const int index=FunctionSlot(args);
  JSAutoRealm realm(cx,state.page); JS::RootedObject object(cx,entry->object), map(cx);
  JS::RootedValue desired(cx,args.get(0)); if(!JS_WrapValue(cx,&desired))return false;
  if(!desired.isObject() || !JS::IsCallable(&desired.toObject()))desired.setNull();
  JS::RootedValue stored(cx,JS::GetReservedSlot(object,2)); map=stored.isObject()?&stored.toObject():JS_NewPlainObject(cx);
  if(!map)return false; JS::SetReservedSlot(object,2,JS::ObjectValue(*map)); JS::RootedValue previous(cx);
  if(!JS_GetElement(cx,map,index,&previous))return false; JS::RootedObject prototype(cx,state.interaction.prototype());
  JS::RootedValue function(cx),receiver(cx,JS::ObjectValue(*object)),ignored(cx);
  static constexpr const char* kHandlerTypes[]={"encrypted","waitingforkey","enterpictureinpicture","leavepictureinpicture"};
  const char* typeName=kHandlerTypes[index]; JS::RootedString type(cx,JS_NewStringCopyZ(cx,typeName));
  if(!type)return false; JS::RootedValueArray<2> values(cx); values[0].setString(type);
  for(int operation=0;operation<2;++operation){values[1].set(operation?desired:previous); if(!values[1].isObject())continue;
    if(!JS_GetProperty(cx,prototype,operation?"addEventListener":"removeEventListener",&function) ||
       !JS::Call(cx,receiver,function,values,&ignored))return false;}
  if(!JS_SetElement(cx,map,index,desired))return false; args.rval().setUndefined(); return true;
}
bool SetMediaKeys(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement"); if(!entry)return false;
  State& state=*entry->owner; JS::RootedValue value(cx,args.get(0));
  if(!value.isNull() && (!state.mediaKeysCheck || !state.mediaKeysCheck(state.mediaKeysCheckContext,value)))
    return WebKit::Fail(cx,"setMediaKeys: value is not a MediaKeys object");
  entry->mediaKeys.set(value);
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr)); if(!promise)return false;
  if(!JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue))return false; args.rval().setObject(*promise); return true;
}
// The element's single TextTrackList: created by WebMedia in the element's
// realm on first use and held by the element's own media map, so it lives
// exactly as long as the element and is the same object on every read.
bool TrackList(JSContext* cx, Wrapper* entry, JS::MutableHandleObject list) {
  State& state=*entry->owner;
  if(!state.textTracks.list || !state.textTracks.append)
    return WebKit::Fail(cx,"HTMLMediaElement text tracks are not bound");
  JSAutoRealm realm(cx,state.page);
  JS::RootedObject object(cx,entry->object), map(cx);
  JS::RootedValue stored(cx,JS::GetReservedSlot(object,2));
  map=stored.isObject()?&stored.toObject():JS_NewPlainObject(cx);
  if(!map)return false;
  JS::SetReservedSlot(object,2,JS::ObjectValue(*map));
  bool cached=false; JS::RootedValue value(cx);
  if(!JS_HasOwnProperty(cx,map,"textTracks",&cached))return false;
  if(cached) {
    if(!JS_GetProperty(cx,map,"textTracks",&value))return false;
    list.set(&value.toObject()); return true;
  }
  if(!state.textTracks.list(cx,list))return false;
  value.setObject(*list);
  return JS_DefineProperty(cx,map,"textTracks",value,JSPROP_READONLY|JSPROP_PERMANENT);
}
bool TextTracksGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement","get textTracks");
  JS::RootedObject list(cx);
  if(!entry || !TrackList(cx,entry,&list))return false;
  args.rval().setObject(*list); return JS_WrapValue(cx,args.rval());
}
bool OptionalString(JSContext* cx,const JS::CallArgs& args,unsigned index,JS::MutableHandleString out) {
  out.set(args.length()>index && !args[index].isUndefined()?JS::ToString(cx,args[index]):JS_GetEmptyString(cx));
  return out!=nullptr;
}
bool AddTextTrack(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement","addTextTrack");
  if(!entry)return false;
  if(!argc)return TypeError(cx,"HTMLMediaElement.addTextTrack: At least 1 argument required, but only 0 passed");
  JS::RootedString kind(cx,JS::ToString(cx,args[0])), label(cx), language(cx);
  std::string text;
  if(!kind || !ToUTF8(cx,kind,&text))return false;
  static constexpr const char* kKinds[]={"subtitles","captions","descriptions","chapters","metadata"};
  const char* known=nullptr;
  for(const char* candidate:kKinds) if(text==candidate) known=candidate;
  if(!known)
    return TypeError(cx,("HTMLMediaElement.addTextTrack: '"+text+
        "' (value of argument 1) is not a valid value for enumeration TextTrackKind.").c_str());
  JS::RootedObject list(cx), track(cx);
  if(!OptionalString(cx,args,1,&label) || !OptionalString(cx,args,2,&language) ||
     !TrackList(cx,entry,&list) || !entry->owner->textTracks.append(cx,list,known,label,language,&track))
    return false;
  args.rval().setObject(*track); return JS_WrapValue(cx,args.rval());
}

// Firefox 161's remaining HTMLMediaElement members. ruyijs decodes no media,
// so an element never leaves HAVE_NOTHING/NETWORK_EMPTY and every member
// answers what Firefox answers for an element without a loaded resource
// (recorded from Firefox 161: duration NaN, empty TimeRanges, error null,
// sinkId "", ...). Script-settable state lives in the element's media map
// (reserved slot 2) next to its event handlers and text track list.
enum MediaValue { MediaErrorValue, Buffered, Played, Seekable, Seeking, Duration, FragmentEnd,
                  SinkId, AudioCaptured, PlaybackRate, DefaultPlaybackRate, PreservesPitch,
                  SrcObject, CrossOrigin };
constexpr const char* kMediaValueNames[] = {
    "error", "buffered", "played", "seekable", "seeking", "duration", "mozFragmentEnd",
    "sinkId", "mozAudioCaptured", "playbackRate", "defaultPlaybackRate", "preservesPitch",
    "srcObject", "crossOrigin"};
bool MediaMap(JSContext* cx, Wrapper* entry, JS::MutableHandleObject map) {
  JS::RootedObject object(cx, entry->object);
  JS::RootedValue stored(cx, JS::GetReservedSlot(object, 2));
  map.set(stored.isObject() ? &stored.toObject() : JS_NewPlainObject(cx));
  if (!map) return false;
  JS::SetReservedSlot(object, 2, JS::ObjectValue(*map));
  return true;
}
// A value stored under `key` in the element's media map (undefined when unset),
// read and written in the element's realm.
bool ReadMediaMap(JSContext* cx, Wrapper* entry, const char* key, JS::MutableHandleValue out) {
  {
    JSAutoRealm realm(cx, entry->owner->page);
    JS::RootedObject map(cx);
    if (!MediaMap(cx, entry, &map) || !JS_GetProperty(cx, map, key, out)) return false;
  }
  return JS_WrapValue(cx, out);
}
bool WriteMediaMap(JSContext* cx, Wrapper* entry, const char* key, JS::HandleValue input) {
  JSAutoRealm realm(cx, entry->owner->page);
  JS::RootedObject map(cx);
  JS::RootedValue value(cx, input);
  return MediaMap(cx, entry, &map) && JS_WrapValue(cx, &value) && JS_SetProperty(cx, map, key, value);
}
bool MediaPortsBound(JSContext* cx, State& state) {
  return state.mediaValues.timeRanges || WebKit::Fail(cx, "HTMLMediaElement media values are not bound");
}
// A DOMException of `name` in the element's realm, as a value to throw or reject with.
bool DomError(JSContext* cx, State& state, const char* name, const char* message, JS::MutableHandleValue out) {
  {
    JSAutoRealm realm(cx, state.page);
    if (!ThrowDOM(state, name, message) && !JS_GetPendingException(cx, out)) return false;
    JS_ClearPendingException(cx);
  }
  return JS_WrapValue(cx, out);
}
bool Rejected(JSContext* cx, JS::HandleValue reason, JS::MutableHandleValue out) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::RejectPromise(cx, promise, reason)) return false;
  out.setObject(*promise);
  return true;
}
bool MediaValueGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int which = FunctionSlot(args);
  Wrapper* entry = ControlReceiver(cx, args.thisv(), "HTMLMediaElement");
  if (!entry) return false;
  State& state = *entry->owner;
  switch (which) {
    case MediaErrorValue: args.rval().setNull(); return true;
    case Seeking: case AudioCaptured: args.rval().setBoolean(false); return true;
    case Duration: case FragmentEnd: args.rval().setNaN(); return true;
    case SinkId: args.rval().setString(JS_GetEmptyString(cx)); return true;
    case Buffered: case Played: case Seekable: {
      if (!MediaPortsBound(cx, state)) return false;
      JS::RootedObject ranges(cx);
      {
        JSAutoRealm realm(cx, state.page);
        if (!state.mediaValues.timeRanges(cx, &ranges)) return false;
      }
      args.rval().setObject(*ranges);
      return JS_WrapValue(cx, args.rval());
    }
    case CrossOrigin: {  // CORS settings attribute: missing -> null, else a keyword
      std::optional<DomString> text;
      if (!state.Check(state.model.GetAttribute(entry->key, u"crossorigin", &text))) return false;
      if (!text) { args.rval().setNull(); return true; }
      std::u16string lower(text->begin(), text->end());
      for (char16_t& c : lower) if (c >= u'A' && c <= u'Z') c = char16_t(c - u'A' + u'a');
      return Value(cx, DomStringView(lower == u"use-credentials" ? u"use-credentials" : u"anonymous"),
                   args.rval());
    }
    default: break;
  }
  if (!ReadMediaMap(cx, entry, kMediaValueNames[which], args.rval())) return false;
  if (args.rval().isUndefined()) {
    if (which == SrcObject) args.rval().setNull();
    else if (which == PreservesPitch) args.rval().setBoolean(true);
    else args.rval().setNumber(1.0);  // playbackRate, defaultPlaybackRate
  }
  return true;
}
bool MediaValueSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int which = FunctionSlot(args);
  Wrapper* entry = ControlReceiver(cx, args.thisv(), "HTMLMediaElement");
  if (!entry) return false;
  State& state = *entry->owner;
  args.rval().setUndefined();
  if (which == CrossOrigin) {
    if (args.get(0).isNullOrUndefined())
      return state.Check(state.model.RemoveAttribute(entry->key, u"crossorigin"));
    DomString text;
    return ControlString(cx, args.get(0), &text) &&
           state.Check(state.model.SetAttribute(entry->key, u"crossorigin", text));
  }
  if (which == PreservesPitch) {
    JS::RootedValue flag(cx, JS::BooleanValue(JS::ToBoolean(args.get(0))));
    return WriteMediaMap(cx, entry, kMediaValueNames[which], flag);
  }
  if (which == SrcObject) {
    JS::RootedValue value(cx, args.get(0));
    if (!value.isNullOrUndefined()) {
      if (!value.isObject() || !state.mediaValues.isStream || !state.mediaValues.isStream(&value.toObject()))
        return WebKit::FailKind(cx, "HTMLMediaElement.srcObject setter: Value being assigned does not "
                                    "implement interface MediaStream.", JSEXN_TYPEERR);
    } else {
      value.setNull();
    }
    return WriteMediaMap(cx, entry, kMediaValueNames[which], value);
  }
  // playbackRate / defaultPlaybackRate: WebIDL double, then HTMLMediaElement's
  // own rules (a negative rate is refused; defaultPlaybackRate is clamped to
  // [1/16, 16] like ClampPlaybackRate, playbackRate is kept as given).
  double rate = 0;
  if (!JS::ToNumber(cx, args.get(0), &rate)) return false;
  if (!std::isfinite(rate))
    return WebKit::FailKind(cx, (std::string("HTMLMediaElement.") + kMediaValueNames[which] +
                                 " setter: Value being assigned is not a finite floating-point value.").c_str(),
                            JSEXN_TYPEERR);
  if (rate < 0) {
    JS::RootedValue error(cx);
    if (!DomError(cx, state, "NotSupportedError", "The operation is not supported.", &error)) return false;
    JS_SetPendingException(cx, error);
    return false;
  }
  if (which == DefaultPlaybackRate && rate != 0) rate = std::clamp(rate, 1.0 / 16, 16.0);
  JS::RootedValue value(cx, JS::NumberValue(rate));
  return WriteMediaMap(cx, entry, kMediaValueNames[which], value);
}
enum MediaCall { GetMetadata, SetSinkId, CaptureStream };
bool MediaCallMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int which = FunctionSlot(args);
  Wrapper* entry = ControlReceiver(cx, args.thisv(), "HTMLMediaElement");
  if (!entry) return false;
  State& state = *entry->owner;
  if (which == GetMetadata) {  // readyState < HAVE_METADATA
    JS::RootedValue error(cx);
    if (!DomError(cx, state, "InvalidStateError",
                  "An attempt was made to use an object that is not, or is no longer, usable", &error))
      return false;
    JS_SetPendingException(cx, error);
    return false;
  }
  if (which == SetSinkId) {
    if (!argc)
      return WebKit::FailKind(cx, "HTMLMediaElement.setSinkId: At least 1 argument required, but only 0 passed",
                              JSEXN_TYPEERR);
    JS::RootedString id(cx, JS::ToString(cx, args[0]));
    if (!id) return false;
    JS::RootedObject promise(cx);
    {
      JSAutoRealm realm(cx, state.page);
      promise = JS::NewPromiseObject(cx, nullptr);
      if (!promise) return false;
      // The current sink is the default device "": choosing it again resolves;
      // any other id names no audio output device this profile has.
      if (JS_GetStringLength(id) == 0) {
        if (!JS::ResolvePromise(cx, promise, JS::UndefinedHandleValue)) return false;
      } else {
        JS::RootedValue error(cx);
        if (!DomError(cx, state, "NotFoundError", "The object can not be found here.", &error) ||
            !JS::RejectPromise(cx, promise, error)) return false;
      }
    }
    args.rval().setObject(*promise);
    return JS_WrapValue(cx, args.rval());
  }
  // captureStream / mozCaptureStream / mozCaptureStreamUntilEnded: a stream with
  // no tracks, since the element plays nothing.
  if (!MediaPortsBound(cx, state)) return false;
  JS::RootedObject stream(cx);
  {
    JSAutoRealm realm(cx, state.page);
    if (!state.mediaValues.newStream(cx, &stream)) return false;
  }
  args.rval().setObject(*stream);
  return JS_WrapValue(cx, args.rval());
}
bool InstallMediaValues(State& state) {
  for (int i = MediaErrorValue; i <= CrossOrigin; ++i) {
    const bool writable = i >= PlaybackRate;
    if (!state.Accessor("HTMLMediaElement", kMediaValueNames[i], MediaValueGet, i,
                        writable ? MediaValueSet : nullptr)) return false;
  }
  if (!state.Method("HTMLMediaElement", "mozGetMetadata", MediaCallMethod, 0, GetMetadata) ||
      !state.Method("HTMLMediaElement", "setSinkId", MediaCallMethod, 1, SetSinkId) ||
      !state.Method("HTMLMediaElement", "captureStream", MediaCallMethod, 0, CaptureStream) ||
      !state.Method("HTMLMediaElement", "mozCaptureStream", MediaCallMethod, 0, CaptureStream) ||
      !state.Method("HTMLMediaElement", "mozCaptureStreamUntilEnded", MediaCallMethod, 0, CaptureStream))
    return false;
  // NETWORK_* and HAVE_* on the interface object and its prototype.
  constexpr struct { const char* name; int value; } kConstants[] = {
      {"NETWORK_EMPTY", 0}, {"NETWORK_IDLE", 1}, {"NETWORK_LOADING", 2}, {"NETWORK_NO_SOURCE", 3},
      {"HAVE_NOTHING", 0}, {"HAVE_METADATA", 1}, {"HAVE_CURRENT_DATA", 2}, {"HAVE_FUTURE_DATA", 3},
      {"HAVE_ENOUGH_DATA", 4}};
  Interface* face = state.Find("HTMLMediaElement");
  JS::RootedObject prototype(state.cx, face->prototype), constructor(state.cx, face->constructor);
  // The prototype copies stay configurable until the binding-order pass has
  // placed them after the attributes (runtime/binding_order_table.cpp
  // SealConstants makes them permanent then), as Firefox lists them last.
  for (const auto& constant : kConstants) {
    JS::RootedValue value(state.cx, JS::Int32Value(constant.value));
    const unsigned sealed = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(state.cx, prototype, constant.name, value, JSPROP_ENUMERATE | JSPROP_READONLY) ||
        !JS_DefineProperty(state.cx, constructor, constant.name, value, sealed)) return false;
  }
  return true;
}

// HTMLVideoElement (Firefox 161): no frame is ever decoded, so the intrinsic
// size and the frame counters are 0 and the playback quality reports no frames.
enum VideoValue { VideoWidthAttr, VideoHeightAttr, IntrinsicWidth, IntrinsicHeight, Poster,
                  ParsedFrames, DecodedFrames, PresentedFrames, PaintedFrames, FrameDelay, HasAudio,
                  DisablePictureInPicture };
constexpr const char* kVideoValueNames[] = {
    "width", "height", "videoWidth", "videoHeight", "poster", "mozParsedFrames", "mozDecodedFrames",
    "mozPresentedFrames", "mozPaintedFrames", "mozFrameDelay", "mozHasAudio", "disablePictureInPicture"};
// HTML "rules for parsing non-negative integers", reflected as unsigned long:
// a missing, invalid or out-of-range value is 0.
uint32_t ReflectedUnsigned(const std::optional<DomString>& text) {
  if (!text) return 0;
  size_t i = 0;
  const DomString& s = *text;
  while (i < s.size() && (s[i] == u' ' || s[i] == u'\t' || s[i] == u'\n' || s[i] == u'\f' || s[i] == u'\r')) ++i;
  if (i < s.size() && s[i] == u'+') ++i;
  if (i >= s.size() || s[i] < u'0' || s[i] > u'9') return 0;
  uint64_t value = 0;
  for (; i < s.size() && s[i] >= u'0' && s[i] <= u'9'; ++i) {
    value = value * 10 + uint64_t(s[i] - u'0');
    if (value > 2147483647u) return 0;
  }
  return uint32_t(value);
}
bool VideoValueGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int which = FunctionSlot(args);
  Wrapper* entry = ControlReceiver(cx, args.thisv(), "HTMLVideoElement");
  if (!entry) return false;
  State& state = *entry->owner;
  std::optional<DomString> text;
  switch (which) {
    case VideoWidthAttr: case VideoHeightAttr:
      if (!state.Check(state.model.GetAttribute(entry->key, which == VideoWidthAttr ? u"width" : u"height", &text)))
        return false;
      args.rval().setNumber(ReflectedUnsigned(text));
      return true;
    case Poster:
      if (!state.Check(state.model.GetAttribute(entry->key, u"poster", &text))) return false;
      if (!text) { args.rval().setString(JS_GetEmptyString(cx)); return true; }
      return Resolved(cx, state, DomStringView(*text), args.rval());
    case HasAudio: args.rval().setBoolean(false); return true;
    case DisablePictureInPicture:
      if (!state.Check(state.model.GetAttribute(entry->key, u"disablepictureinpicture", &text))) return false;
      args.rval().setBoolean(text.has_value());
      return true;
    default: args.rval().setInt32(0); return true;  // intrinsic size, frame counters, delay
  }
}
bool VideoValueSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int which = FunctionSlot(args);
  Wrapper* entry = ControlReceiver(cx, args.thisv(), "HTMLVideoElement");
  if (!entry) return false;
  State& state = *entry->owner;
  args.rval().setUndefined();
  if (which == DisablePictureInPicture) {
    if (!JS::ToBoolean(args.get(0)))
      return state.Check(state.model.RemoveAttribute(entry->key, u"disablepictureinpicture"));
    return state.Check(state.model.SetAttribute(entry->key, u"disablepictureinpicture", u""));
  }
  if (which == Poster) {
    DomString text;
    return ControlString(cx, args.get(0), &text) &&
           state.Check(state.model.SetAttribute(entry->key, u"poster", text));
  }
  // width/height: unsigned long reflection; a value above 2^31-1 stores the default 0.
  uint32_t value = 0;
  if (!JS::ToUint32(cx, args.get(0), &value)) return false;
  if (value > 2147483647u) value = 0;
  const std::string digits = std::to_string(value);
  return state.Check(state.model.SetAttribute(entry->key, which == VideoWidthAttr ? u"width" : u"height",
                                              DomString(digits.begin(), digits.end())));
}
enum VideoCall { PlaybackQuality, PictureInPicture, RequestFrame, CancelFrame };
bool VideoCallMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int which = FunctionSlot(args);
  Wrapper* entry = ControlReceiver(cx, args.thisv(), "HTMLVideoElement");
  if (!entry) return false;
  State& state = *entry->owner;
  if (which == PlaybackQuality) {
    if (!MediaPortsBound(cx, state)) return false;
    JS::RootedObject quality(cx);
    {
      JSAutoRealm realm(cx, state.page);
      const double now = state.mediaValues.now ? state.mediaValues.now(state.mediaValues.nowContext) : 0.0;
      if (!state.mediaValues.playbackQuality(cx, std::isfinite(now) ? now : 0.0, 0, 0, &quality)) return false;
    }
    args.rval().setObject(*quality);
    return JS_WrapValue(cx, args.rval());
  }
  if (which == PictureInPicture) {  // readyState is HAVE_NOTHING
    JS::RootedValue error(cx);
    if (!DomError(cx, state, "InvalidStateError", "Video readyState is HAVE_NOTHING", &error)) return false;
    {
      JSAutoRealm realm(cx, state.page);
      if (!JS_WrapValue(cx, &error) || !Rejected(cx, error, args.rval())) return false;
    }
    return JS_WrapValue(cx, args.rval());
  }
  if (which == RequestFrame) {
    if (!argc)
      return WebKit::FailKind(cx, "HTMLVideoElement.requestVideoFrameCallback: At least 1 argument required, "
                                  "but only 0 passed", JSEXN_TYPEERR);
    if (!args[0].isObject() || !JS::IsCallable(&args[0].toObject()))
      return WebKit::FailKind(cx, "HTMLVideoElement.requestVideoFrameCallback: Argument 1 is not callable.",
                              JSEXN_TYPEERR);
    // No frame is ever presented, so the callback never runs; the handle still
    // counts up per element from 1, as VideoFrameRequestManager's does.
    JS::RootedValue last(cx);
    if (!ReadMediaMap(cx, entry, "videoFrameHandle", &last)) return false;
    const double next = (last.isNumber() ? last.toNumber() : 0) + 1;
    JS::RootedValue handle(cx, JS::NumberValue(next));
    if (!WriteMediaMap(cx, entry, "videoFrameHandle", handle)) return false;
    args.rval().set(handle);
    return true;
  }
  if (!argc)
    return WebKit::FailKind(cx, "HTMLVideoElement.cancelVideoFrameCallback: At least 1 argument required, "
                                "but only 0 passed", JSEXN_TYPEERR);
  uint32_t handle = 0;
  if (!JS::ToUint32(cx, args[0], &handle)) return false;
  args.rval().setUndefined();
  return true;
}
bool InstallVideoElement(State& state) {
  for (int i = VideoWidthAttr; i <= DisablePictureInPicture; ++i) {
    const bool writable = i == VideoWidthAttr || i == VideoHeightAttr || i == Poster || i == DisablePictureInPicture;
    if (!state.Accessor("HTMLVideoElement", kVideoValueNames[i], VideoValueGet, i,
                        writable ? VideoValueSet : nullptr)) return false;
  }
  return state.Method("HTMLVideoElement", "getVideoPlaybackQuality", VideoCallMethod, 0, PlaybackQuality) &&
         state.Method("HTMLVideoElement", "requestPictureInPicture", VideoCallMethod, 0, PictureInPicture) &&
         state.Method("HTMLVideoElement", "requestVideoFrameCallback", VideoCallMethod, 1, RequestFrame) &&
         state.Method("HTMLVideoElement", "cancelVideoFrameCallback", VideoCallMethod, 1, CancelFrame) &&
         state.Accessor("HTMLVideoElement", "onenterpictureinpicture", HandlerGet, 2, HandlerSet) &&
         state.Accessor("HTMLVideoElement", "onleavepictureinpicture", HandlerGet, 3, HandlerSet);
}
}
bool InstallMediaElement(State& state) {
  for(size_t i=0;i<std::size(kFields);++i) {
    bool writable=kFields[i].kind<=4 && i!=CurrentSrc;
    if(!state.Accessor("HTMLMediaElement",kFields[i].name,Get,int32_t(i),writable?Set:nullptr))return false;
  }
  if(!state.Accessor("HTMLMediaElement","mediaKeys",MediaKeysGet,0,nullptr) ||
     !state.Accessor("HTMLMediaElement","onencrypted",HandlerGet,0,HandlerSet) ||
     !state.Accessor("HTMLMediaElement","onwaitingforkey",HandlerGet,1,HandlerSet) ||
     !state.Method("HTMLMediaElement","setMediaKeys",SetMediaKeys,1,0) ||
     !state.Accessor("HTMLMediaElement","textTracks",TextTracksGet,0,nullptr) ||
     !state.Method("HTMLMediaElement","addTextTrack",AddTextTrack,1,0)) return false;
  return state.Method("HTMLMediaElement","load",VoidMethod,0,0) &&
    state.Method("HTMLMediaElement","canPlayType",CanPlayType,1,0) &&
    state.Method("HTMLMediaElement","fastSeek",VoidMethod,1,0) &&
    state.Method("HTMLMediaElement","pause",VoidMethod,0,0) &&
    state.Method("HTMLMediaElement","play",Play,0,0) &&
    InstallMediaValues(state) && InstallVideoElement(state);
}
}
