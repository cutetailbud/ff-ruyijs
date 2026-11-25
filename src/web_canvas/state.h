#ifndef RUYIJS_WEB_CANVAS_STATE_H
#define RUYIJS_WEB_CANVAS_STATE_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "js/RootingAPI.h"
#include "runtime/web_kit.h"

namespace ruyijs {
class PageHelpers;
namespace interaction { class Realm; }
namespace web_geometry { class SystemFonts; }
namespace web_canvas {

// The HTML "context mode" of one canvas: the first successful getContext() fixes
// it and a later request for a different kind returns null. An unsupported kind
// never sets it, so a canvas whose only request was "webgl" still answers "2d".
inline constexpr int32_t kModeNone = 0;
inline constexpr int32_t kMode2D = 1;

// The drawing state of one 2D context is a plain array. save() pushes a copy and
// restore() pops it, so the stack is the only state store and there is no second
// copy of any attribute or of the transform.
enum StateField : int32_t {
  kFillStyle = 0, kStrokeStyle, kLineWidth, kLineCap, kLineJoin, kMiterLimit,
  kLineDashOffset, kFont, kTextAlign, kTextBaseline, kDirection, kGlobalAlpha,
  kGlobalCompositeOperation, kImageSmoothingEnabled, kShadowBlur, kShadowColor,
  kShadowOffsetX, kShadowOffsetY, kFilter, kFontKerning, kFontStretch,
  kFontVariantCaps, kLetterSpacing, kTextRendering, kWordSpacing,
  kLang, kLineDash, kMatrix,
};
// After the six matrix entries: the clip list, an array of clip records
// [evenOdd, path2d, a b c d e f, path commands...] that clip() replaces (never
// mutates), so save()/restore() keep each state's clips.
inline constexpr int32_t kClips = kMatrix + 6;
inline constexpr int32_t kStateFields = kMatrix + 7;

// CanvasRenderingContext2D reserved slots.
inline constexpr int32_t kContextCanvasSlot = 0;
inline constexpr int32_t kContextStateSlot = 1;
inline constexpr int32_t kContextStackSlot = 2;
// The recorded path: a flat array of numeric commands. The path members
// append to it, so they are real operations rather than no-op stubs.
inline constexpr int32_t kContextPathSlot = 3;
inline constexpr int32_t kContextAlphaSlot = 4, kContextReadOftenSlot = 5;
inline constexpr int32_t kContextSlotCount = 6;

// ImageData reserved slots, plus the colorSpace string slot.
inline constexpr int32_t kImageWidthSlot = 0;
inline constexpr int32_t kImageHeightSlot = 1;
inline constexpr int32_t kImageDataSlot = 2;
inline constexpr int32_t kImageSlotCount = 3;

// OffscreenCanvas reserved slots: width, height and the 2D context.
inline constexpr int32_t kOffscreenSlotCount = 5;

// Every canvas keeps one RGBA byte store (a Uint8ClampedArray) in its binding, so
// getImageData/putImageData read and write the same bytes toDataURL/toBlob encode.
// The element and the bitmap are rooted for the binding's life; the realm owns the
// list and drops it at teardown, like the Document Model's per-element facades.
struct CanvasBinding {
  JS::PersistentRootedObject element;
  JS::PersistentRootedObject bitmap;
  JS::PersistentRootedObject context2d;
  JS::PersistentRootedObject bitmapContext;
  JS::PersistentRootedValue printCallback;
  uint32_t width = 0;
  uint32_t height = 0;
  int32_t mode = kModeNone;
  explicit CanvasBinding(JSContext* cx)
      : element(cx), bitmap(cx), context2d(cx), bitmapContext(cx), printCallback(cx, JS::NullValue()) {}
};

struct State {
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  JS::PersistentRootedObject canvasPrototype;
  WebFace context2d, imageData, offscreen;
  WebFace metrics, gradient, pattern, path2d, bitmap, bitmapRenderer, offscreen2d;
  std::vector<std::unique_ptr<CanvasBinding>> canvases;
  // The realm's one 32-byte PNG randomization key; see png_metadata.cpp.
  std::string pngMetadataKey;
  // The realm's font owner for text measurement, created on first use. It caches
  // the process-wide system font collection only, exactly like web_geometry's
  // own per-realm instance; the font selection rules stay in that domain.
  std::unique_ptr<web_geometry::SystemFonts> fonts;
  // Context port for the context kinds the Canvas owner does not implement.
  // Bound by the runtime coordinator after both owners are installed; while it
  // is unbound getContext keeps returning null for those kinds.
  // The getContext options value is handed over as given (the port owner
  // converts it to its own dictionary, so a conversion error throws there).
  bool (*contextPort)(void*, JS::HandleObject, JS::HandleString,
                      JS::HandleValue, JS::MutableHandleValue) = nullptr;
  void* contextPortContext = nullptr;
  // captureStream(): the Media owner's CanvasCaptureMediaStream factory.
  bool (*capturePort)(void*, JS::HandleObject, JS::MutableHandleObject) = nullptr;
  void* capturePortContext = nullptr;
  bool (*elementCheck)(void*, JS::HandleObject, bool*) = nullptr;
  // The inherited content language of a canvas element (its own or an
  // ancestor's lang/xml:lang), or of the document's root element when
  // `element` is null; "" when none. Unbound in a worker.
  bool (*langPort)(void*, JS::HandleObject element, std::string* lang) = nullptr;
  void* langPortContext = nullptr;
  void* elementCheckContext = nullptr;
  bool (*matrixFactory)(void*, const double*, JS::MutableHandleObject) = nullptr;
  void* matrixFactoryContext = nullptr;
  State(JSContext*, JS::HandleObject page, PageHelpers&, JS::HandleObject canvasPrototype,
        interaction::Realm&);
  ~State();
  CanvasBinding* Find(JSObject* element) const;
  CanvasBinding* FindOrCreate(JSObject* element);
};

// The TextMetrics slots, in the order the interface exposes them.
inline constexpr int32_t kMetricCount = 12;
extern const JSClass kContext2DClass, kImageDataClass, kOffscreenClass;
extern const JSClass kOffscreen2DClass;
extern const JSClass kMetricsClass, kGradientClass, kPatternClass, kPath2DClass;
extern const JSClass kBitmapClass, kBitmapRendererClass;
bool InstallTextMetrics(State&);
bool InstallCanvasResources(State&);
bool InstallContext2DExtra(State&, WebFace&);
bool InstallPath2D(State&);
bool InstallBitmap(State&);
bool InstallCanvasTransfers(State&);
bool InstallCanvasTransfersOn(State&, JS::HandleObject holder, const char* iface);
bool InstallOffscreenContext(State&);
bool InstallContextMembers(State&, WebFace&);
bool InstallCanvasFirefox(State&);
bool ContextSettings(JSContext*, unsigned, JS::Value*);
bool DrawFocus(JSContext*, unsigned, JS::Value*);
bool ReadContextOptions(JSContext*, JS::HandleObject, JS::HandleValue);
bool RoundRect(JSContext*, unsigned, JS::Value*);
bool InstallOffscreenEvents(State&);
bool Context2DExtra(JSContext*, unsigned, JS::Value*);
bool MeasureText(JSContext*, unsigned, JS::Value*);
bool CreateCanvasResource(JSContext*, unsigned, JS::Value*);
bool BitmapContext(State&, JS::HandleObject, JS::MutableHandleValue);
bool NewCanvasBitmap(State&, JS::HandleObject, JS::MutableHandleObject);
// image_bitmap_create.cpp: Window.createImageBitmap (slice 25).
bool CreateImageBitmap(JSContext*, unsigned, JS::Value*);
bool AppendCanvasPath(JSContext*, JS::HandleObject, const double*, uint32_t);
// context2d_paint.cpp: the fill operations that write pixels.
bool FillRecordedPath(State&, JS::HandleObject, JS::HandleObject, bool);
bool FillRecordedRect(State&, JS::HandleObject, const double*);
bool StrokeRecordedPath(State&, JS::HandleObject, JS::HandleObject);
bool StrokeRecordedRect(State&, JS::HandleObject, const double*);
bool MarkPathFinished(JSContext*, JS::HandleObject);
// clip() / clip(path, rule): appends the path as it is now to the clip list.
bool RecordClip(JSContext*, JS::HandleObject context, JS::HandleObject owner, bool evenOdd);
// clearRect(): DrawTargetSkia::ClearRect under the transform and clips.
bool ClearRecordedRect(State&, JS::HandleObject, const double*);
namespace skia { struct Clip; }
bool ReadCanvasClips(JSContext*, JS::HandleObject, std::vector<skia::Clip>*);
namespace skia { struct ShadowStyle; }
bool ReadCanvasShadow(JSContext*, JS::HandleObject, skia::ShadowStyle*, bool*);
// strokeText(): the text runs drawn with the stroke style through Skia.
bool StrokeRecordedText(State&, JS::HandleObject, const std::u16string&, double x, double y,
                        double maxWidth);
// fillText() through Skia; *handled is false when a run's font has colour
// glyphs, which the glyph-mask path (FillCanvasText) draws instead.
bool FillRecordedText(State&, JS::HandleObject, const std::u16string&, double x, double y,
                      double maxWidth, bool* handled);

// errors.cpp: the page realm's DOMException, the one error factory here.
bool ThrowDOMException(State& state, const char* name, const char* message);

// element.cpp: members installed on the Document Model's HTMLCanvasElement.
bool CanvasWidthGet(JSContext*, unsigned, JS::Value*);
bool CanvasWidthSet(JSContext*, unsigned, JS::Value*);
bool CanvasHeightGet(JSContext*, unsigned, JS::Value*);
bool CanvasHeightSet(JSContext*, unsigned, JS::Value*);
bool CanvasGetContext(JSContext*, unsigned, JS::Value*);
bool CanvasToDataURL(JSContext*, unsigned, JS::Value*);
bool CanvasToBlob(JSContext*, unsigned, JS::Value*);
bool InstallElement(State&);
// element_context.cpp: getContext/toDataURL/toBlob and the bitmap.
bool InstallElementContext(State&);

// context2d.cpp
bool Context2DCtor(JSContext*, unsigned, JS::Value*);
// The one CanvasRenderingContext2D factory; the context belongs to its realm.
JSObject* NewContext2D(State& state, JS::HandleObject canvas, JS::MutableHandleObject out);
// context2d_state.cpp / context2d_path.cpp register their members on the face.
bool InstallContext2DState(State&, WebFace&);
bool InstallContext2DPath(State&, WebFace&);
bool InstallContext2DFill(State&, WebFace&);
// |into| (optional) draws into that premultiplied RGBA surface instead of the
// canvas, source-over and shifted by (dx, dy): AdjustedTargetForShadow's temp.
struct TextTarget { uint8_t* pixels; int width, height; int dx, dy; };
bool FillCanvasText(State&, JS::HandleObject, const std::u16string&, double, double, double,
                    const TextTarget* into = nullptr);
// context2d_image.cpp: createImageData/getImageData/putImageData.
bool InstallImageMethods(State&, WebFace&);
bool Context2DCanvas(JSContext*, unsigned, JS::Value*);
bool InstallContext2D(State&);

// context2d_state.cpp: the drawing state, save/restore and the state accessors.
bool Context2DSave(JSContext*, unsigned, JS::Value*);
bool Context2DRestore(JSContext*, unsigned, JS::Value*);
bool Context2DStateGet(JSContext*, unsigned, JS::Value*);
bool Context2DStateSet(JSContext*, unsigned, JS::Value*);

// context2d_path.cpp: the recorded path and clearRect.
bool Context2DPathMethod(JSContext*, unsigned, JS::Value*);
// context2d_transform.cpp: the transform matrix methods.
bool Context2DTransformMethod(JSContext*, unsigned, JS::Value*);
bool InstallContext2DTransform(State&, WebFace&);

// image_data.cpp
// The one ImageData factory; the pixel array is a Uint8ClampedArray of w*h*4.
JSObject* NewImageData(State& state, int32_t width, int32_t height,
                       JS::MutableHandleObject out);
bool ImageDataCtor(JSContext*, unsigned, JS::Value*);
bool ImageDataGet(JSContext*, unsigned, JS::Value*);
bool CreateImageData(JSContext*, unsigned, JS::Value*);
bool GetImageData(JSContext*, unsigned, JS::Value*);
bool PutImageData(JSContext*, unsigned, JS::Value*);
bool InstallImageData(State&);

// offscreen.cpp
bool OffscreenCtor(JSContext*, unsigned, JS::Value*);
bool OffscreenGet(JSContext*, unsigned, JS::Value*);
bool OffscreenSet(JSContext*, unsigned, JS::Value*);
bool OffscreenConvertToBlob(JSContext*, unsigned, JS::Value*);
bool OffscreenGetContext(JSContext*, unsigned, JS::Value*);
bool InstallOffscreen(State&);
// offscreen.cpp: the bitmap dimensions of an OffscreenCanvas come from its slots.
bool OffscreenDimension(JSContext*, JS::HandleObject offscreen, const char* name,
                        uint32_t* out);
// internal use: the canvas whose 2D context is being created.
bool EnsureBitmap(State& state, JS::HandleObject canvas, CanvasBinding** out);
}  // namespace web_canvas
}  // namespace ruyijs
#endif
