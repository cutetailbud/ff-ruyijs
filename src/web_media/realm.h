#ifndef RUYIJS_WEB_MEDIA_REALM_H
#define RUYIJS_WEB_MEDIA_REALM_H
#include <cstdint>
#include <memory>
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_media {
struct State;
// Read-only brand port for the existing MediaSource object factory.
bool IsObjectUrlSource(JSObject* object);
// Read-only brand port for the MediaStreamTrack factory (event init dictionaries).
bool IsMediaStreamTrack(JSObject* object);
// HTMLMediaElement/HTMLVideoElement value ports (document_model::MediaValuePorts):
// a new empty TimeRanges, and a VideoPlaybackQuality over the given values,
// both in the current realm.
bool NewEmptyTimeRanges(JSContext*, JS::MutableHandleObject out);
bool NewVideoPlaybackQuality(JSContext*, double creationTime, uint32_t totalFrames,
                             uint32_t droppedFrames, JS::MutableHandleObject out);
// A new MediaStream with no tracks (HTMLMediaElement.captureStream of an element
// that plays nothing), and the MediaStream brand (HTMLMediaElement.srcObject).
bool NewEmptyMediaStream(JSContext*, JS::MutableHandleObject out);
bool IsMediaStream(JSObject* object);
// HTMLMediaElement text-track ports (document_model::TextTrackPorts).
bool NewElementTextTrackList(JSContext*, JS::MutableHandleObject list);
bool AppendElementTextTrack(JSContext*, JS::HandleObject list, const char* kind,
                            JS::HandleString label, JS::HandleString language,
                            JS::MutableHandleObject track);
class Realm {
 public:
  Realm(JSContext*,JS::HandleObject,PageHelpers&,interaction::Realm&,
        browser_environment::Environment&);
  ~Realm();
  bool Install();
  // A Worker realm (docs/owners/WORKER.md): MediaCapabilities with
  // WorkerNavigator.mediaCapabilities, VideoColorSpace and the ImageDecoder /
  // VideoFrame family only; MediaDevices, streams, MSE, EME, text tracks and
  // MediaSession are Window-only in Firefox.
  bool InstallWorker(bool codecs = true);  // codecs: dedicated Workers only
  void SetImageRectPrototype(JS::HandleObject);
  // HTMLMediaElement (Document Model) and AudioContext (Web Audio) brand
  // checks for Navigator.getAutoplayPolicy's overloads.
  using BrandPort = bool (*)(JS::HandleValue);
  void SetAutoplayBrandPorts(BrandPort mediaElement, BrandPort audioContext);
  // [SecureContext] Navigator.mozGetUserMedia (runtime/secure_exposure.cpp).
  bool ConfigureExposure();
  // Browser Environment "media" group port (already schema-validated): the
  // MediaDevices list and the per-content-type decoding answers.
  bool Configure(JS::HandleObject group);
  // The effective values of that group, for the configure snapshot.
  bool Describe(JS::MutableHandleValue result);
  // HTMLCanvasElement.captureStream(): a CanvasCaptureMediaStream over `canvas`
  // (canvas_capture.cpp), reached by the Canvas owner's capture port.
  bool CaptureCanvas(JS::HandleObject canvas, JS::MutableHandleObject stream);
  // The Canvas owner's size of a captured canvas (web_canvas::Realm::ElementSize),
  // read by the capture track's getSettings(); bound by the runtime coordinator.
  using CanvasSizePort = bool (*)(void*, JS::HandleObject, uint32_t*, uint32_t*);
  void SetCanvasSizePort(CanvasSizePort, void*);
  // The remote MediaStreamTrack an RTCRtpReceiver is created with ("remote
  // audio"/"remote video", muted), reached by the RTC owner's track port.
  bool NewRemoteTrack(bool video, JS::MutableHandleObject track);
 private:
  std::unique_ptr<State> state_;
};
}
#endif
