"""HTMLMediaElement / HTMLVideoElement members and the window compat shims, as Firefox has them (160 and 161 agree).

usage: python ci/media_element_members_check.py      (RUYIJS_SHELL picks js.exe)

DataDome's device check (issue #54) reads `HTMLVideoElement.prototype.getVideoPlaybackQuality`
and `"Controllers" in window`; both were missing, together with most of Firefox's media
element surface (videoWidth, buffered, duration, captureStream, the NETWORK_/HAVE_ constants,
...). Every expectation below is Firefox 160 and 161's own answer for a <video> with no source,
recorded with the same expressions; ruyijs plays no media, so an element stays at
HAVE_NOTHING exactly like that one.
"""
import json
import sys

from ruyijs import Context, JSError

MEDIA_ORDER = ("load,canPlayType,fastSeek,play,pause,addTextTrack,mozGetMetadata,setMediaKeys,setSinkId,"
               "captureStream,mozCaptureStream,mozCaptureStreamUntilEnded,error,src,currentSrc,crossOrigin,"
               "networkState,preload,buffered,readyState,seeking,currentTime,duration,paused,"
               "defaultPlaybackRate,playbackRate,played,seekable,ended,autoplay,loop,controls,volume,muted,"
               "defaultMuted,textTracks,srcObject,preservesPitch,mozFragmentEnd,mediaKeys,onencrypted,"
               "onwaitingforkey,sinkId,mozAudioCaptured,NETWORK_EMPTY,NETWORK_IDLE,NETWORK_LOADING,"
               "NETWORK_NO_SOURCE,HAVE_NOTHING,HAVE_METADATA,HAVE_CURRENT_DATA,HAVE_FUTURE_DATA,"
               "HAVE_ENOUGH_DATA,constructor")
VIDEO_ORDER = ("getVideoPlaybackQuality,requestPictureInPicture,requestVideoFrameCallback,"
               "cancelVideoFrameCallback,width,height,videoWidth,videoHeight,poster,mozParsedFrames,"
               "mozDecodedFrames,mozPresentedFrames,mozPaintedFrames,mozFrameDelay,mozHasAudio,"
               "onenterpictureinpicture,onleavepictureinpicture,disablePictureInPicture,constructor")

