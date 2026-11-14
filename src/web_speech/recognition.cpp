// The Web Speech recognition interfaces Firefox 160 exposes on Window:
// SpeechGrammar, SpeechGrammarList, SpeechRecognition (with their webkit
// aliases), SpeechRecognitionAlternative, SpeechRecognitionErrorEvent,
// SpeechRecognitionEvent, SpeechRecognitionPhrase, SpeechRecognitionResult and
// SpeechRecognitionResultList.
//
// There is no recognition service: as in Firefox 160 without an on-device
// model or a granted microphone, start() leaves the recognition waiting and no
// result ever arrives; stop()/abort() of a started recognition fire `end` in a
// task. SpeechRecognition.available() answers from Firefox's on-device model
// languages ("downloadable" with processLocally, else "unavailable");
// install() needs transient user activation, which a headless page never has.
// Recorded deviations: SpeechGrammar.weight throws an Error named
// NS_ERROR_NOT_IMPLEMENTED (Firefox: an XPCOM Exception), `phrases` is a plain
// array (Firefox: an ObservableArray that type-checks pushes), and the webkit
// aliases have no static methods.
#include "web_speech/internal.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/WeakMap.h"
#include "js/Wrapper.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace ruyijs::web_speech {
namespace {
enum GrammarSlot { GrammarSrc = 0, GrammarSlotCount };
enum ListSlot { ListItems = 0, ListSlotCount };
enum PhraseSlot { PhraseText = 0, PhraseBoost, PhraseSlotCount };
// Attributes in member order, then the started flag and the handler slots.
enum RecSlot { RecGrammars = 0, RecLang, RecContinuous, RecInterim, RecPunctuation, RecMaxAlternatives,
               RecProcessLocally, RecPhrases, RecStarted, RecHandlerFirst,
               RecSlotCount = RecHandlerFirst + 11 };
constexpr const char* kRecAttributes[] = {"grammars", "lang", "continuous", "interimResults",
                                          "unspokenPunctuation", "maxAlternatives", "processLocally",
                                          "phrases"};
constexpr const char* kRecHandlers[11] = {"audiostart", "soundstart", "speechstart", "speechend",
                                          "soundend", "audioend", "result", "nomatch", "error",
                                          "start", "end"};
const JSClass kGrammarClass = {"SpeechGrammar", JSCLASS_HAS_RESERVED_SLOTS(GrammarSlotCount)};
const JSClass kGrammarListClass = {"SpeechGrammarList", JSCLASS_HAS_RESERVED_SLOTS(ListSlotCount)};
const JSClass kRecognitionClass = {"SpeechRecognition", JSCLASS_HAS_RESERVED_SLOTS(RecSlotCount)};
const JSClass kPhraseClass = {"SpeechRecognitionPhrase", JSCLASS_HAS_RESERVED_SLOTS(PhraseSlotCount)};
// Never instantiated: no recognition result ever arrives.
const JSClass kAlternativeClass = {"SpeechRecognitionAlternative", 0};
const JSClass kResultClass = {"SpeechRecognitionResult", 0};
const JSClass kResultListClass = {"SpeechRecognitionResultList", 0};
const JSClass kRecognitionEventClass = {"SpeechRecognitionEvent", 0};
const JSClass kRecognitionErrorEventClass = {"SpeechRecognitionErrorEvent", 0};

constexpr const char* kErrorCodes[] = {"no-speech", "aborted", "audio-capture", "network", "not-allowed",
                                       "service-not-allowed", "bad-grammar", "language-not-supported",
                                       "phrases-not-supported"};
// Primary language subtags Firefox 160 reports "downloadable" for with
// processLocally (probed: SpeechRecognition.available on Firefox 160.0, Windows).
constexpr const char* kLocalLanguages[] = {
    "af", "am", "ar", "ay", "az", "bg", "bn", "cs", "da", "de", "el", "en", "es", "et", "fa", "fi",
    "fr", "gn", "gu", "ha", "haw", "he", "hi", "hr", "hu", "hy", "id", "ig", "it", "ja", "ka", "km",
    "kn", "ko", "ku", "ky", "ln", "lt", "lv", "mi", "ml", "mr", "ms", "mt", "nb", "ne", "nl", "nn",
    "no", "ny", "or", "pl", "pt", "qu", "ro", "ru", "rw", "si", "sk", "sl", "sm", "so", "sv", "sw",
    "ta", "te", "tg", "th", "to", "tr", "uk", "ur", "uz", "vi", "yo", "zh", "zu"};

State& Self(const JS::CallArgs& args) { return *static_cast<State*>(WebKit::Owner(args)); }
bool Fail(JSContext* cx, const std::string& text) { return WebKit::Fail(cx, text.c_str()); }
bool Arguments(JSContext* cx, const JS::CallArgs& args, unsigned needed, const char* where) {
  if (args.length() >= needed) return true;
  return Fail(cx, std::string(where) + ": At least " + std::to_string(needed) + " argument" +
                      (needed == 1 ? "" : "s") + " required, but only " + std::to_string(args.length()) +
                      " passed");
}
bool Text(JSContext* cx, JS::HandleString text, std::string* out) {
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
  if (!utf8) return false;
  *out = utf8.get();
  return true;
}
bool DOMError(State& s, const char* name, const char* message, JS::MutableHandleValue out) {
  JSContext* cx = s.cx;
  JS::RootedValue constructor(cx);
  if (!JS_GetProperty(cx, s.kit.page, "DOMException", &constructor) || !constructor.isObject())
    return WebKit::Fail(cx, "Speech recognition requires the page DOMException");
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message)), label(cx, JS_NewStringCopyZ(cx, name));
  if (!text || !label) return false;
  JS::RootedValueArray<2> arguments(cx);
  arguments[0].setString(text);
  arguments[1].setString(label);
  JS::RootedObject error(cx);
  if (!JS::Construct(cx, constructor, arguments, &error)) return false;
  out.setObject(*error);
  return true;
}
bool ThrowDOM(State& s, const char* name, const char* message) {
  JS::RootedValue error(s.cx);
  if (!DOMError(s, name, message, &error)) return false;
  JS_SetPendingException(s.cx, error);
  return false;
}
// Firefox throws an XPCOM NS_ERROR_NOT_IMPLEMENTED Exception; ruyijs has no
// XPCOM Exception owner, so this is an Error with that name and no message.
bool NotImplemented(JSContext* cx) {
  WebKit::FailKind(cx, "", JSEXN_ERR);
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error) || !error.isObject()) return false;
  JS_ClearPendingException(cx);
  JS::RootedObject object(cx, &error.toObject());
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, "NS_ERROR_NOT_IMPLEMENTED"));
  if (!name || !JS_DefineProperty(cx, object, "name", name, 0)) return false;
  JS_SetPendingException(cx, error);
  return false;
}
bool Unwrapped(JS::HandleValue value, const JSClass* clasp, JS::MutableHandleObject out) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != clasp) return false;
  out.set(object);
  return true;
}

