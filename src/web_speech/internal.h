#ifndef RUYIJS_WEB_SPEECH_INTERNAL_H
#define RUYIJS_WEB_SPEECH_INTERNAL_H
#include "web_speech/realm.h"
#include "runtime/web_kit.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ruyijs::web_speech {
// The Context's one voice list (UTF-8), from the profile's "speech" group
// (or its baseline). Firefox enumerates the platform voices asynchronously:
// getVoices() is empty until the enumeration completes in a task and fires
// `voiceschanged`, so the list is published once per Context.
struct Voice { std::string name, lang; };
struct VoiceList {
  std::vector<Voice> voices;
  std::string defaultName;
  bool published = false;  // Enumeration finished; getVoices() reports it.
  bool publishing = false;  // One publication task is already queued.
  // The realm whose queue holds that task; a realm that loses its queue
  // (navigation retires it) gives the publication back.
  const void* publisher = nullptr;
};
// speechSynthesis: owner State, the queue (array of utterances), the current
// utterance, the onvoiceschanged handler, and the run generation that
// invalidates the steps of a cancelled run.
enum SynthSlot { SynthState = 0, SynthQueue, SynthCurrent, SynthHandler, SynthSlotCount };
// Utterance attributes, then the seven event-handler slots in member order.
enum UttSlot { UttText = 0, UttLang, UttVoice, UttVolume, UttRate, UttPitch,
               UttHandlerFirst, UttSlotCount = UttHandlerFirst + 7 };
enum VoiceSlot { VoiceUri = 0, VoiceName, VoiceLang, VoiceLocal, VoiceDefault, VoiceSlotCount };
inline constexpr const char* kHandlers[7] = {"start", "end", "error", "pause", "resume",
                                             "mark", "boundary"};
extern const JSClass kSynthesisClass, kUtteranceClass, kVoiceClass;
inline constexpr int32_t kNoStep = INT32_MIN;
struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events)
      : cx(cx), kit(cx, page, helpers, this), events(events), synthesis(cx), utterance(cx),
        voice(cx), grammar(cx), grammarList(cx), recognition(cx), alternative(cx), errorEvent(cx),
        recognitionEvent(cx), phrase(cx), result(cx), resultList(cx), errorFields(cx), instance(cx),
        dateNow(cx), date(cx) {}
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  WebFace synthesis, utterance, voice;
  // Speech recognition (recognition.cpp, Firefox 160).
  WebFace grammar, grammarList, recognition, alternative, errorEvent, recognitionEvent, phrase, result,
      resultList;
  // SpeechRecognitionErrorEvent fields, keyed by the event object.
  JS::PersistentRootedObject errorFields;
  JS::PersistentRootedObject instance;
  JS::PersistentRootedObject dateNow, date;
  std::shared_ptr<VoiceList> voices;
  bool ownsVoices = false;  // This realm is the Context's voice-list origin.
  bool speaking = false, paused = false;
  uint32_t generation = 0;  // Bumped by cancel(): older steps are dropped.
  int32_t heldStep = kNoStep;  // The step pause() held, resumed by resume().
  double started = 0;       // Date.now() of the current start event.
  Realm::TaskPort taskPort = nullptr;
  void* taskContext = nullptr;
};
bool Receiver(JSContext* cx, JS::HandleValue self, const JSClass* clasp, const std::string& member,
              JS::MutableHandleObject out);
// Shared EventHandler attribute setter: swaps the listener on EventTarget.prototype.
bool SetHandler(State& state, JS::HandleObject target, const char* type, uint32_t slot,
                JS::HandleValue value);
bool SynthesisMethod(JSContext* cx, unsigned argc, JS::Value* vp);
bool SynthesisGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool SynthesisHandlerSet(JSContext* cx, unsigned argc, JS::Value* vp);
bool UtteranceConstruct(JSContext* cx, unsigned argc, JS::Value* vp);
bool UtteranceGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool UtteranceSet(JSContext* cx, unsigned argc, JS::Value* vp);
bool VoiceGet(JSContext* cx, unsigned argc, JS::Value* vp);
// getVoices(): a new array of new SpeechSynthesisVoice objects (Firefox).
bool Voices(State& state, JS::MutableHandleValue out);
// Queues the one task that publishes the enumerated voices and fires
// `voiceschanged` on this realm's speechSynthesis (Firefox's SAPI
// enumeration is asynchronous, so getVoices() is empty before it lands).
bool PostVoices(State& state);
State& Owner(JS::HandleObject synthesis);
// The Window-only speech recognition interfaces Firefox 160 exposes.
bool InstallRecognition(State& state);
JSObject* Queue(State& state);
}
#endif