ROWS = [
    # name, expression (evaluated as an async function body), Firefox 161's answer
    ("media order", "return Object.getOwnPropertyNames(HTMLMediaElement.prototype).join(',')", MEDIA_ORDER),
    ("video order", "return Object.getOwnPropertyNames(HTMLVideoElement.prototype).join(',')", VIDEO_ORDER),
    ("quality order", "return Object.getOwnPropertyNames(VideoPlaybackQuality.prototype).join(',')",
     "creationTime,totalVideoFrames,droppedVideoFrames,constructor"),
    ("defaults", "const v = document.createElement('video'); return [v.videoWidth, v.videoHeight, v.width, "
     "v.height, v.poster, v.error, v.seeking, String(v.duration), String(v.mozFragmentEnd), v.sinkId, "
     "v.mozAudioCaptured, v.playbackRate, v.defaultPlaybackRate, v.preservesPitch, v.srcObject, v.crossOrigin, "
     "v.mozParsedFrames, v.mozFrameDelay, v.mozHasAudio, v.disablePictureInPicture, v.onenterpictureinpicture]",
     [0, 0, 0, 0, "", None, False, "NaN", "NaN", "", False, 1, 1, True, None, None, 0, 0, False, False, None]),
    ("time ranges", "const v = document.createElement('video'); return [v.buffered.length, "
     "Object.prototype.toString.call(v.buffered), v.buffered === v.buffered, v.played.length, v.seekable.length, "
     "(() => { try { v.buffered.start(0); return 'no throw'; } catch (e) { return e.name; } })()]",
     [0, "[object TimeRanges]", False, 0, 0, "IndexSizeError"]),
    ("playback quality", "const q = document.createElement('video').getVideoPlaybackQuality(); return "
     "[Object.prototype.toString.call(q), q.totalVideoFrames, q.droppedVideoFrames, typeof q.creationTime, "
     "q.creationTime > 0, String(q.corruptedVideoFrames)]",
     ["[object VideoPlaybackQuality]", 0, 0, "number", True, "undefined"]),
    ("frame callback", "const v = document.createElement('video'); return [typeof v.requestVideoFrameCallback(() => {}), "
     "v.requestVideoFrameCallback(() => {}), v.cancelVideoFrameCallback(1)]", ["number", 2, None]),
    ("picture in picture", "const v = document.createElement('video'); try { await v.requestPictureInPicture(); "
     "return 'resolved'; } catch (e) { return e.name + ': ' + e.message; }",
     "InvalidStateError: Video readyState is HAVE_NOTHING"),
    ("capture", "const v = document.createElement('video'); return [Object.prototype.toString.call(v.captureStream()), "
     "Object.prototype.toString.call(v.mozCaptureStream())]", ["[object MediaStream]", "[object MediaStream]"]),
    ("sink", "const v = document.createElement('video'); try { await v.setSinkId('x'); return 'resolved'; } "
     "catch (e) { return e.name + ': ' + e.message; }", "NotFoundError: The object can not be found here."),
    ("metadata", "try { document.createElement('video').mozGetMetadata(); return 'no throw'; } catch (e) { "
     "return e.name + ': ' + e.message; }",
     "InvalidStateError: An attempt was made to use an object that is not, or is no longer, usable"),
    ("reflection", "const v = document.createElement('video'); v.setAttribute('crossorigin', 'xyz'); const a = v.crossOrigin; "
     "v.crossOrigin = 'use-credentials'; const b = [v.crossOrigin, v.getAttribute('crossorigin')]; v.crossOrigin = null; "
     "v.setAttribute('width', '320'); v.setAttribute('height', 'x'); v.poster = 'p.png'; "
     "return [a, b, v.crossOrigin, v.hasAttribute('crossorigin'), v.width, v.height, v.poster]",
     ["anonymous", ["use-credentials", "use-credentials"], None, False, 320, 0, "http://127.0.0.1/p.png"]),
    ("rates", "const v = document.createElement('video'); v.playbackRate = 2; const r = [v.playbackRate, v.defaultPlaybackRate]; "
     "try { v.playbackRate = Infinity; } catch (e) { r.push(e.name + ': ' + e.message); } "
     "try { v.srcObject = {}; } catch (e) { r.push(e.name + ': ' + e.message); } return r",
     [2, 1, "TypeError: HTMLMediaElement.playbackRate setter: Value being assigned is not a finite floating-point value.",
      "TypeError: HTMLMediaElement.srcObject setter: Value being assigned does not implement interface MediaStream."]),
    ("constants", "const d = Object.getOwnPropertyDescriptor(HTMLMediaElement.prototype, 'HAVE_ENOUGH_DATA'); "
     "return [HTMLMediaElement.NETWORK_EMPTY, HTMLMediaElement.HAVE_ENOUGH_DATA, document.createElement('video').HAVE_METADATA, "
     "d.configurable, d.writable, d.enumerable]", [0, 4, 1, False, False, True]),
    ("audio has no video members", "const a = document.createElement('audio'); return ['videoWidth' in a, 'mozHasAudio' in a]",
     [False, False]),
    ("window shims", "return ['controllers' in window, typeof Controllers, String(window.controllers), "
     "Object.getOwnPropertyDescriptor(window, 'Controllers').enumerable, typeof Components, "
     "Components.interfaces.nsIDOMNode === Node, Object.keys(Components.interfaces).length]",
     [True, "object", "[object Object]", True, "object", True, 14]),
]


def main():
    failures = 0
    with Context(timeout=60) as context:
        context.load_page("http://127.0.0.1/m.html", "<!doctype html><title>media</title>")
        for name, body, want in ROWS:
            source = "(async () => { " + body + " })().then(v => JSON.stringify(v), e => 'THROW ' + e)"
            try:
                context.eval("globalThis.__row = undefined; " + source + ".then(v => globalThis.__row = v); undefined")
                context.advance(50)
                got = json.loads(context.eval("globalThis.__row"))
            except (JSError, TypeError, ValueError) as error:
                got = f"ERROR {error}"
            ok = got == want
            failures += not ok
            print(("ok  " if ok else "FAIL") + f"  {name}" + ("" if ok else f"\n      got  {got!r}\n      want {want!r}"))
    print("media element members:", "all match Firefox" if not failures else f"{failures} rows differ")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