// ---- SpeechGrammar ---------------------------------------------------------
JSObject* NewGrammar(State& s, JS::HandleObject proto, JS::HandleString src) {
  JS::RootedObject self(s.cx, s.kit.New(&kGrammarClass, proto));
  if (self) JS::SetReservedSlot(self, GrammarSrc, JS::StringValue(src));
  return self;
}
bool GrammarConstruct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "SpeechGrammar constructor: 'new' is required");
  JS::RootedObject fallback(cx, s.grammar.prototype), proto(cx);
  if (!s.kit.ConstructProto(args, fallback, &proto)) return false;
  JS::RootedString empty(cx, JS_GetEmptyString(cx));
  JS::RootedObject self(cx, NewGrammar(s, proto, empty));
  if (!self) return false;
  args.rval().setObject(*self);
  return true;
}
bool GrammarGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kGrammarClass, &self, "SpeechGrammar")) return false;
  if (WebKit::Slot(args) == 1) return NotImplemented(cx);
  args.rval().set(JS::GetReservedSlot(self, GrammarSrc));
  return JS_WrapValue(cx, args.rval());
}
bool GrammarSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  // rval() is the callee's slot (vp[0]): read the callee's slot first.
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kGrammarClass, &self, "SpeechGrammar")) return false;
  args.rval().setUndefined();
  if (slot == 1) {
    double ignored = 0;
    return JS::ToNumber(cx, args.get(0), &ignored) && NotImplemented(cx);
  }
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  JSAutoRealm realm(cx, self);
  JS::RootedValue value(cx, JS::StringValue(text));
  if (!JS_WrapValue(cx, &value)) return false;
  JS::SetReservedSlot(self, GrammarSrc, value);
  return true;
}

