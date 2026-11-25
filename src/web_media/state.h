#ifndef RUYIJS_WEB_MEDIA_STATE_H
#define RUYIJS_WEB_MEDIA_STATE_H
#include "runtime/secure_slot.h"
#include "runtime/web_kit.h"
#include "interaction/realm.h"
#include "browser_environment/environment.h"
#include "web_media/media_codecs.h"
#include "domain/web_media/configuration.h"
#include <map>
#include <string>
#include <vector>
namespace ruyijs::web_media {
// One MediaDeviceInfo the profile's "media" group lists
// (kind|label|deviceId|groupId). Firefox's un-granted answer is a single audio
// input whose label, id and group are all empty.
struct MediaDevice { std::string kind, label, device_id, group_id; };
struct State;
// The "media" group's decoding answer for one page contentType: true when the
// profile pinned it (media_group.cpp), false to use the domain table.
bool ConfiguredDecoding(State& state, const std::string& content_type,
                        domain::web_media::Capability* out);
void RegisterState(State& state);
void UnregisterState(State& state);
State* StateForGlobal(JSObject* global);
struct State {
  State* previous = nullptr;
  JSContext* cx;
  WebKit kit;
  WebFace face, deviceInfo, stream, track, mediaSource, sourceBuffer, sourceBufferList,
      mediaRecorder, capabilities, metadata, session, textTrack, textCue,
      cueList, trackList, vttCue, keySession, keyStatusMap, keySystemAccess,
      keys, keyError, videoColorSpace, imageDecoder, imageTrackList, imageTrack, videoFrame, vttRegion, audioData, canvasCapture,
      audioDecoder, audioEncoder, videoDecoder, videoEncoder, encodedAudioChunk, encodedVideoChunk;
  interaction::Realm& events;
  browser_environment::Environment& environment;
  // The "media" group's effective values. The device list starts at Firefox's
  // one un-granted audio input; decoding answers keyed by the page's own
  // contentType string override the domain table for that content type only.
  std::vector<MediaDevice> devices{{"audioinput", "", "", ""}};
  std::map<std::string, domain::web_media::Capability> decoding;
  JS::PersistentRootedObject navigator, device, capabilitiesObject, mediaSession, imageRectPrototype,
      mediaStreamErrorPrototype, timeRangesPrototype, playbackQualityPrototype;
  // [SecureContext] members (Navigator.mozGetUserMedia/mediaDevices,
  // MediaDevices, MediaDeviceInfo), applied by Realm::ConfigureExposure.
  SecureSlots secure;
  JS::PersistentRootedValue addListener, removeListener, streamPreventDefault, exceptionConstructor, arrayValues;
  // getAutoplayPolicy overload brands, bound by the runtime coordinator.
  bool (*mediaElementBrand)(JS::HandleValue) = nullptr;
  bool (*audioContextBrand)(JS::HandleValue) = nullptr;
  // Canvas owner's element size (Realm::SetCanvasSizePort).
  bool (*canvasSize)(void*, JS::HandleObject, uint32_t*, uint32_t*) = nullptr;
  void* canvasSizeContext = nullptr;
  State(JSContext* c,JS::HandleObject p,PageHelpers& h,interaction::Realm& e,
        browser_environment::Environment& b)
    :cx(c),kit(c,p,h,this),face(c),deviceInfo(c),stream(c),track(c),mediaSource(c),
     sourceBuffer(c),sourceBufferList(c),mediaRecorder(c),capabilities(c),
     metadata(c),session(c),textTrack(c),textCue(c),cueList(c),trackList(c),vttCue(c),
     keySession(c),keyStatusMap(c),keySystemAccess(c),keys(c),keyError(c),videoColorSpace(c),
     imageDecoder(c),imageTrackList(c),imageTrack(c),videoFrame(c),vttRegion(c),audioData(c),canvasCapture(c),
     audioDecoder(c),audioEncoder(c),videoDecoder(c),videoEncoder(c),encodedAudioChunk(c),encodedVideoChunk(c),
     events(e),environment(b),navigator(c),device(c),capabilitiesObject(c),mediaSession(c),imageRectPrototype(c),mediaStreamErrorPrototype(c),timeRangesPrototype(c),playbackQualityPrototype(c),secure(c),
     addListener(c),removeListener(c),streamPreventDefault(c),exceptionConstructor(c),arrayValues(c){ RegisterState(*this); }
  ~State(){ UnregisterState(*this); }
};
extern const JSClass kMediaClass;
bool NavigatorMedia(JSContext*,unsigned,JS::Value*);
bool MediaMethod(JSContext*,unsigned,JS::Value*);
bool MediaHandler(JSContext*,unsigned,JS::Value*);
bool InstallNavigatorMedia(State&);
}
#endif
