#ifndef RUYIJS_WEB_CANVAS_REALM_H
#define RUYIJS_WEB_CANVAS_REALM_H
#include <cstdint>
#include <memory>
#include "js/RootingAPI.h"
#include "domain/canvas/fingerprint_config.h"
struct JSContext;
namespace ruyijs { class PageHelpers; namespace interaction { class Realm; } }
namespace ruyijs::web_canvas {
struct State;
// Canvas owns the canvas element's canvas members, CanvasRenderingContext2D,
// ImageData and OffscreenCanvas. Geometry owns the DOMMatrix snapshot factory.
// The HTMLCanvasElement constructor
// itself stays with the Document Model: it hands this realm the prototype to
// extend, so there is exactly one owner of each constructor and prototype.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        JS::HandleObject canvasPrototype, interaction::Realm& events);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  bool InstallWorker(JS::HandleObject scopePrototype);  // Worker realms (no canvas element)
  // The port HTMLCanvasElement.getContext uses for every kind Canvas does not
  // implement; the WebGL owner answers "webgl"/"experimental-webgl" through it.
  void SetContextPort(bool (*port)(void*, JS::HandleObject, JS::HandleString,
                                   JS::HandleValue, JS::MutableHandleValue), void* context);
  void SetElementCheck(bool (*port)(void*, JS::HandleObject, bool*), void* context);
  // HTMLCanvasElement.captureStream() asks the Media owner for its
  // CanvasCaptureMediaStream (web_media::Realm::CaptureCanvas).
  void SetCaptureStreamPort(bool (*port)(void*, JS::HandleObject, JS::MutableHandleObject), void* context);
  // The content language a canvas context inherits (ctx.lang "inherit"):
  // the canvas element's inherited lang, or the root element's (null element).
  void SetLangPort(bool (*port)(void*, JS::HandleObject, std::string*), void* context);
  // The canvas element's current width and height (its reflected content
  // attributes); the Media owner's canvas-capture track reads its settings here.
  bool ElementSize(JS::HandleObject canvas, uint32_t* width, uint32_t* height);
  void SetMatrixFactory(bool (*port)(void*, const double*, JS::MutableHandleObject), void* context);
  // The Document Model's notice that a canvas element's width or height
  // content attribute was set or removed, by any DOM path (Firefox
  // HTMLCanvasElement::AfterSetAttr). `width`/`height` are the current
  // attribute values (string, or null when absent) the Document Model read;
  // no page method is called. An existing bitmap is re-allocated at the new
  // size, even an unchanged one, and a 2D context returns to its default state.
  bool DimensionsChanged(JS::HandleObject canvas, JS::HandleValue width, JS::HandleValue height);
 private:
  std::unique_ptr<State> state_;
};
// The Context's canvas fingerprint configuration (the donor's --fpfile canvas.*
// keys), applied by the "canvas" environment group. Process-scoped: one Context
// is one runtime process, so every realm of the Context reads it.
void ConfigureFingerprint(const domain::canvas::FingerprintConfig& config);
// The environment's "canvas" group port; `context` is the JSContext.
// CanvasFingerprint.cpp PerturbRGBA at the donor's extraction points (the
// encoder input of toDataURL/toBlob/convertToBlob, getImageData, and WebGL
// readPixels through the runtime's port): un-premultiplied RGBA bytes, logical
// origin and row direction as the donor passes them. A no-op unless the
// configured "canvas" group asks for strength > 0.
void PerturbExportedPixels(uint8_t* rgba, uint32_t width, uint32_t height, int32_t stride,
                           int32_t originX, int32_t originY, int32_t yDirection);
bool ConfigureFingerprintGroup(void* context, JS::HandleObject group);
}  // namespace ruyijs::web_canvas
#endif