// ---- SpeechGrammarList -----------------------------------------------------
JSObject* NewGrammarList(State& s, JS::HandleObject proto) {
  JSContext* cx = s.cx;
  JS::RootedObject self(cx, s.kit.New(&kGrammarListClass, proto)), items(cx, JS::NewArrayObject(cx, 0));
  if (!self || !items) return nullptr;
  JS::SetReservedSlot(self, ListItems, JS::ObjectValue(*items));
  return self;
}
bool GrammarListConstruct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "SpeechGrammarList constructor: 'new' is required");
  JS::RootedObject fallback(cx, s.grammarList.prototype), proto(cx);
  if (!s.kit.ConstructProto(args, fallback, &proto)) return false;
  JS::RootedObject self(cx, NewGrammarList(s, proto));
  if (!self) return false;
  args.rval().setObject(*self);
  return true;
}
bool GrammarListLength(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kGrammarListClass, &self, "SpeechGrammarList")) return false;
  JS::RootedObject items(cx, &JS::GetReservedSlot(self, ListItems).toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, items, &length)) return false;
  args.rval().setNumber(length);
  return true;
}
// item(index), addFromUri(src, weight), addFromString(string, weight).
bool GrammarListMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const int32_t which = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kGrammarListClass, &self, "SpeechGrammarList")) return false;
  const char* names[] = {"SpeechGrammarList.item", "SpeechGrammarList.addFromUri",
                         "SpeechGrammarList.addFromString"};
  if (!Arguments(cx, args, 1, names[which])) return false;
  JS::RootedObject items(cx, &JS::GetReservedSlot(self, ListItems).toObject());
  if (which == 0) {
    uint32_t index = 0, length = 0;
    if (!JS::ToUint32(cx, args[0], &index) || !JS::GetArrayLength(cx, items, &length)) return false;
    if (index >= length) {
      args.rval().setNull();
      return true;
    }
    if (!JS_GetElement(cx, items, index, args.rval())) return false;
    return JS_WrapValue(cx, args.rval());
  }
  JS::RootedString text(cx, JS::ToString(cx, args[0]));
  double weight = 1;
  if (!text || (args.length() > 1 && !args[1].isUndefined() && !JS::ToNumber(cx, args[1], &weight)))
    return false;
  if (!std::isfinite(weight))
    return Fail(cx, std::string(names[which]) + ": Argument 2 is not a finite floating-point value.");
  args.rval().setUndefined();
  if (which == 1) return true;  // Firefox keeps no grammar for a URI.
  JSAutoRealm realm(cx, self);
  JS::RootedObject proto(cx, s.grammar.prototype);
  JS::RootedValue srcValue(cx, JS::StringValue(text));
  if (!JS_WrapObject(cx, &proto) || !JS_WrapValue(cx, &srcValue)) return false;
  JS::RootedString src(cx, srcValue.toString());
  JS::RootedObject grammar(cx, NewGrammar(s, proto, src));
  uint32_t length = 0;
  if (!grammar || !JS::GetArrayLength(cx, items, &length) ||
      !JS_DefineElement(cx, items, length, grammar, JSPROP_ENUMERATE)) return false;
  // The indexed getter's own property: enumerable, configurable, read-only.
  return JS_DefineElement(cx, self, length, grammar, JSPROP_ENUMERATE | JSPROP_READONLY);
}

// ---- SpeechRecognitionPhrase -----------------------------------------------
bool PhraseConstruct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  if (!args.isConstructing())
    return WebKit::Fail(cx, "SpeechRecognitionPhrase constructor: 'new' is required");
  if (!Arguments(cx, args, 1, "SpeechRecognitionPhrase constructor")) return false;
  JS::RootedObject fallback(cx, s.phrase.prototype), proto(cx);
  if (!s.kit.ConstructProto(args, fallback, &proto)) return false;
  JS::RootedString text(cx, JS::ToString(cx, args[0]));
  double boost = 1;
  if (!text || (args.length() > 1 && !args[1].isUndefined() && !JS::ToNumber(cx, args[1], &boost)))
    return false;
  if (!std::isfinite(boost))
    return WebKit::Fail(cx, "SpeechRecognitionPhrase constructor: Argument 2 is not a finite floating-point value.");
  const float stored = float(boost);
  if (!(stored >= 0 && stored <= 10))
    return ThrowDOM(s, "SyntaxError",
                    "SpeechRecognitionPhrase constructor: Boost value must be in range [0.0, 10.0]");
  JS::RootedObject self(cx, s.kit.New(&kPhraseClass, proto));
  if (!self) return false;
  JS::SetReservedSlot(self, PhraseText, JS::StringValue(text));
  JS::SetReservedSlot(self, PhraseBoost, JS::DoubleValue(double(stored)));
  args.rval().setObject(*self);
  return true;
}
bool PhraseGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kPhraseClass, &self, "SpeechRecognitionPhrase")) return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}

// ---- Interfaces with no instance -------------------------------------------
bool NeverGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  static const char* const ifaces[] = {"SpeechRecognitionAlternative", "SpeechRecognitionResult",
                                       "SpeechRecognitionResultList", "SpeechRecognitionEvent"};
  return WebKit::FailBrand(cx, args, ifaces[WebKit::Slot(args) / 8]);
}
bool IllegalConstruct(JSContext* cx, unsigned, JS::Value*) {
  return WebKit::Fail(cx, "Illegal constructor.");
}

