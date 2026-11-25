#include "web_canvas/realm.h"
#include "web_canvas/internal.h"
#include "js/Realm.h"

#include "web_geometry/system_font.h"

namespace ruyijs::web_canvas {
State::State(JSContext* context, JS::HandleObject page, PageHelpers& helpers,
             JS::HandleObject proto, interaction::Realm& eventOwner)
    : cx(context), kit(context, page, helpers, this), events(eventOwner),
      canvasPrototype(context, proto), context2d(context), imageData(context),
      offscreen(context), metrics(context), gradient(context),
      pattern(context), path2d(context), bitmap(context), bitmapRenderer(context),
      offscreen2d(context) {}
State::~State() = default;

CanvasBinding* State::Find(JSObject* element) const {
  for (const auto& binding : canvases)
    if (binding->element == element) return binding.get();
  return nullptr;
}

CanvasBinding* State::FindOrCreate(JSObject* element) {
  if (CanvasBinding* found = Find(element)) return found;
  auto binding = std::make_unique<CanvasBinding>(cx);
  binding->element = element;
  canvases.push_back(std::move(binding));
  return canvases.back().get();
}

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             JS::HandleObject proto, interaction::Realm& events)
    : state_(std::make_unique<State>(cx, page, helpers, proto, events)) {}
Realm::~Realm() = default;

void Realm::SetContextPort(bool (*port)(void*, JS::HandleObject, JS::HandleString,
                                        JS::HandleValue, JS::MutableHandleValue), void* context) {
  state_->contextPort = port;
  state_->contextPortContext = context;
}

void Realm::SetLangPort(bool (*port)(void*, JS::HandleObject, std::string*), void* context) {
  state_->langPort = port;
  state_->langPortContext = context;
}
void Realm::SetCaptureStreamPort(bool (*port)(void*, JS::HandleObject, JS::MutableHandleObject),
                                 void* context) {
  state_->capturePort = port;
  state_->capturePortContext = context;
}

bool Realm::ElementSize(JS::HandleObject canvas, uint32_t* width, uint32_t* height) {
  JSAutoRealm realm(state_->cx, canvas);
  return detail::CanvasDimensions(*state_, canvas, width, height);
}

void Realm::SetElementCheck(bool (*port)(void*, JS::HandleObject, bool*), void* context) {
  state_->elementCheck = port;
  state_->elementCheckContext = context;
}

bool Realm::Install() {
  State& s = *state_;
  // The constructor, prototypes and objects all belong to the installing realm,
  // so an iframe gets its own canvas interfaces and never aliases the parent's.
  const JSAutoRealm realm(s.cx, s.kit.page);
  if (!s.canvasPrototype) return false;
  return InstallElement(s) && InstallElementContext(s) && InstallContext2D(s) &&
         InstallImageData(s) && InstallOffscreen(s) &&
         InstallTextMetrics(s) && InstallCanvasResources(s) && InstallPath2D(s) &&
         InstallBitmap(s) && InstallOffscreenContext(s) && InstallCanvasTransfers(s) &&
         InstallCanvasFirefox(s) && InstallOffscreenEvents(s);
}
// A Worker realm: OffscreenCanvas and its 2D context, ImageData, Path2D,
// TextMetrics, CanvasGradient/Pattern, ImageBitmap and createImageBitmap on
// WorkerGlobalScope.prototype. No HTMLCanvasElement exists there.
bool Realm::InstallWorker(JS::HandleObject scopePrototype) {
  State& s = *state_;
  const JSAutoRealm realm(s.cx, s.kit.page);
  return InstallImageData(s) && InstallOffscreen(s) && InstallTextMetrics(s) && InstallCanvasResources(s) &&
         InstallPath2D(s) && InstallBitmap(s) && InstallOffscreenContext(s) &&
         InstallCanvasTransfersOn(s, scopePrototype, "WorkerGlobalScope") && InstallOffscreenEvents(s);
}
void Realm::SetMatrixFactory(bool (*port)(void*, const double*, JS::MutableHandleObject), void* context) {
  state_->matrixFactory = port;
  state_->matrixFactoryContext = context;
}
bool Realm::DimensionsChanged(JS::HandleObject canvas, JS::HandleValue width,
                              JS::HandleValue height) {
  State& s = *state_;
  CanvasBinding* binding = s.Find(canvas);
  if (!binding) return true;  // No context yet: the size is read when one is created.
  const JSAutoRealm realm(s.cx, s.kit.page);
  uint32_t w = 0, h = 0;
  if (!detail::ParseDimension(s.cx, width, 300, &w) ||
      !detail::ParseDimension(s.cx, height, 150, &h) ||
      !detail::ResizeBitmap(s, *binding, w, h)) return false;
  if (binding->mode != kMode2D || !binding->context2d) return true;
  JS::RootedObject context(s.cx, binding->context2d);
  return detail::ResetContext2D(s, context);
}
}  // namespace ruyijs::web_canvas
