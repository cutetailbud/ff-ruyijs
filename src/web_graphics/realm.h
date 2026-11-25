#ifndef RUYIJS_WEB_GRAPHICS_REALM_H
#define RUYIJS_WEB_GRAPHICS_REALM_H
#include <cstdint>
#include <memory>
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_graphics {
struct State;
// The WebGL owner: WebGLRenderingContext, its object interfaces and the
// WEBGL_debug_renderer_info constants. The HTMLCanvasElement constructor and
// getContext stay with their existing owners; this realm reaches them through
// ports, so there is exactly one owner of each constructor and prototype.
// The canvas owner's extraction filter for readPixels RGBA/UNSIGNED_BYTE
// (ClientWebGLContext::ReadPixels -> PerturbWebGLReadPixelsRGBA8), bound by
// the runtime; null leaves the bytes as read.
using ReadPixelsFilter = void (*)(uint8_t* rgba, uint32_t width, uint32_t height,
                                  int32_t stride, int32_t originX, int32_t originY,
                                  int32_t yDirection);
void SetReadPixelsFilter(ReadPixelsFilter filter);
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        browser_environment::Environment& environment);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  // The WebGL state the canvas context port binds to. The realm owns it; the
  // port's void* context is this State*, never the Realm wrapper.
  State* state() const noexcept;
 private:
  std::unique_ptr<State> state_;
};

// The canvas context port, bound by the runtime coordinator after both owners
// are installed: getContext("webgl")/("experimental-webgl") reach this realm
// through it, and every other kind stays with the Canvas owner.
bool ContextPort(void* context, JS::HandleObject canvas, JS::HandleString kind,
                 JS::HandleValue options, JS::MutableHandleValue out);
}  // namespace ruyijs::web_graphics
#endif