// ---- SpeechRecognition -----------------------------------------------------
std::string RecMember(int32_t slot) {
  return slot < RecHandlerFirst ? kRecAttributes[slot]
                                : std::string("on") + kRecHandlers[slot - RecHandlerFirst];
}
bool RecognitionConstruct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "SpeechRecognition constructor: 'new' is required");
  JS::RootedObject fallback(cx, s.recognition.prototype), proto(cx), listProto(cx, s.grammarList.prototype);
  if (!s.kit.ConstructProto(args, fallback, &proto)) return false;
  JS::RootedObject self(cx, s.kit.New(&kRecognitionClass, proto));
  JS::RootedObject grammars(cx, NewGrammarList(s, listProto)), phrases(cx, JS::NewArrayObject(cx, 0));
  if (!self || !grammars || !phrases) return false;
  JS::SetReservedSlot(self, RecGrammars, JS::ObjectValue(*grammars));
  JS::SetReservedSlot(self, RecLang, JS::StringValue(JS_GetEmptyString(cx)));
  for (int32_t slot : {RecContinuous, RecInterim, RecPunctuation, RecProcessLocally, RecStarted})
    JS::SetReservedSlot(self, slot, JS::BooleanValue(false));
  JS::SetReservedSlot(self, RecMaxAlternatives, JS::Int32Value(1));
  JS::SetReservedSlot(self, RecPhrases, JS::ObjectValue(*phrases));
  for (int32_t slot = RecHandlerFirst; slot < RecSlotCount; ++slot) JS::SetReservedSlot(self, slot, JS::NullValue());
  if (!s.events.Attach(self)) return false;
  args.rval().setObject(*self);
  return true;
}
bool RecognitionGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kRecognitionClass, &self, "SpeechRecognition")) return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}
bool RecognitionSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kRecognitionClass, &self, "SpeechRecognition")) return false;
  JS::RootedValue value(cx, args.get(0)), stored(cx);
  args.rval().setUndefined();
  const std::string where = "SpeechRecognition." + RecMember(slot) + " setter: ";
  if (slot >= RecHandlerFirst)
    return SetHandler(s, self, kRecHandlers[slot - RecHandlerFirst], uint32_t(slot), value);
  if (slot == RecGrammars) {
    if (!value.isObject()) return Fail(cx, where + "Value being assigned is not an object.");
    JS::RootedObject list(cx);
    if (!Unwrapped(value, &kGrammarListClass, &list))
      return Fail(cx, where + "Value being assigned does not implement interface SpeechGrammarList.");
    stored.set(value);
  } else if (slot == RecLang) {
    JSString* text = JS::ToString(cx, value);
    if (!text) return false;
    stored.setString(text);
  } else if (slot == RecMaxAlternatives) {
    uint32_t number = 0;
    if (!JS::ToUint32(cx, value, &number)) return false;
    stored.setNumber(number);
  } else if (slot == RecPhrases) {
    // Firefox's ObservableArray keeps its identity: the backing list is replaced.
    bool array = false;
    if (!value.isObject() || !JS::IsArrayObject(cx, value, &array) || !array)
      return Fail(cx, where + "Value being assigned can't be converted to a sequence.");
    JS::RootedObject source(cx, &value.toObject());
    uint32_t length = 0;
    if (!JS::GetArrayLength(cx, source, &length)) return false;
    JS::RootedObjectVector phrases(cx);
    for (uint32_t i = 0; i < length; ++i) {
      JS::RootedValue item(cx);
      JS::RootedObject phrase(cx);
      if (!JS_GetElement(cx, source, i, &item)) return false;
      if (!item.isObject()) return Fail(cx, where + "Element of value being assigned is not an object.");
      if (!Unwrapped(item, &kPhraseClass, &phrase))
        return Fail(cx, where + "Element of value being assigned does not implement interface "
                                "SpeechRecognitionPhrase.");
      if (!phrases.append(&item.toObject())) return false;
    }
    JS::RootedObject list(cx, &JS::GetReservedSlot(self, RecPhrases).toObject());
    JSAutoRealm realm(cx, list);
    if (!JS::SetArrayLength(cx, list, 0)) return false;
    for (size_t i = 0; i < phrases.length(); ++i) {
      JS::RootedObject phrase(cx, phrases[i]);
      if (!JS_WrapObject(cx, &phrase) || !JS_DefineElement(cx, list, uint32_t(i), phrase, JSPROP_ENUMERATE))
        return false;
    }
    return true;
  } else {
    stored.setBoolean(JS::ToBoolean(value));
  }
  JSAutoRealm realm(cx, self);
  if (!JS_WrapValue(cx, &stored)) return false;
  JS::SetReservedSlot(self, slot, stored);
  return true;
}
// The `end` that stop()/abort() of a started recognition queues.
bool FireEnd(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  JS::RootedObject target(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  args.rval().setUndefined();
  return s.events.Fire(target, "end", false, false);
}
// start(optional MediaStreamTrack), stop(), abort().
bool RecognitionMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const int32_t which = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kRecognitionClass, &self, "SpeechRecognition")) return false;
  args.rval().setUndefined();
  const bool started = JS::GetReservedSlot(self, RecStarted).toBoolean();
  if (which == 0) {
    if (args.length() && !args[0].isUndefined() && !args[0].isObject())
      return WebKit::Fail(cx, "SpeechRecognition.start: Argument 1 is not an object.");
    if (started)
      return ThrowDOM(s, "InvalidStateError",
                      "An attempt was made to use an object that is not, or is no longer, usable");
    // Waiting for a service that never answers (no model, no microphone).
    JS::SetReservedSlot(self, RecStarted, JS::BooleanValue(true));
    return true;
  }
  if (!started) return true;
  JS::SetReservedSlot(self, RecStarted, JS::BooleanValue(false));
  if (!s.taskPort) return true;
  JSAutoRealm realm(cx, s.kit.page);
  JSFunction* function = js::NewFunctionWithReserved(cx, FireEnd, 0, 0, "end");
  if (!function) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(function)), target(cx, self);
  if (!JS_WrapObject(cx, &target)) return false;
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*target));
  return s.taskPort(s.taskContext, task, 0);
}
// ---- SpeechRecognition.available / install ----------------------------------
// A well-formed BCP 47 language tag (RFC 5646 langtag or private use); the
// irregular grandfathered tags are refused, as Firefox does.
bool WellFormed(const std::string& tag, std::string* primary, bool* privateUse) {
  std::vector<std::string> parts;
  size_t start = 0;
  for (;;) {
    const size_t dash = tag.find('-', start);
    parts.push_back(tag.substr(start, dash == std::string::npos ? std::string::npos : dash - start));
    if (dash == std::string::npos) break;
    start = dash + 1;
  }
  auto alpha = [](const std::string& p) {
    return std::all_of(p.begin(), p.end(), [](unsigned char c) { return std::isalpha(c); });
  };
  auto alnum = [](const std::string& p) {
    return std::all_of(p.begin(), p.end(), [](unsigned char c) { return std::isalnum(c); });
  };
  auto digits = [](const std::string& p) {
    return std::all_of(p.begin(), p.end(), [](unsigned char c) { return std::isdigit(c); });
  };
  for (const auto& p : parts)
    if (p.empty() || p.size() > 8 || !alnum(p)) return false;
  *privateUse = false;
  size_t i = 0;
  std::string first = parts[0];
  std::transform(first.begin(), first.end(), first.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  if (first == "x") { *privateUse = true; return parts.size() > 1; }
  if (!alpha(parts[0]) || parts[0].size() == 4 || parts[0].size() < 2) return false;
  *primary = first;
  ++i;
  // extlang (up to three 3-letter subtags after a 2-3 letter language)
  for (int n = 0; n < 3 && i < parts.size() && parts[0].size() <= 3 && parts[i].size() == 3 && alpha(parts[i]); ++n) ++i;
  if (i < parts.size() && parts[i].size() == 4 && alpha(parts[i])) ++i;  // script
  if (i < parts.size() && ((parts[i].size() == 2 && alpha(parts[i])) || (parts[i].size() == 3 && digits(parts[i])))) ++i;
  while (i < parts.size() && ((parts[i].size() >= 5) || (parts[i].size() == 4 && std::isdigit((unsigned char)parts[i][0])))) ++i;
  while (i < parts.size() && parts[i].size() == 1) {
    std::string singleton = parts[i];
    if (singleton == "x" || singleton == "X") {
      *privateUse = true;
      return i + 1 < parts.size();
    }
    ++i;
    size_t count = 0;
    while (i < parts.size() && parts[i].size() >= 2) { ++i; ++count; }
    if (!count) return false;
  }
  return i == parts.size();
}
bool RejectError(State& s, JS::MutableHandleValue result) {
  JS::RootedValue reason(s.cx);
  if (!JS_GetPendingException(s.cx, &reason)) return false;
  JS_ClearPendingException(s.cx);
  return s.kit.RejectWith(reason, result);
}
bool RejectType(State& s, const std::string& text, JS::MutableHandleValue result) {
  WebKit::Fail(s.cx, text.c_str());
  return RejectError(s, result);
}
// available(options) and install(options).
bool RecognitionStatic(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const bool install = WebKit::Slot(args) == 1;
  const std::string where = install ? "SpeechRecognition.install" : "SpeechRecognition.available";
  // A promise-returning operation reports argument errors as a rejection.
  if (!args.length())
    return RejectType(s, where + ": At least 1 argument required, but only 0 passed", args.rval());
  JS::RootedValue options(cx, args[0]), value(cx);
  if (!options.isNullOrUndefined() && !options.isObject())
    return RejectType(s, where + ": Argument 1 can't be converted to a dictionary.", args.rval());
  JS::RootedObject dict(cx, options.isObject() ? &options.toObject() : nullptr);
  if (dict && !JS_GetProperty(cx, dict, "langs", &value)) return RejectError(s, args.rval());
  if (value.isUndefined())
    return RejectType(s, where + ": Missing required 'langs' member of SpeechRecognitionOptions.", args.rval());
  bool array = false;
  if (!value.isObject() || !JS::IsArrayObject(cx, value, &array) || !array)
    return RejectType(s, where + ": 'langs' member of SpeechRecognitionOptions can't be converted to a sequence.",
                      args.rval());
  JS::RootedObject list(cx, &value.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, list, &length)) return RejectError(s, args.rval());
  std::vector<std::string> langs;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue item(cx);
    if (!JS_GetElement(cx, list, i, &item)) return RejectError(s, args.rval());
    JS::RootedString text(cx, JS::ToString(cx, item));
    std::string utf8;
    if (!text || !Text(cx, text, &utf8)) return RejectError(s, args.rval());
    langs.push_back(utf8);
  }
  bool local = false;
  if (dict) {
    if (!JS_GetProperty(cx, dict, "processLocally", &value)) return RejectError(s, args.rval());
    local = JS::ToBoolean(value);
  }
  bool downloadable = local && !langs.empty();
  for (const auto& lang : langs) {
    std::string primary;
    bool privateUse = false;
    if (!WellFormed(lang, &primary, &privateUse)) {
      JS::RootedValue error(cx);
      if (!DOMError(s, "SyntaxError", (where + ": Invalid BCP47 language tag").c_str(), &error))
        return RejectError(s, args.rval());
      return s.kit.RejectWith(error, args.rval());
    }
    if (privateUse || std::find_if(std::begin(kLocalLanguages), std::end(kLocalLanguages),
                                   [&](const char* known) { return primary == known; }) ==
                          std::end(kLocalLanguages))
      downloadable = false;
  }
  if (install) {
    JS::RootedValue error(cx);
    if (!DOMError(s, "NotAllowedError", (where + ": install() requires transient user activation").c_str(), &error))
      return RejectError(s, args.rval());
    return s.kit.RejectWith(error, args.rval());
  }
  JS::RootedString answer(cx, JS_NewStringCopyZ(cx, downloadable ? "downloadable" : "unavailable"));
  if (!answer) return false;
  JS::RootedValue resolved(cx, JS::StringValue(answer));
  return s.kit.Resolve(resolved, args.rval());
}

