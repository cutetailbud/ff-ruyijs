// The one production installer of the Speech Synthesis owner, its Browser
// Environment "speech" group port and the shared EventHandler attribute setter.
#include "web_speech/internal.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/CharacterEncoding.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_speech {
namespace {
bool Window(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  args.rval().setObject(*state.instance);
  return JS_WrapValue(cx, args.rval());
}
bool Last(JSContext* cx, WebFace& face) {
  JS::RootedValue constructor(cx, JS::ObjectValue(*face.constructor));
  return JS_DeleteProperty(cx, face.prototype, "constructor") &&
         JS_DefineProperty(cx, face.prototype, "constructor", constructor, 0);
}
bool Strings(JSContext* cx, JS::HandleObject group, const char* name, std::vector<std::string>* out,
             bool* found) {
  JS::RootedValue value(cx);
  if (!JS_HasProperty(cx, group, name, found) || !*found) return true;
  if (!JS_GetProperty(cx, group, name, &value) || !value.isObject()) return false;
  JS::RootedObject list(cx, &value.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, list, &length)) return false;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue item(cx);
    if (!JS_GetElement(cx, list, index, &item) || !item.isString()) return false;
    JS::RootedString text(cx, item.toString());
    JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
    if (!utf8) return false;
    out->emplace_back(utf8.get());
  }
  return true;
}
}
bool Receiver(JSContext* cx, JS::HandleValue self, const JSClass* clasp, const std::string& member,
              JS::MutableHandleObject out) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != clasp)
    return WebKit::Fail(cx, ("'" + member + "' called on an object that does not implement "
                             "interface " + clasp->name + ".").c_str());
  out.set(object);
  return true;
}
bool SetHandler(State& state, JS::HandleObject target, const char* type, uint32_t slot,
                JS::HandleValue value) {
  JSContext* cx = state.cx;
  JS::RootedValue previous(cx, JS::GetReservedSlot(target, slot)), desired(cx, value);
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JSAutoRealm realm(cx, state.kit.page);
  if (!JS_WrapValue(cx, &desired) || !JS_WrapValue(cx, &previous)) return false;
  JS::RootedObject prototype(cx, state.events.prototype());
  JS::RootedValue function(cx), receiver(cx, JS::ObjectValue(*target)), ignored(cx);
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  if (!name) return false;
  JS::RootedValueArray<2> parameters(cx);
  parameters[0].setString(name);
  for (int operation = 0; operation < 2; ++operation) {
    parameters[1].set(operation ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(cx, prototype, operation ? "addEventListener" : "removeEventListener",
                        &function) ||
        !JS::Call(cx, receiver, function, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(target, slot, desired);
  return true;
}
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, Realm* origin)
    : state_(std::make_unique<State>(cx, page, helpers, events)) {
  state_->ownsVoices = origin == nullptr;
  state_->voices = origin ? origin->state_->voices : std::make_shared<VoiceList>();
}
Realm::~Realm() { Release(); }
// A queued publication dies with this realm's queue: another realm may start it.
void Realm::Release() {
  if (!state_ || !state_->voices || state_->voices->publisher != state_.get()) return;
  state_->voices->publishing = false;
  state_->voices->publisher = nullptr;
}
void Realm::IsolateVoices() {
  State& s = *state_;
  auto own = std::make_shared<VoiceList>();
  own->voices = s.voices->voices;
  own->defaultName = s.voices->defaultName;
  s.voices = std::move(own);
  s.ownsVoices = true;
}
void Realm::SetTaskPort(TaskPort port, void* context) {
  if (!port) Release();
  state_->taskPort = port;
  state_->taskContext = context;
  // The voice list arrives from the platform asynchronously, started by the
  // first getVoices() (Voices): no publication is queued here.
}
bool Realm::Install() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject eventTarget(cx, s.events.prototype());
  JS::RootedValue date(cx), now(cx);
  if (!eventTarget || !JS_GetProperty(cx, s.kit.page, "Date", &date) || !date.isObject()) return false;
  s.date = &date.toObject();
  if (!JS_GetProperty(cx, s.date, "now", &now) || !now.isObject()) return false;
  s.dateNow = &now.toObject();
  const char* S = "SpeechSynthesis";
  const char* U = "SpeechSynthesisUtterance";
  const char* V = "SpeechSynthesisVoice";
  if (!InstallRecognition(s) || !s.kit.Define(S, &kSynthesisClass, eventTarget, WebKit::Reject, &s.synthesis)) return false;
  const char* methods[] = {"speak", "cancel", "pause", "resume", "getVoices"};
  for (int i = 0; i < 5; ++i)
    if (!s.kit.Method(s.synthesis, S, methods[i], SynthesisMethod, i == 0 ? 1 : 0, i)) return false;
  if (!s.kit.Accessor(s.synthesis, S, "pending", SynthesisGet, nullptr, 0) ||
      !s.kit.Accessor(s.synthesis, S, "speaking", SynthesisGet, nullptr, 1) ||
      !s.kit.Accessor(s.synthesis, S, "paused", SynthesisGet, nullptr, 2) ||
      !s.kit.Accessor(s.synthesis, S, "onvoiceschanged", SynthesisGet, SynthesisHandlerSet, 3) ||
      !Last(cx, s.synthesis) ||
      !s.kit.Define(U, &kUtteranceClass, eventTarget, UtteranceConstruct, &s.utterance)) return false;
  const char* attributes[] = {"text", "lang", "voice", "volume", "rate", "pitch"};
  for (int i = 0; i < UttHandlerFirst; ++i)
    if (!s.kit.Accessor(s.utterance, U, attributes[i], UtteranceGet, UtteranceSet, i)) return false;
  for (int i = 0; i < 7; ++i) {
    const std::string name = std::string("on") + kHandlers[i];
    if (!s.kit.Accessor(s.utterance, U, name.c_str(), UtteranceGet, UtteranceSet, UttHandlerFirst + i))
      return false;
  }
  if (!Last(cx, s.utterance) ||
      !s.kit.Define(V, &kVoiceClass, nullptr, WebKit::Reject, &s.voice)) return false;
  const char* voiceFields[] = {"voiceURI", "name", "lang", "localService", "default"};
  for (int i = 0; i < VoiceSlotCount; ++i)
    if (!s.kit.Accessor(s.voice, V, voiceFields[i], VoiceGet, nullptr, i)) return false;
  if (!Last(cx, s.voice)) return false;
  s.instance = s.kit.New(&kSynthesisClass, s.synthesis.prototype);
  JS::RootedObject queue(cx, JS::NewArrayObject(cx, 0));
  if (!s.instance || !queue || !s.events.Attach(s.instance)) return false;
  JS::SetReservedSlot(s.instance, SynthState, JS::PrivateValue(&s));
  JS::SetReservedSlot(s.instance, SynthQueue, JS::ObjectValue(*queue));
  JS::SetReservedSlot(s.instance, SynthCurrent, JS::NullValue());
  JS::SetReservedSlot(s.instance, SynthHandler, JS::NullValue());
  JS::RootedObject getter(cx, s.kit.Function(Window, "get speechSynthesis", 0, 0)), setter(cx);
  return getter &&
         JS_DefineProperty(cx, s.kit.page, "speechSynthesis", getter, setter, JSPROP_ENUMERATE) &&
         s.kit.helpers->Accessor("Window", s.kit.page, "speechSynthesis") &&
         s.kit.helpers->Object(S, s.instance);
}
bool Realm::Configure(JS::HandleObject group) {
  State& s = *state_;
  JSContext* cx = s.cx;
  std::vector<std::string> names, langs;
  bool hasNames = false, hasLangs = false, hasDefault = false;
  if (!Strings(cx, group, "voices", &names, &hasNames) || !Strings(cx, group, "langs", &langs, &hasLangs) ||
      !JS_HasProperty(cx, group, "default", &hasDefault)) return false;
  if (hasNames) {
    s.voices->voices.clear();
    for (size_t i = 0; i < names.size(); ++i)
      s.voices->voices.push_back({names[i], i < langs.size() ? langs[i] : std::string()});
  } else if (hasLangs) {
    for (size_t i = 0; i < s.voices->voices.size() && i < langs.size(); ++i)
      s.voices->voices[i].lang = langs[i];
  }
  // A new profile group is a new enumeration: publish it in a task again.
  if (hasNames) s.voices->published = false;
  if (hasDefault) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, group, "default", &value) || !value.isString()) return false;
    JS::RootedString text(cx, value.toString());
    JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
    if (!utf8) return false;
    s.voices->defaultName = utf8.get();
  }
  return true;  // the next getVoices() starts the new enumeration
}
}  // namespace ruyijs::web_speech