// ---- SpeechRecognitionErrorEvent / SpeechRecognitionEvent --------------------
bool ReadDictionary(JSContext* cx, const JS::CallArgs& args, const char* iface, JS::MutableHandleObject dict) {
  const std::string where = std::string(iface) + " constructor";
  if (!args.isConstructing()) return Fail(cx, where + ": 'new' is required");
  if (!Arguments(cx, args, 2, where.c_str())) return false;
  if (!args[1].isNullOrUndefined() && !args[1].isObject())
    return Fail(cx, where + ": Argument 2 can't be converted to a dictionary.");
  dict.set(args[1].isObject() ? &args[1].toObject() : nullptr);
  return true;
}
bool ErrorEventConstruct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  JS::RootedObject dict(cx);
  if (!ReadDictionary(cx, args, "SpeechRecognitionErrorEvent", &dict)) return false;
  JS::RootedObject fallback(cx, s.errorEvent.prototype), proto(cx);
  if (!s.kit.ConstructProto(args, fallback, &proto)) return false;
  // The page's Event reads type, bubbles, cancelable and composed in order;
  // error and message follow (dictionary members, lexicographic).
  JS::RootedValue eventConstructor(cx);
  if (!JS_GetProperty(cx, s.kit.page, "Event", &eventConstructor)) return false;
  JS::RootedValueArray<2> eventArgs(cx);
  eventArgs[0].set(args[0]);
  eventArgs[1].set(dict ? JS::ObjectValue(*dict) : JS::UndefinedValue());
  JS::RootedObject event(cx);
  if (!JS::Construct(cx, eventConstructor, eventArgs, &event)) return false;
  JS::RootedValue error(cx), message(cx);
  if (dict && !JS_GetProperty(cx, dict, "error", &error)) return false;
  if (error.isUndefined())
    return WebKit::Fail(cx, "SpeechRecognitionErrorEvent constructor: Missing required 'error' member of "
                            "SpeechRecognitionErrorEventInit.");
  JS::RootedString code(cx, JS::ToString(cx, error));
  std::string text;
  if (!code || !Text(cx, code, &text)) return false;
  if (std::find_if(std::begin(kErrorCodes), std::end(kErrorCodes),
                   [&](const char* known) { return text == known; }) == std::end(kErrorCodes))
    return Fail(cx, "SpeechRecognitionErrorEvent constructor: '" + text +
                        "' (value of 'error' member of SpeechRecognitionErrorEventInit) is not a valid value "
                        "for enumeration SpeechRecognitionErrorCode.");
  if (dict && !JS_GetProperty(cx, dict, "message", &message)) return false;
  JS::RootedString note(cx, message.isUndefined() ? JS_GetEmptyString(cx) : JS::ToString(cx, message));
  if (!note || !JS_SetPrototype(cx, event, proto)) return false;
  JS::RootedObject fields(cx, JS::NewArrayObject(cx, 2));
  JS::RootedObject map(cx, s.errorFields);
  JS::RootedValue key(cx, JS::ObjectValue(*event)), entry(cx);
  if (!fields || !JS_DefineElement(cx, fields, 0, code, JSPROP_ENUMERATE) ||
      !JS_DefineElement(cx, fields, 1, note, JSPROP_ENUMERATE)) return false;
  entry.setObject(*fields);
  {
    JSAutoRealm realm(cx, map);
    if (!JS_WrapValue(cx, &key) || !JS_WrapValue(cx, &entry) || !JS::SetWeakMapEntry(cx, map, key, entry))
      return false;
  }
  args.rval().setObject(*event);
  return true;
}
bool ErrorEventGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  JS::RootedObject map(cx, s.errorFields);
  JS::RootedValue entry(cx);
  JSObject* object = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (object) {
    JS::RootedValue key(cx, JS::ObjectValue(*object));
    JSAutoRealm realm(cx, map);
    if (!JS_WrapValue(cx, &key) || !JS::GetWeakMapEntry(cx, map, key, &entry)) return false;
  }
  if (!entry.isObject()) return WebKit::FailBrand(cx, args, "SpeechRecognitionErrorEvent");
  JS::RootedObject fields(cx, &entry.toObject());
  if (!JS_GetElement(cx, fields, uint32_t(WebKit::Slot(args)), args.rval())) return false;
  return JS_WrapValue(cx, args.rval());
}
// No SpeechRecognitionResultList can be created by a page, so the required
// `results` member always fails, after the members before it were read.
bool RecognitionEventConstruct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject dict(cx);
  if (!ReadDictionary(cx, args, "SpeechRecognitionEvent", &dict)) return false;
  JS::RootedString type(cx, JS::ToString(cx, args[0]));
  if (!type) return false;
  JS::RootedValue value(cx);
  for (const char* name : {"bubbles", "cancelable", "composed", "emma", "interpretation", "resultIndex", "results"}) {
    value.setUndefined();
    if (dict && !JS_GetProperty(cx, dict, name, &value)) return false;
    if (std::string(name) == "resultIndex" && !value.isUndefined()) {
      uint32_t ignored = 0;
      if (!JS::ToUint32(cx, value, &ignored)) return false;
    }
  }
  const char* prefix = "SpeechRecognitionEvent constructor: ";
  if (value.isUndefined())
    return Fail(cx, std::string(prefix) + "Missing required 'results' member of SpeechRecognitionEventInit.");
  if (!value.isObject())
    return Fail(cx, std::string(prefix) + "'results' member of SpeechRecognitionEventInit is not an object.");
  return Fail(cx, std::string(prefix) + "'results' member of SpeechRecognitionEventInit does not implement "
                                         "interface SpeechRecognitionResultList.");
}
bool ConstructorLast(JSContext* cx, WebFace& face) {
  JS::RootedValue constructor(cx, JS::ObjectValue(*face.constructor));
  return JS_DeleteProperty(cx, face.prototype, "constructor") &&
         JS_DefineProperty(cx, face.prototype, "constructor", constructor, 0);
}
// A webkit-prefixed alias: its own interface object (named webkitX) over the
// same interface prototype object (Firefox 160).
bool Alias(State& s, const char* name, JSNative native, WebFace& face) {
  JSContext* cx = s.cx;
  JS::RootedObject alias(cx, s.kit.Function(native, name, 0, 0, JSFUN_CONSTRUCTOR));
  JS::RootedObject prototype(cx, face.prototype);
  bool resolved = false;
  return alias && JS_HasOwnProperty(cx, alias, "length", &resolved) &&
         JS_HasOwnProperty(cx, alias, "name", &resolved) &&
         JS_DefineProperty(cx, alias, "prototype", prototype, JSPROP_READONLY | JSPROP_PERMANENT) &&
         JS_DefineProperty(cx, s.kit.page, name, alias, 0) && s.kit.helpers->Constructor(name, alias);
}
bool IteratorFromArray(State& s, WebFace& face) {
  JSContext* cx = s.cx;
  JS::RootedValue arrayCtor(cx), arrayProto(cx), values(cx);
  if (!JS_GetProperty(cx, s.kit.page, "Array", &arrayCtor) || !arrayCtor.isObject()) return false;
  JS::RootedObject ctor(cx, &arrayCtor.toObject());
  if (!JS_GetProperty(cx, ctor, "prototype", &arrayProto) || !arrayProto.isObject()) return false;
  JS::RootedObject proto(cx, &arrayProto.toObject()), target(cx, face.prototype);
  JS::RootedId iterator(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  return JS_GetPropertyById(cx, proto, iterator, &values) &&
         JS_DefinePropertyById(cx, target, iterator, values, 0);
}
}  // namespace

bool InstallRecognition(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject eventTarget(cx, s.events.prototype());
  JS::RootedValue eventValue(cx);
  if (!JS_GetProperty(cx, s.kit.page, "Event", &eventValue) || !eventValue.isObject()) return false;
  JS::RootedObject eventConstructor(cx, &eventValue.toObject()), eventPrototype(cx);
  JS::RootedValue prototypeValue(cx);
  if (!JS_GetProperty(cx, eventConstructor, "prototype", &prototypeValue) || !prototypeValue.isObject())
    return false;
  eventPrototype = &prototypeValue.toObject();
  s.errorFields = JS::NewWeakMapObject(cx);
  if (!s.errorFields) return false;

  const char* G = "SpeechGrammar";
  if (!s.kit.Define(G, &kGrammarClass, nullptr, GrammarConstruct, &s.grammar) ||
      !s.kit.Accessor(s.grammar, G, "src", GrammarGet, GrammarSet, 0) ||
      !s.kit.Accessor(s.grammar, G, "weight", GrammarGet, GrammarSet, 1) || !ConstructorLast(cx, s.grammar) ||
      !Alias(s, "webkitSpeechGrammar", GrammarConstruct, s.grammar)) return false;
  const char* L = "SpeechGrammarList";
  if (!s.kit.Define(L, &kGrammarListClass, nullptr, GrammarListConstruct, &s.grammarList) ||
      !s.kit.Method(s.grammarList, L, "item", GrammarListMethod, 1, 0) ||
      !s.kit.Method(s.grammarList, L, "addFromUri", GrammarListMethod, 1, 1) ||
      !s.kit.Method(s.grammarList, L, "addFromString", GrammarListMethod, 1, 2) ||
      !IteratorFromArray(s, s.grammarList) ||
      !s.kit.Accessor(s.grammarList, L, "length", GrammarListLength, nullptr, 0) ||
      !ConstructorLast(cx, s.grammarList) ||
      !Alias(s, "webkitSpeechGrammarList", GrammarListConstruct, s.grammarList)) return false;
  const char* R = "SpeechRecognition";
  if (!s.kit.Define(R, &kRecognitionClass, eventTarget, RecognitionConstruct, &s.recognition)) return false;
  const char* methods[] = {"start", "stop", "abort"};
  for (int i = 0; i < 3; ++i)
    if (!s.kit.Method(s.recognition, R, methods[i], RecognitionMethod, 0, i)) return false;
  for (int i = 0; i < RecStarted; ++i)
    if (!s.kit.Accessor(s.recognition, R, kRecAttributes[i], RecognitionGet, RecognitionSet, i)) return false;
  for (int i = 0; i < 11; ++i) {
    const std::string name = std::string("on") + kRecHandlers[i];
    if (!s.kit.Accessor(s.recognition, R, name.c_str(), RecognitionGet, RecognitionSet, RecHandlerFirst + i))
      return false;
  }
  JS::RootedObject recognitionConstructor(cx, s.recognition.constructor);
  for (int i = 0; i < 2; ++i) {
    const char* name = i ? "install" : "available";
    JS::RootedObject function(cx, s.kit.Function(RecognitionStatic, name, 1, i));
    if (!function || !JS_DefineProperty(cx, recognitionConstructor, name, function, JSPROP_ENUMERATE) ||
        !s.kit.helpers->Member(function, R, name, "call")) return false;
  }
  if (!ConstructorLast(cx, s.recognition) ||
      !Alias(s, "webkitSpeechRecognition", RecognitionConstruct, s.recognition)) return false;
  const char* A = "SpeechRecognitionAlternative";
  if (!s.kit.Define(A, &kAlternativeClass, nullptr, IllegalConstruct, &s.alternative) ||
      !s.kit.Accessor(s.alternative, A, "transcript", NeverGet, nullptr, 0) ||
      !s.kit.Accessor(s.alternative, A, "confidence", NeverGet, nullptr, 1) || !ConstructorLast(cx, s.alternative))
    return false;
  const char* E = "SpeechRecognitionErrorEvent";
  if (!s.kit.Define(E, &kRecognitionErrorEventClass, eventPrototype, ErrorEventConstruct, &s.errorEvent, 2) ||
      !s.kit.Accessor(s.errorEvent, E, "error", ErrorEventGet, nullptr, 0) ||
      !s.kit.Accessor(s.errorEvent, E, "message", ErrorEventGet, nullptr, 1) || !ConstructorLast(cx, s.errorEvent))
    return false;
  const char* V = "SpeechRecognitionEvent";
  if (!s.kit.Define(V, &kRecognitionEventClass, eventPrototype, RecognitionEventConstruct, &s.recognitionEvent, 2))
    return false;
  const char* eventFields[] = {"resultIndex", "results", "interpretation", "emma"};
  for (int i = 0; i < 4; ++i)
    if (!s.kit.Accessor(s.recognitionEvent, V, eventFields[i], NeverGet, nullptr, 24 + i)) return false;
  if (!ConstructorLast(cx, s.recognitionEvent)) return false;
  const char* P = "SpeechRecognitionPhrase";
  if (!s.kit.Define(P, &kPhraseClass, nullptr, PhraseConstruct, &s.phrase, 1) ||
      !s.kit.Accessor(s.phrase, P, "phrase", PhraseGet, nullptr, PhraseText) ||
      !s.kit.Accessor(s.phrase, P, "boost", PhraseGet, nullptr, PhraseBoost) || !ConstructorLast(cx, s.phrase))
    return false;
  const char* T = "SpeechRecognitionResult";
  if (!s.kit.Define(T, &kResultClass, nullptr, IllegalConstruct, &s.result) ||
      !s.kit.Method(s.result, T, "item", NeverGet, 1, 8) || !IteratorFromArray(s, s.result) ||
      !s.kit.Accessor(s.result, T, "length", NeverGet, nullptr, 9) ||
      !s.kit.Accessor(s.result, T, "isFinal", NeverGet, nullptr, 10) || !ConstructorLast(cx, s.result))
    return false;
  const char* T2 = "SpeechRecognitionResultList";
  if (!s.kit.Define(T2, &kResultListClass, nullptr, IllegalConstruct, &s.resultList) ||
      !s.kit.Method(s.resultList, T2, "item", NeverGet, 1, 16) || !IteratorFromArray(s, s.resultList) ||
      !s.kit.Accessor(s.resultList, T2, "length", NeverGet, nullptr, 17) || !ConstructorLast(cx, s.resultList))
    return false;
  // The event interface objects inherit from Event (LinkInterfaceObjects also
  // derives it from the prototype chain; set here for a realm without that pass).
  JS::RootedObject errorCtor(cx, s.errorEvent.constructor), eventCtor(cx, s.recognitionEvent.constructor);
  return JS_SetPrototype(cx, errorCtor, eventConstructor) && JS_SetPrototype(cx, eventCtor, eventConstructor);
}
}  // namespace ruyijs::web_speech
